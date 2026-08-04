#pragma once

#include <QString>

struct PublishDraft {
    QString id;
    QString name;
    QString description;
    QString defaultTopic;
    QString payload;
    QString formatId = QStringLiteral("json");
    int qos = 0;
    bool retain = false;
    QString createdAt;
    QString updatedAt;
    QString lastUsedAt;
};
