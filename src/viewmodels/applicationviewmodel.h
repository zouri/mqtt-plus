#pragma once

#include <QObject>
#include <QSettings>

#include "viewmodels/logsviewmodel.h"
#include "viewmodels/scriptsviewmodel.h"
#include "viewmodels/settingsviewmodel.h"
#include "viewmodels/workbenchviewmodel.h"

class ApplicationViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(WorkbenchViewModel* workbench READ workbench CONSTANT)
    Q_PROPERTY(LogsViewModel* logs READ logs CONSTANT)
    Q_PROPERTY(ScriptsViewModel* scripts READ scripts CONSTANT)
    Q_PROPERTY(SettingsViewModel* settings READ settings CONSTANT)

public:
    explicit ApplicationViewModel(
        SessionService &sessionService,
        MqttSessionService &mqttService,
        SubscriptionService &subscriptionService,
        EventHistoryService &eventHistoryService,
        ScriptService &scriptService,
        PreferencesController &preferences,
        HistoryStore &historyStore,
        SessionListModel &sessions,
        SubscriptionFilterModel &filteredSubscriptions,
        SubscriptionFilterModel &messageFilterSubscriptions,
        EventStreamModel &messages,
        MessageFilterModel &filteredMessages,
        EventStreamModel &logs,
        ScriptLibraryModel &scripts,
        QSettings &settings,
        QObject *parent = nullptr);

    WorkbenchViewModel *workbench();
    LogsViewModel *logs();
    ScriptsViewModel *scripts();
    SettingsViewModel *settings();

private:
    WorkbenchViewModel m_workbench;
    LogsViewModel m_logs;
    ScriptsViewModel m_scripts;
    SettingsViewModel m_settings;
};
