#pragma once

#include "messageprocessor.h"

#include <QByteArray>
#include <QCborMap>
#include <QMetaType>
#include <QSharedPointer>
#include <QString>

enum class MessageParseState {
    Pending,
    Succeeded,
    Failed,
    SkippedOverload,
    NotRequired,
};

inline QString messageParseStateName(MessageParseState state)
{
    switch (state) {
    case MessageParseState::Pending:
        return QStringLiteral("pending");
    case MessageParseState::Succeeded:
        return QStringLiteral("succeeded");
    case MessageParseState::Failed:
        return QStringLiteral("failed");
    case MessageParseState::SkippedOverload:
        return QStringLiteral("skipped_overload");
    case MessageParseState::NotRequired:
        return QStringLiteral("not_required");
    }
    return QStringLiteral("not_required");
}

struct MessageEnvelope {
    qint64 messageId = 0;
    qint64 sequence = 0;
    QString sessionId;
    QString timestamp;
    QString topic;
    QByteArray payloadBytes;
    int payloadFormat = -1;
};

struct MessageParseTask {
    MessageEnvelope envelope;
    QSharedPointer<const ProcessorRevisionSnapshot> processorRevision;
    QString processorName;
    QCborMap processorParameters;
};

struct MessageParseResult {
    qint64 messageId = 0;
    qint64 sequence = 0;
    QString sessionId;
    QString displayPayload;
    QString displayFormat;
    QString displayError;
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
    MessageParseState state = MessageParseState::NotRequired;
};

Q_DECLARE_METATYPE(MessageParseResult)
