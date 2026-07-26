#include "app/application.h"

#include "services/apputils.h"

#include <QDateTime>
#include <QDebug>

using namespace AppUtils;

Application::Application()
    : m_owner(nullptr)
    , m_settings(QStringLiteral("mqtt-plus"), QStringLiteral("mqtt-plus"))
    , m_preferences(&m_settings, &m_owner)
    , m_scriptService(&m_owner)
    , m_sessionService(
          m_settings,
          m_scriptService,
          m_historyStore,
          m_preferences,
          &m_owner)
    , m_sessionsModel(&m_owner)
    , m_subscriptionsModel(&m_owner)
    , m_filteredSubscriptionsModel(&m_owner)
    , m_messageFilterSubscriptionsModel(&m_owner)
    , m_messagesModel(&m_owner)
    , m_filteredMessagesModel(&m_owner)
    , m_logsModel(&m_owner)
    , m_scriptsModel(&m_owner)
    , m_subscriptionFpsTimer(&m_owner)
    , m_eventHistoryService(
          m_sessionService,
          m_historyStore,
          m_messagesModel,
          m_logsModel,
          m_scriptService,
          timestampNow(),
          m_preferences,
          &m_owner)
    , m_subscriptionService(
          m_sessionService,
          m_scriptService,
          m_eventHistoryService,
          &m_owner)
    , m_mqttService(
          m_sessionService,
          m_subscriptionService,
          m_eventHistoryService,
          &m_owner)
    , m_sessionActivityTimer(&m_owner)
    , m_viewModel(
          m_sessionService,
          m_mqttService,
          m_subscriptionService,
          m_eventHistoryService,
          m_scriptService,
          m_preferences,
          m_historyStore,
          m_sessionsModel,
          m_filteredSubscriptionsModel,
          m_messageFilterSubscriptionsModel,
          m_messagesModel,
          m_filteredMessagesModel,
          m_logsModel,
          m_scriptsModel,
          m_settings,
          &m_owner)
{
    m_filteredSubscriptionsModel.setSourceModel(&m_subscriptionsModel);
    m_messageFilterSubscriptionsModel.setSourceModel(&m_subscriptionsModel);
    m_filteredMessagesModel.setSourceModel(&m_messagesModel);

    QObject::connect(
        &m_mqttService,
        &MqttSessionService::sessionStateChanged,
        &m_sessionsModel,
        [this]() { refreshSessionModels(); });
    QObject::connect(
        &m_subscriptionService,
        &SubscriptionService::subscriptionsChanged,
        &m_subscriptionsModel,
        [this]() { refreshSubscriptionsModel(); });
    QObject::connect(
        &m_eventHistoryService,
        &EventHistoryService::subscriptionActivityChanged,
        &m_subscriptionsModel,
        [this]() {
            if (m_subscriptionFpsTimer.isActive()) {
                return;
            }
            refreshSubscriptionsModel();
            m_subscriptionFpsTimer.start();
        });

    QObject::connect(
        &m_sessionService,
        &SessionService::sessionRuntimeReady,
        &m_mqttService,
        &MqttSessionService::bindSessionSignals);
    QObject::connect(
        &m_sessionService,
        &SessionService::currentSessionHistoryReloadRequested,
        &m_eventHistoryService,
        &EventHistoryService::reloadCurrentSessionHistory);
    QObject::connect(
        &m_sessionService,
        &SessionService::reconnectRequested,
        &m_mqttService,
        [this](SessionState *session) {
            if (session) {
                m_mqttService.connectSession(*session, QStringLiteral("Connecting to"));
            }
        });
    QObject::connect(
        &m_sessionService,
        &SessionService::runtimeError,
        &m_eventHistoryService,
        [this](
            const QString &sessionId,
            const QString &channel,
            const QString &message) {
            if (auto *session = m_sessionService.sessionById(sessionId)) {
                m_eventHistoryService.appendEvent(*session, channel, message);
            }
        });
    QObject::connect(
        &m_sessionService,
        &SessionService::storageError,
        &m_sessionService,
        [this](const QString &message) { reportStorageError(message); });
    QObject::connect(
        &m_sessionService,
        &SessionService::sessionsChanged,
        &m_sessionsModel,
        [this]() { m_sessionsModel.setSessions(m_sessionService.sessions()); });
    QObject::connect(
        &m_sessionService,
        &SessionService::currentSessionChanged,
        &m_subscriptionsModel,
        [this]() {
            refreshSessionModels();
            const auto *session = m_sessionService.currentSession();
            m_messagesModel.setRows(session ? session->runtime.messageRows : QVariantList {});
            m_logsModel.setRows(session ? session->runtime.logRows : QVariantList {});

            if (m_subscriptionService.currentSessionHasActiveSubscriptionFps(
                    QDateTime::currentMSecsSinceEpoch())) {
                m_subscriptionFpsTimer.start();
            } else {
                m_subscriptionFpsTimer.stop();
            }
        });

    QObject::connect(
        &m_scriptService,
        &ScriptService::storageError,
        &m_sessionService,
        [this](const QString &message) { reportStorageError(message); });
    QObject::connect(
        &m_scriptService,
        &ScriptService::scriptsChanged,
        &m_subscriptionsModel,
        [this]() { refreshSubscriptionsModel(); });

    m_subscriptionFpsTimer.setInterval(kSubscriptionFpsRefreshIntervalMs);
    QObject::connect(
        &m_subscriptionFpsTimer,
        &QTimer::timeout,
        &m_subscriptionsModel,
        [this]() {
            const auto *session = m_sessionService.currentSession();
            if (!session
                || !m_subscriptionsModel.updateTopicFps(
                    session->subscriptions,
                    QDateTime::currentMSecsSinceEpoch())) {
                m_subscriptionFpsTimer.stop();
            }
        });

    m_sessionActivityTimer.setInterval(100);
    m_sessionActivityTimer.setSingleShot(true);
    QObject::connect(
        &m_eventHistoryService,
        &EventHistoryService::totalMessageCountChanged,
        &m_sessionActivityTimer,
        [this]() {
            if (!m_sessionActivityTimer.isActive()) {
                m_sessionActivityTimer.start();
            }
        });
    QObject::connect(
        &m_sessionActivityTimer,
        &QTimer::timeout,
        &m_sessionsModel,
        [this]() { m_sessionsModel.setSessions(m_sessionService.sessions()); });

    m_scriptService.loadScripts();
    m_sessionService.loadSessions();
    applyMessageRetentionLimit();
    m_sessionService.setCurrentSessionIndex(0);
}

Application::~Application()
{
    applyExitCleanup();
}

ApplicationViewModel *Application::viewModel()
{
    return &m_viewModel;
}

void Application::reportStorageError(const QString &message)
{
    if (message.isEmpty()) {
        return;
    }

    SessionState *session = m_sessionService.currentSession();
    if (!session && !m_sessionService.sessions().isEmpty()) {
        session = &m_sessionService.sessions().front();
    }
    if (!session) {
        return;
    }

    session->runtime.lastError = message;
    m_eventHistoryService.appendEvent(*session, QStringLiteral("Storage"), message);
    m_sessionsModel.setSessions(m_sessionService.sessions());
    if (session == m_sessionService.currentSession()) {
        m_sessionService.currentSessionChanged();
    }
}

void Application::refreshSubscriptionsModel()
{
    const QVector<SubscriptionEntry> emptySubscriptions;
    const auto *session = m_sessionService.currentSession();
    m_subscriptionsModel.setSubscriptions(
        session ? session->id : QString(),
        session ? session->subscriptions : emptySubscriptions,
        m_scriptService.scripts());
}

void Application::refreshSessionModels()
{
    m_sessionsModel.setSessions(m_sessionService.sessions());
    refreshSubscriptionsModel();
}

void Application::applyMessageRetentionLimit()
{
    const int limit = m_preferences.messageRetentionLimit();
    if (limit <= 0) {
        return;
    }

    for (const SessionState &session : m_sessionService.sessions()) {
        m_historyStore.pruneMessages(session.id, limit);
    }
}

void Application::applyExitCleanup()
{
    m_eventHistoryService.flushPendingMessageHistory();
    applyMessageRetentionLimit();

    const QString clearMessagesMode = m_preferences.clearMessagesOnExit();
    if (clearMessagesMode == QStringLiteral("all")) {
        if (!m_historyStore.clearAllMessages()) {
            qWarning().noquote()
                << "Cannot clear message history on exit:" << m_historyStore.lastError();
        }
    } else if (clearMessagesMode == QStringLiteral("current")) {
        if (const auto *session = m_sessionService.currentSession()) {
            if (!m_historyStore.clearMessages(session->id)) {
                qWarning().noquote()
                    << "Cannot clear current message history on exit:"
                    << m_historyStore.lastError();
            }
        }
    }

    const QString clearLogsMode = m_preferences.clearLogsOnExit();
    if (clearLogsMode == QStringLiteral("all")) {
        if (!m_historyStore.clearAllLogs()) {
            qWarning().noquote()
                << "Cannot clear log history on exit:" << m_historyStore.lastError();
        }
    } else if (clearLogsMode == QStringLiteral("current")) {
        if (const auto *session = m_sessionService.currentSession()) {
            if (!m_historyStore.clearLogs(session->id)) {
                qWarning().noquote()
                    << "Cannot clear current log history on exit:"
                    << m_historyStore.lastError();
            }
        }
    }
}
