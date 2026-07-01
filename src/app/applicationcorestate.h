#pragma once

#include <QSettings>
#include <QString>
#include <QTimer>

#include "app/applicationcontrollercontexts.h"
#include "app/applicationexitcleanup.h"
#include "app/applicationmodelrefresher.h"
#include "app/applicationnotifier.h"
#include "app/applicationsignalbindings.h"
#include "app/applicationsessionrepository.h"
#include "app/applicationsessionruntime.h"
#include "app/applicationstartup.h"
#include "app/applicationviewrefreshcoordinator.h"
#include "app/applicationworkspacedependenciesfactory.h"
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

class QObject;

struct ApplicationCoreState
{
    explicit ApplicationCoreState(QObject *owner);

    QSettings settings;
    ApplicationNotifier notifier;
    ApplicationWorkspaceDependenciesFactory workspaceDependencies;
    ApplicationExitCleanup exitCleanup;
    ApplicationSignalBindings signalBindings;
    ApplicationStartup startup;
    ApplicationControllerContexts controllerContexts;
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
    ApplicationModelRefresher modelRefresher;
    ApplicationViewRefreshCoordinator viewRefreshCoordinator;
    ApplicationSessionRuntime sessionRuntime;
    ApplicationSessionRepository sessionRepository;
    QTimer subscriptionFpsRefreshTimer;
    QString launchTimestamp;
};
