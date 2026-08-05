#pragma once

#include "domain/messageprocessor.h"
#include "domain/processorexecution.h"

#include <QSharedPointer>
#include <QStringList>

enum class RuntimeExecutionMode
{
    ParseWorkerThread,
    HelperProcess,
};

struct RuntimeDescriptor
{
    QString runtimeId;
    QString languageId;
    QString displayName;
    QString runtimeVersion;
    QStringList supportedContractIds;
    QStringList sourceExtensions;
    RuntimeExecutionMode executionMode = RuntimeExecutionMode::ParseWorkerThread;
};

class PreparedProcessor
{
public:
    virtual ~PreparedProcessor() = default;
};

using PreparedProcessorHandle = QSharedPointer<const PreparedProcessor>;

struct ProcessorPreparationResult
{
    ProcessorValidationState state = ProcessorValidationState::InternalError;
    PreparedProcessorHandle prepared;
    QVector<ProcessorDiagnostic> diagnostics;

    bool isReady() const
    {
        return state == ProcessorValidationState::Ready && !prepared.isNull();
    }
};

class ProcessorRuntimeAdapter
{
public:
    virtual ~ProcessorRuntimeAdapter() = default;

    virtual RuntimeDescriptor descriptor() const = 0;
    virtual ProcessorPreparationResult prepare(
        const ProcessorRevisionSnapshot &revision,
        const ProcessorExecutionLimits &limits) = 0;
    virtual ProcessorExecutionResult execute(
        const PreparedProcessorHandle &prepared,
        const MessageProcessorContext &context,
        const ProcessorExecutionLimits &limits) = 0;
};
