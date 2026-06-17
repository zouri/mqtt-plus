#include "app/appfacadeutils.h"

#include <QAbstractSocket>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QRegularExpression>

#include <algorithm>

namespace AppFacadeUtils {

QString timestampNow()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

QString displayTimestamp(const QString &timestamp)
{
    if (timestamp.isEmpty()) {
        return {};
    }

    QDateTime dateTime = QDateTime::fromString(timestamp, Qt::ISODateWithMs);
    if (!dateTime.isValid()) {
        dateTime = QDateTime::fromString(timestamp, Qt::ISODate);
    }
    if (!dateTime.isValid()) {
        return timestamp;
    }

    return dateTime.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
}

QString transportLabel(const QString &transport)
{
    return transport == QStringLiteral("tls") ? QStringLiteral("TLS") : QStringLiteral("TCP");
}

QString protocolVersionLabel(int protocolVersion)
{
    return protocolVersion >= 5 ? QStringLiteral("MQTT 5") : QStringLiteral("MQTT 3.1.1");
}

QList<QByteArray> alpnProtocols(const QString &alpn)
{
    QList<QByteArray> protocols;
    const QStringList parts = alpn.split(QRegularExpression(QStringLiteral("[,;\\s]+")), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        protocols.append(part.trimmed().toUtf8());
    }
    return protocols;
}

QSslKey readPrivateKey(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QByteArray keyData = file.readAll();
    for (const QSsl::KeyAlgorithm algorithm : {QSsl::Rsa, QSsl::Dsa, QSsl::Ec}) {
        QSslKey key(keyData, algorithm);
        if (!key.isNull()) {
            return key;
        }
    }
    return {};
}

QString sanitizeThemeMode(const QString &value)
{
    const QString mode = value.trimmed().toLower();
    if (mode == QStringLiteral("light") || mode == QStringLiteral("dark")) {
        return mode;
    }
    return QStringLiteral("system");
}

QMqttClient::ProtocolVersion toProtocolVersion(int value)
{
    return value >= 5 ? QMqttClient::MQTT_5_0 : QMqttClient::MQTT_3_1_1;
}

QString subscriptionStateName(QMqttSubscription::SubscriptionState state)
{
    switch (state) {
    case QMqttSubscription::Unsubscribed:
        return QStringLiteral("unsubscribed");
    case QMqttSubscription::SubscriptionPending:
        return QStringLiteral("pending");
    case QMqttSubscription::Subscribed:
        return QStringLiteral("subscribed");
    case QMqttSubscription::UnsubscriptionPending:
        return QStringLiteral("unsubscribing");
    case QMqttSubscription::Error:
        return QStringLiteral("error");
    }
    return QStringLiteral("saved");
}

QString clientErrorName(QMqttClient::ClientError error)
{
    switch (error) {
    case QMqttClient::NoError:
        return QCoreApplication::translate("AppFacadeUtils", "No error");
    case QMqttClient::InvalidProtocolVersion:
        return QCoreApplication::translate("AppFacadeUtils", "Protocol version rejected by broker");
    case QMqttClient::IdRejected:
        return QCoreApplication::translate("AppFacadeUtils", "Client ID rejected");
    case QMqttClient::ServerUnavailable:
        return QCoreApplication::translate("AppFacadeUtils", "Broker unavailable");
    case QMqttClient::BadUsernameOrPassword:
        return QCoreApplication::translate("AppFacadeUtils", "Username or password rejected");
    case QMqttClient::NotAuthorized:
        return QCoreApplication::translate("AppFacadeUtils", "Not authorized");
    case QMqttClient::TransportInvalid:
        return QCoreApplication::translate("AppFacadeUtils", "Invalid transport");
    case QMqttClient::ProtocolViolation:
        return QCoreApplication::translate("AppFacadeUtils", "Protocol violation");
    case QMqttClient::UnknownError:
        return QCoreApplication::translate("AppFacadeUtils", "Unknown MQTT error");
    case QMqttClient::Mqtt5SpecificError:
        return QCoreApplication::translate("AppFacadeUtils", "MQTT 5 broker reported an error");
    }
    return QCoreApplication::translate("AppFacadeUtils", "MQTT error");
}

QString messageStatusName(QMqtt::MessageStatus status)
{
    switch (status) {
    case QMqtt::MessageStatus::Unknown:
        return QStringLiteral("queued");
    case QMqtt::MessageStatus::Published:
        return QStringLiteral("published");
    case QMqtt::MessageStatus::Acknowledged:
        return QStringLiteral("acknowledged");
    case QMqtt::MessageStatus::Received:
        return QStringLiteral("received");
    case QMqtt::MessageStatus::Released:
        return QStringLiteral("released");
    case QMqtt::MessageStatus::Completed:
        return QStringLiteral("completed");
    }
    return QStringLiteral("queued");
}

QString socketDiagnostic(QMqttClient *client)
{
    if (!client || !client->transport()) {
        return QString();
    }

    const auto *socket = qobject_cast<QAbstractSocket *>(client->transport());
    if (!socket) {
        return QString();
    }
    return socket->errorString();
}

int topicSpecificityScore(const QString &filter)
{
    int score = filter.count('/');
    for (const QChar character : filter) {
        if (character != QLatin1Char('#')
                && character != QLatin1Char('+')
                && character != QLatin1Char('/')) {
            ++score;
        }
    }
    return score;
}

QString subscriptionDisplayState(
    const SessionState &session,
    const SubscriptionEntry &entry,
    const QMqttClient *client)
{
    if (entry.paused) {
        return QStringLiteral("paused");
    }
    if (!client || client->state() != QMqttClient::Connected) {
        return QStringLiteral("saved");
    }
    return entry.runtimeState.isEmpty() ? QStringLiteral("saved") : entry.runtimeState;
}

QString sessionStateName(const SessionState &session, const QMqttClient *client)
{
    if (session.disconnectRequested && client
        && client->state() != QMqttClient::Disconnected) {
        return QStringLiteral("disconnecting");
    }

    if (!client) {
        return QStringLiteral("disconnected");
    }

    switch (client->state()) {
    case QMqttClient::Disconnected:
        return QStringLiteral("disconnected");
    case QMqttClient::Connecting:
        return QStringLiteral("connecting");
    case QMqttClient::Connected:
        return QStringLiteral("connected");
    }
    return QStringLiteral("disconnected");
}

QVariantMap defaultPublishStatus()
{
    QVariantMap status;
    status.insert(QStringLiteral("state"), QStringLiteral("idle"));
    status.insert(QStringLiteral("topic"), QString());
    status.insert(QStringLiteral("reason"), QString());
    status.insert(QStringLiteral("messageId"), -1);
    status.insert(QStringLiteral("qos"), 0);
    status.insert(QStringLiteral("retain"), false);
    status.insert(QStringLiteral("updatedAt"), QString());
    return status;
}

void pruneRecentMessageTimestamps(QVector<qint64> &timestamps, qint64 nowMs)
{
    const qint64 cutoffMs = nowMs - kSubscriptionFpsWindowMs;
    timestamps.erase(
        std::remove_if(
            timestamps.begin(),
            timestamps.end(),
            [cutoffMs](qint64 timestampMs) { return timestampMs < cutoffMs; }),
        timestamps.end());
}

int recentMessageCount(const QVector<qint64> &timestamps, qint64 nowMs)
{
    const qint64 cutoffMs = nowMs - kSubscriptionFpsWindowMs;
    return static_cast<int>(std::count_if(
        timestamps.cbegin(),
        timestamps.cend(),
        [cutoffMs](qint64 timestampMs) { return timestampMs >= cutoffMs; }));
}

} // namespace AppFacadeUtils
