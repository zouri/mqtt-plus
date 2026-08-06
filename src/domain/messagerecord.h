#pragma once

#include <QByteArray>
#include <QString>

enum class MessageDirection {
    Incoming,
    Outgoing,
};

struct MessageRecord {
    qint64 id = 0;
    QString sessionId;
    QString timestamp;
    MessageDirection direction = MessageDirection::Incoming;
    QString topic;
    int qos = -1;
    bool retain = false;
    bool retainKnown = false;
    QByteArray payloadBytes;
    QString displayPayload;
    QString displayFormat;
    QString displayError;
    QString displayState = QStringLiteral("not_required");
    QString processorId;
    QString processorRevisionId;
    QString processorName;
    QString processorLanguageId;
    QString processorRuntimeId;
    QString processorContentHash;
    QByteArray processorResultCbor;
    QString processorResultPreview;
    QString processorExecutionState = QStringLiteral("not_required");
    QString processorExecutionErrorCode;
    QString processorExecutionError;
    qint64 processorExecutionDurationUs = 0;
    QString payloadPreview;
    QString payloadState = QStringLiteral("full");
    qint64 payloadSize = 0;
    QString payloadHash;
    int payloadFormat = -1;
};

inline QString messageDirectionName(MessageDirection direction)
{
    return direction == MessageDirection::Outgoing
        ? QStringLiteral("outgoing")
        : QStringLiteral("incoming");
}
