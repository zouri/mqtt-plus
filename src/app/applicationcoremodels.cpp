#include "app/applicationcore.h"

#include "app/applicationcorestate.h"
#include "app/applicationworkspacedependenciesfactory.h"
#include "app/logsworkspace.h"
#include "app/scriptsworkspace.h"
#include "app/workbenchworkspacedependencies.h"

WorkbenchWorkspaceDependencies ApplicationCore::workbenchDependencies()
{
    return m_state->workspaceDependencies.workbench();
}

LogsWorkspaceDependencies ApplicationCore::logsWorkspaceDependencies()
{
    return m_state->workspaceDependencies.logs();
}

ScriptsWorkspaceDependencies ApplicationCore::scriptsWorkspaceDependencies()
{
    return m_state->workspaceDependencies.scripts();
}
