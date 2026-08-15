#pragma once

#include "mqttproperties.h"

#include <QString>
#include <QStringList>
#include <QVariant>

namespace SessionConfig {
enum class Transport
{
    Tcp,
    Tls,
    WebSocket,
    SecureWebSocket,
};

inline constexpr int kDefaultPort = 1883;
inline constexpr int kDefaultTlsPort = 8883;
inline constexpr int kDefaultWebSocketPort = 8083;
inline constexpr int kDefaultSecureWebSocketPort = 8084;
inline constexpr int kDefaultKeepAlive = 30;
inline constexpr int kMaximumQos = 2;
inline constexpr quint32 kMaximumSubscriptionIdentifier = 268435455;
}

struct SessionConnectionConfig
{
    QString name;
    QString host = QStringLiteral("broker.emqx.io");
    int port = SessionConfig::kDefaultPort;
    QString transport = QStringLiteral("tcp");
    QString webSocketPath = QStringLiteral("/mqtt");
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
    MqttUserProperties userProperties;
    MqttLastWillConfig lastWill;

    bool operator==(const SessionConnectionConfig &) const = default;
};

namespace SessionConfig {
QString generateClientId();

int sanitizePort(const QVariant &value, const QString &transport);
int sanitizeKeepAlive(const QVariant &value);
int sanitizeBoundedInt(const QVariant &value, int fallback, int minimum, int maximum);
quint16 sanitizeOptionalUInt16(const QVariant &value);
quint32 sanitizeOptionalUInt32(const QVariant &value);
quint32 sanitizeSubscriptionIdentifier(const QVariant &value);
int sanitizeQos(int qos);
Transport transportFromValue(const QVariant &value);
std::optional<Transport> transportFromScheme(const QString &scheme);
Transport transportAt(int index);
QString transportId(Transport transport);
QString transportScheme(Transport transport);
QString transportLabel(Transport transport);
QString transportLabel(const QString &transport);
QStringList transportSchemes();
int transportIndex(Transport transport);
int defaultPort(Transport transport);
bool isDefaultPort(int port);
bool isSecure(Transport transport);
bool usesWebSocket(Transport transport);
QString sanitizeTransport(const QVariant &value);
QString sanitizeWebSocketPath(const QVariant &value);
int sanitizeProtocolVersion(const QVariant &value);

SessionConnectionConfig defaultConfig(int sessionNumber);
}
