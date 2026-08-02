#pragma once

#include "sessionruntime.h"
#include "subscription.h"

#include <QString>
#include <QStringList>
#include <QVector>

struct SessionState {
    QString id;
    QString name;
    QString transport = QStringLiteral("tcp");
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
    bool outputPaused = false;
    bool captureIncoming = true;
    bool captureOutgoing = true;
    QStringList captureIncludeTopicFilters;
    QStringList captureExcludeTopicFilters;
    QVector<SubscriptionEntry> subscriptions;
    SessionRuntimeState runtime;
};
