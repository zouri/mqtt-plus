#pragma once

struct ApplicationCoreState;
struct LogsWorkspaceDependencies;
struct ScriptsWorkspaceDependencies;
struct SettingsWorkspaceDependencies;
struct WorkbenchWorkspaceDependencies;

class ApplicationWorkspaceDependenciesFactory
{
public:
    explicit ApplicationWorkspaceDependenciesFactory(ApplicationCoreState &state);

    WorkbenchWorkspaceDependencies workbench();
    LogsWorkspaceDependencies logs();
    ScriptsWorkspaceDependencies scripts();
    SettingsWorkspaceDependencies settings();

private:
    ApplicationCoreState *m_state = nullptr;
};
