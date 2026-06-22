#include "sessioncontroller.h"

#include "domain/sessionconfig.h"
#include "services/storage/sessionsettingsstore.h"

#include <QDateTime>

SessionController::SessionController(QObject *parent)
    : QObject(parent)
{
}

void SessionController::setDependencies(Dependencies dependencies)
{
    m_dependencies = std::move(dependencies);
}

QVector<SessionState> &SessionController::sessions()
{
    return m_sessions;
}

const QVector<SessionState> &SessionController::sessions() const
{
    return m_sessions;
}

int SessionController::currentIndex() const
{
    return m_currentIndex;
}

void SessionController::setCurrentIndex(int index)
{
    m_currentIndex = index;
}

SessionState *SessionController::currentSession()
{
    if (!isValidIndex(m_currentIndex)) {
        return nullptr;
    }
    return &m_sessions[m_currentIndex];
}

const SessionState *SessionController::currentSession() const
{
    if (!isValidIndex(m_currentIndex)) {
        return nullptr;
    }
    return &m_sessions[m_currentIndex];
}

SessionState *SessionController::sessionById(const QString &sessionId)
{
    for (auto &session : m_sessions) {
        if (session.id == sessionId) {
            return &session;
        }
    }
    return nullptr;
}

const SessionState *SessionController::sessionById(const QString &sessionId) const
{
    for (const auto &session : m_sessions) {
        if (session.id == sessionId) {
            return &session;
        }
    }
    return nullptr;
}

void SessionController::appendSession(const SessionState &session)
{
    m_sessions.append(session);
}

SessionState SessionController::takeSessionAt(int index)
{
    return m_sessions.takeAt(index);
}

void SessionController::clear()
{
    m_sessions.clear();
    m_currentIndex = -1;
}

bool SessionController::isValidIndex(int index) const
{
    return index >= 0 && index < m_sessions.size();
}

void SessionController::setCurrentSessionIndex(int index)
{
    if (!m_dependencies.reloadCurrentSessionHistory || !isValidIndex(index) || index == m_currentIndex) {
        return;
    }

    m_currentIndex = index;
    m_dependencies.reloadCurrentSessionHistory();
    if (m_dependencies.currentSessionHasActiveSubscriptionFps && m_dependencies.setSubscriptionFpsRefreshActive) {
        m_dependencies.setSubscriptionFpsRefreshActive(
            m_dependencies.currentSessionHasActiveSubscriptionFps());
    }
    if (m_dependencies.notifySelectedSessionViewsChanged) {
        m_dependencies.notifySelectedSessionViewsChanged();
    }
}

QVariantMap SessionController::defaultSessionConfig() const
{
    return SessionConfig::defaultConfig(m_sessions.size() + 1);
}

QVariantMap SessionController::sessionConfigAt(int index) const
{
    if (index < 0 || index >= m_sessions.size()) {
        return defaultSessionConfig();
    }

    const auto &session = m_sessions.at(index);
    return SessionSettingsStore::configFromState(session);
}

bool SessionController::updateSessionConfigAt(int index, const QVariantMap &config)
{
    if (!m_dependencies.configureSession || !m_dependencies.updatePublishStatus
            || !m_dependencies.saveSessions || index < 0 || index >= m_sessions.size()) {
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
    m_dependencies.updatePublishStatus(*session, QStringLiteral("idle"), QString(), -1);
    const bool saved = m_dependencies.saveSessions();

    if (reconnect && m_dependencies.connectSession) {
        session->disconnectRequested = false;
        m_dependencies.connectSession(*session, tr("Connecting to"));
    }

    if (m_dependencies.emitSessionsChanged) {
        m_dependencies.emitSessionsChanged();
    }
    if (index == m_currentIndex) {
        if (m_dependencies.notifyCurrentSessionAndSubscriptionsChanged) {
            m_dependencies.notifyCurrentSessionAndSubscriptionsChanged();
        }
    }
    return saved;
}

void SessionController::addSessionWithConfig(const QVariantMap &config)
{
    if (!m_dependencies.createDefaultSession || !m_dependencies.configureSession
            || !m_dependencies.reloadCurrentSessionHistory || !m_dependencies.saveSessions) {
        return;
    }

    const QString fallbackName = config.value(QStringLiteral("name")).toString().trimmed().isEmpty()
        ? tr("Session %1").arg(m_sessions.size() + 1)
        : config.value(QStringLiteral("name")).toString().trimmed();

    SessionState session = m_dependencies.createDefaultSession(fallbackName);
    m_dependencies.configureSession(session, config, false);
    m_sessions.append(session);
    m_currentIndex = m_sessions.size() - 1;
    m_dependencies.reloadCurrentSessionHistory();
    m_dependencies.saveSessions();
    if (m_dependencies.notifySessionCollectionViewsChanged) {
        m_dependencies.notifySessionCollectionViewsChanged();
    }
}

void SessionController::duplicateSessionAt(int index)
{
    if (!m_dependencies.createDefaultSession || !m_dependencies.configureSession
            || !m_dependencies.reloadCurrentSessionHistory || !m_dependencies.saveSessions
            || index < 0 || index >= m_sessions.size()) {
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
    m_dependencies.reloadCurrentSessionHistory();
    m_dependencies.saveSessions();
    if (m_dependencies.notifySessionCollectionViewsChanged) {
        m_dependencies.notifySessionCollectionViewsChanged();
    }
}

void SessionController::removeSessionAt(int index)
{
    if (!m_dependencies.reloadCurrentSessionHistory || !m_dependencies.saveSessions
            || m_sessions.size() <= 1 || index < 0 || index >= m_sessions.size()) {
        return;
    }

    SessionState removed = takeSessionAt(index);
    if (m_dependencies.deleteHistoryWithSession && m_dependencies.deleteHistoryWithSession()
            && m_dependencies.clearSessionHistory) {
        m_dependencies.clearSessionHistory(removed.id);
    }
    if (m_dependencies.destroySessionRuntime) {
        m_dependencies.destroySessionRuntime(removed);
    }

    int indexAfterRemoval = m_currentIndex;
    if (indexAfterRemoval >= m_sessions.size()) {
        indexAfterRemoval = m_sessions.size() - 1;
    }
    if (indexAfterRemoval > index) {
        --indexAfterRemoval;
    }
    m_currentIndex = indexAfterRemoval;

    m_dependencies.reloadCurrentSessionHistory();
    m_dependencies.saveSessions();
    if (m_dependencies.notifySessionCollectionViewsChanged) {
        m_dependencies.notifySessionCollectionViewsChanged();
    }
}

void SessionController::setCurrentOutputPaused(bool paused)
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
    if (!paused) {
        if (m_dependencies.reloadCurrentSessionHistory) {
            m_dependencies.reloadCurrentSessionHistory();
        }
        if (m_dependencies.emitMessageStreamChanged) {
            m_dependencies.emitMessageStreamChanged();
        }
    }
    if (m_dependencies.notifyCurrentSessionViewsChanged) {
        m_dependencies.notifyCurrentSessionViewsChanged();
    }
}
