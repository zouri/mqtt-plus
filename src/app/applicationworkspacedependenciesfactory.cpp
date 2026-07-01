#include "app/applicationworkspacedependenciesfactory.h"

#include "app/applicationcorestate.h"
#include "app/logsworkspace.h"
#include "app/scriptsworkspace.h"
#include "app/settingsworkspace.h"
#include "app/workbenchworkspacedependencies.h"

#include <QObject>

#include <utility>

namespace {

template <typename Signal>
std::function<void(QObject *, std::function<void()>)> bindNotifierSignal(ApplicationCoreState &state, Signal signal)
{
    return [&state, signal](QObject *context, std::function<void()> handler) {
        QObject::connect(&state.notifier, signal, context, [handler = std::move(handler)]() {
            handler();
        });
    };
}

template <typename Signal>
std::function<void(QObject *, std::function<void(const QVariantMap &)>)> bindNotifierRowSignal(
    ApplicationCoreState &state,
    Signal signal)
{
    return [&state, signal](QObject *context, std::function<void(const QVariantMap &)> handler) {
        QObject::connect(&state.notifier, signal, context, [handler = std::move(handler)](const QVariantMap &row) {
            handler(row);
        });
    };
}

} // namespace

ApplicationWorkspaceDependenciesFactory::ApplicationWorkspaceDependenciesFactory(ApplicationCoreState &state)
    : m_state(&state)
{
}

WorkbenchWorkspaceDependencies ApplicationWorkspaceDependenciesFactory::workbench()
{
    return {
        bindNotifierSignal(*m_state, &ApplicationNotifier::currentSessionIndexChanged),
        bindNotifierSignal(*m_state, &ApplicationNotifier::currentSessionChanged),
        bindNotifierSignal(*m_state, &ApplicationNotifier::messageStreamChanged),
        bindNotifierRowSignal(*m_state, &ApplicationNotifier::messageStreamRowAppended),
        bindNotifierSignal(*m_state, &ApplicationNotifier::scriptLibraryChanged),
        &m_state->sessionController,
        &m_state->mqttController,
        &m_state->subscriptionController,
        &m_state->eventController,
        &m_state->sessionsModel,
        &m_state->filteredSubscriptionsModel,
        &m_state->messagesModel,
        &m_state->scriptsModel,
    };
}

LogsWorkspaceDependencies ApplicationWorkspaceDependenciesFactory::logs()
{
    return {
        &m_state->logsModel,
        &m_state->eventController,
        bindNotifierSignal(*m_state, &ApplicationNotifier::logStreamChanged),
        bindNotifierRowSignal(*m_state, &ApplicationNotifier::logStreamRowAppended),
    };
}

ScriptsWorkspaceDependencies ApplicationWorkspaceDependenciesFactory::scripts()
{
    return {
        &m_state->scriptsModel,
        &m_state->scriptController,
        bindNotifierSignal(*m_state, &ApplicationNotifier::scriptLibraryChanged),
        [this]() {
            m_state->modelRefresher.refreshScripts();
        },
        [this]() {
            m_state->notifier.notifyScriptLibraryChanged();
        },
        [this]() {
            m_state->controllerContexts.notifyCurrentSessionAndSubscriptionsChanged();
        },
    };
}

SettingsWorkspaceDependencies ApplicationWorkspaceDependenciesFactory::settings()
{
    return {
        &m_state->themeController,
        &m_state->languageController,
        &m_state->preferencesController,
        &m_state->eventController,
        &m_state->historyStore,
        &m_state->sessionController.sessions(),
        &m_state->messagesModel,
        &m_state->logsModel,
        bindNotifierSignal(*m_state, &ApplicationNotifier::themeModeChanged),
        bindNotifierSignal(*m_state, &ApplicationNotifier::effectiveThemeChanged),
        bindNotifierSignal(*m_state, &ApplicationNotifier::languageModeChanged),
        bindNotifierSignal(*m_state, &ApplicationNotifier::languageChanged),
        bindNotifierSignal(*m_state, &ApplicationNotifier::messageRetentionLimitChanged),
        bindNotifierSignal(*m_state, &ApplicationNotifier::logRetentionLimitChanged),
        bindNotifierSignal(*m_state, &ApplicationNotifier::historyPageSizeChanged),
        bindNotifierSignal(*m_state, &ApplicationNotifier::maxIncomingPayloadBytesChanged),
        bindNotifierSignal(*m_state, &ApplicationNotifier::deleteHistoryWithSessionChanged),
        bindNotifierSignal(*m_state, &ApplicationNotifier::saveMessagesWhenOutputPausedChanged),
        bindNotifierSignal(*m_state, &ApplicationNotifier::clearMessagesOnExitChanged),
        bindNotifierSignal(*m_state, &ApplicationNotifier::clearLogsOnExitChanged),
        bindNotifierSignal(*m_state, &ApplicationNotifier::windowWidthChanged),
        bindNotifierSignal(*m_state, &ApplicationNotifier::windowHeightChanged),
        bindNotifierSignal(*m_state, &ApplicationNotifier::windowMaximizedChanged),
        [this]() {
            m_state->controllerContexts.reloadCurrentSessionHistory();
        },
        [this]() {
            m_state->modelRefresher.refreshScriptTestSamples(m_state->sessionController.currentSession());
        },
        [this]() {
            m_state->notifier.notifyMessageStreamChanged();
        },
        [this]() {
            m_state->notifier.notifyLogStreamChanged();
        },
    };
}
