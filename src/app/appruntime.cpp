#include "app/appruntime.h"

#include "app/appruntimestate.h"
#include "app/appsurfacebuses.h"
#include "app/appworkspacesurface.h"
#include "app/connectionbus.h"
#include "app/logsbus.h"
#include "app/messagesbus.h"
#include "app/scriptsbus.h"
#include "app/sessionsbus.h"
#include "app/settingsbus.h"
#include "app/subscriptionsbus.h"

AppRuntime::AppRuntime()
    : m_uiEvents()
    , m_state(std::make_unique<AppRuntimeState>(&m_uiEvents))
    , m_workspaceSurface(std::make_unique<AppWorkspaceSurface>(state()))
    , m_surfaceBuses(std::make_unique<AppSurfaceBuses>(state()))
{
}

AppRuntime::~AppRuntime() = default;

AppRuntimeState *AppRuntime::state()
{
    return m_state.get();
}

const AppRuntimeState *AppRuntime::state() const
{
    return m_state.get();
}

SessionsBus *AppRuntime::sessions()
{
    return m_workspaceSurface->sessions();
}

SubscriptionsBus *AppRuntime::subscriptions()
{
    return m_workspaceSurface->subscriptions();
}

ConnectionBus *AppRuntime::connection()
{
    return m_workspaceSurface->connection();
}

MessagesBus *AppRuntime::messages()
{
    return m_workspaceSurface->messages();
}

SettingsBus *AppRuntime::settings()
{
    return m_surfaceBuses->settings();
}

ScriptsBus *AppRuntime::scripts()
{
    return m_surfaceBuses->scripts();
}

LogsBus *AppRuntime::logs()
{
    return m_surfaceBuses->logs();
}

UiEventHub *AppRuntime::uiEvents()
{
    return &m_uiEvents;
}

const UiEventHub *AppRuntime::uiEvents() const
{
    return &m_uiEvents;
}
