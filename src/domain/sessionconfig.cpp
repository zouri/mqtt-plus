#include "sessionconfig.h"

#include <QRandomGenerator>

#include <algorithm>
#include <limits>

namespace SessionConfig {
QString generateClientId()
{
    return QStringLiteral("mqtt-plus-%1")
        .arg(QRandomGenerator::global()->bounded(100000, 999999));
}

int sanitizePort(const QVariant &value, const QString &transport)
{
    bool ok = false;
    const int parsed = value.toInt(&ok);
    if (!ok) {
        return transport == QStringLiteral("tls") ? kDefaultTlsPort : kDefaultPort;
    }
    return std::clamp(parsed, 1, 65535);
}

int sanitizeKeepAlive(const QVariant &value)
{
    bool ok = false;
    const int parsed = value.toInt(&ok);
    if (!ok) {
        return kDefaultKeepAlive;
    }
    return std::clamp(parsed, 5, 1200);
}

int sanitizeBoundedInt(const QVariant &value, int fallback, int minimum, int maximum)
{
    bool ok = false;
    const int parsed = value.toInt(&ok);
    if (!ok) {
        return fallback;
    }
    return std::clamp(parsed, minimum, maximum);
}

quint16 sanitizeOptionalUInt16(const QVariant &value)
{
    const QString text = value.toString().trimmed();
    if (text.isEmpty()) {
        return 0;
    }

    bool ok = false;
    const uint parsed = text.toUInt(&ok);
    if (!ok) {
        return 0;
    }
    return static_cast<quint16>(std::clamp<uint>(parsed, 0, 65535));
}

quint32 sanitizeOptionalUInt32(const QVariant &value)
{
    const QString text = value.toString().trimmed();
    if (text.isEmpty()) {
        return 0;
    }

    bool ok = false;
    const quint64 parsed = text.toULongLong(&ok);
    if (!ok) {
        return 0;
    }
    return static_cast<quint32>((std::min)(parsed, static_cast<quint64>((std::numeric_limits<quint32>::max)())));
}

int sanitizeQos(int qos)
{
    return std::clamp(qos, 0, kMaximumQos);
}

QString sanitizeTransport(const QVariant &value)
{
    const QString transport = value.toString().trimmed().toLower();
    return transport == QStringLiteral("tls") ? QStringLiteral("tls") : QStringLiteral("tcp");
}

int sanitizeProtocolVersion(const QVariant &value)
{
    bool ok = false;
    const int parsed = value.toInt(&ok);
    return (ok && parsed == 4) ? 4 : 5;
}

SessionConnectionConfig defaultConfig(int sessionNumber)
{
    SessionConnectionConfig config;
    config.name = QStringLiteral("Session %1").arg(sessionNumber);
    config.clientId = generateClientId();
    return config;
}
}
