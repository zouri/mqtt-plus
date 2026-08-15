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
constexpr qint64 kSubscriptionFpsWindowMs = RecentTrafficWindow::kWindowMs;
constexpr int kSubscriptionFpsRefreshIntervalMs = 250;
constexpr int kSubscriptionRateHistorySampleIntervalMs = 1000;
constexpr int kSubscriptionRateHistoryDurationMs = 10'000;
constexpr int kSubscriptionRateHistorySampleCount =
    kSubscriptionRateHistoryDurationMs / kSubscriptionRateHistorySampleIntervalMs;

QString timestampNow();
QString displayTimestamp(const QString &timestamp);
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

} // namespace AppUtils
