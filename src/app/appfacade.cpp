#include "app/appfacade.h"

#include "app/appeventbus.h"
#include "app/settingsbus.h"
#include "app/appfacadeutils.h"
#include "app/logsbus.h"
#include "app/modelcoordinator.h"
#include "app/scriptsbus.h"
#include "app/sessionlifecycleservice.h"
#include "app/workbenchbus.h"

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
    , m_modelCoordinator(std::make_unique<ModelCoordinator>(
          ModelCoordinator::Dependencies {
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
    , m_sessionLifecycleService(std::make_unique<SessionLifecycleService>(
          SessionLifecycleService::Dependencies {
              .settings = &m_settings,
              .runtimeParent = this,
              .sessionController = &m_sessionController,
              .scriptExists = [this](const QString &scriptId) {
                  return m_scriptController.scriptById(scriptId) != nullptr;
              },
              .sessionById = [this](const QString &sessionId) {
                  return sessionById(sessionId);
              },
              .bindSessionSignals = [this](SessionState *session) {
                  bindSessionSignals(session);
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
          },
          this))
    , m_workbenchBus(std::make_unique<WorkbenchBus>(
          WorkbenchBus::Dependencies {
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
    , m_settingsBus(std::make_unique<SettingsBus>(
          SettingsBus::Dependencies {
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
              .syncScriptTestSamplesModel = [this]() {
                  m_modelCoordinator->refreshScriptTestSamplesModel();
              },
              .publishMessageStreamChanged = [this]() {
                  publishMessageStreamChanged();
              },
              .publishLogsChanged = [this]() {
                  publishLogsChanged();
              },
              .publishScriptTestSamplesChanged = [this]() {
                  publishScriptTestSamplesChanged();
              },
          },
          this))
    , m_scriptsBus(std::make_unique<ScriptsBus>(
          ScriptsBus::Dependencies {
              .scriptsModel = &m_scriptsModel,
              .scriptTestSamplesModel = &m_scriptTestSamplesModel,
              .scriptController = &m_scriptController,
              .sessionController = &m_sessionController,
              .syncScriptsModel = [this]() {
                  m_modelCoordinator->refreshScriptsModel();
              },
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
                  publishScriptsChanged();
              },
          },
          this))
    , m_logsBus(std::make_unique<LogsBus>(
          LogsBus::Dependencies {
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
        .syncScriptTestSamplesModel = [this]() {
            m_modelCoordinator->refreshScriptTestSamplesModel();
        },
        .syncSubscriptionsModel = [this]() {
            m_modelCoordinator->refreshSubscriptionsModel();
        },
        .subscriptionFpsRefreshActive = [this]() {
            return m_subscriptionFpsRefreshTimer.isActive();
        },
        .startSubscriptionFpsRefresh = [this]() {
            m_subscriptionFpsRefreshTimer.start();
        },
        .publishMessageStreamChanged = [this]() {
            publishMessageStreamChanged();
        },
        .publishLogsChanged = [this]() {
            publishLogsChanged();
        },
        .publishMessageStreamRowAppended = [this](const QVariantMap &row) {
            publishMessageStreamRowAppended(row);
        },
        .publishLogsRowAppended = [this](const QVariantMap &row) {
            publishLogsRowAppended(row);
        },
        .publishScriptTestSamplesChanged = [this]() {
            publishScriptTestSamplesChanged();
        },
        .publishSubscriptionsChanged = [this]() {
            publishSubscriptionsChanged();
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
        .publishSessionsChanged = [this]() {
            publishSessionsChanged();
        },
        .publishMessageStreamChanged = [this]() {
            publishMessageStreamChanged();
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
        .publishSessionAndSubscriptionViewsChanged = [this]() {
            publishSessionAndSubscriptionViewsChanged();
        },
        .publishCurrentSessionAndSubscriptionsChanged = [this]() {
            publishCurrentSessionAndSubscriptionsChanged();
        },
        .syncSubscriptionsModel = [this]() {
            m_modelCoordinator->refreshSubscriptionsModel();
        },
        .publishSubscriptionsChanged = [this]() {
            publishSubscriptionsChanged();
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
        .publishSessionViewsChanged = [this]() {
            publishSessionViewsChanged();
        },
        .publishCurrentSessionViewsChanged = [this]() {
            publishCurrentSessionViewsChanged();
        },
        .publishSessionAndSubscriptionViewsChanged = [this]() {
            publishSessionAndSubscriptionViewsChanged();
        },
    });
    m_filteredSubscriptionsModel.setSourceModel(&m_subscriptionsModel);
    connect(this, &AppFacade::sessionsChanged, m_workbenchBus.get(), &WorkbenchBus::sessionsChanged);
    connect(this, &AppFacade::currentSessionIndexChanged, m_workbenchBus.get(), &WorkbenchBus::currentSessionIndexChanged);
    connect(this, &AppFacade::currentSessionChanged, m_workbenchBus.get(), &WorkbenchBus::currentSessionChanged);
    connect(this, &AppFacade::subscriptionsChanged, m_workbenchBus.get(), &WorkbenchBus::subscriptionsChanged);
    connect(this, &AppFacade::messageStreamChanged, m_workbenchBus.get(), &WorkbenchBus::messageStreamChanged);
    connect(this, &AppFacade::messageStreamRowAppended, m_workbenchBus.get(), &WorkbenchBus::messageStreamRowAppended);
    connect(this, &AppFacade::scriptsChanged, m_scriptsBus.get(), &ScriptsBus::scriptsChanged);
    connect(this, &AppFacade::scriptTestSamplesChanged, m_scriptsBus.get(), &ScriptsBus::scriptTestSamplesChanged);
    connect(this, &AppFacade::logsChanged, m_logsBus.get(), &LogsBus::logsChanged);
    connect(this, &AppFacade::logsRowAppended, m_logsBus.get(), &LogsBus::logsRowAppended);
    connect(this, &AppFacade::themeModeChanged, m_settingsBus.get(), &SettingsBus::themeModeChanged);
    connect(this, &AppFacade::effectiveThemeChanged, m_settingsBus.get(), &SettingsBus::effectiveThemeChanged);
    connect(this, &AppFacade::languageModeChanged, m_settingsBus.get(), &SettingsBus::languageModeChanged);
    connect(this, &AppFacade::languageChanged, m_settingsBus.get(), &SettingsBus::languageChanged);
    connect(this, &AppFacade::messageRetentionLimitChanged, m_settingsBus.get(), &SettingsBus::messageRetentionLimitChanged);
    connect(this, &AppFacade::logRetentionLimitChanged, m_settingsBus.get(), &SettingsBus::logRetentionLimitChanged);
    connect(this, &AppFacade::historyPageSizeChanged, m_settingsBus.get(), &SettingsBus::historyPageSizeChanged);
    connect(this, &AppFacade::deleteHistoryWithSessionChanged, m_settingsBus.get(), &SettingsBus::deleteHistoryWithSessionChanged);
    connect(this, &AppFacade::saveMessagesWhenOutputPausedChanged, m_settingsBus.get(), &SettingsBus::saveMessagesWhenOutputPausedChanged);
    connect(this, &AppFacade::clearMessagesOnExitChanged, m_settingsBus.get(), &SettingsBus::clearMessagesOnExitChanged);
    connect(this, &AppFacade::clearLogsOnExitChanged, m_settingsBus.get(), &SettingsBus::clearLogsOnExitChanged);
    connect(this, &AppFacade::windowWidthChanged, m_settingsBus.get(), &SettingsBus::windowWidthChanged);
    connect(this, &AppFacade::windowHeightChanged, m_settingsBus.get(), &SettingsBus::windowHeightChanged);
    connect(this, &AppFacade::windowMaximizedChanged, m_settingsBus.get(), &SettingsBus::windowMaximizedChanged);
    connect(&m_scriptController, &ScriptController::storageError, this, &AppFacade::reportStorageError);
    connect(&m_themeController, &ThemeController::modeChanged, this, [this]() {
        publishThemeModeChanged();
    });
    connect(&m_themeController, &ThemeController::effectiveThemeChanged, this, [this]() {
        publishEffectiveThemeChanged();
    });
    connect(&m_languageController, &LanguageController::modeChanged, this, [this]() {
        publishLanguageModeChanged();
    });
    connect(&m_languageController, &LanguageController::languageChanged, this, [this]() {
        m_modelCoordinator->refreshSessionsModel();
        m_modelCoordinator->refreshSubscriptionsModel();
        publishCurrentSessionChanged();
        publishSessionsChanged();
        publishSubscriptionsChanged();
        publishLanguageChanged();
    });
    connect(&m_preferencesController, &PreferencesController::messageRetentionLimitChanged, this, [this]() {
        publishMessageRetentionLimitChanged();
    });
    connect(&m_preferencesController, &PreferencesController::logRetentionLimitChanged, this, [this]() {
        publishLogRetentionLimitChanged();
    });
    connect(&m_preferencesController, &PreferencesController::historyPageSizeChanged, this, [this]() {
        reloadCurrentSessionHistory();
        publishMessageStreamChanged();
        publishLogsChanged();
        publishHistoryPageSizeChanged();
    });
    connect(&m_preferencesController, &PreferencesController::deleteHistoryWithSessionChanged, this, [this]() {
        publishDeleteHistoryWithSessionChanged();
    });
    connect(&m_preferencesController, &PreferencesController::saveMessagesWhenOutputPausedChanged, this, [this]() {
        publishSaveMessagesWhenOutputPausedChanged();
    });
    connect(&m_preferencesController, &PreferencesController::clearMessagesOnExitChanged, this, [this]() {
        publishClearMessagesOnExitChanged();
    });
    connect(&m_preferencesController, &PreferencesController::clearLogsOnExitChanged, this, [this]() {
        publishClearLogsOnExitChanged();
    });
    connect(&m_preferencesController, &PreferencesController::windowWidthChanged, this, [this]() {
        publishWindowWidthChanged();
    });
    connect(&m_preferencesController, &PreferencesController::windowHeightChanged, this, [this]() {
        publishWindowHeightChanged();
    });
    connect(&m_preferencesController, &PreferencesController::windowMaximizedChanged, this, [this]() {
        publishWindowMaximizedChanged();
    });

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

WorkbenchBus *AppFacade::workbench()
{
    return m_workbenchBus.get();
}

SettingsBus *AppFacade::settings()
{
    return m_settingsBus.get();
}

ScriptsBus *AppFacade::scripts()
{
    return m_scriptsBus.get();
}

LogsBus *AppFacade::logs()
{
    return m_logsBus.get();
}

void AppFacade::setEventBus(AppEventBus *eventBus)
{
    m_eventBus = eventBus;
}

SessionState *AppFacade::currentSessionState()
{
    return m_sessionController.currentSession();
}

const SessionState *AppFacade::currentSessionState() const
{
    return m_sessionController.currentSession();
}

SessionState *AppFacade::sessionById(const QString &sessionId)
{
    return m_sessionController.sessionById(sessionId);
}

const SessionState *AppFacade::sessionById(const QString &sessionId) const
{
    return m_sessionController.sessionById(sessionId);
}

void AppFacade::bindSessionSignals(SessionState *session)
{
    m_mqttController.bindSessionSignals(session);
}

void AppFacade::configureSession(SessionState &session, const QVariantMap &config, bool keepNameFallback)
{
    m_sessionLifecycleService->configureSession(session, config, keepNameFallback);
}

void AppFacade::initializeSessionRuntime(SessionState *session)
{
    m_sessionLifecycleService->initializeSessionRuntime(session);
}

void AppFacade::destroySessionRuntime(SessionState &session)
{
    m_sessionLifecycleService->destroySessionRuntime(session);
}

void AppFacade::connectSession(SessionState &session, const QString &eventPrefix)
{
    m_mqttController.connectSession(session, eventPrefix);
}

QSslConfiguration AppFacade::sslConfigurationForSession(const SessionState &session, QString &errorMessage) const
{
    return m_mqttController.sslConfigurationForSession(session, errorMessage);
}

void AppFacade::updatePublishStatus(
    SessionState &session,
    const QString &state,
    const QString &reason,
    qint32 messageId)
{
    m_mqttController.updatePublishStatus(session, state, reason, messageId);
}

void AppFacade::appendEvent(SessionState &session, const QString &channel, const QString &message)
{
    m_eventController.appendEvent(session, channel, message);
}

void AppFacade::refreshSubscriptionFps()
{
    m_subscriptionController.refreshSubscriptionFps();
}

void AppFacade::reloadCurrentSessionHistory()
{
    m_eventController.reloadCurrentSessionHistory();
}

void AppFacade::loadScripts()
{
    m_scriptController.loadScripts();
    m_modelCoordinator->refreshScriptsModel();
}

void AppFacade::loadSessions()
{
    m_sessionLifecycleService->loadSessions();
}

bool AppFacade::saveSessions()
{
    return m_sessionLifecycleService->saveSessions();
}

SessionState AppFacade::createDefaultSession(const QString &name)
{
    return m_sessionLifecycleService->createDefaultSession(name);
}

void AppFacade::reportStorageError(const QString &message)
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

void AppFacade::publishSessionsChanged()
{
    if (m_eventBus) {
        m_eventBus->publishSessionsChanged();
    }
    emit sessionsChanged();
}

void AppFacade::publishCurrentSessionIndexChanged()
{
    if (m_eventBus) {
        m_eventBus->publishCurrentSessionIndexChanged();
    }
    emit currentSessionIndexChanged();
}

void AppFacade::publishCurrentSessionChanged()
{
    if (m_eventBus) {
        m_eventBus->publishCurrentSessionChanged();
    }
    emit currentSessionChanged();
}

void AppFacade::publishSubscriptionsChanged()
{
    if (m_eventBus) {
        m_eventBus->publishSubscriptionsChanged();
    }
    emit subscriptionsChanged();
}

void AppFacade::publishMessageStreamChanged()
{
    if (m_eventBus) {
        m_eventBus->publishMessageStreamChanged();
    }
    emit messageStreamChanged();
}

void AppFacade::publishLogsChanged()
{
    if (m_eventBus) {
        m_eventBus->publishLogsChanged();
    }
    emit logsChanged();
}

void AppFacade::publishMessageStreamRowAppended(const QVariantMap &row)
{
    if (m_eventBus) {
        m_eventBus->publishMessageStreamRowAppended(row);
    }
    emit messageStreamRowAppended(row);
}

void AppFacade::publishLogsRowAppended(const QVariantMap &row)
{
    if (m_eventBus) {
        m_eventBus->publishLogsRowAppended(row);
    }
    emit logsRowAppended(row);
}

void AppFacade::publishScriptsChanged()
{
    if (m_eventBus) {
        m_eventBus->publishScriptsChanged();
    }
    emit scriptsChanged();
}

void AppFacade::publishScriptTestSamplesChanged()
{
    if (m_eventBus) {
        m_eventBus->publishScriptTestSamplesChanged();
    }
    emit scriptTestSamplesChanged();
}

void AppFacade::publishThemeModeChanged()
{
    if (m_eventBus) {
        m_eventBus->publishThemeModeChanged();
    }
    emit themeModeChanged();
}

void AppFacade::publishEffectiveThemeChanged()
{
    if (m_eventBus) {
        m_eventBus->publishEffectiveThemeChanged();
    }
    emit effectiveThemeChanged();
}

void AppFacade::publishLanguageModeChanged()
{
    if (m_eventBus) {
        m_eventBus->publishLanguageModeChanged();
    }
    emit languageModeChanged();
}

void AppFacade::publishLanguageChanged()
{
    if (m_eventBus) {
        m_eventBus->publishLanguageChanged();
    }
    emit languageChanged();
}

void AppFacade::publishMessageRetentionLimitChanged()
{
    if (m_eventBus) {
        m_eventBus->publishMessageRetentionLimitChanged();
    }
    emit messageRetentionLimitChanged();
}

void AppFacade::publishLogRetentionLimitChanged()
{
    if (m_eventBus) {
        m_eventBus->publishLogRetentionLimitChanged();
    }
    emit logRetentionLimitChanged();
}

void AppFacade::publishHistoryPageSizeChanged()
{
    if (m_eventBus) {
        m_eventBus->publishHistoryPageSizeChanged();
    }
    emit historyPageSizeChanged();
}

void AppFacade::publishDeleteHistoryWithSessionChanged()
{
    if (m_eventBus) {
        m_eventBus->publishDeleteHistoryWithSessionChanged();
    }
    emit deleteHistoryWithSessionChanged();
}

void AppFacade::publishSaveMessagesWhenOutputPausedChanged()
{
    if (m_eventBus) {
        m_eventBus->publishSaveMessagesWhenOutputPausedChanged();
    }
    emit saveMessagesWhenOutputPausedChanged();
}

void AppFacade::publishClearMessagesOnExitChanged()
{
    if (m_eventBus) {
        m_eventBus->publishClearMessagesOnExitChanged();
    }
    emit clearMessagesOnExitChanged();
}

void AppFacade::publishClearLogsOnExitChanged()
{
    if (m_eventBus) {
        m_eventBus->publishClearLogsOnExitChanged();
    }
    emit clearLogsOnExitChanged();
}

void AppFacade::publishWindowWidthChanged()
{
    if (m_eventBus) {
        m_eventBus->publishWindowWidthChanged();
    }
    emit windowWidthChanged();
}

void AppFacade::publishWindowHeightChanged()
{
    if (m_eventBus) {
        m_eventBus->publishWindowHeightChanged();
    }
    emit windowHeightChanged();
}

void AppFacade::publishWindowMaximizedChanged()
{
    if (m_eventBus) {
        m_eventBus->publishWindowMaximizedChanged();
    }
    emit windowMaximizedChanged();
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

void AppFacade::publishCurrentSessionViewsChanged()
{
    m_modelCoordinator->refreshSessionsModel();
    publishCurrentSessionChanged();
}

void AppFacade::publishCurrentSessionAndSubscriptionsChanged()
{
    m_modelCoordinator->refreshSessionsModel();
    m_modelCoordinator->refreshSubscriptionsModel();
    publishCurrentSessionChanged();
    publishSubscriptionsChanged();
}

void AppFacade::publishSessionViewsChanged()
{
    m_modelCoordinator->refreshSessionsModel();
    publishSessionsChanged();
    publishCurrentSessionChanged();
}

void AppFacade::publishSessionAndSubscriptionViewsChanged()
{
    m_modelCoordinator->refreshSessionsModel();
    m_modelCoordinator->refreshSubscriptionsModel();
    publishSessionsChanged();
    publishCurrentSessionChanged();
    publishSubscriptionsChanged();
}

void AppFacade::publishSelectedSessionViewsChanged()
{
    m_modelCoordinator->syncSelectedSessionModels();
    publishCurrentSessionIndexChanged();
    publishCurrentSessionChanged();
    publishSubscriptionsChanged();
    publishMessageStreamChanged();
    publishLogsChanged();
    publishScriptsChanged();
    publishScriptTestSamplesChanged();
}

void AppFacade::publishSessionCollectionViewsChanged()
{
    m_modelCoordinator->syncSessionCollectionModels();
    publishSessionsChanged();
    publishCurrentSessionIndexChanged();
    publishCurrentSessionChanged();
    publishSubscriptionsChanged();
    publishMessageStreamChanged();
    publishLogsChanged();
    publishScriptsChanged();
}
