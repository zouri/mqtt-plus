#include "sessionservice.h"

#include "controllers/mqttsessionservice.h"
#include "controllers/subscriptionservice.h"
#include "domain/sessionconfig.h"
#include "services/storage/sessionsettingsstore.h"
#include "services/storage/historystore.h"

#include <QDateTime>

SessionService::SessionService(QObject *parent)
    : QObject(parent)
{
}

void SessionService::setDependencies(const Dependencies &dependencies)
{
    m_dependencies = dependencies;
}

QVector<SessionState> &SessionService::sessions()
{
    return m_sessions;
}

const QVector<SessionState> &SessionService::sessions() const
{
    return m_sessions;
}

int SessionService::currentIndex() const
{
    return m_currentIndex;
}

void SessionService::setCurrentIndex(int index)
{
    m_currentIndex = index;
}

SessionState *SessionService::currentSession()
{
    if (!isValidIndex(m_currentIndex)) {
        return nullptr;
    }
    return &m_sessions[m_currentIndex];
}

const SessionState *SessionService::currentSession() const
{
    if (!isValidIndex(m_currentIndex)) {
        return nullptr;
    }
    return &m_sessions[m_currentIndex];
}

SessionState *SessionService::sessionById(const QString &sessionId)
{
    for (auto &session : m_sessions) {
        if (session.id == sessionId) {
            return &session;
        }
    }
    return nullptr;
}

const SessionState *SessionService::sessionById(const QString &sessionId) const
{
    for (const auto &session : m_sessions) {
        if (session.id == sessionId) {
            return &session;
        }
    }
    return nullptr;
}

void SessionService::appendSession(const SessionState &session)
{
    m_sessions.append(session);
}

SessionState SessionService::takeSessionAt(int index)
{
    return m_sessions.takeAt(index);
}

void SessionService::clear()
{
    m_sessions.clear();
    m_currentIndex = -1;
}

bool SessionService::isValidIndex(int index) const
{
    return index >= 0 && index < m_sessions.size();
}

void SessionService::setCurrentSessionIndex(int index)
{
    if (!m_dependencies.subscriptionController || !m_dependencies.subscriptionFpsRefreshTimer || !isValidIndex(index) || index == m_currentIndex) {
        return;
    }

    m_currentIndex = index;
    if (m_dependencies.reloadCurrentSessionHistory) {
        m_dependencies.reloadCurrentSessionHistory();
    }
    if (m_dependencies.subscriptionController->currentSessionHasActiveSubscriptionFps(QDateTime::currentMSecsSinceEpoch())) {
        m_dependencies.subscriptionFpsRefreshTimer->start();
    } else {
        m_dependencies.subscriptionFpsRefreshTimer->stop();
    }
    if (m_dependencies.refreshAllModels) {
        m_dependencies.refreshAllModels();
    }
    emit currentSessionIndexChanged();
    emit currentSessionChanged();
}

QVariantMap SessionService::defaultSessionConfig() const
{
    return SessionConfig::defaultConfig(m_sessions.size() + 1);
}

QVariantMap SessionService::sessionConfigAt(int index) const
{
    if (index < 0 || index >= m_sessions.size()) {
        return defaultSessionConfig();
    }

    const auto &session = m_sessions.at(index);
    return SessionSettingsStore::configFromState(session);
}

bool SessionService::updateSessionConfigAt(int index, const QVariantMap &config)
{
    if (!m_dependencies.configureSession || !m_dependencies.mqttController || !m_dependencies.saveSessions || index < 0 || index >= m_sessions.size()) {
        return false;
    }

    auto *session = &m_sessions[index];
    auto *client = session->client;
    if (!client) {
        return false;
    }
    const bool reconnect = client->state() != QMqttClient::Disconnected;
    if (reconnect) {
        session->disconnectRequested = true;
        client->disconnectFromHost();
    }

    m_dependencies.configureSession(*session, config, true);
    session->lastError.clear();
    session->sessionRestored = false;
    m_dependencies.mqttController->updatePublishStatus(*session, QStringLiteral("idle"));
    const bool saved = m_dependencies.saveSessions();

    if (reconnect) {
        session->disconnectRequested = false;
        m_dependencies.mqttController->connectSession(*session, tr("Connecting to"));
    }

    if (m_dependencies.refreshSessionsModel) {
        m_dependencies.refreshSessionsModel();
    }
    if (index == m_currentIndex) {
        if (m_dependencies.refreshSessionAndSubscriptionModels) {
            m_dependencies.refreshSessionAndSubscriptionModels();
        }
        emit currentSessionChanged();
    }
    return saved;
}

void SessionService::addSessionWithConfig(const QVariantMap &config)
{
    if (!m_dependencies.createDefaultSession || !m_dependencies.configureSession || !m_dependencies.saveSessions) {
        return;
    }

    const QString fallbackName = config.value(QStringLiteral("name")).toString().trimmed().isEmpty()
        ? tr("Session %1").arg(m_sessions.size() + 1)
        : config.value(QStringLiteral("name")).toString().trimmed();

    SessionState session = m_dependencies.createDefaultSession(fallbackName);
    m_dependencies.configureSession(session, config, false);
    m_sessions.append(session);
    m_currentIndex = m_sessions.size() - 1;
    if (m_dependencies.reloadCurrentSessionHistory) {
        m_dependencies.reloadCurrentSessionHistory();
    }
    m_dependencies.saveSessions();
    if (m_dependencies.refreshAllModels) {
        m_dependencies.refreshAllModels();
    }
    if (m_dependencies.refreshScriptsModel) {
        m_dependencies.refreshScriptsModel();
    }
    if (m_dependencies.refreshSessionsModel) {
        m_dependencies.refreshSessionsModel();
    }
    emit currentSessionIndexChanged();
    emit currentSessionChanged();
}

void SessionService::duplicateSessionAt(int index)
{
    if (!m_dependencies.createDefaultSession || !m_dependencies.configureSession || !m_dependencies.saveSessions || index < 0 || index >= m_sessions.size()) {
        return;
    }

    const auto &source = m_sessions.at(index);
    const QVariantMap config = SessionSettingsStore::duplicateConfigFromState(source);

    SessionState session = m_dependencies.createDefaultSession(tr("%1 Copy").arg(source.name));
    m_dependencies.configureSession(session, config, false);
    session.outputPaused = source.outputPaused;
    session.subscriptionFormats = source.subscriptionFormats;
    session.subscriptions = source.subscriptions;
    for (auto &subscription : session.subscriptions) {
        subscription.runtimeSubscription.clear();
        subscription.runtimeState = QStringLiteral("saved");
        subscription.grantedQos = -1;
        subscription.lastError.clear();
        subscription.recentMessageTimestampsMs.clear();
    }

    m_sessions.append(session);
    m_currentIndex = m_sessions.size() - 1;
    if (m_dependencies.reloadCurrentSessionHistory) {
        m_dependencies.reloadCurrentSessionHistory();
    }
    m_dependencies.saveSessions();
    if (m_dependencies.refreshAllModels) {
        m_dependencies.refreshAllModels();
    }
    if (m_dependencies.refreshScriptsModel) {
        m_dependencies.refreshScriptsModel();
    }
    if (m_dependencies.refreshSessionsModel) {
        m_dependencies.refreshSessionsModel();
    }
    emit currentSessionIndexChanged();
    emit currentSessionChanged();
}

void SessionService::removeSessionAt(int index)
{
    if (!m_dependencies.historyStore || !m_dependencies.destroySessionRuntime || !m_dependencies.saveSessions || m_sessions.size() <= 1 || index < 0 || index >= m_sessions.size()) {
        return;
    }

    SessionState removed = takeSessionAt(index);
    if (m_dependencies.deleteHistoryWithSession && m_dependencies.deleteHistoryWithSession()) {
        m_dependencies.historyStore->clearSessionHistory(removed.id);
    }
    m_dependencies.destroySessionRuntime(removed);

    int indexAfterRemoval = m_currentIndex;
    if (indexAfterRemoval >= m_sessions.size()) {
        indexAfterRemoval = m_sessions.size() - 1;
    }
    if (indexAfterRemoval > index) {
        --indexAfterRemoval;
    }
    m_currentIndex = indexAfterRemoval;

    if (m_dependencies.reloadCurrentSessionHistory) {
        m_dependencies.reloadCurrentSessionHistory();
    }
    m_dependencies.saveSessions();
    if (m_dependencies.refreshAllModels) {
        m_dependencies.refreshAllModels();
    }
    if (m_dependencies.refreshScriptsModel) {
        m_dependencies.refreshScriptsModel();
    }
    if (m_dependencies.refreshSessionsModel) {
        m_dependencies.refreshSessionsModel();
    }
    emit currentSessionIndexChanged();
    emit currentSessionChanged();
}

void SessionService::setCurrentOutputPaused(bool paused)
{
    if (!m_dependencies.saveSessions) {
        return;
    }

    auto *session = currentSession();
    if (!session || session->outputPaused == paused) {
        return;
    }

    session->outputPaused = paused;
    m_dependencies.saveSessions();
    if (!paused && m_dependencies.reloadCurrentSessionHistory) {
        m_dependencies.reloadCurrentSessionHistory();
    }
    if (m_dependencies.refreshCurrentSessionModels) {
        m_dependencies.refreshCurrentSessionModels();
    }
    emit currentSessionChanged();
}
