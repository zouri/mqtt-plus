#pragma once

#include <QByteArray>
#include <QMetaType>
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
    QString scriptId;
    QString scriptName;
    QString scriptCode;
};

struct MessageParseResult {
    qint64 messageId = 0;
    qint64 sequence = 0;
    QString sessionId;
    QString parsedPayload;
    QString parsedFormat;
    QString parseError;
    QString scriptId;
    QString scriptName;
    MessageParseState state = MessageParseState::NotRequired;
};

Q_DECLARE_METATYPE(MessageParseResult)
