#include "viewmodels/applicationviewmodel.h"

ApplicationViewModel::ApplicationViewModel(ApplicationCore *core, QObject *parent)
    : QObject(parent)
    , m_navigation(this)
    , m_workbench(core, this)
    , m_logs(core, this)
    , m_scripts(core, this)
    , m_settings(core, this)
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
