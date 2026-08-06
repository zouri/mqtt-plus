#pragma once

#include "domain/messageprocessor.h"
#include "domain/processorexecution.h"

#include <QSharedPointer>
#include <QVector>
#include <QtGlobal>

#include <memory>

class ProcessorRuntimeAdapter;

class MessageProcessorEngine
{
public:
    explicit MessageProcessorEngine(
        QVector<QSharedPointer<ProcessorRuntimeAdapter>> adapters,
        int maxPreparedCacheEntries = 32);
    ~MessageProcessorEngine();

    Q_DISABLE_COPY_MOVE(MessageProcessorEngine)

    ProcessorValidationResult validate(
        const ProcessorRevisionSnapshot &revision,
        const ProcessorExecutionLimits &limits = {});
    ProcessorExecutionResult execute(
        const ProcessorRevisionSnapshot &revision,
        const MessageProcessorContext &context,
        const ProcessorExecutionLimits &limits = {});

private:
    class Private;
    std::unique_ptr<Private> m_private;
};
