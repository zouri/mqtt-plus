#include "app/appruntimestate.h"

#include "app/appcontrollerwiring.h"
#include "app/apphistoryservices.h"
#include "app/appmodelsync.h"
#include "app/appscriptservices.h"
#include "app/appsurfacedependencies.h"
#include "app/appsessionruntimebindings.h"
#include "app/appsessionlifecycle.h"
#include "app/appruntimeutils.h"
#include "app/uieventhub.h"

#include <memory>
#include <QtGlobal>

using namespace AppRuntimeUtils;

AppRuntimeState::AppRuntimeState(UiEventHub *uiEvents, QObject *parent)
    : QObject(parent)
    , m_uiEvents(uiEvents)
    , m_settings(QStringLiteral("mqtt-plus"), QStringLiteral("mqtt-plus"))
    , m_sessionController(this)
    , m_scriptController(this)
    , m_subscriptionController(this)
    , m_mqttController(this)
    , m_eventController(this)
    , m_themeController(&m_settings, this)
    , m_languageController(&m_settings, this)
    , m_preferencesController(&m_settings, this)
    , m_sessionsModel(this)
    , m_subscriptionsModel(this)
    , m_filteredSubscriptionsModel(this)
    , m_messagesModel(this)
    , m_logsModel(this)
    , m_scriptsModel(this)
    , m_scriptTestSamplesModel(this)
    , m_historyServices(std::make_unique<AppHistoryServices>(
          AppHistoryServices::Dependencies {
              .eventController = &m_eventController,
              .preferencesController = &m_preferencesController,
              .currentSession = [this]() {
                  return currentSessionState();
              },
          }))
    , m_modelSync(std::make_unique<AppModelSync>(
          AppModelSync::Dependencies {
              .sessionController = &m_sessionController,
              .subscriptionController = &m_subscriptionController,
              .scriptController = &m_scriptController,
              .sessionsModel = &m_sessionsModel,
              .subscriptionsModel = &m_subscriptionsModel,
              .messagesModel = &m_messagesModel,
              .logsModel = &m_logsModel,
              .scriptsModel = &m_scriptsModel,
              .scriptTestSamplesModel = &m_scriptTestSamplesModel,
              .currentSession = [this]() {
                  return currentSessionState();
              },
          }))
    , m_scriptServices(std::make_unique<AppScriptServices>(
          AppScriptServices::Dependencies {
              .scriptController = &m_scriptController,
              .sessionController = &m_sessionController,
              .scriptsModel = &m_scriptsModel,
              .scriptTestSamplesModel = &m_scriptTestSamplesModel,
              .modelSync = m_modelSync.get(),
              .saveSessions = [this]() {
                  return saveSessions();
              },
              .publishCurrentSessionAndSubscriptionsChanged = [this]() {
                  publishCurrentSessionAndSubscriptionsChanged();
              },
              .publishSessionAndSubscriptionViewsChanged = [this]() {
                  publishSessionAndSubscriptionViewsChanged();
              },
              .publishScriptsChanged = [this]() {
                  m_uiEvents->publishScriptsChanged();
              },
          }))
    , m_surfaceDependencies(std::make_unique<AppSurfaceDependencies>(
          AppSurfaceDependencies::Dependencies {
              .uiEvents = m_uiEvents,
              .modelSync = m_modelSync.get(),
              .scriptServices = m_scriptServices.get(),
              .historyServices = m_historyServices.get(),
              .themeController = &m_themeController,
              .languageController = &m_languageController,
              .preferencesController = &m_preferencesController,
              .sessionController = &m_sessionController,
              .subscriptionController = &m_subscriptionController,
              .mqttController = &m_mqttController,
              .eventController = &m_eventController,
              .sessionsModel = &m_sessionsModel,
              .filteredSubscriptionsModel = &m_filteredSubscriptionsModel,
              .messagesModel = &m_messagesModel,
              .logsModel = &m_logsModel,
              .currentSession = [this]() {
                  return currentSessionState();
              },
              .reloadCurrentSessionHistory = [this]() {
                  reloadCurrentSessionHistory();
              },
          }))
    , m_sessionRuntimeBindings(std::make_unique<AppSessionRuntimeBindings>(
          AppSessionRuntimeBindings::Dependencies {
              .bindSessionSignals = [this](SessionState *session) {
                  m_mqttController.bindSessionSignals(session);
              },
              .refreshSubscriptionFps = [this]() {
                  refreshSubscriptionFps();
              },
          },
          this))
    , m_sessionLifecycle(std::make_unique<AppSessionLifecycle>(
          AppSessionLifecycle::Dependencies {
              .settings = &m_settings,
              .runtimeParent = this,
              .sessionController = &m_sessionController,
              .scriptExists = [this](const QString &scriptId) {
                  return m_scriptServices->scriptExists(scriptId);
              },
              .sessionById = [this](const QString &sessionId) {
                  return sessionById(sessionId);
              },
              .bindSessionSignals = [this](SessionState *session) {
                  m_sessionRuntimeBindings->bindSessionSignals(session);
              },
              .appendEvent = [this](SessionState &session, const QString &channel, const QString &message) {
                  appendEvent(session, channel, message);
              },
              .reloadCurrentSessionHistory = [this]() {
                  reloadCurrentSessionHistory();
              },
              .publishSessionViewsChanged = [this]() {
                  publishSessionViewsChanged();
              },
              .publishSessionCollectionViewsChanged = [this]() {
                  publishSessionCollectionViewsChanged();
              },
              .reportStorageError = [this](const QString &message) {
                  reportStorageError(message);
              },
          }))
{
    Q_ASSERT(m_uiEvents);

    m_controllerWiring = std::make_unique<AppControllerWiring>(
        AppControllerWiring::Dependencies {
            .eventController = &m_eventController,
            .sessionController = &m_sessionController,
            .subscriptionController = &m_subscriptionController,
            .mqttController = &m_mqttController,
            .scriptController = &m_scriptController,
            .themeController = &m_themeController,
            .languageController = &m_languageController,
            .preferencesController = &m_preferencesController,
            .modelSync = m_modelSync.get(),
            .uiEvents = m_uiEvents,
            .historyStore = m_historyServices->historyStore(),
            .messagesModel = &m_messagesModel,
            .logsModel = &m_logsModel,
            .subscriptionsModel = &m_subscriptionsModel,
            .launchTimestamp = timestampNow(),
            .currentSession = [this]() {
                return currentSessionState();
            },
            .sessionById = [this](const QString &sessionId) {
                return sessionById(sessionId);
            },
            .subscriptionFpsRefreshActive = [this]() {
                return m_sessionRuntimeBindings->subscriptionFpsRefreshActive();
            },
            .startSubscriptionFpsRefresh = [this]() {
                m_sessionRuntimeBindings->startSubscriptionFpsRefresh();
            },
            .setSubscriptionFpsRefreshActive = [this](bool active) {
                m_sessionRuntimeBindings->setSubscriptionFpsRefreshActive(active);
            },
            .appendEvent = [this](SessionState &session, const QString &channel, const QString &message) {
                m_eventController.appendEvent(session, channel, message);
            },
            .reloadCurrentSessionHistory = [this]() {
                m_eventController.reloadCurrentSessionHistory();
            },
            .reportStorageError = [this](const QString &message) {
                reportStorageError(message);
            },
            .configureSession = [this](SessionState &session, const QVariantMap &config, bool keepNameFallback) {
                m_sessionLifecycle->configureSession(session, config, keepNameFallback);
            },
            .updatePublishStatus =
                [this](SessionState &session, const QString &state, const QString &reason, qint32 messageId) {
                    m_mqttController.updatePublishStatus(session, state, reason, messageId);
                },
            .saveSessions = [this]() {
                return m_sessionLifecycle->saveSessions();
            },
            .connectSession = [this](SessionState &session, const QString &eventPrefix) {
                m_mqttController.connectSession(session, eventPrefix);
            },
            .createDefaultSession = [this](const QString &name) {
                return m_sessionLifecycle->createDefaultSession(name);
            },
            .destroySessionRuntime = [this](SessionState &session) {
                m_sessionLifecycle->destroySessionRuntime(session);
            },
            .publishSelectedSessionViewsChanged = [this]() {
                publishSelectedSessionViewsChanged();
            },
            .publishCurrentSessionViewsChanged = [this]() {
                publishCurrentSessionViewsChanged();
            },
            .publishCurrentSessionAndSubscriptionsChanged = [this]() {
                publishCurrentSessionAndSubscriptionsChanged();
            },
            .publishSessionCollectionViewsChanged = [this]() {
                publishSessionCollectionViewsChanged();
            },
            .publishSessionViewsChanged = [this]() {
                publishSessionViewsChanged();
            },
            .publishSessionAndSubscriptionViewsChanged = [this]() {
                publishSessionAndSubscriptionViewsChanged();
            },
        });
    m_filteredSubscriptionsModel.setSourceModel(&m_subscriptionsModel);

    m_scriptServices->loadScripts();
    loadSessions();
}

AppRuntimeState::~AppRuntimeState()
{
    applyExitCleanup();
}

AppSurfaceDependencies *AppRuntimeState::surfaceDependencies()
{
    return m_surfaceDependencies.get();
}

SessionState *AppRuntimeState::currentSessionState()
{
    return m_sessionController.currentSession();
}

const SessionState *AppRuntimeState::currentSessionState() const
{
    return m_sessionController.currentSession();
}

SessionState *AppRuntimeState::sessionById(const QString &sessionId)
{
    return m_sessionController.sessionById(sessionId);
}

const SessionState *AppRuntimeState::sessionById(const QString &sessionId) const
{
    return m_sessionController.sessionById(sessionId);
}

void AppRuntimeState::appendEvent(SessionState &session, const QString &channel, const QString &message)
{
    m_eventController.appendEvent(session, channel, message);
}

void AppRuntimeState::refreshSubscriptionFps()
{
    m_subscriptionController.refreshSubscriptionFps();
}

void AppRuntimeState::reloadCurrentSessionHistory()
{
    m_eventController.reloadCurrentSessionHistory();
}

void AppRuntimeState::loadSessions()
{
    m_sessionLifecycle->loadSessions();
}

bool AppRuntimeState::saveSessions()
{
    return m_sessionLifecycle->saveSessions();
}

void AppRuntimeState::reportStorageError(const QString &message)
{
    if (message.isEmpty()) {
        return;
    }

    if (auto *session = currentSessionState()) {
        session->lastError = message;
        appendEvent(*session, QStringLiteral("Storage"), message);
    }

    publishSessionViewsChanged();
}

void AppRuntimeState::applyExitCleanup()
{
    m_historyServices->applyExitCleanup();
}

void AppRuntimeState::publishCurrentSessionViewsChanged()
{
    m_modelSync->refreshSessionsModel();
    m_uiEvents->publishCurrentSessionChanged();
}

void AppRuntimeState::publishCurrentSessionAndSubscriptionsChanged()
{
    m_modelSync->refreshSessionsModel();
    m_modelSync->refreshSubscriptionsModel();
    m_uiEvents->publishCurrentSessionChanged();
    m_uiEvents->publishSubscriptionsChanged();
}

void AppRuntimeState::publishSessionViewsChanged()
{
    m_modelSync->refreshSessionsModel();
    m_uiEvents->publishSessionsChanged();
    m_uiEvents->publishCurrentSessionChanged();
}

void AppRuntimeState::publishSessionAndSubscriptionViewsChanged()
{
    m_modelSync->refreshSessionsModel();
    m_modelSync->refreshSubscriptionsModel();
    m_uiEvents->publishSessionsChanged();
    m_uiEvents->publishCurrentSessionChanged();
    m_uiEvents->publishSubscriptionsChanged();
}

void AppRuntimeState::publishSelectedSessionViewsChanged()
{
    m_modelSync->syncSelectedSessionModels();
    m_uiEvents->publishCurrentSessionIndexChanged();
    m_uiEvents->publishCurrentSessionChanged();
    m_uiEvents->publishSubscriptionsChanged();
    m_uiEvents->publishMessageStreamChanged();
    m_uiEvents->publishLogsChanged();
    m_uiEvents->publishScriptsChanged();
    m_uiEvents->publishScriptTestSamplesChanged();
}

void AppRuntimeState::publishSessionCollectionViewsChanged()
{
    m_modelSync->syncSessionCollectionModels();
    m_uiEvents->publishSessionsChanged();
    m_uiEvents->publishCurrentSessionIndexChanged();
    m_uiEvents->publishCurrentSessionChanged();
    m_uiEvents->publishSubscriptionsChanged();
    m_uiEvents->publishMessageStreamChanged();
    m_uiEvents->publishLogsChanged();
    m_uiEvents->publishScriptsChanged();
}
