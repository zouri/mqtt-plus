#include "app/appcontrollerwiring.h"

#include "app/appmodelsync.h"
#include "app/uieventhub.h"
#include "controllers/eventcontroller.h"
#include "controllers/languagecontroller.h"
#include "controllers/mqttcontroller.h"
#include "controllers/preferencescontroller.h"
#include "controllers/scriptcontroller.h"
#include "controllers/sessioncontroller.h"
#include "controllers/subscriptioncontroller.h"
#include "controllers/themecontroller.h"
#include "models/eventstreammodel.h"
#include "services/storage/historystore.h"

#include <QDateTime>
#include <QObject>
#include <QtGlobal>

#include <utility>

AppControllerWiring::AppControllerWiring(Dependencies dependencies)
    : m_dependencies(std::move(dependencies))
{
    Q_ASSERT(m_dependencies.eventController);
    Q_ASSERT(m_dependencies.sessionController);
    Q_ASSERT(m_dependencies.subscriptionController);
    Q_ASSERT(m_dependencies.mqttController);
    Q_ASSERT(m_dependencies.scriptController);
    Q_ASSERT(m_dependencies.themeController);
    Q_ASSERT(m_dependencies.languageController);
    Q_ASSERT(m_dependencies.preferencesController);
    Q_ASSERT(m_dependencies.modelSync);
    Q_ASSERT(m_dependencies.uiEvents);
    Q_ASSERT(m_dependencies.historyStore);
    Q_ASSERT(m_dependencies.messagesModel);
    Q_ASSERT(m_dependencies.logsModel);
    Q_ASSERT(m_dependencies.subscriptionsModel);
    Q_ASSERT(m_dependencies.subscriptionFpsRefreshActive);
    Q_ASSERT(m_dependencies.startSubscriptionFpsRefresh);
    Q_ASSERT(m_dependencies.setSubscriptionFpsRefreshActive);

    bindEventController();
    bindSessionController();
    bindSubscriptionController();
    bindMqttController();
    bindControllerSignals();
}

void AppControllerWiring::bindEventController()
{
    const auto &deps = m_dependencies;

    deps.eventController->setDependencies(EventController::Dependencies {
        .currentSession = deps.currentSession,
        .sessionById = deps.sessionById,
        .historyPageSize = [preferencesController = deps.preferencesController]() {
            return preferencesController->historyPageSize();
        },
        .messageRetentionLimit = [preferencesController = deps.preferencesController]() {
            return preferencesController->messageRetentionLimit();
        },
        .logRetentionLimit = [preferencesController = deps.preferencesController]() {
            return preferencesController->logRetentionLimit();
        },
        .saveMessagesWhenOutputPaused = [preferencesController = deps.preferencesController]() {
            return preferencesController->saveMessagesWhenOutputPaused();
        },
        .scriptById = [scriptController = deps.scriptController](const QString &scriptId) {
            return scriptController->scriptById(scriptId);
        },
        .scriptName = [scriptController = deps.scriptController](const QString &scriptId) {
            return scriptController->scriptName(scriptId);
        },
        .bestSubscriptionForTopic =
            [subscriptionController = deps.subscriptionController](const SessionState &session, const QString &topic) {
                return subscriptionController->bestSubscriptionForTopic(session, topic);
            },
        .syncScriptTestSamplesModel = [modelSync = deps.modelSync]() {
            modelSync->refreshScriptTestSamplesModel();
        },
        .syncSubscriptionsModel = [modelSync = deps.modelSync]() {
            modelSync->refreshSubscriptionsModel();
        },
        .subscriptionFpsRefreshActive = deps.subscriptionFpsRefreshActive,
        .startSubscriptionFpsRefresh = deps.startSubscriptionFpsRefresh,
        .publishMessageStreamChanged = [uiEvents = deps.uiEvents]() {
            uiEvents->publishMessageStreamChanged();
        },
        .publishLogsChanged = [uiEvents = deps.uiEvents]() {
            uiEvents->publishLogsChanged();
        },
        .publishMessageStreamRowAppended = [uiEvents = deps.uiEvents](const QVariantMap &row) {
            uiEvents->publishMessageStreamRowAppended(row);
        },
        .publishLogsRowAppended = [uiEvents = deps.uiEvents](const QVariantMap &row) {
            uiEvents->publishLogsRowAppended(row);
        },
        .publishScriptTestSamplesChanged = [uiEvents = deps.uiEvents]() {
            uiEvents->publishScriptTestSamplesChanged();
        },
        .publishSubscriptionsChanged = [uiEvents = deps.uiEvents]() {
            uiEvents->publishSubscriptionsChanged();
        },
        .historyStore = deps.historyStore,
        .messagesModel = deps.messagesModel,
        .logsModel = deps.logsModel,
        .launchTimestamp = deps.launchTimestamp,
    });
}

void AppControllerWiring::bindSessionController()
{
    const auto &deps = m_dependencies;

    deps.sessionController->setDependencies(SessionController::Dependencies {
        .reloadCurrentSessionHistory = deps.reloadCurrentSessionHistory,
        .currentSessionHasActiveSubscriptionFps = [subscriptionController = deps.subscriptionController]() {
            return subscriptionController->currentSessionHasActiveSubscriptionFps(QDateTime::currentMSecsSinceEpoch());
        },
        .setSubscriptionFpsRefreshActive = deps.setSubscriptionFpsRefreshActive,
        .configureSession = deps.configureSession,
        .updatePublishStatus = deps.updatePublishStatus,
        .saveSessions = deps.saveSessions,
        .connectSession = deps.connectSession,
        .createDefaultSession = deps.createDefaultSession,
        .deleteHistoryWithSession = [preferencesController = deps.preferencesController]() {
            return preferencesController->deleteHistoryWithSession();
        },
        .clearSessionHistory = [historyStore = deps.historyStore](const QString &sessionId) {
            historyStore->clearSessionHistory(sessionId);
        },
        .destroySessionRuntime = deps.destroySessionRuntime,
        .publishSelectedSessionViewsChanged = deps.publishSelectedSessionViewsChanged,
        .publishCurrentSessionViewsChanged = deps.publishCurrentSessionViewsChanged,
        .publishCurrentSessionAndSubscriptionsChanged = deps.publishCurrentSessionAndSubscriptionsChanged,
        .publishSessionCollectionViewsChanged = deps.publishSessionCollectionViewsChanged,
        .publishSessionsChanged = [uiEvents = deps.uiEvents]() {
            uiEvents->publishSessionsChanged();
        },
        .publishMessageStreamChanged = [uiEvents = deps.uiEvents]() {
            uiEvents->publishMessageStreamChanged();
        },
    });
}

void AppControllerWiring::bindSubscriptionController()
{
    const auto &deps = m_dependencies;

    deps.subscriptionController->setDependencies(SubscriptionController::Dependencies {
        .currentSession = deps.currentSession,
        .sessionById = deps.sessionById,
        .appendEvent = deps.appendEvent,
        .scriptExists = [scriptController = deps.scriptController](const QString &scriptId) {
            return scriptController->scriptById(scriptId) != nullptr;
        },
        .saveSessions = deps.saveSessions,
        .publishSessionAndSubscriptionViewsChanged = deps.publishSessionAndSubscriptionViewsChanged,
        .publishCurrentSessionAndSubscriptionsChanged = deps.publishCurrentSessionAndSubscriptionsChanged,
        .syncSubscriptionsModel = [modelSync = deps.modelSync]() {
            modelSync->refreshSubscriptionsModel();
        },
        .publishSubscriptionsChanged = [uiEvents = deps.uiEvents]() {
            uiEvents->publishSubscriptionsChanged();
        },
        .setSubscriptionFpsRefreshActive = deps.setSubscriptionFpsRefreshActive,
        .setTopicFpsRows = [subscriptionsModel = deps.subscriptionsModel](const QVector<SubscriptionFpsRow> &rows) {
            subscriptionsModel->setTopicFpsRows(rows);
        },
    });
}

void AppControllerWiring::bindMqttController()
{
    const auto &deps = m_dependencies;

    deps.mqttController->setDependencies(MqttController::Dependencies {
        .currentSession = deps.currentSession,
        .sessionById = deps.sessionById,
        .appendEvent = deps.appendEvent,
        .restoreActiveSubscriptions =
            [subscriptionController = deps.subscriptionController](SessionState &session, bool emitEvents) {
                subscriptionController->restoreActiveSubscriptions(session, emitEvents);
            },
        .resetRuntimeSubscriptions = [subscriptionController = deps.subscriptionController](SessionState &session) {
            subscriptionController->resetRuntimeSubscriptions(session);
        },
        .appendIncomingMessage =
            [eventController = deps.eventController](
                const QString &sessionId,
                const QString &topic,
                const QByteArray &payloadBytes) {
                eventController->appendIncomingMessage(sessionId, topic, payloadBytes);
            },
        .publishSessionViewsChanged = deps.publishSessionViewsChanged,
        .publishCurrentSessionViewsChanged = deps.publishCurrentSessionViewsChanged,
        .publishSessionAndSubscriptionViewsChanged = deps.publishSessionAndSubscriptionViewsChanged,
    });
}

void AppControllerWiring::bindControllerSignals()
{
    const auto &deps = m_dependencies;

    QObject::connect(
        deps.scriptController,
        &ScriptController::storageError,
        deps.uiEvents,
        [reportStorageError = deps.reportStorageError](const QString &message) {
            reportStorageError(message);
        });
    QObject::connect(deps.themeController, &ThemeController::modeChanged, deps.uiEvents, [uiEvents = deps.uiEvents]() {
        uiEvents->publishThemeModeChanged();
    });
    QObject::connect(
        deps.themeController,
        &ThemeController::effectiveThemeChanged,
        deps.uiEvents,
        [uiEvents = deps.uiEvents]() {
            uiEvents->publishEffectiveThemeChanged();
        });
    QObject::connect(
        deps.languageController,
        &LanguageController::modeChanged,
        deps.uiEvents,
        [uiEvents = deps.uiEvents]() {
            uiEvents->publishLanguageModeChanged();
        });
    QObject::connect(
        deps.languageController,
        &LanguageController::languageChanged,
        deps.uiEvents,
        [modelSync = deps.modelSync, uiEvents = deps.uiEvents]() {
            modelSync->refreshSessionsModel();
            modelSync->refreshSubscriptionsModel();
            uiEvents->publishCurrentSessionChanged();
            uiEvents->publishSessionsChanged();
            uiEvents->publishSubscriptionsChanged();
            uiEvents->publishLanguageChanged();
        });
    QObject::connect(
        deps.preferencesController,
        &PreferencesController::messageRetentionLimitChanged,
        deps.uiEvents,
        [uiEvents = deps.uiEvents]() {
            uiEvents->publishMessageRetentionLimitChanged();
        });
    QObject::connect(
        deps.preferencesController,
        &PreferencesController::logRetentionLimitChanged,
        deps.uiEvents,
        [uiEvents = deps.uiEvents]() {
            uiEvents->publishLogRetentionLimitChanged();
        });
    QObject::connect(
        deps.preferencesController,
        &PreferencesController::historyPageSizeChanged,
        deps.uiEvents,
        [reloadCurrentSessionHistory = deps.reloadCurrentSessionHistory, uiEvents = deps.uiEvents]() {
            reloadCurrentSessionHistory();
            uiEvents->publishMessageStreamChanged();
            uiEvents->publishLogsChanged();
            uiEvents->publishHistoryPageSizeChanged();
        });
    QObject::connect(
        deps.preferencesController,
        &PreferencesController::deleteHistoryWithSessionChanged,
        deps.uiEvents,
        [uiEvents = deps.uiEvents]() {
            uiEvents->publishDeleteHistoryWithSessionChanged();
        });
    QObject::connect(
        deps.preferencesController,
        &PreferencesController::saveMessagesWhenOutputPausedChanged,
        deps.uiEvents,
        [uiEvents = deps.uiEvents]() {
            uiEvents->publishSaveMessagesWhenOutputPausedChanged();
        });
    QObject::connect(
        deps.preferencesController,
        &PreferencesController::clearMessagesOnExitChanged,
        deps.uiEvents,
        [uiEvents = deps.uiEvents]() {
            uiEvents->publishClearMessagesOnExitChanged();
        });
    QObject::connect(
        deps.preferencesController,
        &PreferencesController::clearLogsOnExitChanged,
        deps.uiEvents,
        [uiEvents = deps.uiEvents]() {
            uiEvents->publishClearLogsOnExitChanged();
        });
    QObject::connect(
        deps.preferencesController,
        &PreferencesController::windowWidthChanged,
        deps.uiEvents,
        [uiEvents = deps.uiEvents]() {
            uiEvents->publishWindowWidthChanged();
        });
    QObject::connect(
        deps.preferencesController,
        &PreferencesController::windowHeightChanged,
        deps.uiEvents,
        [uiEvents = deps.uiEvents]() {
            uiEvents->publishWindowHeightChanged();
        });
    QObject::connect(
        deps.preferencesController,
        &PreferencesController::windowMaximizedChanged,
        deps.uiEvents,
        [uiEvents = deps.uiEvents]() {
            uiEvents->publishWindowMaximizedChanged();
        });
}
