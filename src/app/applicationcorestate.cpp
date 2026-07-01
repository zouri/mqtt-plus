#include "app/applicationcorestate.h"

ApplicationCoreState::ApplicationCoreState(QObject *owner)
    : settings(QStringLiteral("mqtt-plus"), QStringLiteral("mqtt-plus"))
    , workspaceDependencies(*this)
    , sessionController(owner)
    , scriptController(owner)
    , subscriptionController(&controllerContexts.subscription(), owner)
    , mqttController(&controllerContexts.mqtt(), owner)
    , eventController(&controllerContexts.event(), owner)
    , themeController(&settings, owner)
    , languageController(&settings, owner)
    , preferencesController(&settings, owner)
    , sessionsModel(owner)
    , subscriptionsModel(owner)
    , filteredSubscriptionsModel(owner)
    , messagesModel(owner)
    , logsModel(owner)
    , scriptsModel(owner)
    , scriptTestSamplesModel(owner)
    , modelRefresher(
          sessionController,
          scriptController,
          subscriptionController,
          sessionsModel,
          subscriptionsModel,
          scriptsModel,
          scriptTestSamplesModel)
    , sessionRuntime(
          owner,
          {
              [this](const QString &sessionId) {
                  return controllerContexts.sessionById(sessionId);
              },
              [this](SessionState &session, const QString &channel, const QString &message) {
                  controllerContexts.appendEvent(session, channel, message);
              },
              [this]() {
                  controllerContexts.notifySessionViewsChanged();
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
    exitCleanup.setDependencies({
        &eventController,
        &historyStore,
        &preferencesController,
        &sessionController,
    });
    viewRefreshCoordinator.setDependencies({
        &notifier,
        &modelRefresher,
        &sessionController,
        &eventController,
        &messagesModel,
        &logsModel,
    });
    signalBindings.setDependencies({
        &controllerContexts,
        &notifier,
        &viewRefreshCoordinator,
        &languageController,
        &preferencesController,
        &scriptController,
        &subscriptionController,
        &themeController,
        &subscriptionFpsRefreshTimer,
    });
    startup.setDependencies({
        &controllerContexts,
        &modelRefresher,
        &sessionRepository,
        &scriptController,
        &sessionController,
    });
    controllerContexts.setDependencies({
        {
            &sessionRuntime,
            &sessionRepository,
            &viewRefreshCoordinator,
            &subscriptionController,
            &mqttController,
            &eventController,
            &preferencesController,
            &historyStore,
            &subscriptionFpsRefreshTimer,
        },
        {
            &viewRefreshCoordinator,
            &sessionController,
            &subscriptionController,
            &mqttController,
            &eventController,
        },
        {
            &viewRefreshCoordinator,
            &sessionController,
            &scriptController,
            &subscriptionController,
            &eventController,
            &preferencesController,
            &historyStore,
            &messagesModel,
            &logsModel,
            &scriptTestSamplesModel,
            &subscriptionFpsRefreshTimer,
            &launchTimestamp,
        },
        {
            &sessionRepository,
            &viewRefreshCoordinator,
            &sessionController,
            &scriptController,
            &subscriptionController,
            &eventController,
            &subscriptionsModel,
            &subscriptionFpsRefreshTimer,
        },
    });
    sessionController.setCore(&controllerContexts.session());
    filteredSubscriptionsModel.setSourceModel(&subscriptionsModel);
}
