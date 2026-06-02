#include "appcontroller.h"

#include "appcontrollerutils.h"
#include "sessionconfig.h"

#include <QMqttConnectionProperties>
#include <QUuid>

using namespace AppControllerUtils;

void AppController::setCurrentSessionIndex(int index)
{
    if (index < 0 || index >= m_sessions.size() || index == m_currentSessionIndex) {
        return;
    }

    m_currentSessionIndex = index;
    reloadCurrentSessionHistory();
    if (currentSessionHasActiveSubscriptionFps(QDateTime::currentMSecsSinceEpoch())) {
        m_subscriptionFpsRefreshTimer.start();
    } else {
        m_subscriptionFpsRefreshTimer.stop();
    }
    emit currentSessionIndexChanged();
    emit currentSessionChanged();
    emit subscriptionsChanged();
    emit eventStreamChanged();
    emit scriptLibraryChanged();
    emit scriptTestSamplesChanged();
}

QVariantMap AppController::defaultSessionConfig() const
{
    return SessionConfig::defaultConfig(m_sessions.size() + 1);
}

QVariantMap AppController::sessionConfigAt(int index) const
{
    if (index < 0 || index >= m_sessions.size()) {
        return defaultSessionConfig();
    }

    return SessionConfig::configFromSession(m_sessions.at(index));
}

bool AppController::updateSessionConfigAt(int index, const QVariantMap &config)
{
    if (index < 0 || index >= m_sessions.size()) {
        return false;
    }

    auto *session = &m_sessions[index];
    const bool reconnect = session->client->state() != QMqttClient::Disconnected;
    if (reconnect) {
        session->disconnectRequested = true;
        session->client->disconnectFromHost();
    }

    configureSession(*session, config, true);
    session->lastError.clear();
    session->sessionRestored = false;
    updatePublishStatus(*session, QStringLiteral("idle"));
    saveSessions();

    if (reconnect) {
        session->disconnectRequested = false;
        connectSession(*session, QStringLiteral("Connecting to"));
    }

    emit sessionsChanged();
    if (index == m_currentSessionIndex) {
        emit currentSessionChanged();
        emit subscriptionsChanged();
    }
    return true;
}

void AppController::addSessionWithConfig(const QVariantMap &config)
{
    const QString fallbackName = config.value(QStringLiteral("name")).toString().trimmed().isEmpty()
        ? QStringLiteral("Session %1").arg(m_sessions.size() + 1)
        : config.value(QStringLiteral("name")).toString().trimmed();

    SessionState session = createDefaultSession(fallbackName);
    configureSession(session, config, false);
    m_sessions.append(session);
    m_currentSessionIndex = m_sessions.size() - 1;
    reloadCurrentSessionHistory();
    saveSessions();
    emit sessionsChanged();
    emit currentSessionIndexChanged();
    emit currentSessionChanged();
    emit subscriptionsChanged();
    emit eventStreamChanged();
    emit scriptLibraryChanged();
}

void AppController::duplicateSessionAt(int index)
{
    if (index < 0 || index >= m_sessions.size()) {
        return;
    }

    const auto &source = m_sessions.at(index);
    const QVariantMap config = SessionConfig::duplicateConfigFromSession(source);

    SessionState session = createDefaultSession(QStringLiteral("%1 Copy").arg(source.name));
    configureSession(session, config, false);
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
    m_currentSessionIndex = m_sessions.size() - 1;
    reloadCurrentSessionHistory();
    saveSessions();
    emit sessionsChanged();
    emit currentSessionIndexChanged();
    emit currentSessionChanged();
    emit subscriptionsChanged();
    emit eventStreamChanged();
    emit scriptLibraryChanged();
}

void AppController::removeSessionAt(int index)
{
    if (m_sessions.size() <= 1 || index < 0 || index >= m_sessions.size()) {
        return;
    }

    SessionState removed = m_sessions.takeAt(index);
    if (removed.client) {
        removed.client->disconnectFromHost();
        removed.client->deleteLater();
    }
    if (removed.connectTimeoutTimer) {
        removed.connectTimeoutTimer->deleteLater();
    }

    if (m_currentSessionIndex >= m_sessions.size()) {
        m_currentSessionIndex = m_sessions.size() - 1;
    }
    if (m_currentSessionIndex > index) {
        --m_currentSessionIndex;
    }

    reloadCurrentSessionHistory();
    saveSessions();
    emit sessionsChanged();
    emit currentSessionIndexChanged();
    emit currentSessionChanged();
    emit subscriptionsChanged();
    emit eventStreamChanged();
    emit scriptLibraryChanged();
}

void AppController::setCurrentOutputPaused(bool paused)
{
    auto *session = currentSessionState();
    if (!session || session->outputPaused == paused) {
        return;
    }

    session->outputPaused = paused;
    saveSessions();
    if (!paused) {
        reloadCurrentSessionHistory();
        emit eventStreamChanged();
    }
    emit currentSessionChanged();
}

AppController::SessionState *AppController::currentSessionState()
{
    if (m_currentSessionIndex < 0 || m_currentSessionIndex >= m_sessions.size()) {
        return nullptr;
    }
    return &m_sessions[m_currentSessionIndex];
}

const AppController::SessionState *AppController::currentSessionState() const
{
    if (m_currentSessionIndex < 0 || m_currentSessionIndex >= m_sessions.size()) {
        return nullptr;
    }
    return &m_sessions[m_currentSessionIndex];
}

AppController::SessionState *AppController::sessionById(const QString &sessionId)
{
    for (auto &session : m_sessions) {
        if (session.id == sessionId) {
            return &session;
        }
    }
    return nullptr;
}

const AppController::SessionState *AppController::sessionById(const QString &sessionId) const
{
    for (const auto &session : m_sessions) {
        if (session.id == sessionId) {
            return &session;
        }
    }
    return nullptr;
}

void AppController::configureSession(SessionState &session, const QVariantMap &config, bool keepNameFallback)
{
    QString name = config.value(QStringLiteral("name")).toString().trimmed();
    if (name.isEmpty() && !keepNameFallback) {
        name = session.name;
    }
    if (!name.isEmpty()) {
        session.name = name;
    }

    session.transport = SessionConfig::sanitizeTransport(config.value(QStringLiteral("transport")));
    session.protocolVersion = SessionConfig::sanitizeProtocolVersion(config.value(QStringLiteral("protocolVersion"), 5));
    session.sslSecure = config.value(QStringLiteral("sslSecure"), true).toBool();
    session.alpn = config.value(QStringLiteral("alpn")).toString().trimmed();
    session.certificateType = config.value(QStringLiteral("certificateType"), QStringLiteral("ca")).toString() == QStringLiteral("self")
        ? QStringLiteral("self")
        : QStringLiteral("ca");
    session.caFile = config.value(QStringLiteral("caFile")).toString().trimmed();
    session.clientCertificateFile = config.value(QStringLiteral("clientCertificateFile")).toString().trimmed();
    session.clientKeyFile = config.value(QStringLiteral("clientKeyFile")).toString().trimmed();
    session.connectTimeoutSeconds =
        SessionConfig::sanitizeBoundedInt(config.value(QStringLiteral("connectTimeoutSeconds"), 10), 10, 1, 300);
    session.sessionExpiryInterval = SessionConfig::sanitizeOptionalUInt32(config.value(QStringLiteral("sessionExpiryInterval"), 0));
    session.receiveMaximum = SessionConfig::sanitizeOptionalUInt16(config.value(QStringLiteral("receiveMaximum")));
    session.maximumPacketSize = SessionConfig::sanitizeOptionalUInt32(config.value(QStringLiteral("maximumPacketSize")));
    session.topicAliasMaximum = SessionConfig::sanitizeOptionalUInt16(config.value(QStringLiteral("topicAliasMaximum")));
    session.requestResponseInformation =
        config.value(QStringLiteral("requestResponseInformation"), false).toBool();
    session.requestProblemInformation =
        config.value(QStringLiteral("requestProblemInformation"), false).toBool();
    session.authenticationMethod = config.value(QStringLiteral("authenticationMethod")).toString().trimmed();
    session.authenticationData = config.value(QStringLiteral("authenticationData")).toString();

    QString host = config.value(QStringLiteral("host")).toString().trimmed();
    if (host.isEmpty()) {
        host = QStringLiteral("broker.emqx.io");
    }

    session.client->setHostname(host);
    session.client->setPort(SessionConfig::sanitizePort(config.value(QStringLiteral("port")), session.transport));
    session.client->setProtocolVersion(toProtocolVersion(session.protocolVersion));

    QString clientId = config.value(QStringLiteral("clientId")).toString().trimmed();
    if (clientId.isEmpty()) {
        clientId = SessionConfig::generateClientId();
    }
    session.client->setClientId(clientId);
    session.client->setUsername(config.value(QStringLiteral("username")).toString());
    session.client->setPassword(config.value(QStringLiteral("password")).toString());
    session.client->setCleanSession(config.value(QStringLiteral("cleanSession"), true).toBool());
    session.client->setKeepAlive(SessionConfig::sanitizeKeepAlive(
        config.value(QStringLiteral("keepAliveSeconds"), SessionConfig::kDefaultKeepAlive)));
    session.client->setAutoKeepAlive(true);
    if (session.connectTimeoutTimer) {
        session.connectTimeoutTimer->setInterval(session.connectTimeoutSeconds * 1000);
    }
    QMqttConnectionProperties connectionProperties;
    connectionProperties.setSessionExpiryInterval(session.sessionExpiryInterval);
    if (session.receiveMaximum > 0) {
        connectionProperties.setMaximumReceive(session.receiveMaximum);
    }
    if (session.maximumPacketSize > 0) {
        connectionProperties.setMaximumPacketSize(session.maximumPacketSize);
    }
    if (session.topicAliasMaximum > 0) {
        connectionProperties.setMaximumTopicAlias(session.topicAliasMaximum);
    }
    connectionProperties.setRequestResponseInformation(session.requestResponseInformation);
    connectionProperties.setRequestProblemInformation(session.requestProblemInformation);
    if (!session.authenticationMethod.isEmpty()) {
        connectionProperties.setAuthenticationMethod(session.authenticationMethod);
        connectionProperties.setAuthenticationData(session.authenticationData.toUtf8());
    }
    session.client->setConnectionProperties(connectionProperties);
}

void AppController::initializeSessionRuntime(SessionState *session)
{
    if (!session) {
        return;
    }

    if (!session->connectTimeoutTimer) {
        session->connectTimeoutTimer = new QTimer(this);
        session->connectTimeoutTimer->setSingleShot(true);
        connect(session->connectTimeoutTimer, &QTimer::timeout, this, [this, sessionId = session->id]() {
            auto *boundSession = sessionById(sessionId);
            if (!boundSession || !boundSession->client || boundSession->client->state() != QMqttClient::Connecting) {
                return;
            }

            boundSession->lastError = QStringLiteral("Connection timed out.");
            appendEvent(*boundSession, QStringLiteral("Error"), boundSession->lastError);
            boundSession->client->disconnectFromHost();
            emit sessionsChanged();
            emit currentSessionChanged();
        });
    }

}

void AppController::loadSessions()
{
    const int count = m_settings.beginReadArray(QStringLiteral("sessions"));
    for (int i = 0; i < count; ++i) {
        SessionConfig::LoadedSession loaded = SessionConfig::readSessionSettings(
            m_settings,
            i,
            [this](const QString &scriptId) { return scriptById(scriptId) != nullptr; });
        SessionState session = loaded.session;

        session.client = new QMqttClient(this);
        session.client->setAutoKeepAlive(true);
        initializeSessionRuntime(&session);
        configureSession(session, loaded.config, false);
        session.publishStatus = defaultPublishStatus();
        bindSessionSignals(&session);
        m_sessions.append(session);
    }
    m_settings.endArray();

    if (m_sessions.isEmpty()) {
        m_sessions.append(createDefaultSession(QStringLiteral("Session 1")));
        saveSessions();
    }

    m_currentSessionIndex = 0;
    reloadCurrentSessionHistory();
    emit sessionsChanged();
    emit currentSessionIndexChanged();
    emit currentSessionChanged();
    emit subscriptionsChanged();
    emit eventStreamChanged();
}

void AppController::saveSessions()
{
    SessionConfig::writeSessionSettings(m_settings, m_sessions);
}

AppController::SessionState AppController::createDefaultSession(const QString &name) const
{
    SessionState session;
    session.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    session.name = name;
    session.transport = QStringLiteral("tcp");
    session.protocolVersion = 5;
    session.publishStatus = defaultPublishStatus();
    session.client = new QMqttClient(const_cast<AppController *>(this));
    session.client->setAutoKeepAlive(true);
    const_cast<AppController *>(this)->initializeSessionRuntime(&session);
    QVariantMap config = SessionConfig::defaultConfig(1);
    config.insert(QStringLiteral("name"), name);
    const_cast<AppController *>(this)->configureSession(session, config, false);
    const_cast<AppController *>(this)->bindSessionSignals(&session);
    return session;
}

