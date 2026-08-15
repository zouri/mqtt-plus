#pragma once

#include "messagecapturepolicy.h"
#include "sessionruntime.h"
#include "subscription.h"

#include <QString>
#include <QVector>

struct SessionState {
    QString id;
    QString name;
    QString transport = QStringLiteral("tcp");
    QString webSocketPath = QStringLiteral("/mqtt");
    int protocolVersion = 5;
    bool sslSecure = true;
    QString alpn;
    QString certificateType = QStringLiteral("ca");
    QString caFile;
    QString clientCertificateFile;
    QString clientKeyFile;
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
    QVector<SubscriptionEntry> subscriptions;
    SessionRuntimeState runtime;
};
