#include "viewmodels/applicationviewmodel.h"

ApplicationViewModel::ApplicationViewModel(
    WorkbenchCorePort *workbenchCore,
    LogsCorePort *logsCore,
    ScriptsCorePort *scriptsCore,
    SettingsCorePort *settingsCore,
    QObject *parent)
    : QObject(parent)
    , m_navigation(this)
    , m_workbench(workbenchCore, this)
    , m_logs(logsCore, this)
    , m_scripts(scriptsCore, this)
    , m_settings(settingsCore, this)
{
}

NavigationViewModel *ApplicationViewModel::navigation()
{
    return &m_navigation;
}

WorkbenchViewModel *ApplicationViewModel::workbench()
{
    return &m_workbench;
}

LogsViewModel *ApplicationViewModel::logs()
{
    return &m_logs;
}

ScriptsViewModel *ApplicationViewModel::scripts()
{
    return &m_scripts;
}

SettingsViewModel *ApplicationViewModel::settings()
{
    return &m_settings;
}
