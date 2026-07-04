#pragma once

#include <QString>
#include <QVariantMap>

struct PublishStatus {
    QString state = QStringLiteral("idle");
    QString topic;
    QString reason;
    qint32 messageId = -1;
    int qos = 0;
    bool retain = false;
    int format = -1;
    QString formatName;
    QString updatedAt;

    QVariantMap toVariantMap() const
    {
        QVariantMap map;
        map.insert(QStringLiteral("state"), state);
        map.insert(QStringLiteral("topic"), topic);
        map.insert(QStringLiteral("reason"), reason);
        map.insert(QStringLiteral("messageId"), messageId);
        map.insert(QStringLiteral("qos"), qos);
        map.insert(QStringLiteral("retain"), retain);
        map.insert(QStringLiteral("updatedAt"), updatedAt);
        if (format >= 0) {
            map.insert(QStringLiteral("format"), format);
        }
        if (!formatName.isEmpty()) {
            map.insert(QStringLiteral("formatName"), formatName);
        }
        return map;
    }
};
