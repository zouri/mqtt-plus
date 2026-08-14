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
#include <QUuid>

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
    bool retain,
    const QString &sourceLabel)
{
    auto *session = m_sessionService.currentSession();
    auto *client = session ? session->runtime.client : nullptr;
    PublishStatus status;
    status.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    status.sessionId = session ? session->id : QString();
    status.sessionName = session ? session->name : QString();
    status.sourceLabel = sourceLabel;
    status.topic = topic.trimmed();
    status.qos = SessionConfig::sanitizeQos(qos);
    status.retain = retain;
    status.format = format;
    const PayloadFormat payloadFormat = PayloadCodec::formatFromInt(format);
    status.formatName = PayloadCodec::formatName(payloadFormat);
    status.updatedAt = timestampNow();

    const auto fail = [this, session, &status](const QString &reason) {
        status.state = QStringLiteral("failed");
        status.reason = reason;
        status.updatedAt = timestampNow();
        if (session) {
            session->runtime.publishStatus = status;
            m_eventHistoryService.appendEvent(*session, QStringLiteral("Publish"), reason);
        }
        emitPublishProgress(status);
        emit sessionStateChanged();
        return false;
    };

    if (!session || !client) {
        return fail(tr("Select a connection before publishing."));
    }

    const QString trimmedTopic = status.topic;
    if (trimmedTopic.isEmpty()) {
        return fail(tr("Topic cannot be empty."));
    }

    const QMqttTopicName topicName(trimmedTopic);
    if (!topicName.isValid()) {
        return fail(tr("Invalid topic name: %1").arg(trimmedTopic));
    }

    if (client->state() != QMqttClient::Connected) {
        return fail(tr("Connect before publishing."));
    }

    QByteArray payloadBytes;
    QString error;
    if (!PayloadCodec::encodeForPublish(payloadFormat, payload, payloadBytes, error)) {
        return fail(QStringLiteral("%1 (%2)").arg(error, status.formatName));
    }

    const int publishQos = status.qos;
    if (publishQos > 0 && m_pendingPublishes.size() >= 256) {
        return fail(tr("Too many publishes are waiting for broker confirmation."));
    }

    status.state = QStringLiteral("queued");
    session->runtime.publishStatus = status;

    const qint32 messageId = client->publish(topicName, payloadBytes, publishQos, retain);
    if (messageId < 0) {
        return fail(tr("Qt MQTT rejected the publish request."));
    }

    status.messageId = messageId;
    status.state = publishQos == 0 ? QStringLiteral("sent") : QStringLiteral("queued");
    status.updatedAt = timestampNow();
    session->runtime.publishStatus = status;
    if (publishQos > 0) {
        m_pendingPublishes.insert(pendingKey(session->id, messageId), status);
    }

    session->runtime.recentPublishedTraffic.add(
        QDateTime::currentMSecsSinceEpoch(),
        payloadBytes.size());
    recordRecentPublish(trimmedTopic, payload, format, publishQos, retain, payloadBytes.size());
    m_eventHistoryService.appendPublishedMessage(
        session->id,
        trimmedTopic,
        payloadBytes,
        format,
        publishQos,
        retain);
    m_eventHistoryService.appendEvent(
        *session,
        QStringLiteral("Publish"),
        QStringLiteral("Queued %1 (QoS %2%3)")
            .arg(trimmedTopic)
            .arg(publishQos)
            .arg(retain ? QStringLiteral(", retain") : QString()));
    emitPublishProgress(status);
    emit sessionStateChanged();
    return true;
}

QVariantList MqttSessionService::recentPublishes() const
{
    return m_recentPublishes;
}

void MqttSessionService::clearRecentPublishes()
{
    if (m_recentPublishes.isEmpty()) {
        return;
    }
    m_recentPublishes.clear();
    m_recentPublishBytes = 0;
    emit recentPublishesChanged();
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

        finishPendingPublishes(
            sessionId,
            tr("Connection closed before broker confirmation."));

        emit sessionStateChanged();
    });

    connect(client, &QObject::destroyed, this, [this, sessionId = session->id]() {
        finishPendingPublishes(
            sessionId,
            tr("Connection closed before broker confirmation."));
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
            m_eventHistoryService.queueIncomingMessage(sessionId, topic.name(), message);
        });

    connect(client, &QMqttClient::messageSent, this, [this, sessionId = session->id](qint32 messageId) {
        if (auto *boundSession = m_sessionService.sessionById(sessionId)) {
            const QString key = pendingKey(sessionId, messageId);
            auto pending = m_pendingPublishes.find(key);
            if (pending != m_pendingPublishes.end()) {
                pending->state = QStringLiteral("sent");
                pending->updatedAt = timestampNow();
                emitPublishProgress(*pending);
            }
            const QString currentState = boundSession->runtime.publishStatus.state;
            const bool brokerConfirmationRecorded = currentState == QStringLiteral("failed")
                || currentState == QStringLiteral("acknowledged")
                || currentState == QStringLiteral("completed");
            if (boundSession->runtime.publishStatus.messageId == messageId
                && !brokerConfirmationRecorded) {
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
                const QString key = pendingKey(sessionId, messageId);
                auto pending = m_pendingPublishes.find(key);
                if (pending == m_pendingPublishes.end()) {
                    return;
                }

                QString reason = properties.reason();
                if (reason.isEmpty()) {
                    reason = pending->reason;
                }
                const bool brokerRejected = static_cast<quint8>(properties.reasonCode()) >= 0x80;
                if (brokerRejected && reason.isEmpty()) {
                    reason = tr("Broker rejected the publish request.");
                }
                pending->state = brokerRejected
                    ? QStringLiteral("failed")
                    : messageStatusName(status);
                pending->reason = reason;
                pending->updatedAt = timestampNow();
                const PublishStatus progress = *pending;
                if (boundSession->runtime.publishStatus.messageId == messageId) {
                    updatePublishStatus(*boundSession, progress.state, reason, messageId);
                }
                emitPublishProgress(progress);
                if (progress.state == QStringLiteral("failed")
                    || progress.state == QStringLiteral("acknowledged")
                    || progress.state == QStringLiteral("completed")) {
                    m_pendingPublishes.erase(pending);
                }
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

void MqttSessionService::emitPublishProgress(const PublishStatus &status)
{
    emit publishProgress(status.toVariantMap());
}

void MqttSessionService::finishPendingPublishes(const QString &sessionId, const QString &reason)
{
    SessionState *session = m_sessionService.sessionById(sessionId);
    for (auto it = m_pendingPublishes.begin(); it != m_pendingPublishes.end();) {
        if (it->sessionId != sessionId) {
            ++it;
            continue;
        }
        PublishStatus status = *it;
        status.state = QStringLiteral("failed");
        status.reason = reason;
        status.updatedAt = timestampNow();
        if (session && session->runtime.publishStatus.requestId == status.requestId) {
            session->runtime.publishStatus = status;
        }
        emitPublishProgress(status);
        it = m_pendingPublishes.erase(it);
    }
}

void MqttSessionService::recordRecentPublish(
    const QString &topic,
    const QString &payload,
    int format,
    int qos,
    bool retain,
    qint64 encodedSize)
{
    QVariantMap entry {
        {QStringLiteral("topic"), topic},
        {QStringLiteral("payload"), payload},
        {QStringLiteral("format"), format},
        {QStringLiteral("formatName"), PayloadCodec::formatName(PayloadCodec::formatFromInt(format))},
        {QStringLiteral("qos"), qos},
        {QStringLiteral("retain"), retain},
        {QStringLiteral("publishedAt"), QDateTime::currentDateTime().toString(Qt::ISODate)},
        {QStringLiteral("encodedSize"), encodedSize},
    };
    for (qsizetype index = m_recentPublishes.size() - 1; index >= 0; --index) {
        const QVariantMap existing = m_recentPublishes.at(index).toMap();
        if (existing.value(QStringLiteral("topic")) == topic
            && existing.value(QStringLiteral("payload")) == payload
            && existing.value(QStringLiteral("format")) == format
            && existing.value(QStringLiteral("qos")) == qos
            && existing.value(QStringLiteral("retain")) == retain) {
            m_recentPublishBytes -= existing.value(QStringLiteral("encodedSize")).toLongLong();
            m_recentPublishes.removeAt(index);
        }
    }
    m_recentPublishes.prepend(entry);
    m_recentPublishBytes += encodedSize;
    while (m_recentPublishes.size() > 10 || m_recentPublishBytes > 16 * 1024 * 1024) {
        const QVariantMap removed = m_recentPublishes.takeLast().toMap();
        m_recentPublishBytes -= removed.value(QStringLiteral("encodedSize")).toLongLong();
    }
    emit recentPublishesChanged();
}

QString MqttSessionService::pendingKey(const QString &sessionId, qint32 messageId)
{
    return QStringLiteral("%1:%2").arg(sessionId).arg(messageId);
}
