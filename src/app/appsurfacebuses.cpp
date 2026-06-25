#include "app/appsurfacebuses.h"

#include "app/appsurfacedependencies.h"
#include "app/appruntimestate.h"
#include "app/logsbus.h"
#include "app/scriptsbus.h"
#include "app/settingsbus.h"

AppSurfaceBuses::AppSurfaceBuses(AppRuntimeState *state)
    : m_settingsBus(std::make_unique<SettingsBus>(state->surfaceDependencies()->settingsBusDependencies(), state))
    , m_scriptsBus(std::make_unique<ScriptsBus>(state->surfaceDependencies()->scriptsBusDependencies(), state))
    , m_logsBus(std::make_unique<LogsBus>(state->surfaceDependencies()->logsBusDependencies(), state))
{
    state->surfaceDependencies()->bindSurfaceBusSignals(m_settingsBus.get(), m_scriptsBus.get(), m_logsBus.get());
}

AppSurfaceBuses::~AppSurfaceBuses() = default;

SettingsBus *AppSurfaceBuses::settings()
{
    return m_settingsBus.get();
}

ScriptsBus *AppSurfaceBuses::scripts()
{
    return m_scriptsBus.get();
}

LogsBus *AppSurfaceBuses::logs()
{
    return m_logsBus.get();
}
