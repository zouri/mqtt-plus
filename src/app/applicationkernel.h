#pragma once

#include "app/appcontext.h"
#include "app/appeventbus.h"

class ApplicationKernel
{
public:
    ApplicationKernel();

    AppEventBus *bus();

private:
    AppContext m_context;
    AppEventBus m_bus;
};
