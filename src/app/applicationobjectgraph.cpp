#include "app/applicationobjectgraph.h"

#include "app/applicationcorestate.h"

#include <QObject>
#include <utility>

namespace {

WorkbenchViewModel::Dependencies workbenchDependencies(ApplicationCoreState &state)
{
    return {
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.sessionController, &SessionController::currentSessionIndexChanged, context, std::move(handler));
        },
        [&state](QObject *context, std::function<void()> handler) {
            auto shared = std::make_shared<std::function<void()>>(std::move(handler));
            QObject::connect(&state.sessionController, &SessionController::currentSessionChanged, context, [shared]() { (*shared)(); });
            QObject::connect(&state.mqttController, &MqttController::sessionStateChanged, context, [shared]() { (*shared)(); });
        },
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.eventController, &EventController::messageStreamChanged, context, std::move(handler));
        },
        [&state](QObject *context, std::function<void(const QVariantMap &)> handler) {
            QObject::connect(&state.eventController, &EventController::messageAppended, context, std::move(handler));
        },
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.scriptController, &ScriptController::scriptsChanged, context, std::move(handler));
        },
        &state.sessionController,
        &state.mqttController,
        &state.subscriptionController,
        &state.eventController,
        &state.sessionsModel,
        &state.filteredSubscriptionsModel,
        &state.messagesModel,
        &state.scriptsModel,
    };
}

LogsViewModel::Dependencies logsDependencies(ApplicationCoreState &state)
{
    return {
        &state.logsModel,
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.eventController, &EventController::logStreamChanged, context, std::move(handler));
        },
        [&state](QObject *context, std::function<void(const QVariantMap &)> handler) {
            QObject::connect(&state.eventController, &EventController::logAppended, context, std::move(handler));
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
            QObject::connect(&state.scriptController, &ScriptController::scriptsChanged, context, std::move(handler));
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
            state.scriptController.scriptsChanged();
            return savedId;
        },
    };
}

SettingsViewModel::Dependencies settingsDependencies(ApplicationCoreState &state)
{
    return {
        &state.themeController,
        &state.languageController,
        &state.preferencesController,
        &state.eventController,
        &state.historyStore,
        &state.sessionController.sessions(),
        &state.messagesModel,
        &state.logsModel,
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.themeController, &ThemeController::modeChanged, context, std::move(handler));
        },
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.themeController, &ThemeController::effectiveThemeChanged, context, std::move(handler));
        },
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.languageController, &LanguageController::modeChanged, context, std::move(handler));
        },
        [&state](QObject *context, std::function<void()> handler) {
            QObject::connect(&state.languageController, &LanguageController::languageChanged, context, std::move(handler));
        },
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
                    ? &state.sessionController.currentSession()->messageRows
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
    : m_state(std::make_unique<ApplicationCoreState>(nullptr))
    , m_viewModel(
          workbenchDependencies(*m_state),
          logsDependencies(*m_state),
          scriptsDependencies(*m_state),
          settingsDependencies(*m_state))
{
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
