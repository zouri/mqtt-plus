#include "mqttsessionservice.h"

#include "usecases/eventhistoryservice.h"
#include "usecases/sessionservice.h"
#include "usecases/subscriptionservice.h"
#include "services/apputils.h"
#include "domain/sessionconfig.h"
#include "services/payload/payloadcodec.h"

#include <QSslCertificate>
#include <QSslSocket>
#include <QDateTime>

#include <algorithm>

using namespace AppUtils;

MqttSessionService::MqttSessionService(
    SessionService &sessionService,
    SubscriptionService &subscriptionService,
    EventHistoryService &eventHistoryService,
    QObject *parent)
    : QObject(parent)
    , m_sessionService(sessionService)
    , m_subscriptionService(subscriptionService)
    , m_eventHistoryService(eventHistoryService)
{
}

void MqttSessionService::connectCurrentSession()
{
    auto *session = m_sessionService.currentSession();
    auto *client = session ? session->runtime.client : nullptr;
    if (!session || !client) {
        return;
    }

    if (client->hostname().trimmed().isEmpty()) {
        const QString message = QStringLiteral("Broker host cannot be empty.");
        session->runtime.lastError = message;
        m_eventHistoryService.appendEvent(*session, QStringLiteral("Connection"), message);
        emit sessionStateChanged();
        return;
    }

    if (client->clientId().trimmed().isEmpty()) {
        const QString message = QStringLiteral("Client ID cannot be empty.");
        session->runtime.lastError = message;
        m_eventHistoryService.appendEvent(*session, QStringLiteral("Connection"), message);
        emit sessionStateChanged();
        return;
    }

    session->runtime.disconnectRequested = false;
    session->runtime.reconnectPending = false;
    session->runtime.sessionRestored = false;
    session->runtime.lastError.clear();
    updatePublishStatus(*session, QStringLiteral("idle"));
    connectSession(*session, QStringLiteral("Connecting to"));

    emit sessionStateChanged();
}

void MqttSessionService::disconnectCurrentSession()
{
    auto *session = m_sessionService.currentSession();
    auto *client = session ? session->runtime.client : nullptr;
    if (!session || !client) {
        return;
    }

    session->runtime.disconnectRequested = true;
    session->runtime.reconnectPending = false;
    if (session->runtime.connectTimeoutTimer) {
        session->runtime.connectTimeoutTimer->stop();
    }
    client->disconnectFromHost();
    emit sessionStateChanged();
}

bool MqttSessionService::publishCurrentSession(
    const QString &topic,
    const QString &payload,
    int format,
    int qos,
    bool retain)
{
    auto *session = m_sessionService.currentSession();
    auto *client = session ? session->runtime.client : nullptr;
    if (!session || !client) {
        return false;
    }

    const QString trimmedTopic = topic.trimmed();
    if (trimmedTopic.isEmpty()) {
        m_eventHistoryService.appendEvent(
            *session,
            QStringLiteral("Publish"),
            QStringLiteral("Topic cannot be empty."));
        return false;
    }

    const QMqttTopicName topicName(trimmedTopic);
    if (!topicName.isValid()) {
        m_eventHistoryService.appendEvent(
            *session,
            QStringLiteral("Publish"),
            QStringLiteral("Invalid topic name: %1").arg(trimmedTopic));
        return false;
    }

    if (client->state() != QMqttClient::Connected) {
        m_eventHistoryService.appendEvent(
            *session,
            QStringLiteral("Publish"),
            QStringLiteral("Connect before publishing."));
        return false;
    }

    QByteArray payloadBytes;
    QString error;
    const PayloadFormat payloadFormat = PayloadCodec::formatFromInt(format);
    if (!PayloadCodec::encodeForPublish(payloadFormat, payload, payloadBytes, error)) {
        m_eventHistoryService.appendEvent(
            *session,
            QStringLiteral("Publish"),
            QStringLiteral("%1 (%2)").arg(error).arg(PayloadCodec::formatName(payloadFormat)));
        return false;
    }

    const int publishQos = SessionConfig::sanitizeQos(qos);
    PublishStatus status;
    status.state = QStringLiteral("queued");
    status.topic = trimmedTopic;
    status.qos = publishQos;
    status.retain = retain;
    status.format = format;
    status.formatName = PayloadCodec::formatName(payloadFormat);
    status.updatedAt = timestampNow();
    session->runtime.publishStatus = status;

    const qint32 messageId = client->publish(topicName, payloadBytes, publishQos, retain);
    if (messageId < 0) {
        updatePublishStatus(*session, QStringLiteral("failed"), tr("Qt MQTT rejected the publish request."));
        m_eventHistoryService.appendEvent(
            *session,
            QStringLiteral("Publish"),
            QStringLiteral("Publish rejected for %1").arg(trimmedTopic));
    } else {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        session->runtime.recentPublishedTimestampsMs.append(nowMs);
        while (!session->runtime.recentPublishedTimestampsMs.isEmpty()
               && session->runtime.recentPublishedTimestampsMs.constFirst() < nowMs - 1000) {
            session->runtime.recentPublishedTimestampsMs.removeFirst();
        }
        updatePublishStatus(
            *session,
            publishQos == 0 ? QStringLiteral("sent") : QStringLiteral("queued"),
            QString(),
            messageId);
        m_eventHistoryService.appendPublishedMessage(
            session->id,
            trimmedTopic,
            payloadBytes,
            format,
            status.qos,
            retain);
        m_eventHistoryService.appendEvent(
            *session,
            QStringLiteral("Publish"),
            QStringLiteral("Queued %1 (QoS %2%3)")
                .arg(trimmedTopic)
                .arg(publishQos)
                .arg(retain ? QStringLiteral(", retain") : QString()));
    }

    emit sessionStateChanged();
    return messageId >= 0;
}

void MqttSessionService::bindSessionSignals(SessionState *session)
{
    auto *client = session ? session->runtime.client : nullptr;
    if (!session || !client) {
        return;
    }

    connect(client, &QMqttClient::connected, this, [this, sessionId = session->id]() {
        if (auto *boundSession = m_sessionService.sessionById(sessionId)) {
            if (boundSession->runtime.connectTimeoutTimer) {
                boundSession->runtime.connectTimeoutTimer->stop();
            }
            const auto *boundClient = boundSession->runtime.client;
            boundSession->runtime.disconnectRequested = false;
            boundSession->runtime.reconnectPending = false;
            boundSession->runtime.connectedAtMs = QDateTime::currentMSecsSinceEpoch();
            boundSession->runtime.connectionStartedAtMs = 0;
            boundSession->runtime.lastError.clear();
            boundSession->runtime.brokerInfo =
                QStringLiteral("%1 • %2 • client %3")
                    .arg(protocolVersionLabel(boundSession->protocolVersion))
                    .arg(transportLabel(boundSession->transport))
                    .arg(boundClient ? boundClient->clientId() : QString());
            m_eventHistoryService.appendEvent(
                *boundSession,
                QStringLiteral("Connection"),
                QStringLiteral("Connected to broker"));
            m_subscriptionService.restoreActiveSubscriptions(*boundSession, false);
        }

        emit sessionStateChanged();
    });

    connect(client, &QMqttClient::disconnected, this, [this, sessionId = session->id]() {
        if (auto *boundSession = m_sessionService.sessionById(sessionId)) {
            const bool reconnect = boundSession->runtime.reconnectPending;
            if (boundSession->runtime.connectTimeoutTimer) {
                boundSession->runtime.connectTimeoutTimer->stop();
            }
            const QString message = boundSession->runtime.disconnectRequested
                ? QStringLiteral("Disconnected")
                : QStringLiteral("Connection closed by broker");
            boundSession->runtime.disconnectRequested = false;
            boundSession->runtime.reconnectPending = false;
            boundSession->runtime.connectedAtMs = 0;
            boundSession->runtime.connectionStartedAtMs = 0;
            m_subscriptionService.resetRuntimeSubscriptions(*boundSession);
            m_eventHistoryService.appendEvent(
                *boundSession,
                QStringLiteral("Connection"),
                message);
            if (reconnect) {
                connectSession(*boundSession, QStringLiteral("Connecting to"));
            }
        }

        emit sessionStateChanged();
    });

    connect(client, &QMqttClient::stateChanged, this, [this]() {
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

            if (auto *boundSession = m_sessionService.sessionById(sessionId)) {
                QString message = clientErrorName(error);
                const QString socketText = socketDiagnostic(boundSession->runtime.client);
                if (!socketText.isEmpty() && socketText != message) {
                    message = QStringLiteral("%1 (%2)").arg(message).arg(socketText);
                }
                boundSession->runtime.lastError = message;
                m_eventHistoryService.appendEvent(
                    *boundSession,
                    QStringLiteral("Error"),
                    message);
            }

            emit sessionStateChanged();
        });

    connect(client, &QMqttClient::brokerSessionRestored, this, [this, sessionId = session->id]() {
        if (auto *boundSession = m_sessionService.sessionById(sessionId)) {
            boundSession->runtime.sessionRestored = true;
            m_eventHistoryService.appendEvent(
                *boundSession,
                QStringLiteral("Connection"),
                QStringLiteral("Broker session restored"));
        }
        emit sessionStateChanged();
    });

    connect(
        client,
        &QMqttClient::messageReceived,
        this,
        [this, sessionId = session->id](const QByteArray &message, const QMqttTopicName &topic) {
            m_eventHistoryService.appendIncomingMessage(sessionId, topic.name(), message);
        });

    connect(client, &QMqttClient::messageSent, this, [this, sessionId = session->id](qint32 messageId) {
        if (auto *boundSession = m_sessionService.sessionById(sessionId)) {
            if (boundSession->runtime.publishStatus.messageId == messageId) {
                updatePublishStatus(*boundSession, QStringLiteral("sent"), QString(), messageId);
            }
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
            if (auto *boundSession = m_sessionService.sessionById(sessionId)) {
                if (boundSession->runtime.publishStatus.messageId != messageId) {
                    return;
                }

                QString reason = properties.reason();
                if (reason.isEmpty()) {
                    reason = boundSession->runtime.publishStatus.reason;
                }
                updatePublishStatus(*boundSession, messageStatusName(status), reason, messageId);
            }
            emit sessionStateChanged();
        });

    connect(client, &QMqttClient::pingResponseReceived, this, [this, sessionId = session->id]() {
        if (auto *boundSession = m_sessionService.sessionById(sessionId)) {
            boundSession->runtime.brokerInfo =
                QStringLiteral("%1 • %2 • ping ok")
                    .arg(protocolVersionLabel(boundSession->protocolVersion))
                    .arg(transportLabel(boundSession->transport));
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

    session.runtime.connectionStartedAtMs = QDateTime::currentMSecsSinceEpoch();
    session.runtime.connectedAtMs = 0;

    if (session.runtime.connectTimeoutTimer) {
        session.runtime.connectTimeoutTimer->start((std::max)(1, session.connectTimeoutSeconds) * 1000);
    }

    m_eventHistoryService.appendEvent(
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
            session.runtime.connectionStartedAtMs = 0;
            m_eventHistoryService.appendEvent(session, QStringLiteral("Error"), tlsError);
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
