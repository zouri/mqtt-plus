#pragma once

#include "app/appbus.h"
#include "app/appruntime.h"
#include "app/uieventhub.h"

class ApplicationKernel
{
public:
    ApplicationKernel();

    AppBus *appBus();
    UiEventHub *events();
    AppRuntime *runtime();

private:
    AppRuntime m_runtime;
    AppBus m_appBus;
};
