#pragma once

#include <QSettings>
#include <QString>
#include <QTimer>
#include <QVariantList>

#include "app/applicationsessionrepository.h"
#include "app/applicationsessionruntime.h"
#include "controllers/eventcontroller.h"
#include "controllers/languagecontroller.h"
#include "controllers/mqttcontroller.h"
#include "controllers/preferencescontroller.h"
#include "controllers/scriptcontroller.h"
#include "controllers/sessioncontroller.h"
#include "controllers/subscriptioncontroller.h"
#include "controllers/themecontroller.h"
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
    SessionController sessionController;
    ScriptController scriptController;
    SubscriptionController subscriptionController;
    MqttController mqttController;
    EventController eventController;
    ThemeController themeController;
    LanguageController languageController;
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
