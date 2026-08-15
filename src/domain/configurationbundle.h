#pragma once

#include "domain/messagecapturepolicy.h"
#include "domain/publishdraft.h"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

namespace ConfigurationTransfer {

struct SubscriptionData {
    QString topic;
    QString alias;
    int qos = 0;
    int format = 0;
    QString color;
    bool paused = false;
    MqttSubscriptionOptions options;
};

struct SessionData {
    QString sourceId;
    QString name;
    QString host;
    int port = 1883;
    QString transport = QStringLiteral("tcp");
    QString webSocketPath = QStringLiteral("/mqtt");
    int protocolVersion = 5;
    bool sslSecure = true;
    QString alpn;
    QString certificateType = QStringLiteral("ca");
    QByteArray caCertificate;
    QByteArray clientCertificate;
    QByteArray clientKey;
    QString clientId;
    QString username;
    QString password;
    bool cleanSession = true;
    int keepAliveSeconds = 30;
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
    bool outputPaused = false;
    MessageCapturePolicy capturePolicy;
    QVector<SubscriptionData> subscriptions;
};

struct Bundle {
    QVector<SessionData> sessions;
    QVector<PublishDraft> drafts;
    QVariantMap preferences;
};

struct ParseResult {
    bool ok = false;
    QString format;
    QString errorMessage;
    QStringList warnings;
    Bundle bundle;
    int sensitiveFieldCount = 0;
};

struct SerializeResult {
    bool ok = false;
    QByteArray content;
    QString errorMessage;
    QStringList warnings;
};

} // namespace ConfigurationTransfer
