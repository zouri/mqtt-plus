#include "app/appfacade.h"

#include "app/appsettingsfacade.h"
#include "app/appfacadeutils.h"
#include "app/logstreamfacade.h"
#include "app/scriptlibraryfacade.h"
#include "app/workbenchfacade.h"

#include <QDateTime>

#include <memory>

using namespace AppFacadeUtils;

AppFacade::AppFacade(QObject *parent)
    : QObject(parent)
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
    , m_workbenchFacade(std::make_unique<WorkbenchFacade>(
          WorkbenchFacade::Dependencies {
              .sessionsModel = &m_sessionsModel,
              .filteredSubscriptionsModel = &m_filteredSubscriptionsModel,
              .messagesModel = &m_messagesModel,
              .sessionController = &m_sessionController,
              .subscriptionController = &m_subscriptionController,
              .mqttController = &m_mqttController,
              .eventController = &m_eventController,
              .currentSession = [this]() {
                  return currentSessionState();
              },
              .subscriptionByTopic = [this](const SessionState *session, const QString &topic) {
                  return session ? m_subscriptionController.subscriptionByTopic(session, topic) : nullptr;
              },
          },
          this))
    , m_settingsFacade(std::make_unique<AppSettingsFacade>(
          AppSettingsFacade::Dependencies {
              .themeController = &m_themeController,
              .languageController = &m_languageController,
              .preferencesController = &m_preferencesController,
              .sessionController = &m_sessionController,
              .historyStore = &m_historyStore,
              .messagesModel = &m_messagesModel,
              .logsModel = &m_logsModel,
              .flushPendingMessageHistory = [this]() {
                  m_eventController.flushPendingMessageHistory();
              },
              .reloadCurrentSessionHistory = [this]() {
                  reloadCurrentSessionHistory();
              },
              .refreshScriptTestSamplesModel = [this]() {
                  refreshScriptTestSamplesModel();
              },
              .emitMessageStreamChanged = [this]() {
                  emit messageStreamChanged();
              },
              .emitLogStreamChanged = [this]() {
                  emit logStreamChanged();
              },
              .emitScriptTestSamplesChanged = [this]() {
                  emit scriptTestSamplesChanged();
              },
          },
          this))
    , m_scriptLibraryFacade(std::make_unique<ScriptLibraryFacade>(
          ScriptLibraryFacade::Dependencies {
              .scriptsModel = &m_scriptsModel,
              .scriptTestSamplesModel = &m_scriptTestSamplesModel,
              .scriptController = &m_scriptController,
              .sessionController = &m_sessionController,
              .refreshScriptsModel = [this]() {
                  refreshScriptsModel();
              },
              .saveSessions = [this]() {
                  return saveSessions();
              },
              .notifyCurrentSessionAndSubscriptionsChanged = [this]() {
                  notifyCurrentSessionAndSubscriptionsChanged();
              },
              .notifySessionAndSubscriptionViewsChanged = [this]() {
                  notifySessionAndSubscriptionViewsChanged();
              },
              .emitScriptLibraryChanged = [this]() {
                  emit scriptLibraryChanged();
              },
          },
          this))
    , m_logStreamFacade(std::make_unique<LogStreamFacade>(
          LogStreamFacade::Dependencies {
              .logsModel = &m_logsModel,
              .eventController = &m_eventController,
          },
          this))
{
    m_launchTimestamp = timestampNow();
    m_eventController.setDependencies(EventController::Dependencies {
        .currentSession = [this]() {
            return currentSessionState();
        },
        .sessionById = [this](const QString &sessionId) {
            return sessionById(sessionId);
        },
        .historyPageSize = [this]() {
            return m_preferencesController.historyPageSize();
        },
        .messageRetentionLimit = [this]() {
            return m_preferencesController.messageRetentionLimit();
        },
        .logRetentionLimit = [this]() {
            return m_preferencesController.logRetentionLimit();
        },
        .maxIncomingPayloadBytes = [this]() {
            return m_preferencesController.maxIncomingPayloadBytes();
        },
        .saveMessagesWhenOutputPaused = [this]() {
            return m_preferencesController.saveMessagesWhenOutputPaused();
        },
        .scriptById = [this](const QString &scriptId) {
            return m_scriptController.scriptById(scriptId);
        },
        .scriptName = [this](const QString &scriptId) {
            return m_scriptController.scriptName(scriptId);
        },
        .bestSubscriptionForTopic = [this](const SessionState &session, const QString &topic) {
            return m_subscriptionController.bestSubscriptionForTopic(session, topic);
        },
        .refreshScriptTestSamplesModel = [this]() {
            refreshScriptTestSamplesModel();
        },
        .refreshSubscriptionsModel = [this]() {
            refreshSubscriptionsModel();
        },
        .subscriptionFpsRefreshActive = [this]() {
            return m_subscriptionFpsRefreshTimer.isActive();
        },
        .startSubscriptionFpsRefresh = [this]() {
            m_subscriptionFpsRefreshTimer.start();
        },
        .emitMessageStreamChanged = [this]() {
            emit messageStreamChanged();
        },
        .emitLogStreamChanged = [this]() {
            emit logStreamChanged();
        },
        .emitMessageStreamRowAppended = [this](const QVariantMap &row) {
            emit messageStreamRowAppended(row);
        },
        .emitLogStreamRowAppended = [this](const QVariantMap &row) {
            emit logStreamRowAppended(row);
        },
        .emitScriptTestSamplesChanged = [this]() {
            emit scriptTestSamplesChanged();
        },
        .emitSubscriptionsChanged = [this]() {
            emit subscriptionsChanged();
        },
        .historyStore = &m_historyStore,
        .messagesModel = &m_messagesModel,
        .logsModel = &m_logsModel,
        .launchTimestamp = m_launchTimestamp,
    });
    m_sessionController.setDependencies(SessionController::Dependencies {
        .reloadCurrentSessionHistory = [this]() {
            reloadCurrentSessionHistory();
        },
        .currentSessionHasActiveSubscriptionFps = [this]() {
            return m_subscriptionController.currentSessionHasActiveSubscriptionFps(QDateTime::currentMSecsSinceEpoch());
        },
        .setSubscriptionFpsRefreshActive = [this](bool active) {
            if (active) {
                m_subscriptionFpsRefreshTimer.start();
            } else {
                m_subscriptionFpsRefreshTimer.stop();
            }
        },
        .configureSession = [this](SessionState &session, const QVariantMap &config, bool keepNameFallback) {
            configureSession(session, config, keepNameFallback);
        },
        .updatePublishStatus = [this](
                                   SessionState &session,
                                   const QString &state,
                                   const QString &reason,
                                   qint32 messageId) {
            updatePublishStatus(session, state, reason, messageId);
        },
        .saveSessions = [this]() {
            return saveSessions();
        },
        .connectSession = [this](SessionState &session, const QString &eventPrefix) {
            connectSession(session, eventPrefix);
        },
        .createDefaultSession = [this](const QString &name) {
            return createDefaultSession(name);
        },
        .deleteHistoryWithSession = [this]() {
            return m_preferencesController.deleteHistoryWithSession();
        },
        .clearSessionHistory = [this](const QString &sessionId) {
            m_historyStore.clearSessionHistory(sessionId);
        },
        .destroySessionRuntime = [this](SessionState &session) {
            destroySessionRuntime(session);
        },
        .notifySelectedSessionViewsChanged = [this]() {
            notifySelectedSessionViewsChanged();
        },
        .notifyCurrentSessionViewsChanged = [this]() {
            notifyCurrentSessionViewsChanged();
        },
        .notifyCurrentSessionAndSubscriptionsChanged = [this]() {
            notifyCurrentSessionAndSubscriptionsChanged();
        },
        .notifySessionCollectionViewsChanged = [this]() {
            notifySessionCollectionViewsChanged();
        },
        .emitSessionsChanged = [this]() {
            emit sessionsChanged();
        },
        .emitMessageStreamChanged = [this]() {
            emit messageStreamChanged();
        },
    });
    m_subscriptionController.setDependencies(SubscriptionController::Dependencies {
        .currentSession = [this]() {
            return currentSessionState();
        },
        .sessionById = [this](const QString &sessionId) {
            return sessionById(sessionId);
        },
        .appendEvent = [this](SessionState &session, const QString &channel, const QString &message) {
            appendEvent(session, channel, message);
        },
        .scriptExists = [this](const QString &scriptId) {
            return m_scriptController.scriptById(scriptId) != nullptr;
        },
        .saveSessions = [this]() {
            return saveSessions();
        },
        .notifySessionAndSubscriptionViewsChanged = [this]() {
            notifySessionAndSubscriptionViewsChanged();
        },
        .notifyCurrentSessionAndSubscriptionsChanged = [this]() {
            notifyCurrentSessionAndSubscriptionsChanged();
        },
        .refreshSubscriptionsModel = [this]() {
            refreshSubscriptionsModel();
        },
        .emitSubscriptionsChanged = [this]() {
            emit subscriptionsChanged();
        },
        .setSubscriptionFpsRefreshActive = [this](bool active) {
            if (active) {
                m_subscriptionFpsRefreshTimer.start();
            } else {
                m_subscriptionFpsRefreshTimer.stop();
            }
        },
        .setTopicFpsRows = [this](const QVector<SubscriptionFpsRow> &rows) {
            m_subscriptionsModel.setTopicFpsRows(rows);
        },
    });
    m_mqttController.setDependencies(MqttController::Dependencies {
        .currentSession = [this]() {
            return currentSessionState();
        },
        .sessionById = [this](const QString &sessionId) {
            return sessionById(sessionId);
        },
        .appendEvent = [this](SessionState &session, const QString &channel, const QString &message) {
            appendEvent(session, channel, message);
        },
        .restoreActiveSubscriptions = [this](SessionState &session, bool emitEvents) {
            m_subscriptionController.restoreActiveSubscriptions(session, emitEvents);
        },
        .resetRuntimeSubscriptions = [this](SessionState &session) {
            m_subscriptionController.resetRuntimeSubscriptions(session);
        },
        .appendIncomingMessage = [this](
                                     const QString &sessionId,
                                     const QString &topic,
                                     const QByteArray &payloadBytes) {
            m_eventController.appendIncomingMessage(sessionId, topic, payloadBytes);
        },
        .notifySessionViewsChanged = [this]() {
            notifySessionViewsChanged();
        },
        .notifyCurrentSessionViewsChanged = [this]() {
            notifyCurrentSessionViewsChanged();
        },
        .notifySessionAndSubscriptionViewsChanged = [this]() {
            notifySessionAndSubscriptionViewsChanged();
        },
    });
    m_filteredSubscriptionsModel.setSourceModel(&m_subscriptionsModel);
    connect(this, &AppFacade::sessionsChanged, m_workbenchFacade.get(), &WorkbenchFacade::sessionsChanged);
    connect(this, &AppFacade::currentSessionIndexChanged, m_workbenchFacade.get(), &WorkbenchFacade::currentSessionIndexChanged);
    connect(this, &AppFacade::currentSessionChanged, m_workbenchFacade.get(), &WorkbenchFacade::currentSessionChanged);
    connect(this, &AppFacade::subscriptionsChanged, m_workbenchFacade.get(), &WorkbenchFacade::subscriptionsChanged);
    connect(this, &AppFacade::messageStreamChanged, m_workbenchFacade.get(), &WorkbenchFacade::messageStreamChanged);
    connect(this, &AppFacade::messageStreamRowAppended, m_workbenchFacade.get(), &WorkbenchFacade::messageStreamRowAppended);
    connect(this, &AppFacade::scriptLibraryChanged, m_scriptLibraryFacade.get(), &ScriptLibraryFacade::scriptLibraryChanged);
    connect(this, &AppFacade::scriptTestSamplesChanged, m_scriptLibraryFacade.get(), &ScriptLibraryFacade::scriptTestSamplesChanged);
    connect(this, &AppFacade::logStreamChanged, m_logStreamFacade.get(), &LogStreamFacade::logStreamChanged);
    connect(this, &AppFacade::logStreamRowAppended, m_logStreamFacade.get(), &LogStreamFacade::logStreamRowAppended);
    connect(this, &AppFacade::themeModeChanged, m_settingsFacade.get(), &AppSettingsFacade::themeModeChanged);
    connect(this, &AppFacade::effectiveThemeChanged, m_settingsFacade.get(), &AppSettingsFacade::effectiveThemeChanged);
    connect(this, &AppFacade::languageModeChanged, m_settingsFacade.get(), &AppSettingsFacade::languageModeChanged);
    connect(this, &AppFacade::languageChanged, m_settingsFacade.get(), &AppSettingsFacade::languageChanged);
    connect(this, &AppFacade::messageRetentionLimitChanged, m_settingsFacade.get(), &AppSettingsFacade::messageRetentionLimitChanged);
    connect(this, &AppFacade::logRetentionLimitChanged, m_settingsFacade.get(), &AppSettingsFacade::logRetentionLimitChanged);
    connect(this, &AppFacade::historyPageSizeChanged, m_settingsFacade.get(), &AppSettingsFacade::historyPageSizeChanged);
    connect(this, &AppFacade::maxIncomingPayloadBytesChanged, m_settingsFacade.get(), &AppSettingsFacade::maxIncomingPayloadBytesChanged);
    connect(this, &AppFacade::deleteHistoryWithSessionChanged, m_settingsFacade.get(), &AppSettingsFacade::deleteHistoryWithSessionChanged);
    connect(this, &AppFacade::saveMessagesWhenOutputPausedChanged, m_settingsFacade.get(), &AppSettingsFacade::saveMessagesWhenOutputPausedChanged);
    connect(this, &AppFacade::clearMessagesOnExitChanged, m_settingsFacade.get(), &AppSettingsFacade::clearMessagesOnExitChanged);
    connect(this, &AppFacade::clearLogsOnExitChanged, m_settingsFacade.get(), &AppSettingsFacade::clearLogsOnExitChanged);
    connect(this, &AppFacade::windowWidthChanged, m_settingsFacade.get(), &AppSettingsFacade::windowWidthChanged);
    connect(this, &AppFacade::windowHeightChanged, m_settingsFacade.get(), &AppSettingsFacade::windowHeightChanged);
    connect(this, &AppFacade::windowMaximizedChanged, m_settingsFacade.get(), &AppSettingsFacade::windowMaximizedChanged);
    connect(&m_scriptController, &ScriptController::storageError, this, &AppFacade::reportStorageError);
    connect(&m_themeController, &ThemeController::modeChanged, this, &AppFacade::themeModeChanged);
    connect(&m_themeController, &ThemeController::effectiveThemeChanged, this, &AppFacade::effectiveThemeChanged);
    connect(&m_languageController, &LanguageController::modeChanged, this, &AppFacade::languageModeChanged);
    connect(&m_languageController, &LanguageController::languageChanged, this, [this]() {
        refreshSessionsModel();
        refreshSubscriptionsModel();
        emit currentSessionChanged();
        emit sessionsChanged();
        emit subscriptionsChanged();
        emit languageChanged();
    });
    connect(&m_preferencesController, &PreferencesController::messageRetentionLimitChanged, this, &AppFacade::messageRetentionLimitChanged);
    connect(&m_preferencesController, &PreferencesController::logRetentionLimitChanged, this, &AppFacade::logRetentionLimitChanged);
    connect(&m_preferencesController, &PreferencesController::historyPageSizeChanged, this, [this]() {
        reloadCurrentSessionHistory();
        emit messageStreamChanged();
        emit logStreamChanged();
        emit historyPageSizeChanged();
    });
    connect(&m_preferencesController, &PreferencesController::maxIncomingPayloadBytesChanged, this, &AppFacade::maxIncomingPayloadBytesChanged);
    connect(&m_preferencesController, &PreferencesController::deleteHistoryWithSessionChanged, this, &AppFacade::deleteHistoryWithSessionChanged);
    connect(&m_preferencesController, &PreferencesController::saveMessagesWhenOutputPausedChanged, this, &AppFacade::saveMessagesWhenOutputPausedChanged);
    connect(&m_preferencesController, &PreferencesController::clearMessagesOnExitChanged, this, &AppFacade::clearMessagesOnExitChanged);
    connect(&m_preferencesController, &PreferencesController::clearLogsOnExitChanged, this, &AppFacade::clearLogsOnExitChanged);
    connect(&m_preferencesController, &PreferencesController::windowWidthChanged, this, &AppFacade::windowWidthChanged);
    connect(&m_preferencesController, &PreferencesController::windowHeightChanged, this, &AppFacade::windowHeightChanged);
    connect(&m_preferencesController, &PreferencesController::windowMaximizedChanged, this, &AppFacade::windowMaximizedChanged);

    m_subscriptionFpsRefreshTimer.setInterval(kSubscriptionFpsRefreshIntervalMs);
    connect(
        &m_subscriptionFpsRefreshTimer,
        &QTimer::timeout,
        this,
        &AppFacade::refreshSubscriptionFps);
    loadScripts();
    loadSessions();
}

AppFacade::~AppFacade()
{
    applyExitCleanup();
}

void AppFacade::applyExitCleanup()
{
    m_eventController.flushPendingMessageHistory();

    const auto clearMessages = [this](const QString &mode) {
        if (mode == QStringLiteral("all")) {
            m_historyStore.clearAllMessages();
        } else if (mode == QStringLiteral("current")) {
            if (auto *session = currentSessionState()) {
                m_historyStore.clearMessages(session->id);
            }
        }
    };
    const auto clearLogs = [this](const QString &mode) {
        if (mode == QStringLiteral("all")) {
            m_historyStore.clearAllLogs();
        } else if (mode == QStringLiteral("current")) {
            if (auto *session = currentSessionState()) {
                m_historyStore.clearLogs(session->id);
            }
        }
    };

    clearMessages(m_preferencesController.clearMessagesOnExit());
    clearLogs(m_preferencesController.clearLogsOnExit());
}

void AppFacade::notifyCurrentSessionViewsChanged()
{
    refreshSessionsModel();
    emit currentSessionChanged();
}

void AppFacade::notifyCurrentSessionAndSubscriptionsChanged()
{
    refreshSessionsModel();
    refreshSubscriptionsModel();
    emit currentSessionChanged();
    emit subscriptionsChanged();
}

void AppFacade::notifySessionViewsChanged()
{
    refreshSessionsModel();
    emit sessionsChanged();
    emit currentSessionChanged();
}

void AppFacade::notifySessionAndSubscriptionViewsChanged()
{
    refreshSessionsModel();
    refreshSubscriptionsModel();
    emit sessionsChanged();
    emit currentSessionChanged();
    emit subscriptionsChanged();
}

void AppFacade::notifySelectedSessionViewsChanged()
{
    refreshSubscriptionsModel();
    m_messagesModel.setRows(currentSessionState() ? currentSessionState()->messageRows : QVariantList {});
    m_logsModel.setRows(currentSessionState() ? currentSessionState()->logRows : QVariantList {});
    refreshScriptTestSamplesModel();
    emit currentSessionIndexChanged();
    emit currentSessionChanged();
    emit subscriptionsChanged();
    emit messageStreamChanged();
    emit logStreamChanged();
    emit scriptLibraryChanged();
    emit scriptTestSamplesChanged();
}

void AppFacade::notifySessionCollectionViewsChanged()
{
    refreshSessionsModel();
    refreshSubscriptionsModel();
    m_messagesModel.setRows(currentSessionState() ? currentSessionState()->messageRows : QVariantList {});
    m_logsModel.setRows(currentSessionState() ? currentSessionState()->logRows : QVariantList {});
    refreshScriptsModel();
    refreshScriptTestSamplesModel();
    emit sessionsChanged();
    emit currentSessionIndexChanged();
    emit currentSessionChanged();
    emit subscriptionsChanged();
    emit messageStreamChanged();
    emit logStreamChanged();
    emit scriptLibraryChanged();
}
