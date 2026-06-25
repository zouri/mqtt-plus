#include "app/appsurfacedependencies.h"

#include "app/apphistoryservices.h"
#include "app/appmodelsync.h"
#include "app/uieventhub.h"
#include "app/appscriptservices.h"
#include "controllers/eventcontroller.h"
#include "controllers/subscriptioncontroller.h"

#include <QObject>
#include <utility>

AppSurfaceDependencies::AppSurfaceDependencies(Dependencies dependencies)
    : m_dependencies(std::move(dependencies))
{
}

MqttWorkspaceCoordinator::Dependencies AppSurfaceDependencies::workspaceCoordinatorDependencies()
{
    return MqttWorkspaceCoordinator::Dependencies {
        .sessionsModel = m_dependencies.sessionsModel,
        .filteredSubscriptionsModel = m_dependencies.filteredSubscriptionsModel,
        .messagesModel = m_dependencies.messagesModel,
        .sessionController = m_dependencies.sessionController,
        .subscriptionController = m_dependencies.subscriptionController,
        .mqttController = m_dependencies.mqttController,
        .eventController = m_dependencies.eventController,
        .currentSession = m_dependencies.currentSession,
        .subscriptionByTopic = [this](const SessionState *session, const QString &topic) {
            return session ? m_dependencies.subscriptionController->subscriptionByTopic(session, topic) : nullptr;
        },
    };
}

SettingsBus::Dependencies AppSurfaceDependencies::settingsBusDependencies()
{
    return SettingsBus::Dependencies {
        .themeController = m_dependencies.themeController,
        .languageController = m_dependencies.languageController,
        .preferencesController = m_dependencies.preferencesController,
        .sessionController = m_dependencies.sessionController,
        .historyStore = m_dependencies.historyServices->historyStore(),
        .messagesModel = m_dependencies.messagesModel,
        .logsModel = m_dependencies.logsModel,
        .flushPendingMessageHistory = [this]() {
            m_dependencies.eventController->flushPendingMessageHistory();
        },
        .reloadCurrentSessionHistory = m_dependencies.reloadCurrentSessionHistory,
        .syncScriptTestSamplesModel = [this]() {
            m_dependencies.modelSync->refreshScriptTestSamplesModel();
        },
        .publishMessageStreamChanged = [this]() {
            m_dependencies.uiEvents->publishMessageStreamChanged();
        },
        .publishLogsChanged = [this]() {
            m_dependencies.uiEvents->publishLogsChanged();
        },
        .publishScriptTestSamplesChanged = [this]() {
            m_dependencies.uiEvents->publishScriptTestSamplesChanged();
        },
    };
}

ScriptsBus::Dependencies AppSurfaceDependencies::scriptsBusDependencies()
{
    return m_dependencies.scriptServices->scriptsBusDependencies();
}

LogsBus::Dependencies AppSurfaceDependencies::logsBusDependencies()
{
    return LogsBus::Dependencies {
        .logsModel = m_dependencies.logsModel,
        .eventController = m_dependencies.eventController,
    };
}

void AppSurfaceDependencies::bindWorkspaceSurfaceSignals(MqttWorkspaceCoordinator *coordinator)
{
    QObject::connect(m_dependencies.uiEvents, &UiEventHub::sessionsChanged, coordinator, &MqttWorkspaceCoordinator::sessionsChanged);
    QObject::connect(
        m_dependencies.uiEvents,
        &UiEventHub::currentSessionIndexChanged,
        coordinator,
        &MqttWorkspaceCoordinator::currentSessionIndexChanged);
    QObject::connect(
        m_dependencies.uiEvents,
        &UiEventHub::currentSessionChanged,
        coordinator,
        &MqttWorkspaceCoordinator::currentSessionChanged);
    QObject::connect(
        m_dependencies.uiEvents,
        &UiEventHub::subscriptionsChanged,
        coordinator,
        &MqttWorkspaceCoordinator::subscriptionsChanged);
    QObject::connect(
        m_dependencies.uiEvents,
        &UiEventHub::messageStreamChanged,
        coordinator,
        &MqttWorkspaceCoordinator::messageStreamChanged);
    QObject::connect(
        m_dependencies.uiEvents,
        &UiEventHub::messageStreamRowAppended,
        coordinator,
        &MqttWorkspaceCoordinator::messageStreamRowAppended);
}

void AppSurfaceDependencies::bindSurfaceBusSignals(SettingsBus *settingsBus, ScriptsBus *scriptsBus, LogsBus *logsBus)
{
    QObject::connect(m_dependencies.uiEvents, &UiEventHub::scriptsChanged, scriptsBus, &ScriptsBus::scriptsChanged);
    QObject::connect(m_dependencies.uiEvents, &UiEventHub::scriptTestSamplesChanged, scriptsBus, &ScriptsBus::scriptTestSamplesChanged);
    QObject::connect(m_dependencies.uiEvents, &UiEventHub::logsChanged, logsBus, &LogsBus::logsChanged);
    QObject::connect(m_dependencies.uiEvents, &UiEventHub::logsRowAppended, logsBus, &LogsBus::logsRowAppended);
    QObject::connect(m_dependencies.uiEvents, &UiEventHub::themeModeChanged, settingsBus, &SettingsBus::themeModeChanged);
    QObject::connect(m_dependencies.uiEvents, &UiEventHub::effectiveThemeChanged, settingsBus, &SettingsBus::effectiveThemeChanged);
    QObject::connect(m_dependencies.uiEvents, &UiEventHub::languageModeChanged, settingsBus, &SettingsBus::languageModeChanged);
    QObject::connect(m_dependencies.uiEvents, &UiEventHub::languageChanged, settingsBus, &SettingsBus::languageChanged);
    QObject::connect(
        m_dependencies.uiEvents,
        &UiEventHub::messageRetentionLimitChanged,
        settingsBus,
        &SettingsBus::messageRetentionLimitChanged);
    QObject::connect(m_dependencies.uiEvents, &UiEventHub::logRetentionLimitChanged, settingsBus, &SettingsBus::logRetentionLimitChanged);
    QObject::connect(m_dependencies.uiEvents, &UiEventHub::historyPageSizeChanged, settingsBus, &SettingsBus::historyPageSizeChanged);
    QObject::connect(
        m_dependencies.uiEvents,
        &UiEventHub::deleteHistoryWithSessionChanged,
        settingsBus,
        &SettingsBus::deleteHistoryWithSessionChanged);
    QObject::connect(
        m_dependencies.uiEvents,
        &UiEventHub::saveMessagesWhenOutputPausedChanged,
        settingsBus,
        &SettingsBus::saveMessagesWhenOutputPausedChanged);
    QObject::connect(
        m_dependencies.uiEvents,
        &UiEventHub::clearMessagesOnExitChanged,
        settingsBus,
        &SettingsBus::clearMessagesOnExitChanged);
    QObject::connect(m_dependencies.uiEvents, &UiEventHub::clearLogsOnExitChanged, settingsBus, &SettingsBus::clearLogsOnExitChanged);
    QObject::connect(m_dependencies.uiEvents, &UiEventHub::windowWidthChanged, settingsBus, &SettingsBus::windowWidthChanged);
    QObject::connect(m_dependencies.uiEvents, &UiEventHub::windowHeightChanged, settingsBus, &SettingsBus::windowHeightChanged);
    QObject::connect(m_dependencies.uiEvents, &UiEventHub::windowMaximizedChanged, settingsBus, &SettingsBus::windowMaximizedChanged);
}
