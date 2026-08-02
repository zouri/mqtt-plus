#pragma once

#include "publishstatus.h"
#include "recenttrafficwindow.h"

#include <QHash>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVector>

#include <QMqttClient>

struct SessionRuntimeState {
    bool disconnectRequested = false;
    bool reconnectPending = false;
    bool sessionRestored = false;
    QString lastError;
    QString brokerInfo;
    QHash<QString, int> subscriptionFormats;
    PublishStatus publishStatus;
    QVariantList messageRows;
    QVariantList logRows;
    qint64 totalMessageCount = 0;
    qint64 viewedMessageCount = 0;
    qint64 oldestLoadedMessageId = 0;
    qint64 oldestLoadedLogId = 0;
    qint64 connectedAtMs = 0;
    qint64 connectionStartedAtMs = 0;
    RecentTrafficWindow recentReceivedTraffic;
    RecentTrafficWindow recentPublishedTraffic;
    bool loadedAllMessageHistory = false;
    bool loadedAllLogHistory = false;
    QMqttClient *client = nullptr;
    QTimer *connectTimeoutTimer = nullptr;
};
