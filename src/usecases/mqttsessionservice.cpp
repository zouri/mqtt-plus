#include "mqttsessionservice.h"

#include "usecases/eventhistoryservice.h"
#include "usecases/subscriptionservice.h"
#include "services/apputils.h"
#include "domain/sessionconfig.h"
#include "services/payload/payloadcodec.h"

#include <QSslCertificate>
#include <QSslSocket>

#include <algorithm>

using namespace AppUtils;

namespace {
QString clientErrorLogName(QMqttClient::ClientError error)
{
    switch (error) {
    case QMqttClient::NoError:
        return QStringLiteral("No error");
    case QMqttClient::InvalidProtocolVersion:
        return QStringLiteral("Protocol version rejected by broker");
    case QMqttClient::IdRejected:
        return QStringLiteral("Client ID rejected");
    case QMqttClient::ServerUnavailable:
        return QStringLiteral("Broker unavailable");
    case QMqttClient::BadUsernameOrPassword:
        return QStringLiteral("Username or password rejected");
    case QMqttClient::NotAuthorized:
        return QStringLiteral("Not authorized");
    case QMqttClient::TransportInvalid:
        return QStringLiteral("Invalid transport");
    case QMqttClient::ProtocolViolation:
        return QStringLiteral("Protocol violation");
    case QMqttClient::UnknownError:
        return QStringLiteral("Unknown MQTT error");
    case QMqttClient::Mqtt5SpecificError:
        return QStringLiteral("MQTT 5 broker reported an error");
    }
    return QStringLiteral("MQTT error");
}
}

MqttSessionService::MqttSessionService(QObject *parent)
    : QObject(parent)
{
}

void MqttSessionService::setDependencies(const Dependencies &dependencies)
{
    m_dependencies = dependencies;
}

void MqttSessionService::connectCurrentSession()
{
    auto *session = m_dependencies.currentSessionState();
    auto *client = session ? session->runtime.client : nullptr;
    if (!session || !client) {
        return;
    }

    if (client->hostname().trimmed().isEmpty()) {
        session->runtime.lastError = tr("Broker host cannot be empty.");
        m_dependencies.appendEvent(*session, QStringLiteral("Connection"), QStringLiteral("Broker host cannot be empty."));
        if (m_dependencies.refreshModels) {
            m_dependencies.refreshModels();
        }
        emit sessionStateChanged();
        return;
    }

    if (client->clientId().trimmed().isEmpty()) {
        session->runtime.lastError = tr("Client ID cannot be empty.");
        m_dependencies.appendEvent(*session, QStringLiteral("Connection"), QStringLiteral("Client ID cannot be empty."));
        if (m_dependencies.refreshModels) {
            m_dependencies.refreshModels();
        }
        emit sessionStateChanged();
        return;
    }

    session->runtime.disconnectRequested = false;
    session->runtime.sessionRestored = false;
    session->runtime.lastError.clear();
    updatePublishStatus(*session, QStringLiteral("idle"));
    connectSession(*session, QStringLiteral("Connecting to"));

    if (m_dependencies.refreshModels) {
        m_dependencies.refreshModels();
    }
    emit sessionStateChanged();
}

void MqttSessionService::disconnectCurrentSession()
{
    auto *session = m_dependencies.currentSessionState();
    auto *client = session ? session->runtime.client : nullptr;
    if (!session || !client) {
        return;
    }

    session->runtime.disconnectRequested = true;
    if (session->runtime.connectTimeoutTimer) {
        session->runtime.connectTimeoutTimer->stop();
    }
    client->disconnectFromHost();
    if (m_dependencies.refreshModels) {
        m_dependencies.refreshModels();
    }
    emit sessionStateChanged();
}

void MqttSessionService::publishCurrentSession(
    const QString &topic,
    const QString &payload,
    int format,
    int qos,
    bool retain)
{
    auto *session = m_dependencies.currentSessionState();
    auto *client = session ? session->runtime.client : nullptr;
    if (!session || !client) {
        return;
    }

    const QString trimmedTopic = topic.trimmed();
    if (trimmedTopic.isEmpty()) {
        m_dependencies.appendEvent(*session, QStringLiteral("Publish"), QStringLiteral("Topic cannot be empty."));
        return;
    }

    const QMqttTopicName topicName(trimmedTopic);
    if (!topicName.isValid()) {
        m_dependencies.appendEvent(*session, QStringLiteral("Publish"), QStringLiteral("Invalid topic name: %1").arg(trimmedTopic));
        return;
    }

    if (client->state() != QMqttClient::Connected) {
        m_dependencies.appendEvent(*session, QStringLiteral("Publish"), QStringLiteral("Connect before publishing."));
        return;
    }

    QByteArray payloadBytes;
    QString error;
    const PayloadFormat payloadFormat = PayloadCodec::formatFromInt(format);
    if (!PayloadCodec::encodeForPublish(payloadFormat, payload, payloadBytes, error)) {
        m_dependencies.appendEvent(
            *session,
            QStringLiteral("Publish"),
            QStringLiteral("%1 (%2)").arg(error).arg(PayloadCodec::formatName(payloadFormat)));
        return;
    }

    PublishStatus status;
    status.state = QStringLiteral("queued");
    status.topic = trimmedTopic;
    status.qos = SessionConfig::sanitizeQos(qos);
    status.retain = retain;
    status.format = format;
    status.formatName = PayloadCodec::formatName(payloadFormat);
    status.updatedAt = timestampNow();
    session->runtime.publishStatus = status;

    const qint32 messageId = client->publish(topicName, payloadBytes, SessionConfig::sanitizeQos(qos), retain);
    if (messageId < 0) {
        updatePublishStatus(*session, QStringLiteral("failed"), tr("Qt MQTT rejected the publish request."));
        m_dependencies.appendEvent(*session, QStringLiteral("Publish"), QStringLiteral("Publish rejected for %1").arg(trimmedTopic));
    } else {
        updatePublishStatus(*session, QStringLiteral("queued"), QString(), messageId);
        (*m_dependencies.eventController).appendPublishedMessage(session->id, trimmedTopic, payloadBytes, format);
        m_dependencies.appendEvent(
            *session,
            QStringLiteral("Publish"),
            QStringLiteral("Queued %1 (QoS %2%3)")
                .arg(trimmedTopic)
                .arg(SessionConfig::sanitizeQos(qos))
                .arg(retain ? QStringLiteral(", retain") : QString()));
    }

    if (m_dependencies.refreshModels) {
        m_dependencies.refreshModels();
    }
    emit sessionStateChanged();
}

void MqttSessionService::bindSessionSignals(SessionState *session)
{
    auto *client = session ? session->runtime.client : nullptr;
    if (!session || !client) {
        return;
    }

    connect(client, &QMqttClient::connected, this, [this, sessionId = session->id]() {
        if (auto *boundSession = m_dependencies.sessionById(sessionId)) {
            if (boundSession->runtime.connectTimeoutTimer) {
                boundSession->runtime.connectTimeoutTimer->stop();
            }
            const auto *boundClient = boundSession->runtime.client;
            boundSession->runtime.disconnectRequested = false;
            boundSession->runtime.lastError.clear();
            boundSession->runtime.brokerInfo =
                QStringLiteral("%1 • %2 • client %3")
                    .arg(protocolVersionLabel(boundSession->protocolVersion))
                    .arg(transportLabel(boundSession->transport))
                    .arg(boundClient ? boundClient->clientId() : QString());
            m_dependencies.appendEvent(*boundSession, QStringLiteral("Connection"), QStringLiteral("Connected to broker"));
            (*m_dependencies.subscriptionController).restoreActiveSubscriptions(*boundSession, false);
        }

        if (m_dependencies.refreshModels) {
            m_dependencies.refreshModels();
        }
        emit sessionStateChanged();
    });

    connect(client, &QMqttClient::disconnected, this, [this, sessionId = session->id]() {
        if (auto *boundSession = m_dependencies.sessionById(sessionId)) {
            if (boundSession->runtime.connectTimeoutTimer) {
                boundSession->runtime.connectTimeoutTimer->stop();
            }
            const QString message = boundSession->runtime.disconnectRequested
                ? QStringLiteral("Disconnected")
                : QStringLiteral("Connection closed by broker");
            boundSession->runtime.disconnectRequested = false;
            (*m_dependencies.subscriptionController).resetRuntimeSubscriptions(*boundSession);
            m_dependencies.appendEvent(*boundSession, QStringLiteral("Connection"), message);
        }

        if (m_dependencies.refreshModels) {
            m_dependencies.refreshModels();
        }
        emit sessionStateChanged();
    });

    connect(client, &QMqttClient::stateChanged, this, [this]() {
        if (m_dependencies.refreshModels) {
            m_dependencies.refreshModels();
        }
        emit sessionStateChanged();
    });

    connect(
        client,
        &QMqttClient::errorChanged,
        this,
        [this, sessionId = session->id](QMqttClient::ClientError error) {
            if (error == QMqttClient::NoError) {
                return;
            }

            if (auto *boundSession = m_dependencies.sessionById(sessionId)) {
                QString message = clientErrorName(error);
                QString logMessage = clientErrorLogName(error);
                const QString socketText = socketDiagnostic(boundSession->runtime.client);
                if (!socketText.isEmpty() && socketText != message) {
                    message = QStringLiteral("%1 (%2)").arg(message).arg(socketText);
                }
                if (!socketText.isEmpty() && socketText != logMessage) {
                    logMessage = QStringLiteral("%1 (%2)").arg(logMessage).arg(socketText);
                }
                boundSession->runtime.lastError = message;
                m_dependencies.appendEvent(*boundSession, QStringLiteral("Error"), logMessage);
            }

            if (m_dependencies.refreshModels) {
                m_dependencies.refreshModels();
            }
            emit sessionStateChanged();
        });

    connect(client, &QMqttClient::brokerSessionRestored, this, [this, sessionId = session->id]() {
        if (auto *boundSession = m_dependencies.sessionById(sessionId)) {
            boundSession->runtime.sessionRestored = true;
            m_dependencies.appendEvent(*boundSession, QStringLiteral("Connection"), QStringLiteral("Broker session restored"));
        }
        if (m_dependencies.refreshModels) {
            m_dependencies.refreshModels();
        }
        emit sessionStateChanged();
    });

    connect(
        client,
        &QMqttClient::messageReceived,
        this,
        [this, sessionId = session->id](const QByteArray &message, const QMqttTopicName &topic) {
            (*m_dependencies.eventController).appendIncomingMessage(sessionId, topic.name(), message);
        });

    connect(client, &QMqttClient::messageSent, this, [this, sessionId = session->id](qint32 messageId) {
        if (auto *boundSession = m_dependencies.sessionById(sessionId)) {
            if (boundSession->runtime.publishStatus.messageId == messageId) {
                updatePublishStatus(*boundSession, QStringLiteral("sent"), QString(), messageId);
            }
        }
        if (m_dependencies.refreshModels) {
            m_dependencies.refreshModels();
        }
        emit sessionStateChanged();
    });

    connect(
        client,
        &QMqttClient::messageStatusChanged,
        this,
        [this, sessionId = session->id](
            qint32 messageId,
            QMqtt::MessageStatus status,
            const QMqttMessageStatusProperties &properties) {
            if (auto *boundSession = m_dependencies.sessionById(sessionId)) {
                if (boundSession->runtime.publishStatus.messageId != messageId) {
                    return;
                }

                QString reason = properties.reason();
                if (reason.isEmpty()) {
                    reason = boundSession->runtime.publishStatus.reason;
                }
                updatePublishStatus(*boundSession, messageStatusName(status), reason, messageId);
            }
            if (m_dependencies.refreshModels) {
                m_dependencies.refreshModels();
            }
            emit sessionStateChanged();
        });

    connect(client, &QMqttClient::pingResponseReceived, this, [this, sessionId = session->id]() {
        if (auto *boundSession = m_dependencies.sessionById(sessionId)) {
            boundSession->runtime.brokerInfo =
                QStringLiteral("%1 • %2 • ping ok")
                    .arg(protocolVersionLabel(boundSession->protocolVersion))
                    .arg(transportLabel(boundSession->transport));
        }
        if (m_dependencies.refreshModels) {
            m_dependencies.refreshModels();
        }
        emit sessionStateChanged();
    });
}

void MqttSessionService::connectSession(SessionState &session, const QString &eventPrefix)
{
    auto *client = session.runtime.client;
    if (!client) {
        return;
    }

    if (session.runtime.connectTimeoutTimer) {
        session.runtime.connectTimeoutTimer->start((std::max)(1, session.connectTimeoutSeconds) * 1000);
    }

    m_dependencies.appendEvent(
        session,
        QStringLiteral("Connection"),
        QStringLiteral("%1 %2:%3 over %4 using %5")
            .arg(eventPrefix)
            .arg(client->hostname())
            .arg(client->port())
            .arg(transportLabel(session.transport))
            .arg(protocolVersionLabel(session.protocolVersion)));

    if (session.transport == QStringLiteral("tls")) {
        QString tlsError;
        const QSslConfiguration configuration = sslConfigurationForSession(session, tlsError);
        if (!tlsError.isEmpty()) {
            if (session.runtime.connectTimeoutTimer) {
                session.runtime.connectTimeoutTimer->stop();
            }
            session.runtime.lastError = tlsError;
            m_dependencies.appendEvent(session, QStringLiteral("Error"), tlsError);
            return;
        }
        client->connectToHostEncrypted(configuration);
    } else {
        client->connectToHost();
    }
}

QSslConfiguration MqttSessionService::sslConfigurationForSession(const SessionState &session, QString &errorMessage) const
{
    QSslConfiguration configuration = QSslConfiguration::defaultConfiguration();
    configuration.setPeerVerifyMode(session.sslSecure ? QSslSocket::AutoVerifyPeer : QSslSocket::VerifyNone);

    const QList<QByteArray> protocols = alpnProtocols(session.alpn);
    if (!protocols.isEmpty()) {
        configuration.setAllowedNextProtocols(protocols);
    }

    const QString caPath = session.caFile.trimmed();
    if (!caPath.isEmpty()) {
        const QList<QSslCertificate> certificates = QSslCertificate::fromPath(caPath);
        if (certificates.isEmpty()) {
            errorMessage = QStringLiteral("CA certificate file could not be loaded.");
            return configuration;
        }
        configuration.setCaCertificates(certificates);
    }

    const QString certificatePath = session.clientCertificateFile.trimmed();
    if (!certificatePath.isEmpty()) {
        const QList<QSslCertificate> certificates = QSslCertificate::fromPath(certificatePath);
        if (certificates.isEmpty()) {
            errorMessage = QStringLiteral("Client certificate file could not be loaded.");
            return configuration;
        }
        configuration.setLocalCertificate(certificates.first());
    }

    const QString keyPath = session.clientKeyFile.trimmed();
    if (!keyPath.isEmpty()) {
        const QSslKey privateKey = readPrivateKey(keyPath);
        if (privateKey.isNull()) {
            errorMessage = QStringLiteral("Client key file could not be loaded.");
            return configuration;
        }
        configuration.setPrivateKey(privateKey);
    }

    return configuration;
}

void MqttSessionService::updatePublishStatus(
    SessionState &session,
    const QString &state,
    const QString &reason,
    qint32 messageId)
{
    session.runtime.publishStatus.state = state;
    session.runtime.publishStatus.reason = reason;
    session.runtime.publishStatus.updatedAt = timestampNow();
    if (messageId >= 0) {
        session.runtime.publishStatus.messageId = messageId;
    }
}
