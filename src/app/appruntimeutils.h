#pragma once

#include "domain/session.h"
#include "domain/subscription.h"

#include <QByteArray>
#include <QList>
#include <QMqttClient>
#include <QMqttSubscription>
#include <QSslKey>
#include <QString>
#include <QVariantMap>
#include <QVector>

namespace AppRuntimeUtils {

constexpr int kMaxVisibleEventRows = 1200;
constexpr qint64 kSubscriptionFpsWindowMs = 1000;
constexpr int kSubscriptionFpsRefreshIntervalMs = 250;

QString timestampNow();
QString displayTimestamp(const QString &timestamp);
QString transportLabel(const QString &transport);
QString protocolVersionLabel(int protocolVersion);
QList<QByteArray> alpnProtocols(const QString &alpn);
QSslKey readPrivateKey(const QString &path);
QString sanitizeThemeMode(const QString &value);
QMqttClient::ProtocolVersion toProtocolVersion(int value);
QString subscriptionStateName(QMqttSubscription::SubscriptionState state);
QString clientErrorName(QMqttClient::ClientError error);
QString messageStatusName(QMqtt::MessageStatus status);
QString socketDiagnostic(QMqttClient *client);
int topicSpecificityScore(const QString &filter);
QString subscriptionDisplayState(
    const SessionState &session,
    const SubscriptionEntry &entry,
    const QMqttClient *client);
QString sessionStateName(const SessionState &session, const QMqttClient *client);
QVariantMap defaultPublishStatus();
void pruneRecentMessageTimestamps(QVector<qint64> &timestamps, qint64 nowMs);
int recentMessageCount(const QVector<qint64> &timestamps, qint64 nowMs);

} // namespace AppRuntimeUtils
