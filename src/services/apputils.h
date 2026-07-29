#pragma once

#include "domain/session.h"

#include <QByteArray>
#include <QList>
#include <QMqttClient>
#include <QMqttSubscription>
#include <QSslKey>
#include <QString>
#include <QVector>

namespace AppUtils {

constexpr int kMaxVisibleEventRows = 1200;
constexpr qint64 kSubscriptionFpsWindowMs = 1000;
constexpr int kSubscriptionFpsRefreshIntervalMs = 250;
constexpr int kSubscriptionRateHistoryDurationMs = 10'000;
constexpr int kSubscriptionRateHistorySampleCount =
    kSubscriptionRateHistoryDurationMs / kSubscriptionFpsRefreshIntervalMs;

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
QString sessionStateName(const SessionState &session, const QMqttClient *client);
void pruneRecentMessageTimestamps(QVector<qint64> &timestamps, qint64 nowMs);
int recentMessageCount(const QVector<qint64> &timestamps, qint64 nowMs);
void appendRecentTrafficSample(QVector<TrafficSample> &samples, qint64 nowMs, qint64 byteCount);
int recentTrafficSampleCount(const QVector<TrafficSample> &samples, qint64 nowMs);
qint64 recentTrafficByteCount(const QVector<TrafficSample> &samples, qint64 nowMs);

} // namespace AppUtils
