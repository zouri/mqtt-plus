#pragma once

#include <QString>
#include <QVariantMap>

namespace SessionConfig {
inline constexpr int kDefaultPort = 1883;
inline constexpr int kDefaultTlsPort = 8883;
inline constexpr int kDefaultKeepAlive = 30;

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
}
