#pragma once

#include "services/processors/processorruntimeadapter.h"

class JavaScriptRuntimeAdapter : public ProcessorRuntimeAdapter
{
public:
    RuntimeDescriptor descriptor() const override;
    ProcessorPreparationResult prepare(
        const ProcessorRevisionSnapshot &revision,
        const ProcessorExecutionLimits &limits) override;
    ProcessorExecutionResult execute(
        const PreparedProcessorHandle &prepared,
        const MessageProcessorContext &context,
        const ProcessorExecutionLimits &limits) override;
};
