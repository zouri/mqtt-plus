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
        session->lastError = message;
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

} // namespace

ApplicationCoreState::ApplicationCoreState(QObject *parent)
    : settings(QStringLiteral("mqtt-plus"), QStringLiteral("mqtt-plus"))
    , sessionController(parent)
    , scriptController(parent)
    , subscriptionController(parent)
    , mqttController(parent)
    , eventController(parent)
    , themeController(&settings, parent)
    , languageController(&settings, parent)
    , preferencesController(&settings, parent)
    , sessionsModel(parent)
    , subscriptionsModel(parent)
    , filteredSubscriptionsModel(parent)
    , messagesModel(parent)
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
            sessionsModel.notifyRefresh();
            subscriptionsModel.setSource(sessionController.currentSession());
            scriptTestSamplesModel.setSource(sessionController.currentSession() ? &sessionController.currentSession()->messageRows : nullptr);
        },
        [this]() {
            sessionsModel.notifyRefresh();
            subscriptionsModel.setSource(sessionController.currentSession());
        },
        [this]() {
            sessionsModel.notifyRefresh();
            subscriptionsModel.setSource(sessionController.currentSession());
            if (auto *s = sessionController.currentSession()) {
                messagesModel.setRows(s->messageRows);
                logsModel.setRows(s->logRows);
            }
            scriptTestSamplesModel.setSource(sessionController.currentSession() ? &sessionController.currentSession()->messageRows : nullptr);
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
            sessionsModel.notifyRefresh();
            subscriptionsModel.setSource(sessionController.currentSession());
            subscriptionController.subscriptionsChanged();
        },
        [this]() {
            sessionsModel.notifyRefresh();
            subscriptionsModel.setSource(sessionController.currentSession());
            if (auto *s = sessionController.currentSession()) {
                messagesModel.setRows(s->messageRows);
                logsModel.setRows(s->logRows);
            }
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
            subscriptionsModel.setSource(sessionController.currentSession());
        },
        [this]() {
            scriptTestSamplesModel.setSource(sessionController.currentSession() ? &sessionController.currentSession()->messageRows : nullptr);
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
            subscriptionsModel.setSource(sessionController.currentSession());
        },
    });
    filteredSubscriptionsModel.setSourceModel(&subscriptionsModel);

    QObject::connect(
        &scriptController,
        &ScriptController::storageError,
        &sessionController,
        [this](const QString &message) {
            reportStorageError(*this, message);
        });

    subscriptionFpsRefreshTimer.setInterval(kSubscriptionFpsRefreshIntervalMs);
    QObject::connect(
        &subscriptionFpsRefreshTimer,
        &QTimer::timeout,
        &subscriptionController,
        &SubscriptionController::refreshSubscriptionFps);
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
    sessionsModel.notifyRefresh();
    subscriptionsModel.setSource(sessionController.currentSession());
    eventController.reloadCurrentSessionHistory();
    sessionController.currentSessionIndexChanged();
    sessionController.currentSessionChanged();
}
