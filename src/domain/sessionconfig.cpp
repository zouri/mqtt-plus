#include "sessionconfig.h"

#include <QRandomGenerator>

#include <algorithm>
#include <limits>

namespace {
QVariantMap baseConfig(
    const QString &name,
    const QVariant &host,
    const QVariant &port,
    const QString &transport,
    int protocolVersion)
{
    QVariantMap config;
    config.insert(QStringLiteral("name"), name);
    config.insert(QStringLiteral("host"), host);
    config.insert(QStringLiteral("port"), port);
    config.insert(QStringLiteral("transport"), transport);
    config.insert(QStringLiteral("protocolVersion"), protocolVersion);
    return config;
}

void addDefaultDetails(QVariantMap &config)
{
    config.insert(QStringLiteral("sslSecure"), true);
    config.insert(QStringLiteral("alpn"), QString());
    config.insert(QStringLiteral("certificateType"), QStringLiteral("ca"));
    config.insert(QStringLiteral("caFile"), QString());
    config.insert(QStringLiteral("clientCertificateFile"), QString());
    config.insert(QStringLiteral("clientKeyFile"), QString());
    config.insert(QStringLiteral("clientId"), SessionConfig::generateClientId());
    config.insert(QStringLiteral("username"), QString());
    config.insert(QStringLiteral("password"), QString());
    config.insert(QStringLiteral("cleanSession"), true);
    config.insert(QStringLiteral("keepAliveSeconds"), SessionConfig::kDefaultKeepAlive);
    config.insert(QStringLiteral("connectTimeoutSeconds"), 10);
    config.insert(QStringLiteral("sessionExpiryInterval"), 0);
    config.insert(QStringLiteral("receiveMaximum"), QString());
    config.insert(QStringLiteral("maximumPacketSize"), QString());
    config.insert(QStringLiteral("topicAliasMaximum"), QString());
    config.insert(QStringLiteral("requestResponseInformation"), false);
    config.insert(QStringLiteral("requestProblemInformation"), false);
    config.insert(QStringLiteral("authenticationMethod"), QString());
    config.insert(QStringLiteral("authenticationData"), QString());
}
}

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
    return std::clamp(qos, 0, 1);
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

QVariantMap defaultConfig(int sessionNumber)
{
    QVariantMap config = baseConfig(
        QStringLiteral("Session %1").arg(sessionNumber),
        QStringLiteral("broker.emqx.io"),
        kDefaultPort,
        QStringLiteral("tcp"),
        5);
    addDefaultDetails(config);
    return config;
}
}
