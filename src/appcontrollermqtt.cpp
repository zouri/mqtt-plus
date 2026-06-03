#include "appcontroller.h"

#include "appcontrollerutils.h"
#include "sessionconfig.h"

#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslSocket>

#include <algorithm>

using namespace AppControllerUtils;

void AppController::connectCurrentSession()
{
    auto *session = currentSessionState();
    auto *client = session ? session->client : nullptr;
    if (!session || !client) {
        return;
    }

    if (client->hostname().trimmed().isEmpty()) {
        session->lastError = QStringLiteral("Broker host cannot be empty.");
        appendEvent(*session, QStringLiteral("Connection"), session->lastError);
        notifySessionViewsChanged();
        return;
    }

    if (client->clientId().trimmed().isEmpty()) {
        session->lastError = QStringLiteral("Client ID cannot be empty.");
        appendEvent(*session, QStringLiteral("Connection"), session->lastError);
        notifySessionViewsChanged();
        return;
    }

    session->disconnectRequested = false;
    session->sessionRestored = false;
    session->lastError.clear();
    updatePublishStatus(*session, QStringLiteral("idle"));
    connectSession(*session, QStringLiteral("Connecting to"));

    notifySessionViewsChanged();
}

void AppController::disconnectCurrentSession()
{
    auto *session = currentSessionState();
    auto *client = session ? session->client : nullptr;
    if (!session || !client) {
        return;
    }

    session->disconnectRequested = true;
    if (session->connectTimeoutTimer) {
        session->connectTimeoutTimer->stop();
    }
    client->disconnectFromHost();
    notifySessionViewsChanged();
}

void AppController::publishCurrentSession(
    const QString &topic,
    const QString &payload,
    int format,
    int qos,
    bool retain)
{
    auto *session = currentSessionState();
    auto *client = session ? session->client : nullptr;
    if (!session || !client) {
        return;
    }

    const QString trimmedTopic = topic.trimmed();
    if (trimmedTopic.isEmpty()) {
        appendEvent(*session, QStringLiteral("Publish"), QStringLiteral("Topic cannot be empty."));
        return;
    }

    const QMqttTopicName topicName(trimmedTopic);
    if (!topicName.isValid()) {
        appendEvent(*session, QStringLiteral("Publish"), QStringLiteral("Invalid topic name: %1").arg(trimmedTopic));
        return;
    }

    if (client->state() != QMqttClient::Connected) {
        appendEvent(*session, QStringLiteral("Publish"), QStringLiteral("Connect before publishing."));
        return;
    }

    QByteArray payloadBytes;
    QString error;
    const PayloadFormat payloadFormat = PayloadCodec::formatFromInt(format);
    if (!PayloadCodec::encodeForPublish(payloadFormat, payload, payloadBytes, error)) {
        appendEvent(
            *session,
            QStringLiteral("Publish"),
            QStringLiteral("%1 (%2)").arg(error).arg(PayloadCodec::formatName(payloadFormat)));
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
        updatePublishStatus(*session, QStringLiteral("failed"), QStringLiteral("Qt MQTT rejected the publish request."));
        appendEvent(*session, QStringLiteral("Publish"), QStringLiteral("Publish rejected for %1").arg(trimmedTopic));
    } else {
        updatePublishStatus(*session, QStringLiteral("queued"), QString(), messageId);
        appendEvent(
            *session,
            QStringLiteral("Publish"),
            QStringLiteral("Queued %1 (QoS %2%3)")
                .arg(trimmedTopic)
                .arg(SessionConfig::sanitizeQos(qos))
                .arg(retain ? QStringLiteral(", retain") : QString()));
    }

    notifyCurrentSessionViewsChanged();
}

void AppController::bindSessionSignals(SessionState *session)
{
    auto *client = session ? session->client : nullptr;
    if (!session || !client) {
        return;
    }

    connect(client, &QMqttClient::connected, this, [this, sessionId = session->id]() {
        if (auto *boundSession = sessionById(sessionId)) {
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
            appendEvent(*boundSession, QStringLiteral("Connection"), QStringLiteral("Connected to broker"));
            restoreActiveSubscriptions(*boundSession, false);
        }

        notifySessionAndSubscriptionViewsChanged();
    });

    connect(client, &QMqttClient::disconnected, this, [this, sessionId = session->id]() {
        if (auto *boundSession = sessionById(sessionId)) {
            if (boundSession->connectTimeoutTimer) {
                boundSession->connectTimeoutTimer->stop();
            }
            const QString message = boundSession->disconnectRequested
                ? QStringLiteral("Disconnected")
                : QStringLiteral("Connection closed by broker");
            boundSession->disconnectRequested = false;
            appendEvent(*boundSession, QStringLiteral("Connection"), message);
        }

        notifySessionAndSubscriptionViewsChanged();
    });

    connect(client, &QMqttClient::stateChanged, this, [this]() {
        notifySessionAndSubscriptionViewsChanged();
    });

    connect(
        client,
        &QMqttClient::errorChanged,
        this,
        [this, sessionId = session->id](QMqttClient::ClientError error) {
            if (error == QMqttClient::NoError) {
                return;
            }

            if (auto *boundSession = sessionById(sessionId)) {
                QString message = clientErrorName(error);
                const QString socketText = socketDiagnostic(boundSession->client);
                if (!socketText.isEmpty() && socketText != message) {
                    message = QStringLiteral("%1 (%2)").arg(message).arg(socketText);
                }
                boundSession->lastError = message;
                appendEvent(*boundSession, QStringLiteral("Error"), message);
            }

            notifySessionViewsChanged();
        });

    connect(client, &QMqttClient::brokerSessionRestored, this, [this, sessionId = session->id]() {
        if (auto *boundSession = sessionById(sessionId)) {
            boundSession->sessionRestored = true;
            appendEvent(*boundSession, QStringLiteral("Connection"), QStringLiteral("Broker session restored"));
        }
        notifySessionViewsChanged();
    });

    connect(
        client,
        &QMqttClient::messageReceived,
        this,
        [this, sessionId = session->id](const QByteArray &message, const QMqttTopicName &topic) {
            appendIncomingMessage(sessionId, topic.name(), message);
        });

    connect(client, &QMqttClient::messageSent, this, [this, sessionId = session->id](qint32 messageId) {
        if (auto *boundSession = sessionById(sessionId)) {
            if (boundSession->publishStatus.value(QStringLiteral("messageId")).toInt() == messageId) {
                updatePublishStatus(*boundSession, QStringLiteral("sent"), QString(), messageId);
            }
        }
        notifyCurrentSessionViewsChanged();
    });

    connect(
        client,
        &QMqttClient::messageStatusChanged,
        this,
        [this, sessionId = session->id](
            qint32 messageId,
            QMqtt::MessageStatus status,
            const QMqttMessageStatusProperties &properties) {
            if (auto *boundSession = sessionById(sessionId)) {
                if (boundSession->publishStatus.value(QStringLiteral("messageId")).toInt() != messageId) {
                    return;
                }

                QString reason = properties.reason();
                if (reason.isEmpty()) {
                    reason = boundSession->publishStatus.value(QStringLiteral("reason")).toString();
                }
                updatePublishStatus(*boundSession, messageStatusName(status), reason, messageId);
            }
            notifyCurrentSessionViewsChanged();
        });

    connect(client, &QMqttClient::pingResponseReceived, this, [this, sessionId = session->id]() {
        if (auto *boundSession = sessionById(sessionId)) {
            boundSession->brokerInfo =
                QStringLiteral("%1 • %2 • ping ok")
                    .arg(protocolVersionLabel(boundSession->protocolVersion))
                    .arg(transportLabel(boundSession->transport));
        }
        notifySessionViewsChanged();
    });
}

void AppController::connectSession(SessionState &session, const QString &eventPrefix)
{
    auto *client = session.client;
    if (!client) {
        return;
    }

    if (session.connectTimeoutTimer) {
        session.connectTimeoutTimer->start((std::max)(1, session.connectTimeoutSeconds) * 1000);
    }

    appendEvent(
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
            if (session.connectTimeoutTimer) {
                session.connectTimeoutTimer->stop();
            }
            session.lastError = tlsError;
            appendEvent(session, QStringLiteral("Error"), tlsError);
            return;
        }
        client->connectToHostEncrypted(configuration);
    } else {
        client->connectToHost();
    }
}

QSslConfiguration AppController::sslConfigurationForSession(const SessionState &session, QString &errorMessage) const
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

void AppController::updatePublishStatus(
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
