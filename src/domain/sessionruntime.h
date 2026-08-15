#pragma once

#include "publishstatus.h"
#include "recenttrafficwindow.h"

#include <QHash>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QMqttClient>

class QWebSocket;

struct SessionRuntimeState {
    bool disconnectRequested = false;
    bool reconnectPending = false;
    bool sessionRestored = false;
    QString lastError;
    QString brokerInfo;
    QHash<QString, int> subscriptionFormats;
    PublishStatus publishStatus;
    qint64 totalMessageCount = 0;
    qint64 viewedMessageCount = 0;
    qint64 connectedAtMs = 0;
    qint64 connectionStartedAtMs = 0;
    RecentTrafficWindow recentReceivedTraffic;
    RecentTrafficWindow recentPublishedTraffic;
    QMqttClient *client = nullptr;
    QPointer<QWebSocket> webSocket;
    QTimer *connectTimeoutTimer = nullptr;
};
