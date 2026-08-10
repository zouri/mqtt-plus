#pragma once

#include <QObject>
#include <QSettings>

#include "viewmodels/logsviewmodel.h"
#include "viewmodels/draftsviewmodel.h"
#include "viewmodels/processorsviewmodel.h"
#include "viewmodels/settingsviewmodel.h"
#include "viewmodels/updateviewmodel.h"
#include "viewmodels/workbenchviewmodel.h"
#include "usecases/configurationtransferservice.h"

class EventHistoryService;
class DraftLibraryService;
class DraftLibraryModel;
class NotificationCenterModel;
class PreferencesController;
class ProcessorLibrary;
class ProcessorLibraryModel;
class SessionService;
class SubscriptionService;

class ApplicationViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(WorkbenchViewModel* workbench READ workbench CONSTANT)
    Q_PROPERTY(DraftsViewModel* drafts READ drafts CONSTANT)
    Q_PROPERTY(LogsViewModel* logs READ logs CONSTANT)
    Q_PROPERTY(ProcessorsViewModel* processors READ processors CONSTANT)
    Q_PROPERTY(SettingsViewModel* settings READ settings CONSTANT)
    Q_PROPERTY(ConfigurationTransferService* configurationTransfer READ configurationTransfer CONSTANT)
    Q_PROPERTY(PreferencesController* preferences READ preferences CONSTANT)
    Q_PROPERTY(EventHistoryService* eventHistory READ eventHistory CONSTANT)
    Q_PROPERTY(SessionService* sessionService READ sessionService CONSTANT)
    Q_PROPERTY(SubscriptionService* subscriptionService READ subscriptionService CONSTANT)
    Q_PROPERTY(NotificationCenterModel* notifications READ notifications CONSTANT)
    Q_PROPERTY(UpdateViewModel* updates READ updates CONSTANT)

public:
    explicit ApplicationViewModel(
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
        UpdateController &updateController,
        QSettings &settings,
        QObject *parent = nullptr);

    WorkbenchViewModel *workbench();
    DraftsViewModel *drafts();
    LogsViewModel *logs();
    ProcessorsViewModel *processors();
    SettingsViewModel *settings();
    ConfigurationTransferService *configurationTransfer();
    PreferencesController *preferences();
    EventHistoryService *eventHistory();
    SessionService *sessionService();
    SubscriptionService *subscriptionService();
    NotificationCenterModel *notifications();
    UpdateViewModel *updates();

private:
    SettingsViewModel m_settings;
    UpdateViewModel m_updates;
    ProcessorsViewModel m_processors;
    WorkbenchViewModel m_workbench;
    DraftsViewModel m_drafts;
    LogsViewModel m_logs;
    ConfigurationTransferService m_configurationTransfer;
    PreferencesController *m_preferences;
    EventHistoryService *m_eventHistory;
    SessionService *m_sessionService;
    SubscriptionService *m_subscriptionService;
    NotificationCenterModel *m_notifications;
};
