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
        return defaultPort(transportFromValue(transport));
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

quint32 sanitizeSubscriptionIdentifier(const QVariant &value)
{
    bool ok = false;
    const qulonglong identifier = value.toString().trimmed().toULongLong(&ok);
    return ok
            && identifier > 0
            && identifier <= kMaximumSubscriptionIdentifier
        ? static_cast<quint32>(identifier)
        : 0;
}

int sanitizeQos(int qos)
{
    return std::clamp(qos, 0, kMaximumQos);
}

Transport transportFromValue(const QVariant &value)
{
    const QString transport = value.toString().trimmed().toLower();
    if (transport == QStringLiteral("tls")) {
        return Transport::Tls;
    }
    if (transport == QStringLiteral("ws")) {
        return Transport::WebSocket;
    }
    if (transport == QStringLiteral("wss")) {
        return Transport::SecureWebSocket;
    }
    return Transport::Tcp;
}

std::optional<Transport> transportFromScheme(const QString &scheme)
{
    const QString normalized = scheme.trimmed().toLower();
    if (normalized == QStringLiteral("mqtt")) {
        return Transport::Tcp;
    }
    if (normalized == QStringLiteral("mqtts")) {
        return Transport::Tls;
    }
    if (normalized == QStringLiteral("ws")) {
        return Transport::WebSocket;
    }
    if (normalized == QStringLiteral("wss")) {
        return Transport::SecureWebSocket;
    }
    return std::nullopt;
}

Transport transportAt(int index)
{
    switch (index) {
    case 1: return Transport::Tls;
    case 2: return Transport::WebSocket;
    case 3: return Transport::SecureWebSocket;
    default: return Transport::Tcp;
    }
}

QString transportId(Transport transport)
{
    switch (transport) {
    case Transport::Tcp: return QStringLiteral("tcp");
    case Transport::Tls: return QStringLiteral("tls");
    case Transport::WebSocket: return QStringLiteral("ws");
    case Transport::SecureWebSocket: return QStringLiteral("wss");
    }
    return QStringLiteral("tcp");
}

QString transportScheme(Transport transport)
{
    switch (transport) {
    case Transport::Tcp: return QStringLiteral("mqtt");
    case Transport::Tls: return QStringLiteral("mqtts");
    case Transport::WebSocket: return QStringLiteral("ws");
    case Transport::SecureWebSocket: return QStringLiteral("wss");
    }
    return QStringLiteral("mqtt");
}

QStringList transportSchemes()
{
    return {
        QStringLiteral("mqtt://"),
        QStringLiteral("mqtts://"),
        QStringLiteral("ws://"),
        QStringLiteral("wss://"),
    };
}

int transportIndex(Transport transport)
{
    switch (transport) {
    case Transport::Tcp: return 0;
    case Transport::Tls: return 1;
    case Transport::WebSocket: return 2;
    case Transport::SecureWebSocket: return 3;
    }
    return 0;
}

int defaultPort(Transport transport)
{
    switch (transport) {
    case Transport::Tcp: return kDefaultPort;
    case Transport::Tls: return kDefaultTlsPort;
    case Transport::WebSocket: return kDefaultWebSocketPort;
    case Transport::SecureWebSocket: return kDefaultSecureWebSocketPort;
    }
    return kDefaultPort;
}

bool isDefaultPort(int port)
{
    return port == kDefaultPort
        || port == kDefaultTlsPort
        || port == kDefaultWebSocketPort
        || port == kDefaultSecureWebSocketPort;
}

bool isSecure(Transport transport)
{
    return transport == Transport::Tls || transport == Transport::SecureWebSocket;
}

bool usesWebSocket(Transport transport)
{
    return transport == Transport::WebSocket || transport == Transport::SecureWebSocket;
}

QString sanitizeTransport(const QVariant &value)
{
    return transportId(transportFromValue(value));
}

QString sanitizeWebSocketPath(const QVariant &value)
{
    QString path = value.toString().trimmed();
    if (path.isEmpty()) {
        return QStringLiteral("/mqtt");
    }
    if (!path.startsWith(QLatin1Char('/'))) {
        path.prepend(QLatin1Char('/'));
    }
    return path;
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
