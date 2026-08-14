#pragma once

#include <QString>
#include <QVariant>

namespace SessionConfig {
inline constexpr int kDefaultPort = 1883;
inline constexpr int kDefaultTlsPort = 8883;
inline constexpr int kDefaultKeepAlive = 30;
inline constexpr int kMaximumQos = 2;
}

struct SessionConnectionConfig
{
    QString name;
    QString host = QStringLiteral("broker.emqx.io");
    int port = SessionConfig::kDefaultPort;
    QString transport = QStringLiteral("tcp");
    int protocolVersion = 5;
    bool sslSecure = true;
    QString alpn;
    QString certificateType = QStringLiteral("ca");
    QString caFile;
    QString clientCertificateFile;
    QString clientKeyFile;
    QString clientId;
    QString username;
    QString password;
    bool cleanSession = true;
    int keepAliveSeconds = SessionConfig::kDefaultKeepAlive;
    int connectTimeoutSeconds = 10;
    quint32 sessionExpiryInterval = 0;
    quint16 receiveMaximum = 0;
    quint32 maximumPacketSize = 0;
    quint16 topicAliasMaximum = 0;
    bool requestResponseInformation = false;
    bool requestProblemInformation = false;
    QString authenticationMethod;
    QString authenticationData;

    bool operator==(const SessionConnectionConfig &) const = default;
};

namespace SessionConfig {
QString generateClientId();

int sanitizePort(const QVariant &value, const QString &transport);
int sanitizeKeepAlive(const QVariant &value);
int sanitizeBoundedInt(const QVariant &value, int fallback, int minimum, int maximum);
quint16 sanitizeOptionalUInt16(const QVariant &value);
quint32 sanitizeOptionalUInt32(const QVariant &value);
int sanitizeQos(int qos);
QString sanitizeTransport(const QVariant &value);
int sanitizeProtocolVersion(const QVariant &value);

SessionConnectionConfig defaultConfig(int sessionNumber);
}
