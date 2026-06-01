#pragma once

#include "appcontroller.h"

#include <QSettings>
#include <QVariantMap>

#include <functional>

namespace SessionConfig {
inline constexpr int kDefaultPort = 1883;
inline constexpr int kDefaultTlsPort = 8883;
inline constexpr int kDefaultKeepAlive = 30;

struct LoadedSession {
    AppController::SessionState session;
    QVariantMap config;
};

QString generateClientId();

int sanitizePort(const QVariant &value, const QString &transport);
int sanitizeKeepAlive(const QVariant &value);
int sanitizeBoundedInt(const QVariant &value, int fallback, int minimum, int maximum);
quint16 sanitizeOptionalUInt16(const QVariant &value);
quint32 sanitizeOptionalUInt32(const QVariant &value);
int sanitizeQos(int qos);
QString sanitizeTransport(const QVariant &value);
int sanitizeProtocolVersion(const QVariant &value);

QVariantMap defaultConfig(int sessionNumber);
QVariantMap configFromSession(const AppController::SessionState &session);
QVariantMap duplicateConfigFromSession(const AppController::SessionState &session);
LoadedSession readSessionSettings(
    QSettings &settings,
    int index,
    const std::function<bool(const QString &)> &scriptExists);
void writeSessionSettings(QSettings &settings, const QVector<AppController::SessionState> &sessions);
}
