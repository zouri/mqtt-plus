#pragma once

#include <QObject>

#include <memory>

struct ApplicationCoreState;
struct LogsWorkspaceDependencies;
struct ScriptsWorkspaceDependencies;
struct SettingsWorkspaceDependencies;
struct WorkbenchWorkspaceDependencies;

class ApplicationObjectGraph;

class ApplicationCore : public QObject
{
    Q_OBJECT

public:
    explicit ApplicationCore(QObject *parent = nullptr);
    ~ApplicationCore() override;

private:
    friend class ApplicationObjectGraph;

    WorkbenchWorkspaceDependencies workbenchDependencies();
    LogsWorkspaceDependencies logsWorkspaceDependencies();
    ScriptsWorkspaceDependencies scriptsWorkspaceDependencies();
    SettingsWorkspaceDependencies settingsWorkspaceDependencies();

    std::unique_ptr<ApplicationCoreState> m_state;
};
