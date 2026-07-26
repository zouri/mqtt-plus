#include "viewmodels/applicationviewmodel.h"

#include "usecases/eventhistoryservice.h"
#include "usecases/preferencescontroller.h"
#include "usecases/sessionservice.h"
#include "usecases/subscriptionservice.h"

ApplicationViewModel::ApplicationViewModel(
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
    QObject *parent)
    : QObject(parent)
    , m_workbench(
          sessionService,
          mqttService,
          subscriptionService,
          eventHistoryService,
          sessions,
          filteredSubscriptions,
          messageFilterSubscriptions,
          messages,
          filteredMessages,
          scripts,
          this)
    , m_logs(eventHistoryService, logs, this)
    , m_scripts(scriptService, scripts, this)
    , m_settings(
          preferences,
          eventHistoryService,
          historyStore,
          sessionService.sessions(),
          settings,
          this)
    , m_preferences(&preferences)
    , m_eventHistory(&eventHistoryService)
    , m_sessionService(&sessionService)
    , m_subscriptionService(&subscriptionService)
{
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

PreferencesController *ApplicationViewModel::preferences()
{
    return m_preferences;
}

EventHistoryService *ApplicationViewModel::eventHistory()
{
    return m_eventHistory;
}

SessionService *ApplicationViewModel::sessionService()
{
    return m_sessionService;
}

SubscriptionService *ApplicationViewModel::subscriptionService()
{
    return m_subscriptionService;
}
