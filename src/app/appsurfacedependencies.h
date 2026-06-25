#pragma once

#include "app/logsbus.h"
#include "app/mqttworkspacecoordinator.h"
#include "app/scriptsbus.h"
#include "app/settingsbus.h"

#include <functional>

class AppHistoryServices;
class AppModelSync;
class UiEventHub;
class AppScriptServices;
class EventController;
class EventStreamModel;
class LanguageController;
class MqttController;
class PreferencesController;
class ScriptController;
class SessionController;
class SessionListModel;
struct SessionState;
class SubscriptionController;
class SubscriptionFilterModel;
class ThemeController;

class AppSurfaceDependencies
{
public:
    struct Dependencies
    {
        UiEventHub *uiEvents = nullptr;
        AppModelSync *modelSync = nullptr;
        AppScriptServices *scriptServices = nullptr;
        AppHistoryServices *historyServices = nullptr;
        ThemeController *themeController = nullptr;
        LanguageController *languageController = nullptr;
        PreferencesController *preferencesController = nullptr;
        SessionController *sessionController = nullptr;
        SubscriptionController *subscriptionController = nullptr;
        MqttController *mqttController = nullptr;
        EventController *eventController = nullptr;
        SessionListModel *sessionsModel = nullptr;
        SubscriptionFilterModel *filteredSubscriptionsModel = nullptr;
        EventStreamModel *messagesModel = nullptr;
        EventStreamModel *logsModel = nullptr;
        std::function<SessionState *()> currentSession;
        std::function<void()> reloadCurrentSessionHistory;
    };

    explicit AppSurfaceDependencies(Dependencies dependencies);

    MqttWorkspaceCoordinator::Dependencies workspaceCoordinatorDependencies();
    SettingsBus::Dependencies settingsBusDependencies();
    ScriptsBus::Dependencies scriptsBusDependencies();
    LogsBus::Dependencies logsBusDependencies();
    void bindWorkspaceSurfaceSignals(MqttWorkspaceCoordinator *coordinator);
    void bindSurfaceBusSignals(SettingsBus *settingsBus, ScriptsBus *scriptsBus, LogsBus *logsBus);

private:
    Dependencies m_dependencies;
};
