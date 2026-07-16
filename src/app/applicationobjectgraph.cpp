#include "app/applicationobjectgraph.h"

#include "app/applicationcorestate.h"
#include "services/apputils.h"

#include <QObject>
#include <utility>

namespace {

WorkbenchViewModel::Dependencies workbenchDependencies(ApplicationCoreState &state)
{
    // Keep QML bound to ViewModels only. Controllers emit domain/runtime
    // signals, this object graph adapts them to ViewModel notifications, and
    // QML consumes those through Q_PROPERTY bindings or Connections blocks.
    return {
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.sessionController, &SessionService::currentSessionIndexChanged, context, std::move(handler));
        },
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.sessionController, &SessionService::currentSessionChanged, context, std::move(handler));
        },
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.mqttController, &MqttSessionService::sessionStateChanged, context, std::move(handler));
        },
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.eventController, &EventHistoryService::messageStreamChanged, context, std::move(handler));
        },
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.eventController, &EventHistoryService::totalMessageCountChanged, context, std::move(handler));
        },
        [&state](QObject *context, std::function<void(const QVariantMap &)> handler) {
            QObject::connect(&state.eventController, &EventHistoryService::messageAppended, context, std::move(handler));
        },
        [&state](QObject *context, std::function<void(int)> handler) {
            QObject::connect(&state.eventController, &EventHistoryService::messageRowsAppended, context, std::move(handler));
        },
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.scriptController, &ScriptService::scriptsChanged, context, std::move(handler));
        },
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.subscriptionController, &SubscriptionService::subscriptionsChanged, context, std::move(handler));
        },
        &state.sessionController,
        &state.mqttController,
        &state.subscriptionController,
        &state.eventController,
        &state.sessionsModel,
        &state.filteredSubscriptionsModel,
        &state.messageFilterSubscriptionsModel,
        &state.messagesModel,
        &state.filteredMessagesModel,
        &state.scriptsModel,
    };
}

LogsViewModel::Dependencies logsDependencies(ApplicationCoreState &state)
{
    return {
        &state.logsModel,
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.eventController, &EventHistoryService::logStreamChanged, context, std::move(handler));
        },
        [&state](QObject *context, std::function<void(const QVariantMap &)> handler) {
            QObject::connect(&state.eventController, &EventHistoryService::logAppended, context, std::move(handler));
        },
        [&state]() {
            state.eventController.clearCurrentLogs();
        },
        [&state]() {
            return state.eventController.loadOlderCurrentSessionLogs();
        },
    };
}

ScriptsViewModel::Dependencies scriptsDependencies(ApplicationCoreState &state)
{
    return {
        &state.scriptsModel,
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.scriptController, &ScriptService::scriptsChanged, context, std::move(handler));
        },
        [&state](
            const QString &id,
            const QString &name,
            const QString &description,
            const QString &code) {
            const QString savedId = state.scriptController.upsertScript(id, name, description, code);
            if (savedId.isEmpty()) {
                return QString();
            }
            state.scriptsModel.notifyRefresh();
            return savedId;
        },
    };
}

SettingsViewModel::Dependencies settingsDependencies(ApplicationCoreState &state)
{
    return {
        &state.preferencesController,
        &state.eventController,
        &state.historyStore,
        &state.sessionController.sessions(),
        &state.messagesModel,
        &state.logsModel,
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.preferencesController, &PreferencesController::messageRetentionLimitChanged, context, std::move(handler));
        },
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.preferencesController, &PreferencesController::logRetentionLimitChanged, context, std::move(handler));
        },
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.preferencesController, &PreferencesController::historyPageSizeChanged, context, std::move(handler));
        },
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.preferencesController, &PreferencesController::maxIncomingPayloadBytesChanged, context, std::move(handler));
        },
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.preferencesController, &PreferencesController::deleteHistoryWithSessionChanged, context, std::move(handler));
        },
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.preferencesController, &PreferencesController::saveMessagesWhenOutputPausedChanged, context, std::move(handler));
        },
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.preferencesController, &PreferencesController::autoCollapseConnectionListOnConnectChanged, context, std::move(handler));
        },
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.preferencesController, &PreferencesController::clearMessagesOnExitChanged, context, std::move(handler));
        },
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.preferencesController, &PreferencesController::clearLogsOnExitChanged, context, std::move(handler));
        },
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.preferencesController, &PreferencesController::windowWidthChanged, context, std::move(handler));
        },
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.preferencesController, &PreferencesController::windowHeightChanged, context, std::move(handler));
        },
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.preferencesController, &PreferencesController::windowMaximizedChanged, context, std::move(handler));
        },
        [&state]() {
            state.eventController.reloadCurrentSessionHistory();
            state.eventController.messageStreamChanged();
            state.eventController.logStreamChanged();
        },
        [&state]() {
            state.scriptTestSamplesModel.setSource(
                state.sessionController.currentSession()
                    ? &state.sessionController.currentSession()->runtime.messageRows
                    : nullptr);
        },
        [&state]() {
            state.eventController.messageStreamChanged();
        },
        [&state]() {
            state.eventController.logStreamChanged();
        },
    };
}

} // namespace

ApplicationObjectGraph::ApplicationObjectGraph()
    : m_owner(nullptr)
    , m_state(std::make_unique<ApplicationCoreState>(&m_owner))
    , m_viewModel(
          workbenchDependencies(*m_state),
          logsDependencies(*m_state),
          scriptsDependencies(*m_state),
          settingsDependencies(*m_state),
          &m_state->settings)
{
    m_state->launchTimestamp = AppUtils::timestampNow();
    m_state->runStartup();
}

ApplicationObjectGraph::~ApplicationObjectGraph()
{
    if (m_state) {
        m_state->applyExitCleanup();
    }
}

ApplicationViewModel *ApplicationObjectGraph::viewModel()
{
    return &m_viewModel;
}

SettingsViewModel *ApplicationObjectGraph::settingsViewModel()
{
    return m_viewModel.settings();
}
