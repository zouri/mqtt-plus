#include "viewmodels/applicationviewmodel.h"

#include "usecases/eventhistoryservice.h"
#include "usecases/draftlibraryservice.h"
#include "models/notificationcentermodel.h"
#include "usecases/preferencescontroller.h"
#include "usecases/sessionservice.h"
#include "usecases/subscriptionservice.h"

#include <algorithm>

ApplicationViewModel::ApplicationViewModel(
    SessionService &sessionService,
    MqttSessionService &mqttService,
    SubscriptionService &subscriptionService,
    EventHistoryService &eventHistoryService,
    ProcessorLibrary &processorLibrary,
    DraftLibraryService &draftService,
    PreferencesController &preferences,
    HistoryStore &historyStore,
    SessionListModel &sessions,
    SubscriptionFilterModel &filteredSubscriptions,
    SubscriptionFilterModel &messageFilterSubscriptions,
    EventStreamModel &messages,
    MessageFilterModel &filteredMessages,
    EventStreamModel &logs,
    ProcessorLibraryModel &processors,
    DraftLibraryModel &drafts,
    NotificationCenterModel &notifications,
    QSettings &settings,
    QObject *parent)
    : QObject(parent)
    , m_settings(
          preferences,
          eventHistoryService,
          historyStore,
          sessionService.sessions(),
          settings,
          this)
    , m_processors(
          processorLibrary,
          processors,
          [&sessionService](const QString &processorId) {
              QStringList sessionNames;
              for (const SessionState &session : sessionService.sessions()) {
                  const bool used = std::any_of(
                      session.subscriptions.cbegin(),
                      session.subscriptions.cend(),
                      [&processorId](const SubscriptionEntry &subscription) {
                          return subscription.processor.processorId == processorId;
                      });
                  if (used) {
                      sessionNames.append(session.name);
                  }
              }
              return sessionNames;
          },
          this)
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
          processors,
          this)
    , m_drafts(draftService, drafts, this)
    , m_logs(eventHistoryService, logs, this)
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

ProcessorsViewModel *ApplicationViewModel::processors()
{
    return &m_processors;
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
