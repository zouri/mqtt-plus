#pragma once

#include "messageprocessor.h"
#include "mqttproperties.h"
#include "recenttrafficwindow.h"

#include <QPointer>
#include <QString>

#include <QMqttSubscription>

struct SubscriptionEntry {
    QString topic;
    QString alias;
    int requestedQos = 0;
    int grantedQos = -1;
    int format = 0;
    MqttSubscriptionOptions options;
    ProcessorReference processor;
    QString color;
    bool paused = false;
    QString runtimeState = QStringLiteral("saved");
    QString lastError;
    QPointer<QMqttSubscription> runtimeSubscription;
    RecentTrafficWindow recentMessages;
};
