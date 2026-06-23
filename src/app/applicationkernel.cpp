#include "app/applicationkernel.h"

#include "app/appfacade.h"

ApplicationKernel::ApplicationKernel()
    : m_context()
    , m_bus(m_context.facade())
{
    m_context.facade()->setEventBus(&m_bus);
}

AppEventBus *ApplicationKernel::bus()
{
    return &m_bus;
}
