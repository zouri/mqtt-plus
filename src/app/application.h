#pragma once

#include "models/eventstreammodel.h"
#include "models/draftlibrarymodel.h"
#include "models/messagefiltermodel.h"
#include "models/notificationcentermodel.h"
#include "models/processorlibrarymodel.h"
#include "models/sessionlistmodel.h"
#include "models/subscriptionfiltermodel.h"
#include "models/subscriptionlistmodel.h"
#include "models/topictreemodel.h"
#include "services/storage/historystore.h"
#include "services/storage/historywriterworker.h"
#include "services/parsing/messageparseworker.h"
#include "services/processors/processorlibrary.h"
#include "services/update/githubupdateservice.h"
#include "usecases/eventhistoryservice.h"
#include "usecases/draftlibraryservice.h"
#include "usecases/mqttsessionservice.h"
#include "usecases/preferencescontroller.h"
#include "usecases/sessionservice.h"
#include "usecases/subscriptionservice.h"
#include "usecases/updatecontroller.h"
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
    void refreshTopicTreeModel();
    void queueTopicObservations(
        const QString &sessionId,
        const QVector<TopicObservation> &observations);
    void flushTopicObservations();
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
    ProcessorLibrary m_processorLibrary;
    DraftLibraryService m_draftService;
    SessionService m_sessionService;
    SessionListModel m_sessionsModel;
    SubscriptionListModel m_subscriptionsModel;
    SubscriptionFilterModel m_filteredSubscriptionsModel;
    SubscriptionFilterModel m_messageFilterSubscriptionsModel;
    TopicTreeModel m_topicTreeModel;
    EventStreamModel m_messagesModel;
    MessageFilterModel m_filteredMessagesModel;
    EventStreamModel m_logsModel;
    ProcessorLibraryModel m_processorsModel;
    DraftLibraryModel m_draftsModel;
    NotificationCenterModel m_notifications;
    GitHubUpdateService m_updateService;
    UpdateController m_updateController;
    QTimer m_subscriptionFpsTimer;
    QTimer m_topicTreeUpdateTimer;
    EventHistoryService m_eventHistoryService;
    SubscriptionService m_subscriptionService;
    MqttSessionService m_mqttService;
    QTimer m_sessionActivityTimer;
    QString m_pendingTopicSessionId;
    QVector<TopicObservation> m_pendingTopicObservations;
    ApplicationViewModel m_viewModel;
};
