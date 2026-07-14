#include "app/applicationcorestate.h"

#include "app/applicationsessionconfigurator.h"
#include "services/apputils.h"

#include <QObject>

using namespace AppUtils;

namespace {

void reportStorageError(ApplicationCoreState &state, const QString &message)
{
    if (message.isEmpty()) {
        return;
    }

    if (auto *session = state.sessionController.currentSession()) {
        session->runtime.lastError = message;
        state.eventController.appendEvent(*session, QStringLiteral("Storage"), message);
        state.sessionsModel.notifyRefresh();
        state.sessionController.currentSessionChanged();
    }
}

void clearExitMessages(ApplicationCoreState &state, const QString &mode)
{
    if (mode == QStringLiteral("all")) {
        state.historyStore.clearAllMessages();
        return;
    }

    if (mode == QStringLiteral("current")) {
        if (auto *session = state.sessionController.currentSession()) {
            state.historyStore.clearMessages(session->id);
        }
    }
}

void clearExitLogs(ApplicationCoreState &state, const QString &mode)
{
    if (mode == QStringLiteral("all")) {
        state.historyStore.clearAllLogs();
        return;
    }

    if (mode == QStringLiteral("current")) {
        if (auto *session = state.sessionController.currentSession()) {
            state.historyStore.clearLogs(session->id);
        }
    }
}

SessionState *currentSession(ApplicationCoreState &state)
{
    return state.sessionController.currentSession();
}

void refreshSubscriptionsModel(ApplicationCoreState &state)
{
    state.subscriptionsModel.setSource(currentSession(state));
}

void refreshScriptTestSamplesModel(ApplicationCoreState &state)
{
    auto *session = currentSession(state);
    state.scriptTestSamplesModel.setSource(session ? &session->runtime.messageRows : nullptr);
}

void refreshEventStreamModels(ApplicationCoreState &state)
{
    if (auto *session = currentSession(state)) {
        state.messagesModel.setRows(session->runtime.messageRows);
        state.logsModel.setRows(session->runtime.logRows);
    }
}

void refreshSessionModels(ApplicationCoreState &state)
{
    state.sessionsModel.notifyRefresh();
    refreshSubscriptionsModel(state);
}

void refreshSessionModelsAndSamples(ApplicationCoreState &state)
{
    refreshSessionModels(state);
    refreshScriptTestSamplesModel(state);
}

void refreshCurrentSessionViews(ApplicationCoreState &state)
{
    refreshSessionModels(state);
    refreshEventStreamModels(state);
    refreshScriptTestSamplesModel(state);
}

} // namespace

ApplicationCoreState::ApplicationCoreState(QObject *parent)
    : settings(QStringLiteral("mqtt-plus"), QStringLiteral("mqtt-plus"))
    , sessionController(parent)
    , scriptController(parent)
    , subscriptionController(parent)
    , mqttController(parent)
    , eventController(parent)
    , preferencesController(&settings, parent)
    , sessionsModel(parent)
    , subscriptionsModel(parent)
    , filteredSubscriptionsModel(parent)
    , messageFilterSubscriptionsModel(parent)
    , messagesModel(parent)
    , filteredMessagesModel(parent)
    , logsModel(parent)
    , scriptsModel(parent)
    , scriptTestSamplesModel(parent)
    , sessionRuntime(
          parent,
          {
              [this](const QString &sessionId) {
                  return sessionController.sessionById(sessionId);
              },
              [this](SessionState &session, const QString &channel, const QString &message) {
                  eventController.appendEvent(session, channel, message);
              },
              [this]() {
                  sessionsModel.notifyRefresh();
              },
              [this](SessionState *session) {
                  mqttController.bindSessionSignals(session);
              },
          })
    , sessionRepository(
          settings,
          sessionController,
          scriptController,
          sessionRuntime)
{
    sessionsModel.setSource(&sessionController.sessions());
    scriptsModel.setSource(&scriptController.scripts());
    subscriptionsModel.setScriptNameLookup([this](const QString &id) {
        return scriptController.scriptName(id);
    });

    sessionController.setDependencies({
        &historyStore,
        &subscriptionController,
        &mqttController,
        &subscriptionFpsRefreshTimer,
        [this]() {
            return preferencesController.deleteHistoryWithSession();
        },
        [this]() {
            QString errorMessage;
            if (sessionRepository.saveSessions(errorMessage)) {
                return true;
            }
            reportStorageError(*this, errorMessage.isEmpty() ? QStringLiteral("Cannot save sessions.") : errorMessage);
            return false;
        },
        [](SessionState &session, const QVariantMap &config, bool keepNameFallback) {
            ApplicationSessionConfigurator::applyConfig(session, config, keepNameFallback);
        },
        [this](SessionState *session) {
            sessionRuntime.initialize(session);
        },
        [this](SessionState &session) {
            sessionRuntime.destroy(session);
        },
        [this](const QString &name) {
            return sessionRuntime.createDefaultSession(name);
        },
        [this]() {
            eventController.reloadCurrentSessionHistory();
        },
        [this]() {
            sessionsModel.notifyRefresh();
        },
        [this]() {
            refreshSessionModelsAndSamples(*this);
        },
        [this]() {
            refreshSessionModels(*this);
        },
        [this]() {
            refreshCurrentSessionViews(*this);
        },
        [this]() {
            scriptsModel.notifyRefresh();
        },
    });
    mqttController.setDependencies({
        &subscriptionController,
        &eventController,
        [this]() {
            return sessionController.currentSession();
        },
        [this](const QString &sessionId) {
            return sessionController.sessionById(sessionId);
        },
        [this](SessionState &session, const QString &channel, const QString &message) {
            eventController.appendEvent(session, channel, message);
        },
        [this]() {
            refreshSessionModels(*this);
            subscriptionController.subscriptionsChanged();
        },
        [this]() {
            refreshSessionModels(*this);
            refreshEventStreamModels(*this);
        },
    });
    eventController.setDependencies({
        &historyStore,
        &messagesModel,
        &logsModel,
        &scriptTestSamplesModel,
        &scriptController,
        &subscriptionController,
        &subscriptionFpsRefreshTimer,
        &launchTimestamp,
        &preferencesController,
        [this]() {
            return sessionController.currentSession();
        },
        [this](const QString &sessionId) {
            return sessionController.sessionById(sessionId);
        },
        [this]() {
            refreshSubscriptionsModel(*this);
        },
        [this]() {
            refreshScriptTestSamplesModel(*this);
        },
    });
    subscriptionController.setDependencies({
        &subscriptionsModel,
        &scriptController,
        &eventController,
        &subscriptionFpsRefreshTimer,
        [this]() {
            return sessionController.currentSession();
        },
        [this](const QString &sessionId) {
            return sessionController.sessionById(sessionId);
        },
        [this]() {
            QString errorMessage;
            if (sessionRepository.saveSessions(errorMessage)) {
                return true;
            }
            reportStorageError(*this, errorMessage.isEmpty() ? QStringLiteral("Cannot save sessions.") : errorMessage);
            return false;
        },
        [this]() {
            refreshSubscriptionsModel(*this);
        },
    });
    filteredSubscriptionsModel.setSourceModel(&subscriptionsModel);
    messageFilterSubscriptionsModel.setSourceModel(&subscriptionsModel);
    filteredMessagesModel.setSourceModel(&messagesModel);

    QObject::connect(
        &scriptController,
        &ScriptService::storageError,
        &sessionController,
        [this](const QString &message) {
            reportStorageError(*this, message);
        });

    subscriptionFpsRefreshTimer.setInterval(kSubscriptionFpsRefreshIntervalMs);
    QObject::connect(
        &subscriptionFpsRefreshTimer,
        &QTimer::timeout,
        &subscriptionController,
        &SubscriptionService::refreshSubscriptionFps);
}

void ApplicationCoreState::applyExitCleanup()
{
    eventController.flushPendingMessageHistory();
    clearExitMessages(*this, preferencesController.clearMessagesOnExit());
    clearExitLogs(*this, preferencesController.clearLogsOnExit());
}

void ApplicationCoreState::runStartup()
{
    scriptController.loadScripts();
    scriptsModel.notifyRefresh();

    QString errorMessage;
    if (!sessionRepository.loadSessions(errorMessage)) {
        reportStorageError(*this, errorMessage.isEmpty() ? QStringLiteral("Cannot load sessions.") : errorMessage);
    }

    sessionController.setCurrentIndex(0);
    refreshSessionModels(*this);
    eventController.reloadCurrentSessionHistory();
    sessionController.currentSessionIndexChanged();
    sessionController.currentSessionChanged();
}
