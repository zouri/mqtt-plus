#include "mqttcontroller.h"

#include "app/appfacadeutils.h"
#include "domain/sessionconfig.h"
#include "services/payload/payloadcodec.h"

#include <QSslCertificate>
#include <QSslSocket>

#include <algorithm>
#include <utility>

using namespace AppFacadeUtils;

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

MqttController::MqttController(QObject *parent)
    : QObject(parent)
{
}

void MqttController::setDependencies(Dependencies dependencies)
{
    m_dependencies = std::move(dependencies);
}

void MqttController::connectCurrentSession()
{
    auto *session = m_dependencies.currentSession ? m_dependencies.currentSession() : nullptr;
    auto *client = session ? session->client : nullptr;
    if (!session || !client) {
        return;
    }

    if (client->hostname().trimmed().isEmpty()) {
        session->lastError = tr("Broker host cannot be empty.");
        if (m_dependencies.appendEvent) {
            m_dependencies.appendEvent(
                *session,
                QStringLiteral("Connection"),
                QStringLiteral("Broker host cannot be empty."));
        }
        if (m_dependencies.notifySessionViewsChanged) {
            m_dependencies.notifySessionViewsChanged();
        }
        return;
    }

    if (client->clientId().trimmed().isEmpty()) {
        session->lastError = tr("Client ID cannot be empty.");
        if (m_dependencies.appendEvent) {
            m_dependencies.appendEvent(
                *session,
                QStringLiteral("Connection"),
                QStringLiteral("Client ID cannot be empty."));
        }
        if (m_dependencies.notifySessionViewsChanged) {
            m_dependencies.notifySessionViewsChanged();
        }
        return;
    }

    session->disconnectRequested = false;
    session->sessionRestored = false;
    session->lastError.clear();
    updatePublishStatus(*session, QStringLiteral("idle"));
    connectSession(*session, QStringLiteral("Connecting to"));

    if (m_dependencies.notifySessionViewsChanged) {
        m_dependencies.notifySessionViewsChanged();
    }
}

void MqttController::disconnectCurrentSession()
{
    auto *session = m_dependencies.currentSession ? m_dependencies.currentSession() : nullptr;
    auto *client = session ? session->client : nullptr;
    if (!session || !client) {
        return;
    }

    session->disconnectRequested = true;
    if (session->connectTimeoutTimer) {
        session->connectTimeoutTimer->stop();
    }
    client->disconnectFromHost();
    if (m_dependencies.notifySessionViewsChanged) {
        m_dependencies.notifySessionViewsChanged();
    }
}

void MqttController::publishCurrentSession(
    const QString &topic,
    const QString &payload,
    int format,
    int qos,
    bool retain)
{
    auto *session = m_dependencies.currentSession ? m_dependencies.currentSession() : nullptr;
    auto *client = session ? session->client : nullptr;
    if (!session || !client) {
        return;
    }

    const QString trimmedTopic = topic.trimmed();
    if (trimmedTopic.isEmpty()) {
        if (m_dependencies.appendEvent) {
            m_dependencies.appendEvent(*session, QStringLiteral("Publish"), QStringLiteral("Topic cannot be empty."));
        }
        return;
    }

    const QMqttTopicName topicName(trimmedTopic);
    if (!topicName.isValid()) {
        if (m_dependencies.appendEvent) {
            m_dependencies.appendEvent(
                *session,
                QStringLiteral("Publish"),
                QStringLiteral("Invalid topic name: %1").arg(trimmedTopic));
        }
        return;
    }

    if (client->state() != QMqttClient::Connected) {
        if (m_dependencies.appendEvent) {
            m_dependencies.appendEvent(*session, QStringLiteral("Publish"), QStringLiteral("Connect before publishing."));
        }
        return;
    }

    QByteArray payloadBytes;
    QString error;
    const PayloadFormat payloadFormat = PayloadCodec::formatFromInt(format);
    if (!PayloadCodec::encodeForPublish(payloadFormat, payload, payloadBytes, error)) {
        if (m_dependencies.appendEvent) {
            m_dependencies.appendEvent(
                *session,
                QStringLiteral("Publish"),
                QStringLiteral("%1 (%2)").arg(error).arg(PayloadCodec::formatName(payloadFormat)));
        }
        return;
    }

    QVariantMap status = defaultPublishStatus();
    status.insert(QStringLiteral("state"), QStringLiteral("queued"));
    status.insert(QStringLiteral("topic"), trimmedTopic);
    status.insert(QStringLiteral("qos"), SessionConfig::sanitizeQos(qos));
    status.insert(QStringLiteral("retain"), retain);
    status.insert(QStringLiteral("format"), format);
    status.insert(QStringLiteral("formatName"), PayloadCodec::formatName(payloadFormat));
    status.insert(QStringLiteral("reason"), QString());
    status.insert(QStringLiteral("updatedAt"), timestampNow());
    session->publishStatus = status;

    const qint32 messageId = client->publish(topicName, payloadBytes, SessionConfig::sanitizeQos(qos), retain);
    if (messageId < 0) {
        updatePublishStatus(*session, QStringLiteral("failed"), tr("Qt MQTT rejected the publish request."));
        if (m_dependencies.appendEvent) {
            m_dependencies.appendEvent(
                *session,
                QStringLiteral("Publish"),
                QStringLiteral("Publish rejected for %1").arg(trimmedTopic));
        }
    } else {
        updatePublishStatus(*session, QStringLiteral("queued"), QString(), messageId);
        if (m_dependencies.appendEvent) {
            m_dependencies.appendEvent(
                *session,
                QStringLiteral("Publish"),
                QStringLiteral("Queued %1 (QoS %2%3)")
                    .arg(trimmedTopic)
                    .arg(SessionConfig::sanitizeQos(qos))
                    .arg(retain ? QStringLiteral(", retain") : QString()));
        }
    }

    if (m_dependencies.notifyCurrentSessionViewsChanged) {
        m_dependencies.notifyCurrentSessionViewsChanged();
    }
}

void MqttController::bindSessionSignals(SessionState *session)
{
    auto *client = session ? session->client : nullptr;
    if (!session || !client) {
        return;
    }

    connect(client, &QMqttClient::connected, this, [this, sessionId = session->id]() {
        if (auto *boundSession = m_dependencies.sessionById ? m_dependencies.sessionById(sessionId) : nullptr) {
            if (boundSession->connectTimeoutTimer) {
                boundSession->connectTimeoutTimer->stop();
            }
            const auto *boundClient = boundSession->client;
            boundSession->disconnectRequested = false;
            boundSession->lastError.clear();
            boundSession->brokerInfo =
                QStringLiteral("%1 • %2 • client %3")
                    .arg(protocolVersionLabel(boundSession->protocolVersion))
                    .arg(transportLabel(boundSession->transport))
                    .arg(boundClient ? boundClient->clientId() : QString());
            if (m_dependencies.appendEvent) {
                m_dependencies.appendEvent(
                    *boundSession,
                    QStringLiteral("Connection"),
                    QStringLiteral("Connected to broker"));
            }
            if (m_dependencies.restoreActiveSubscriptions) {
                m_dependencies.restoreActiveSubscriptions(*boundSession, false);
            }
        }

        if (m_dependencies.notifySessionAndSubscriptionViewsChanged) {
            m_dependencies.notifySessionAndSubscriptionViewsChanged();
        }
    });

    connect(client, &QMqttClient::disconnected, this, [this, sessionId = session->id]() {
        if (auto *boundSession = m_dependencies.sessionById ? m_dependencies.sessionById(sessionId) : nullptr) {
            if (boundSession->connectTimeoutTimer) {
                boundSession->connectTimeoutTimer->stop();
            }
            const QString message = boundSession->disconnectRequested
                ? QStringLiteral("Disconnected")
                : QStringLiteral("Connection closed by broker");
            boundSession->disconnectRequested = false;
            if (m_dependencies.resetRuntimeSubscriptions) {
                m_dependencies.resetRuntimeSubscriptions(*boundSession);
            }
            if (m_dependencies.appendEvent) {
                m_dependencies.appendEvent(*boundSession, QStringLiteral("Connection"), message);
            }
        }

        if (m_dependencies.notifySessionAndSubscriptionViewsChanged) {
            m_dependencies.notifySessionAndSubscriptionViewsChanged();
        }
    });

    connect(client, &QMqttClient::stateChanged, this, [this]() {
        if (m_dependencies.notifySessionAndSubscriptionViewsChanged) {
            m_dependencies.notifySessionAndSubscriptionViewsChanged();
        }
    });

    connect(
        client,
        &QMqttClient::errorChanged,
        this,
        [this, sessionId = session->id](QMqttClient::ClientError error) {
            if (error == QMqttClient::NoError) {
                return;
            }

            if (auto *boundSession = m_dependencies.sessionById ? m_dependencies.sessionById(sessionId) : nullptr) {
                QString message = clientErrorName(error);
                QString logMessage = clientErrorLogName(error);
                const QString socketText = socketDiagnostic(boundSession->client);
                if (!socketText.isEmpty() && socketText != message) {
                    message = QStringLiteral("%1 (%2)").arg(message).arg(socketText);
                }
                if (!socketText.isEmpty() && socketText != logMessage) {
                    logMessage = QStringLiteral("%1 (%2)").arg(logMessage).arg(socketText);
                }
                boundSession->lastError = message;
                if (m_dependencies.appendEvent) {
                    m_dependencies.appendEvent(*boundSession, QStringLiteral("Error"), logMessage);
                }
            }

            if (m_dependencies.notifySessionViewsChanged) {
                m_dependencies.notifySessionViewsChanged();
            }
        });

    connect(client, &QMqttClient::brokerSessionRestored, this, [this, sessionId = session->id]() {
        if (auto *boundSession = m_dependencies.sessionById ? m_dependencies.sessionById(sessionId) : nullptr) {
            boundSession->sessionRestored = true;
            if (m_dependencies.appendEvent) {
                m_dependencies.appendEvent(
                    *boundSession,
                    QStringLiteral("Connection"),
                    QStringLiteral("Broker session restored"));
            }
        }
        if (m_dependencies.notifySessionViewsChanged) {
            m_dependencies.notifySessionViewsChanged();
        }
    });

    connect(
        client,
        &QMqttClient::messageReceived,
        this,
        [this, sessionId = session->id](const QByteArray &message, const QMqttTopicName &topic) {
            if (m_dependencies.appendIncomingMessage) {
                m_dependencies.appendIncomingMessage(sessionId, topic.name(), message);
            }
        });

    connect(client, &QMqttClient::messageSent, this, [this, sessionId = session->id](qint32 messageId) {
        if (auto *boundSession = m_dependencies.sessionById ? m_dependencies.sessionById(sessionId) : nullptr) {
            if (boundSession->publishStatus.value(QStringLiteral("messageId")).toInt() == messageId) {
                updatePublishStatus(*boundSession, QStringLiteral("sent"), QString(), messageId);
            }
        }
        if (m_dependencies.notifyCurrentSessionViewsChanged) {
            m_dependencies.notifyCurrentSessionViewsChanged();
        }
    });

    connect(
        client,
        &QMqttClient::messageStatusChanged,
        this,
        [this, sessionId = session->id](
            qint32 messageId,
            QMqtt::MessageStatus status,
            const QMqttMessageStatusProperties &properties) {
            if (auto *boundSession = m_dependencies.sessionById ? m_dependencies.sessionById(sessionId) : nullptr) {
                if (boundSession->publishStatus.value(QStringLiteral("messageId")).toInt() != messageId) {
                    return;
                }

                QString reason = properties.reason();
                if (reason.isEmpty()) {
                    reason = boundSession->publishStatus.value(QStringLiteral("reason")).toString();
                }
                updatePublishStatus(*boundSession, messageStatusName(status), reason, messageId);
            }
            if (m_dependencies.notifyCurrentSessionViewsChanged) {
                m_dependencies.notifyCurrentSessionViewsChanged();
            }
        });

    connect(client, &QMqttClient::pingResponseReceived, this, [this, sessionId = session->id]() {
        if (auto *boundSession = m_dependencies.sessionById ? m_dependencies.sessionById(sessionId) : nullptr) {
            boundSession->brokerInfo =
                QStringLiteral("%1 • %2 • ping ok")
                    .arg(protocolVersionLabel(boundSession->protocolVersion))
                    .arg(transportLabel(boundSession->transport));
        }
        if (m_dependencies.notifySessionViewsChanged) {
            m_dependencies.notifySessionViewsChanged();
        }
    });
}

void MqttController::connectSession(SessionState &session, const QString &eventPrefix)
{
    auto *client = session.client;
    if (!client) {
        return;
    }

    if (session.connectTimeoutTimer) {
        session.connectTimeoutTimer->start((std::max)(1, session.connectTimeoutSeconds) * 1000);
    }

    if (m_dependencies.appendEvent) {
        m_dependencies.appendEvent(
            session,
            QStringLiteral("Connection"),
            QStringLiteral("%1 %2:%3 over %4 using %5")
                .arg(eventPrefix)
                .arg(client->hostname())
                .arg(client->port())
                .arg(transportLabel(session.transport))
                .arg(protocolVersionLabel(session.protocolVersion)));
    }

    if (session.transport == QStringLiteral("tls")) {
        QString tlsError;
        const QSslConfiguration configuration = sslConfigurationForSession(session, tlsError);
        if (!tlsError.isEmpty()) {
            if (session.connectTimeoutTimer) {
                session.connectTimeoutTimer->stop();
            }
            session.lastError = tlsError;
            if (m_dependencies.appendEvent) {
                m_dependencies.appendEvent(session, QStringLiteral("Error"), tlsError);
            }
            return;
        }
        client->connectToHostEncrypted(configuration);
    } else {
        client->connectToHost();
    }
}

QSslConfiguration MqttController::sslConfigurationForSession(const SessionState &session, QString &errorMessage) const
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

void MqttController::updatePublishStatus(
    SessionState &session,
    const QString &state,
    const QString &reason,
    qint32 messageId)
{
    session.publishStatus.insert(QStringLiteral("state"), state);
    session.publishStatus.insert(QStringLiteral("reason"), reason);
    session.publishStatus.insert(QStringLiteral("updatedAt"), timestampNow());
    if (messageId >= 0) {
        session.publishStatus.insert(QStringLiteral("messageId"), messageId);
    }
}
