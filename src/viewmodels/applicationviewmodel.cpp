#include "viewmodels/applicationviewmodel.h"

#include "usecases/eventhistoryservice.h"
#include "usecases/draftlibraryservice.h"
#include "models/notificationcentermodel.h"
#include "usecases/preferencescontroller.h"
#include "usecases/sessionservice.h"
#include "usecases/subscriptionservice.h"

ApplicationViewModel::ApplicationViewModel(
    SessionService &sessionService,
    MqttSessionService &mqttService,
    SubscriptionService &subscriptionService,
    EventHistoryService &eventHistoryService,
    ScriptService &scriptService,
    DraftLibraryService &draftService,
    PreferencesController &preferences,
    HistoryStore &historyStore,
    SessionListModel &sessions,
    SubscriptionFilterModel &filteredSubscriptions,
    SubscriptionFilterModel &messageFilterSubscriptions,
    EventStreamModel &messages,
    MessageFilterModel &filteredMessages,
    EventStreamModel &logs,
    ScriptLibraryModel &scripts,
    DraftLibraryModel &drafts,
    NotificationCenterModel &notifications,
    QSettings &settings,
    QObject *parent)
    : QObject(parent)
    , m_workbench(
          sessionService,
          mqttService,
          draftService,
          drafts,
          subscriptionService,
          eventHistoryService,
          sessions,
          filteredSubscriptions,
          messageFilterSubscriptions,
          messages,
          filteredMessages,
          scripts,
          this)
    , m_drafts(draftService, drafts, sessionService, mqttService, this)
    , m_logs(eventHistoryService, logs, this)
    , m_scripts(scriptService, scripts, this)
    , m_settings(
          preferences,
          eventHistoryService,
          historyStore,
          sessionService.sessions(),
          settings,
          this)
    , m_configurationTransfer(
          sessionService,
          draftService,
          preferences,
          settings,
          {},
          this)
    , m_preferences(&preferences)
    , m_eventHistory(&eventHistoryService)
    , m_sessionService(&sessionService)
    , m_subscriptionService(&subscriptionService)
    , m_notifications(&notifications)
{
    connect(
        &m_configurationTransfer,
        &ConfigurationTransferService::portableSettingsImported,
        &m_settings,
        &SettingsViewModel::reloadPortableSettings);
}

WorkbenchViewModel *ApplicationViewModel::workbench()
{
    return &m_workbench;
}

DraftsViewModel *ApplicationViewModel::drafts()
{
    return &m_drafts;
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

ConfigurationTransferService *ApplicationViewModel::configurationTransfer()
{
    return &m_configurationTransfer;
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

NotificationCenterModel *ApplicationViewModel::notifications()
{
    return m_notifications;
}
