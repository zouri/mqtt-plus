#pragma once

#include <QObject>

#include "viewmodels/logsviewmodel.h"
#include "viewmodels/navigationviewmodel.h"
#include "viewmodels/scriptsviewmodel.h"
#include "viewmodels/settingsviewmodel.h"
#include "viewmodels/workbenchviewmodel.h"

class ApplicationViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(NavigationViewModel* navigation READ navigation CONSTANT)
    Q_PROPERTY(WorkbenchViewModel* workbench READ workbench CONSTANT)
    Q_PROPERTY(LogsViewModel* logs READ logs CONSTANT)
    Q_PROPERTY(ScriptsViewModel* scripts READ scripts CONSTANT)
    Q_PROPERTY(SettingsViewModel* settings READ settings CONSTANT)

public:
    explicit ApplicationViewModel(QObject *parent = nullptr);
    explicit ApplicationViewModel(
        const WorkbenchViewModel::Dependencies &workbenchDependencies,
        const LogsViewModel::Dependencies &logsDependencies,
        const ScriptsViewModel::Dependencies &scriptsDependencies,
        const SettingsViewModel::Dependencies &settingsDependencies,
        QObject *parent = nullptr);

    NavigationViewModel *navigation();
    WorkbenchViewModel *workbench();
    LogsViewModel *logs();
    ScriptsViewModel *scripts();
    SettingsViewModel *settings();

private:
    NavigationViewModel m_navigation;
    WorkbenchViewModel m_workbench;
    LogsViewModel m_logs;
    ScriptsViewModel m_scripts;
    SettingsViewModel m_settings;
};
