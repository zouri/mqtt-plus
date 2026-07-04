#pragma once

#include <QSettings>
#include <QString>
#include <QTimer>
#include <QVariantList>

#include "app/applicationsessionrepository.h"
#include "app/applicationsessionruntime.h"
#include "usecases/eventhistoryservice.h"
#include "usecases/mqttsessionservice.h"
#include "usecases/preferencescontroller.h"
#include "usecases/scriptservice.h"
#include "usecases/sessionservice.h"
#include "usecases/subscriptionservice.h"
#include "models/eventstreammodel.h"
#include "models/scriptlibrarymodel.h"
#include "models/scripttestsamplesmodel.h"
#include "models/sessionlistmodel.h"
#include "models/subscriptionfiltermodel.h"
#include "models/subscriptionlistmodel.h"
#include "services/storage/historystore.h"

struct ApplicationCoreState
{
    explicit ApplicationCoreState(QObject *parent = nullptr);

    void applyExitCleanup();
    void runStartup();

    QSettings settings;
    SessionService sessionController;
    ScriptService scriptController;
    SubscriptionService subscriptionController;
    MqttSessionService mqttController;
    EventHistoryService eventController;
    PreferencesController preferencesController;
    HistoryStore historyStore;
    SessionListModel sessionsModel;
    SubscriptionListModel subscriptionsModel;
    SubscriptionFilterModel filteredSubscriptionsModel;
    EventStreamModel messagesModel;
    EventStreamModel logsModel;
    ScriptLibraryModel scriptsModel;
    ScriptTestSamplesModel scriptTestSamplesModel;
    ApplicationSessionRuntime sessionRuntime;
    ApplicationSessionRepository sessionRepository;
    QTimer subscriptionFpsRefreshTimer;
    QString launchTimestamp;
};
