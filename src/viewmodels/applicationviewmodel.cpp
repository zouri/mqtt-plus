#include "viewmodels/applicationviewmodel.h"

ApplicationViewModel::ApplicationViewModel(QObject *parent)
    : ApplicationViewModel(
          WorkbenchViewModel::Dependencies {},
          LogsViewModel::Dependencies {},
          ScriptsViewModel::Dependencies {},
          SettingsViewModel::Dependencies {},
          parent)
{
}

ApplicationViewModel::ApplicationViewModel(
    const WorkbenchViewModel::Dependencies &workbenchDependencies,
    const LogsViewModel::Dependencies &logsDependencies,
    const ScriptsViewModel::Dependencies &scriptsDependencies,
    const SettingsViewModel::Dependencies &settingsDependencies,
    QObject *parent)
    : QObject(parent)
    , m_navigation(this)
    , m_workbench(workbenchDependencies, this)
    , m_logs(logsDependencies, this)
    , m_scripts(scriptsDependencies, this)
    , m_settings(settingsDependencies, this)
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
