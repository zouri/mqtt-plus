#include "defaultmessageprocessorengine.h"

#include "messageprocessorengine.h"
#include "runtimes/javascriptruntimeadapter.h"
#include "runtimes/luaruntimeadapter.h"

#include <QSharedPointer>

std::unique_ptr<MessageProcessorEngine> createDefaultMessageProcessorEngine()
{
    return std::make_unique<MessageProcessorEngine>(
        QVector<QSharedPointer<ProcessorRuntimeAdapter>> {
            QSharedPointer<LuaRuntimeAdapter>::create(),
            QSharedPointer<JavaScriptRuntimeAdapter>::create(),
        });
}
