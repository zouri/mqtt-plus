#include "app/applicationobjectgraph.h"

ApplicationObjectGraph::ApplicationObjectGraph()
    : m_workbenchWorkspace(m_core.workbenchDependencies())
    , m_logsWorkspace(m_core.logsWorkspaceDependencies())
    , m_scriptsWorkspace(m_core.scriptsWorkspaceDependencies())
    , m_settingsWorkspace(m_core.settingsWorkspaceDependencies())
    , m_viewModel(
          &m_workbenchWorkspace,
          &m_logsWorkspace,
          &m_scriptsWorkspace,
          &m_settingsWorkspace)
{
}

ApplicationViewModel *ApplicationObjectGraph::viewModel()
{
    return &m_viewModel;
}

SettingsViewModel *ApplicationObjectGraph::settingsViewModel()
{
    return m_viewModel.settings();
}
