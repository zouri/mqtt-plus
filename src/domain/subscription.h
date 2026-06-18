#pragma once

#include <QPointer>
#include <QString>
#include <QVector>

#include <QMqttSubscription>

struct SubscriptionEntry {
    QString topic;
    QString alias;
    int requestedQos = 0;
    int grantedQos = -1;
    int format = 0;
    QString scriptId;
    bool paused = false;
    QString runtimeState = QStringLiteral("saved");
    QString lastError;
    QPointer<QMqttSubscription> runtimeSubscription;
    QVector<qint64> recentMessageTimestampsMs;
};
