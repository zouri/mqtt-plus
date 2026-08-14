#pragma once

#include "messageparsing.h"

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

inline void applyParseOutcome(MessageRecord &message, const ParseOutcome &outcome)
{
    message.displayPayload = outcome.displayPayload;
    message.displayFormat = outcome.displayFormat;
    message.displayError = outcome.displayError;
    message.displayState = messageParseStateName(outcome.state);
    message.processorId = outcome.processorId;
    message.processorRevisionId = outcome.processorRevisionId;
    message.processorName = outcome.processorName;
    message.processorLanguageId = outcome.processorLanguageId;
    message.processorRuntimeId = outcome.processorRuntimeId;
    message.processorContentHash = outcome.processorContentHash;
    message.processorResultCbor = outcome.processorResultCbor;
    message.processorResultPreview = outcome.processorResultPreview;
    message.processorExecutionState = outcome.processorExecutionState;
    message.processorExecutionErrorCode = outcome.processorExecutionErrorCode;
    message.processorExecutionError = outcome.processorExecutionError;
    message.processorExecutionDurationUs = outcome.processorExecutionDurationUs;
}

inline QString messageDirectionName(MessageDirection direction)
{
    return direction == MessageDirection::Outgoing
        ? QStringLiteral("outgoing")
        : QStringLiteral("incoming");
}
