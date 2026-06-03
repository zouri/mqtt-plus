#pragma once

#include "subscription.h"

#include <QHash>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include <QMqttClient>

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
    bool disconnectRequested = false;
    bool sessionRestored = false;
    QString lastError;
    QString brokerInfo;
    QVector<SubscriptionEntry> subscriptions;
    QHash<QString, int> subscriptionFormats;
    QVariantMap publishStatus;
    QVariantList eventRows;
    qint64 oldestLoadedEventId = 0;
    bool loadedAllEventHistory = false;
    QMqttClient *client = nullptr;
    QTimer *connectTimeoutTimer = nullptr;
};
