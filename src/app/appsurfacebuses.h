#pragma once

#include <memory>

class AppRuntimeState;
class LogsBus;
class ScriptsBus;
class SettingsBus;

class AppSurfaceBuses
{
public:
    explicit AppSurfaceBuses(AppRuntimeState *state);
    ~AppSurfaceBuses();

    SettingsBus *settings();
    ScriptsBus *scripts();
    LogsBus *logs();

private:
    std::unique_ptr<SettingsBus> m_settingsBus;
    std::unique_ptr<ScriptsBus> m_scriptsBus;
    std::unique_ptr<LogsBus> m_logsBus;
};
