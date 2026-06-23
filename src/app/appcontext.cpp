#include "app/appcontext.h"

#include "app/appfacade.h"

AppContext::AppContext()
    : m_facade(std::make_unique<AppFacade>())
{
}

AppContext::~AppContext() = default;

AppFacade *AppContext::facade()
{
    return m_facade.get();
}

const AppFacade *AppContext::facade() const
{
    return m_facade.get();
}
