#include "app/applicationobjectgraph.h"

#include "app/applicationcorestate.h"

#include <QObject>

#include <functional>
#include <utility>

namespace {

template <typename Signal>
std::function<void(QObject *, std::function<void()>)> bindNotifierSignal(ApplicationCoreState &state, Signal signal)
{
    return [&state, signal](QObject *context, std::function<void()> handler) {
        QObject::connect(&state.core, signal, context, [handler = std::move(handler)]() {
            handler();
        });
    };
}

std::function<void(QObject *, std::function<void(const QVariantMap &)>)> bindLogRowAppended(ApplicationCoreState &state)
{
    return [&state](QObject *context, std::function<void(const QVariantMap &)> handler) {
        QObject::connect(
            &state.core,
            &ApplicationCore::logStreamRowAppended,
            context,
            [handler = std::move(handler)](const QVariantMap &row) {
                handler(row);
            });
    };
}

std::function<void(QObject *, std::function<void(const QVariantMap &)>)> bindMessageRowAppended(ApplicationCoreState &state)
{
    return [&state](QObject *context, std::function<void(const QVariantMap &)> handler) {
        QObject::connect(
            &state.core,
            &ApplicationCore::messageStreamRowAppended,
            context,
            [handler = std::move(handler)](const QVariantMap &row) {
                handler(row);
            });
    };
}

WorkbenchViewModel::Dependencies workbenchDependencies(ApplicationCoreState &state)
{
    return {
        bindNotifierSignal(state, &ApplicationCore::currentSessionIndexChanged),
        bindNotifierSignal(state, &ApplicationCore::currentSessionChanged),
        bindNotifierSignal(state, &ApplicationCore::messageStreamChanged),
        bindMessageRowAppended(state),
        bindNotifierSignal(state, &ApplicationCore::scriptLibraryChanged),
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
        bindNotifierSignal(state, &ApplicationCore::logStreamChanged),
        bindLogRowAppended(state),
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
        bindNotifierSignal(state, &ApplicationCore::scriptLibraryChanged),
        [&state](
            const QString &id,
            const QString &name,
            const QString &description,
            const QString &code) {
            const QString savedId = state.scriptController.upsertScript(id, name, description, code);
            if (savedId.isEmpty()) {
                return QString();
            }
            state.modelRefresher.refreshScripts();
            state.core.notifyScriptLibraryChanged();
            state.viewRefreshCoordinator.notifyCurrentSessionAndSubscriptionsChanged();
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
        bindNotifierSignal(state, &ApplicationCore::themeModeChanged),
        bindNotifierSignal(state, &ApplicationCore::effectiveThemeChanged),
        bindNotifierSignal(state, &ApplicationCore::languageModeChanged),
        bindNotifierSignal(state, &ApplicationCore::languageChanged),
        bindNotifierSignal(state, &ApplicationCore::messageRetentionLimitChanged),
        bindNotifierSignal(state, &ApplicationCore::logRetentionLimitChanged),
        bindNotifierSignal(state, &ApplicationCore::historyPageSizeChanged),
        bindNotifierSignal(state, &ApplicationCore::maxIncomingPayloadBytesChanged),
        bindNotifierSignal(state, &ApplicationCore::deleteHistoryWithSessionChanged),
        bindNotifierSignal(state, &ApplicationCore::saveMessagesWhenOutputPausedChanged),
        bindNotifierSignal(state, &ApplicationCore::clearMessagesOnExitChanged),
        bindNotifierSignal(state, &ApplicationCore::clearLogsOnExitChanged),
        bindNotifierSignal(state, &ApplicationCore::windowWidthChanged),
        bindNotifierSignal(state, &ApplicationCore::windowHeightChanged),
        bindNotifierSignal(state, &ApplicationCore::windowMaximizedChanged),
        [&state]() {
            state.viewRefreshCoordinator.reloadCurrentSessionHistory();
        },
        [&state]() {
            state.modelRefresher.refreshScriptTestSamples(state.sessionController.currentSession());
        },
        [&state]() {
            state.core.notifyMessageStreamChanged();
        },
        [&state]() {
            state.core.notifyLogStreamChanged();
        },
    };
}

} // namespace

ApplicationObjectGraph::ApplicationObjectGraph()
    : m_viewModel(
          workbenchDependencies(*m_core.m_state),
          logsDependencies(*m_core.m_state),
          scriptsDependencies(*m_core.m_state),
          settingsDependencies(*m_core.m_state))
{
}

ApplicationViewModel *ApplicationObjectGraph::viewModel()
{
    return &m_viewModel;
}

SettingsViewModel *ApplicationObjectGraph::settingsViewModel()
{
    return m_viewModel.settings();
}
