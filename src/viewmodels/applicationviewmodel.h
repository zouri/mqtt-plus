#pragma once

#include <QObject>
#include <QSettings>

#include "viewmodels/logsviewmodel.h"
#include "viewmodels/scriptsviewmodel.h"
#include "viewmodels/settingsviewmodel.h"
#include "viewmodels/workbenchviewmodel.h"

class EventHistoryService;
class PreferencesController;
class SessionService;
class SubscriptionService;

class ApplicationViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(WorkbenchViewModel* workbench READ workbench CONSTANT)
    Q_PROPERTY(LogsViewModel* logs READ logs CONSTANT)
    Q_PROPERTY(ScriptsViewModel* scripts READ scripts CONSTANT)
    Q_PROPERTY(SettingsViewModel* settings READ settings CONSTANT)
    Q_PROPERTY(PreferencesController* preferences READ preferences CONSTANT)
    Q_PROPERTY(EventHistoryService* eventHistory READ eventHistory CONSTANT)
    Q_PROPERTY(SessionService* sessionService READ sessionService CONSTANT)
    Q_PROPERTY(SubscriptionService* subscriptionService READ subscriptionService CONSTANT)

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
    PreferencesController *preferences();
    EventHistoryService *eventHistory();
    SessionService *sessionService();
    SubscriptionService *subscriptionService();

private:
    WorkbenchViewModel m_workbench;
    LogsViewModel m_logs;
    ScriptsViewModel m_scripts;
    SettingsViewModel m_settings;
    PreferencesController *m_preferences;
    EventHistoryService *m_eventHistory;
    SessionService *m_sessionService;
    SubscriptionService *m_subscriptionService;
};
