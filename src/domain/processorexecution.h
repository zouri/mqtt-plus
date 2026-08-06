#pragma once

#include <QByteArray>
#include <QCborMap>
#include <QCborValue>
#include <QString>
#include <QVector>
#include <QtGlobal>

struct MessageProcessorContext
{
    QString topic;
    QByteArray payload;
    QString receivedAt;
    QString format;
    QString decoded;
    QString decodeError;
    QCborMap parameters;
};

struct ProcessorExecutionLimits
{
    static constexpr int kMaximumWallTimeMilliseconds = 500;

    int wallTimeMilliseconds = 50;
    qsizetype maxResultBytes = 256 * 1024;
    qsizetype maxDiagnosticsBytes = 16 * 1024;
    int maxResultDepth = 32;
    qsizetype maxCollectionEntries = 100000;
    int maxPreviewCharacters = 4096;
};

enum class ProcessorExecutionState
{
    Succeeded,
    InvalidSource,
    RuntimeUnavailable,
    PreparationFailed,
    ExecutionFailed,
    TimedOut,
    Cancelled,
    OutputLimitExceeded,
    UnsupportedResult,
    InternalError,
};

inline QString processorExecutionStateName(ProcessorExecutionState state)
{
    switch (state) {
    case ProcessorExecutionState::Succeeded:
        return QStringLiteral("succeeded");
    case ProcessorExecutionState::InvalidSource:
        return QStringLiteral("invalid_source");
    case ProcessorExecutionState::RuntimeUnavailable:
        return QStringLiteral("runtime_unavailable");
    case ProcessorExecutionState::PreparationFailed:
        return QStringLiteral("preparation_failed");
    case ProcessorExecutionState::ExecutionFailed:
        return QStringLiteral("execution_failed");
    case ProcessorExecutionState::TimedOut:
        return QStringLiteral("timed_out");
    case ProcessorExecutionState::Cancelled:
        return QStringLiteral("cancelled");
    case ProcessorExecutionState::OutputLimitExceeded:
        return QStringLiteral("output_limit_exceeded");
    case ProcessorExecutionState::UnsupportedResult:
        return QStringLiteral("unsupported_result");
    case ProcessorExecutionState::InternalError:
        return QStringLiteral("internal_error");
    }
    return QStringLiteral("internal_error");
}

enum class ProcessorValidationState
{
    Ready,
    InvalidSource,
    RuntimeUnavailable,
    PreparationFailed,
    InternalError,
};

struct ProcessorDiagnostic
{
    QString code;
    QString message;
    QString file;
    int line = -1;
    int column = -1;
};

struct ProcessorValidationResult
{
    ProcessorValidationState state = ProcessorValidationState::InternalError;
    QVector<ProcessorDiagnostic> diagnostics;

    bool isReady() const
    {
        return state == ProcessorValidationState::Ready;
    }
};

struct ProcessorExecutionResult
{
    ProcessorExecutionState state = ProcessorExecutionState::InternalError;
    QCborValue value;
    QString preview;
    QVector<ProcessorDiagnostic> diagnostics;
    qint64 durationMicroseconds = 0;

    bool succeeded() const
    {
        return state == ProcessorExecutionState::Succeeded;
    }
};
