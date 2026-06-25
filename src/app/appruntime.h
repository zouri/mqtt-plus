#pragma once

#include "app/uieventhub.h"

#include <memory>

class AppRuntimeState;
class AppSurfaceBuses;
class AppWorkspaceSurface;
class ConnectionBus;
class LogsBus;
class MessagesBus;
class ScriptsBus;
class SessionsBus;
class SettingsBus;
class SubscriptionsBus;

class AppRuntime
{
public:
    AppRuntime();
    ~AppRuntime();

    AppRuntimeState *state();
    const AppRuntimeState *state() const;

    SessionsBus *sessions();
    SubscriptionsBus *subscriptions();
    ConnectionBus *connection();
    MessagesBus *messages();
    SettingsBus *settings();
    ScriptsBus *scripts();
    LogsBus *logs();

    UiEventHub *uiEvents();
    const UiEventHub *uiEvents() const;

private:
    UiEventHub m_uiEvents;
    std::unique_ptr<AppRuntimeState> m_state;
    std::unique_ptr<AppWorkspaceSurface> m_workspaceSurface;
    std::unique_ptr<AppSurfaceBuses> m_surfaceBuses;
};
