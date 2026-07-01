#include "app/applicationcorestate.h"

#include "app/applicationcore.h"
#include "app/applicationsessionconfigurator.h"
#include "services/apputils.h"

#include <QObject>

using namespace AppUtils;

namespace {

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

ApplicationCoreState::ApplicationCoreState(ApplicationCore &owner)
    : settings(QStringLiteral("mqtt-plus"), QStringLiteral("mqtt-plus"))
    , core(owner)
    , sessionController(&owner)
    , scriptController(&owner)
    , subscriptionController(&owner)
    , mqttController(&owner)
    , eventController(&owner)
    , themeController(&settings, &owner)
    , languageController(&settings, &owner)
    , preferencesController(&settings, &owner)
    , sessionsModel(&owner)
    , subscriptionsModel(&owner)
    , filteredSubscriptionsModel(&owner)
    , messagesModel(&owner)
    , logsModel(&owner)
    , scriptsModel(&owner)
    , scriptTestSamplesModel(&owner)
    , modelRefresher(
          sessionController,
          scriptController,
          subscriptionController,
          sessionsModel,
          subscriptionsModel,
          scriptsModel,
          scriptTestSamplesModel)
    , sessionRuntime(
          &owner,
          {
              [this](const QString &sessionId) {
                  return sessionController.sessionById(sessionId);
              },
              [this](SessionState &session, const QString &channel, const QString &message) {
                  eventController.appendEvent(session, channel, message);
              },
              [this]() {
                  viewRefreshCoordinator.notifySessionViewsChanged();
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
    viewRefreshCoordinator.setDependencies({
        &core,
        &modelRefresher,
        &sessionController,
        &eventController,
        &messagesModel,
        &logsModel,
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
            viewRefreshCoordinator.reportStorageError(
                errorMessage.isEmpty() ? QStringLiteral("Cannot save sessions.") : errorMessage);
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
            viewRefreshCoordinator.reloadCurrentSessionHistory();
        },
        [this]() {
            viewRefreshCoordinator.notifyCurrentSessionViewsChanged();
        },
        [this]() {
            viewRefreshCoordinator.notifyCurrentSessionAndSubscriptionsChanged();
        },
        [this]() {
            viewRefreshCoordinator.notifySelectedSessionViewsChanged();
        },
        [this]() {
            viewRefreshCoordinator.notifySessionCollectionViewsChanged();
        },
        [this]() {
            viewRefreshCoordinator.emitSessionsChanged();
        },
        [this]() {
            viewRefreshCoordinator.emitMessageStreamChanged();
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
            viewRefreshCoordinator.notifyCurrentSessionViewsChanged();
        },
        [this]() {
            viewRefreshCoordinator.notifySessionViewsChanged();
        },
        [this]() {
            viewRefreshCoordinator.notifySessionAndSubscriptionViewsChanged();
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
            viewRefreshCoordinator.refreshSubscriptionsModel();
        },
        [this]() {
            viewRefreshCoordinator.refreshScriptTestSamplesModel();
        },
        [this]() {
            viewRefreshCoordinator.emitSubscriptionsChanged();
        },
        [this]() {
            viewRefreshCoordinator.emitMessageStreamChanged();
        },
        [this]() {
            viewRefreshCoordinator.emitLogStreamChanged();
        },
        [this](const QVariantMap &row) {
            viewRefreshCoordinator.emitMessageStreamRowAppended(row);
        },
        [this](const QVariantMap &row) {
            viewRefreshCoordinator.emitLogStreamRowAppended(row);
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
            viewRefreshCoordinator.reportStorageError(
                errorMessage.isEmpty() ? QStringLiteral("Cannot save sessions.") : errorMessage);
            return false;
        },
        [this]() {
            viewRefreshCoordinator.refreshSubscriptionsModel();
        },
        [this]() {
            viewRefreshCoordinator.notifyCurrentSessionAndSubscriptionsChanged();
        },
        [this]() {
            viewRefreshCoordinator.notifySessionAndSubscriptionViewsChanged();
        },
        [this]() {
            viewRefreshCoordinator.emitSubscriptionsChanged();
        },
    });
    filteredSubscriptionsModel.setSourceModel(&subscriptionsModel);
}

void ApplicationCoreState::applyExitCleanup()
{
    eventController.flushPendingMessageHistory();
    clearExitMessages(*this, preferencesController.clearMessagesOnExit());
    clearExitLogs(*this, preferencesController.clearLogsOnExit());
}

void ApplicationCoreState::installSignalBindings()
{
    QObject::connect(
        &scriptController,
        &ScriptController::storageError,
        &core,
        [this](const QString &message) {
            viewRefreshCoordinator.reportStorageError(message);
        });

    QObject::connect(&themeController, &ThemeController::modeChanged, &core, &ApplicationCore::notifyThemeModeChanged);
    QObject::connect(&themeController, &ThemeController::effectiveThemeChanged, &core, &ApplicationCore::notifyEffectiveThemeChanged);
    QObject::connect(&languageController, &LanguageController::modeChanged, &core, &ApplicationCore::notifyLanguageModeChanged);
    QObject::connect(&languageController, &LanguageController::languageChanged, &core, [this]() {
        viewRefreshCoordinator.notifyLanguageChanged();
    });
    QObject::connect(
        &preferencesController,
        &PreferencesController::messageRetentionLimitChanged,
        &core,
        &ApplicationCore::notifyMessageRetentionLimitChanged);
    QObject::connect(
        &preferencesController,
        &PreferencesController::logRetentionLimitChanged,
        &core,
        &ApplicationCore::notifyLogRetentionLimitChanged);
    QObject::connect(&preferencesController, &PreferencesController::historyPageSizeChanged, &core, [this]() {
        viewRefreshCoordinator.notifyHistoryPageSizeChanged();
    });
    QObject::connect(
        &preferencesController,
        &PreferencesController::maxIncomingPayloadBytesChanged,
        &core,
        &ApplicationCore::notifyMaxIncomingPayloadBytesChanged);
    QObject::connect(
        &preferencesController,
        &PreferencesController::deleteHistoryWithSessionChanged,
        &core,
        &ApplicationCore::notifyDeleteHistoryWithSessionChanged);
    QObject::connect(
        &preferencesController,
        &PreferencesController::saveMessagesWhenOutputPausedChanged,
        &core,
        &ApplicationCore::notifySaveMessagesWhenOutputPausedChanged);
    QObject::connect(
        &preferencesController,
        &PreferencesController::clearMessagesOnExitChanged,
        &core,
        &ApplicationCore::notifyClearMessagesOnExitChanged);
    QObject::connect(
        &preferencesController,
        &PreferencesController::clearLogsOnExitChanged,
        &core,
        &ApplicationCore::notifyClearLogsOnExitChanged);
    QObject::connect(
        &preferencesController,
        &PreferencesController::windowWidthChanged,
        &core,
        &ApplicationCore::notifyWindowWidthChanged);
    QObject::connect(
        &preferencesController,
        &PreferencesController::windowHeightChanged,
        &core,
        &ApplicationCore::notifyWindowHeightChanged);
    QObject::connect(
        &preferencesController,
        &PreferencesController::windowMaximizedChanged,
        &core,
        &ApplicationCore::notifyWindowMaximizedChanged);

    subscriptionFpsRefreshTimer.setInterval(kSubscriptionFpsRefreshIntervalMs);
    QObject::connect(
        &subscriptionFpsRefreshTimer,
        &QTimer::timeout,
        &subscriptionController,
        &SubscriptionController::refreshSubscriptionFps);
}

void ApplicationCoreState::runStartup()
{
    scriptController.loadScripts();
    modelRefresher.refreshScripts();

    QString errorMessage;
    if (!sessionRepository.loadSessions(errorMessage)) {
        viewRefreshCoordinator.reportStorageError(
            errorMessage.isEmpty() ? QStringLiteral("Cannot load sessions.") : errorMessage);
    }

    sessionController.setCurrentIndex(0);
    viewRefreshCoordinator.reloadCurrentSessionHistory();
    viewRefreshCoordinator.notifySessionCollectionViewsChanged();
}
