#pragma once

#include "app/applicationcore.h"
#include "app/logsworkspace.h"
#include "app/scriptsworkspace.h"
#include "app/settingsworkspace.h"
#include "app/workbenchworkspace.h"
#include "viewmodels/applicationviewmodel.h"

class ApplicationObjectGraph
{
public:
    ApplicationObjectGraph();

    ApplicationViewModel *viewModel();
    SettingsViewModel *settingsViewModel();

private:
    ApplicationCore m_core;
    WorkbenchWorkspace m_workbenchWorkspace;
    LogsWorkspace m_logsWorkspace;
    ScriptsWorkspace m_scriptsWorkspace;
    SettingsWorkspace m_settingsWorkspace;
    ApplicationViewModel m_viewModel;
};
