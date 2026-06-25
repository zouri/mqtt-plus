#include "app/applicationkernel.h"

ApplicationKernel::ApplicationKernel()
    : m_runtime()
    , m_appBus(&m_runtime)
{
}

AppBus *ApplicationKernel::appBus()
{
    return &m_appBus;
}

UiEventHub *ApplicationKernel::events()
{
    return m_runtime.uiEvents();
}

AppRuntime *ApplicationKernel::runtime()
{
    return &m_runtime;
}
