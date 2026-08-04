#pragma once

#include "models/eventstreammodel.h"
#include "models/draftlibrarymodel.h"
#include "models/messagefiltermodel.h"
#include "models/notificationcentermodel.h"
#include "models/scriptlibrarymodel.h"
#include "models/sessionlistmodel.h"
#include "models/subscriptionfiltermodel.h"
#include "models/subscriptionlistmodel.h"
#include "services/storage/historystore.h"
#include "services/storage/historywriterworker.h"
#include "services/parsing/messageparseworker.h"
#include "usecases/eventhistoryservice.h"
#include "usecases/draftlibraryservice.h"
#include "usecases/mqttsessionservice.h"
#include "usecases/preferencescontroller.h"
#include "usecases/scriptservice.h"
#include "usecases/sessionservice.h"
#include "usecases/subscriptionservice.h"
#include "viewmodels/applicationviewmodel.h"

#include <QObject>
#include <QSettings>
#include <QString>
#include <QTimer>
#include <QThread>

class Application
{
public:
    Application();
    ~Application();

    ApplicationViewModel *viewModel();

private:
    void reportStorageError(const QString &message);
    void refreshSubscriptionsModel();
    void refreshSessionModels();
    void applyMessageRetentionLimit();
    void applyExitCleanup();

    QObject m_owner;
    QSettings m_settings;
    PreferencesController m_preferences;
    HistoryStore m_historyStore;
    QThread m_historyWriterThread;
    HistoryWriterWorker *m_historyWriter = nullptr;
    QThread m_messageParserThread;
    MessageParseWorker *m_messageParser = nullptr;
    ScriptService m_scriptService;
    DraftLibraryService m_draftService;
    SessionService m_sessionService;
    SessionListModel m_sessionsModel;
    SubscriptionListModel m_subscriptionsModel;
    SubscriptionFilterModel m_filteredSubscriptionsModel;
    SubscriptionFilterModel m_messageFilterSubscriptionsModel;
    EventStreamModel m_messagesModel;
    MessageFilterModel m_filteredMessagesModel;
    EventStreamModel m_logsModel;
    ScriptLibraryModel m_scriptsModel;
    DraftLibraryModel m_draftsModel;
    NotificationCenterModel m_notifications;
    QTimer m_subscriptionFpsTimer;
    EventHistoryService m_eventHistoryService;
    SubscriptionService m_subscriptionService;
    MqttSessionService m_mqttService;
    QTimer m_sessionActivityTimer;
    ApplicationViewModel m_viewModel;
};
