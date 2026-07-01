#include "app/applicationcontrollercontexts.h"

#include "app/applicationsessionconfigurator.h"
#include "app/applicationsessionrepository.h"
#include "app/applicationsessionruntime.h"
#include "app/applicationviewrefreshcoordinator.h"
#include "controllers/eventcontroller.h"
#include "controllers/mqttcontroller.h"
#include "controllers/preferencescontroller.h"
#include "controllers/scriptcontroller.h"
#include "controllers/sessioncontroller.h"
#include "controllers/subscriptioncontroller.h"

namespace {

SessionState *currentSession(SessionController *sessionController)
{
    return sessionController->currentSession();
}

const SessionState *currentSession(const SessionController *sessionController)
{
    return sessionController->currentSession();
}

bool saveSessions(
    ApplicationSessionRepository &sessionRepository,
    ApplicationViewRefreshCoordinator &viewRefreshCoordinator)
{
    QString errorMessage;
    if (sessionRepository.saveSessions(errorMessage)) {
        return true;
    }
    viewRefreshCoordinator.reportStorageError(
        errorMessage.isEmpty() ? QStringLiteral("Cannot save sessions.") : errorMessage);
    return false;
}

} // namespace

ApplicationSessionControllerContextAdapter::ApplicationSessionControllerContextAdapter(
    ApplicationSessionControllerContextDependencies *dependencies)
    : m_dependencies(dependencies)
{
}

HistoryStore &ApplicationSessionControllerContextAdapter::historyStore()
{
    return *m_dependencies->historyStore;
}

SubscriptionController &ApplicationSessionControllerContextAdapter::subscriptionController()
{
    return *m_dependencies->subscriptionController;
}

QTimer &ApplicationSessionControllerContextAdapter::subscriptionFpsRefreshTimer()
{
    return *m_dependencies->subscriptionFpsRefreshTimer;
}

bool ApplicationSessionControllerContextAdapter::deleteHistoryWithSession() const
{
    return m_dependencies->preferencesController->deleteHistoryWithSession();
}

bool ApplicationSessionControllerContextAdapter::saveSessions()
{
    return ::saveSessions(*m_dependencies->sessionRepository, *m_dependencies->viewRefreshCoordinator);
}

void ApplicationSessionControllerContextAdapter::configureSession(
    SessionState &session,
    const QVariantMap &config,
    bool keepNameFallback)
{
    ApplicationSessionConfigurator::applyConfig(session, config, keepNameFallback);
}

void ApplicationSessionControllerContextAdapter::initializeSessionRuntime(SessionState *session)
{
    m_dependencies->sessionRuntime->initialize(session);
}

void ApplicationSessionControllerContextAdapter::destroySessionRuntime(SessionState &session)
{
    m_dependencies->sessionRuntime->destroy(session);
}

void ApplicationSessionControllerContextAdapter::connectSession(SessionState &session, const QString &eventPrefix)
{
    m_dependencies->mqttController->connectSession(session, eventPrefix);
}

SessionState ApplicationSessionControllerContextAdapter::createDefaultSession(const QString &name)
{
    return m_dependencies->sessionRuntime->createDefaultSession(name);
}

void ApplicationSessionControllerContextAdapter::updatePublishStatus(
    SessionState &session,
    const QString &state,
    const QString &reason,
    qint32 messageId)
{
    m_dependencies->mqttController->updatePublishStatus(session, state, reason, messageId);
}

void ApplicationSessionControllerContextAdapter::reloadCurrentSessionHistory()
{
    m_dependencies->eventController->reloadCurrentSessionHistory();
}

void ApplicationSessionControllerContextAdapter::notifyCurrentSessionViewsChanged()
{
    m_dependencies->viewRefreshCoordinator->notifyCurrentSessionViewsChanged();
}

void ApplicationSessionControllerContextAdapter::notifyCurrentSessionAndSubscriptionsChanged()
{
    m_dependencies->viewRefreshCoordinator->notifyCurrentSessionAndSubscriptionsChanged();
}

void ApplicationSessionControllerContextAdapter::notifySelectedSessionViewsChanged()
{
    m_dependencies->viewRefreshCoordinator->notifySelectedSessionViewsChanged();
}

void ApplicationSessionControllerContextAdapter::notifySessionCollectionViewsChanged()
{
    m_dependencies->viewRefreshCoordinator->notifySessionCollectionViewsChanged();
}

void ApplicationSessionControllerContextAdapter::refreshSessionsModel()
{
    m_dependencies->viewRefreshCoordinator->refreshSessionsModel();
}

void ApplicationSessionControllerContextAdapter::emitSessionsChanged()
{
    m_dependencies->viewRefreshCoordinator->emitSessionsChanged();
}

void ApplicationSessionControllerContextAdapter::emitMessageStreamChanged()
{
    m_dependencies->viewRefreshCoordinator->emitMessageStreamChanged();
}

ApplicationMqttControllerContextAdapter::ApplicationMqttControllerContextAdapter(
    ApplicationMqttControllerContextDependencies *dependencies)
    : m_dependencies(dependencies)
{
}

SessionState *ApplicationMqttControllerContextAdapter::currentSessionState()
{
    return currentSession(m_dependencies->sessionController);
}

SessionState *ApplicationMqttControllerContextAdapter::sessionById(const QString &sessionId)
{
    return m_dependencies->sessionController->sessionById(sessionId);
}

const SessionState *ApplicationMqttControllerContextAdapter::sessionById(const QString &sessionId) const
{
    return m_dependencies->sessionController->sessionById(sessionId);
}

SubscriptionController &ApplicationMqttControllerContextAdapter::subscriptionController()
{
    return *m_dependencies->subscriptionController;
}

EventController &ApplicationMqttControllerContextAdapter::eventController()
{
    return *m_dependencies->eventController;
}

void ApplicationMqttControllerContextAdapter::updatePublishStatus(
    SessionState &session,
    const QString &state,
    const QString &reason,
    qint32 messageId)
{
    m_dependencies->mqttController->updatePublishStatus(session, state, reason, messageId);
}

void ApplicationMqttControllerContextAdapter::appendEvent(
    SessionState &session,
    const QString &channel,
    const QString &message)
{
    m_dependencies->eventController->appendEvent(session, channel, message);
}

QSslConfiguration ApplicationMqttControllerContextAdapter::sslConfigurationForSession(
    const SessionState &session,
    QString &errorMessage) const
{
    return m_dependencies->mqttController->sslConfigurationForSession(session, errorMessage);
}

void ApplicationMqttControllerContextAdapter::notifyCurrentSessionViewsChanged()
{
    m_dependencies->viewRefreshCoordinator->notifyCurrentSessionViewsChanged();
}

void ApplicationMqttControllerContextAdapter::notifySessionViewsChanged()
{
    m_dependencies->viewRefreshCoordinator->notifySessionViewsChanged();
}

void ApplicationMqttControllerContextAdapter::notifySessionAndSubscriptionViewsChanged()
{
    m_dependencies->viewRefreshCoordinator->notifySessionAndSubscriptionViewsChanged();
}

ApplicationEventControllerContextAdapter::ApplicationEventControllerContextAdapter(
    ApplicationEventControllerContextDependencies *dependencies)
    : m_dependencies(dependencies)
{
}

SessionState *ApplicationEventControllerContextAdapter::currentSessionState()
{
    return currentSession(m_dependencies->sessionController);
}

SessionState *ApplicationEventControllerContextAdapter::sessionById(const QString &sessionId)
{
    return m_dependencies->sessionController->sessionById(sessionId);
}

HistoryStore &ApplicationEventControllerContextAdapter::historyStore()
{
    return *m_dependencies->historyStore;
}

EventStreamModel &ApplicationEventControllerContextAdapter::messagesModel()
{
    return *m_dependencies->messagesModel;
}

EventStreamModel &ApplicationEventControllerContextAdapter::logsModel()
{
    return *m_dependencies->logsModel;
}

ScriptTestSamplesModel &ApplicationEventControllerContextAdapter::scriptTestSamplesModel()
{
    return *m_dependencies->scriptTestSamplesModel;
}

ScriptController &ApplicationEventControllerContextAdapter::scriptController()
{
    return *m_dependencies->scriptController;
}

SubscriptionController &ApplicationEventControllerContextAdapter::subscriptionController()
{
    return *m_dependencies->subscriptionController;
}

QTimer &ApplicationEventControllerContextAdapter::subscriptionFpsRefreshTimer()
{
    return *m_dependencies->subscriptionFpsRefreshTimer;
}

QString ApplicationEventControllerContextAdapter::launchTimestamp() const
{
    return *m_dependencies->launchTimestamp;
}

int ApplicationEventControllerContextAdapter::historyPageSize() const
{
    return m_dependencies->preferencesController->historyPageSize();
}

int ApplicationEventControllerContextAdapter::messageRetentionLimit() const
{
    return m_dependencies->preferencesController->messageRetentionLimit();
}

int ApplicationEventControllerContextAdapter::logRetentionLimit() const
{
    return m_dependencies->preferencesController->logRetentionLimit();
}

int ApplicationEventControllerContextAdapter::maxIncomingPayloadBytes() const
{
    return m_dependencies->preferencesController->maxIncomingPayloadBytes();
}

bool ApplicationEventControllerContextAdapter::saveMessagesWhenOutputPaused() const
{
    return m_dependencies->preferencesController->saveMessagesWhenOutputPaused();
}

void ApplicationEventControllerContextAdapter::appendEvent(
    SessionState &session,
    const QString &channel,
    const QString &message)
{
    m_dependencies->eventController->appendEvent(session, channel, message);
}

void ApplicationEventControllerContextAdapter::appendIncomingMessage(
    const QString &sessionId,
    const QString &topic,
    const QByteArray &payloadBytes)
{
    m_dependencies->eventController->appendIncomingMessage(sessionId, topic, payloadBytes);
}

LuaScriptResult ApplicationEventControllerContextAdapter::parseIncomingPayload(
    const SessionState &session,
    const SubscriptionEntry *subscription,
    const QString &topic,
    const QByteArray &payloadBytes,
    const QString &timestamp,
    QString &scriptNameOut,
    QString &decodedPayloadOut) const
{
    return m_dependencies->eventController->parseIncomingPayload(
        session,
        subscription,
        topic,
        payloadBytes,
        timestamp,
        scriptNameOut,
        decodedPayloadOut);
}

QString ApplicationEventControllerContextAdapter::scriptName(const QString &id) const
{
    return m_dependencies->scriptController->scriptName(id);
}

void ApplicationEventControllerContextAdapter::refreshSubscriptionsModel()
{
    m_dependencies->viewRefreshCoordinator->refreshSubscriptionsModel();
}

void ApplicationEventControllerContextAdapter::refreshScriptTestSamplesModel()
{
    m_dependencies->viewRefreshCoordinator->refreshScriptTestSamplesModel();
}

void ApplicationEventControllerContextAdapter::emitSubscriptionsChanged()
{
    m_dependencies->viewRefreshCoordinator->emitSubscriptionsChanged();
}

void ApplicationEventControllerContextAdapter::emitMessageStreamChanged()
{
    m_dependencies->viewRefreshCoordinator->emitMessageStreamChanged();
}

void ApplicationEventControllerContextAdapter::emitLogStreamChanged()
{
    m_dependencies->viewRefreshCoordinator->emitLogStreamChanged();
}

void ApplicationEventControllerContextAdapter::emitMessageStreamRowAppended(const QVariantMap &row)
{
    m_dependencies->viewRefreshCoordinator->emitMessageStreamRowAppended(row);
}

void ApplicationEventControllerContextAdapter::emitLogStreamRowAppended(const QVariantMap &row)
{
    m_dependencies->viewRefreshCoordinator->emitLogStreamRowAppended(row);
}

ApplicationSubscriptionControllerContextAdapter::ApplicationSubscriptionControllerContextAdapter(
    ApplicationSubscriptionControllerContextDependencies *dependencies)
    : m_dependencies(dependencies)
{
}

SessionState *ApplicationSubscriptionControllerContextAdapter::currentSessionState()
{
    return currentSession(m_dependencies->sessionController);
}

const SessionState *ApplicationSubscriptionControllerContextAdapter::currentSessionState() const
{
    return currentSession(m_dependencies->sessionController);
}

SessionState *ApplicationSubscriptionControllerContextAdapter::sessionById(const QString &sessionId)
{
    return m_dependencies->sessionController->sessionById(sessionId);
}

SubscriptionListModel &ApplicationSubscriptionControllerContextAdapter::subscriptionsModel()
{
    return *m_dependencies->subscriptionsModel;
}

ScriptController &ApplicationSubscriptionControllerContextAdapter::scriptController()
{
    return *m_dependencies->scriptController;
}

QTimer &ApplicationSubscriptionControllerContextAdapter::subscriptionFpsRefreshTimer()
{
    return *m_dependencies->subscriptionFpsRefreshTimer;
}

bool ApplicationSubscriptionControllerContextAdapter::saveSessions()
{
    return ::saveSessions(*m_dependencies->sessionRepository, *m_dependencies->viewRefreshCoordinator);
}

void ApplicationSubscriptionControllerContextAdapter::appendEvent(
    SessionState &session,
    const QString &channel,
    const QString &message)
{
    m_dependencies->eventController->appendEvent(session, channel, message);
}

qreal ApplicationSubscriptionControllerContextAdapter::subscriptionFps(
    const SubscriptionEntry &entry,
    qint64 nowMs) const
{
    return m_dependencies->subscriptionController->subscriptionFps(entry, nowMs);
}

bool ApplicationSubscriptionControllerContextAdapter::currentSessionHasActiveSubscriptionFps(qint64 nowMs) const
{
    return m_dependencies->subscriptionController->currentSessionHasActiveSubscriptionFps(nowMs);
}

void ApplicationSubscriptionControllerContextAdapter::refreshSubscriptionsModel()
{
    m_dependencies->viewRefreshCoordinator->refreshSubscriptionsModel();
}

void ApplicationSubscriptionControllerContextAdapter::notifyCurrentSessionAndSubscriptionsChanged()
{
    m_dependencies->viewRefreshCoordinator->notifyCurrentSessionAndSubscriptionsChanged();
}

void ApplicationSubscriptionControllerContextAdapter::notifySessionAndSubscriptionViewsChanged()
{
    m_dependencies->viewRefreshCoordinator->notifySessionAndSubscriptionViewsChanged();
}

void ApplicationSubscriptionControllerContextAdapter::emitSubscriptionsChanged()
{
    m_dependencies->viewRefreshCoordinator->emitSubscriptionsChanged();
}

ApplicationControllerContexts::ApplicationControllerContexts()
    : m_session(&m_dependencies.session)
    , m_mqtt(&m_dependencies.mqtt)
    , m_event(&m_dependencies.event)
    , m_subscription(&m_dependencies.subscription)
{
}

void ApplicationControllerContexts::setDependencies(const ApplicationControllerContextsDependencies &dependencies)
{
    m_dependencies = dependencies;
}

SessionControllerContext &ApplicationControllerContexts::session()
{
    return m_session;
}

MqttControllerContext &ApplicationControllerContexts::mqtt()
{
    return m_mqtt;
}

EventControllerContext &ApplicationControllerContexts::event()
{
    return m_event;
}

SubscriptionControllerContext &ApplicationControllerContexts::subscription()
{
    return m_subscription;
}

SessionState *ApplicationControllerContexts::sessionById(const QString &sessionId)
{
    return m_dependencies.mqtt.sessionController->sessionById(sessionId);
}

void ApplicationControllerContexts::appendEvent(
    SessionState &session,
    const QString &channel,
    const QString &message)
{
    m_dependencies.event.eventController->appendEvent(session, channel, message);
}

void ApplicationControllerContexts::reportStorageError(const QString &message)
{
    m_dependencies.session.viewRefreshCoordinator->reportStorageError(message);
}

void ApplicationControllerContexts::reloadCurrentSessionHistory()
{
    m_dependencies.event.eventController->reloadCurrentSessionHistory();
}

void ApplicationControllerContexts::notifyCurrentSessionAndSubscriptionsChanged()
{
    m_dependencies.session.viewRefreshCoordinator->notifyCurrentSessionAndSubscriptionsChanged();
}

void ApplicationControllerContexts::notifySessionViewsChanged()
{
    m_dependencies.session.viewRefreshCoordinator->notifySessionViewsChanged();
}

void ApplicationControllerContexts::notifySessionCollectionViewsChanged()
{
    m_dependencies.session.viewRefreshCoordinator->notifySessionCollectionViewsChanged();
}
