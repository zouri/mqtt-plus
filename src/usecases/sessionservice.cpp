#include "sessionservice.h"

#include "domain/sessionconfig.h"
#include "services/apputils.h"
#include "services/storage/historystore.h"
#include "services/storage/sessionsettingsstore.h"
#include "usecases/preferencescontroller.h"
#include "usecases/scriptservice.h"

#include <QMqttClient>
#include <QMqttConnectionProperties>
#include <QSettings>
#include <QTimer>
#include <QUuid>

#include <algorithm>
#include <utility>

using namespace AppUtils;

SessionService::SessionService(
    QSettings &settings,
    ScriptService &scriptService,
    HistoryStore &historyStore,
    PreferencesController &preferences,
    QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_scriptService(scriptService)
    , m_historyStore(historyStore)
    , m_preferences(preferences)
{
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

SessionState *SessionService::currentSession()
{
    return isValidIndex(m_currentIndex) ? &m_sessions[m_currentIndex] : nullptr;
}

const SessionState *SessionService::currentSession() const
{
    return isValidIndex(m_currentIndex) ? &m_sessions[m_currentIndex] : nullptr;
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

bool SessionService::loadSessions()
{
    for (SessionState &session : m_sessions) {
        destroySessionRuntime(session);
    }
    m_sessions.clear();
    m_currentIndex = -1;

    const int count = m_settings.beginReadArray(QStringLiteral("sessions"));
    m_sessions.reserve(count > 0 ? count : 1);
    for (int i = 0; i < count; ++i) {
        SessionSettingsStore::LoadedSession loaded = SessionSettingsStore::readSession(m_settings, i);
        for (SubscriptionEntry &subscription : loaded.session.subscriptions) {
            if (!subscription.scriptId.isEmpty()
                    && !m_scriptService.scriptById(subscription.scriptId)) {
                subscription.scriptId.clear();
            }
        }

        initializeSessionRuntime(loaded.session);
        applyConfig(loaded.session, loaded.config, false);
        m_sessions.append(std::move(loaded.session));
        emit sessionRuntimeReady(&m_sessions.last());
    }
    m_settings.endArray();

    bool loaded = true;
    if (m_sessions.isEmpty()) {
        m_sessions.append(createDefaultSession(tr("Session 1")));
        emit sessionRuntimeReady(&m_sessions.last());
        loaded = saveSessions();
    }

    emit sessionsChanged();
    return loaded;
}

bool SessionService::saveSessions()
{
    QString errorMessage;
    if (SessionSettingsStore::writeSessions(m_settings, m_sessions, errorMessage)) {
        return true;
    }

    emit storageError(
        errorMessage.isEmpty() ? tr("Cannot save sessions.") : errorMessage);
    return false;
}

void SessionService::setCurrentSessionIndex(int index)
{
    if (!isValidIndex(index) || index == m_currentIndex) {
        return;
    }

    m_currentIndex = index;
    emit currentSessionHistoryReloadRequested();
    emit currentSessionIndexChanged();
    emit currentSessionChanged();
}

QVariantMap SessionService::defaultSessionConfig() const
{
    return SessionConfig::defaultConfig(m_sessions.size() + 1);
}

QVariantMap SessionService::sessionConfigAt(int index) const
{
    if (!isValidIndex(index)) {
        return defaultSessionConfig();
    }
    return SessionSettingsStore::configFromState(m_sessions.at(index));
}

bool SessionService::updateSessionConfigAt(int index, const QVariantMap &config)
{
    if (!isValidIndex(index)) {
        return false;
    }

    auto &session = m_sessions[index];
    auto *client = session.runtime.client;
    if (!client) {
        return false;
    }

    const bool reconnect = client->state() != QMqttClient::Disconnected;
    if (reconnect) {
        session.runtime.disconnectRequested = true;
        client->disconnectFromHost();
    }

    applyConfig(session, config, true);
    session.runtime.lastError.clear();
    session.runtime.sessionRestored = false;
    session.runtime.publishStatus.state = QStringLiteral("idle");
    session.runtime.publishStatus.reason.clear();
    session.runtime.publishStatus.updatedAt = timestampNow();
    const bool saved = saveSessions();

    if (reconnect) {
        session.runtime.disconnectRequested = false;
        emit reconnectRequested(&session);
    }

    emit sessionsChanged();
    if (index == m_currentIndex) {
        emit currentSessionChanged();
    }
    return saved;
}

void SessionService::addSessionWithConfig(const QVariantMap &config)
{
    const QString configuredName = config.value(QStringLiteral("name")).toString().trimmed();
    const QString fallbackName = configuredName.isEmpty()
        ? tr("Session %1").arg(m_sessions.size() + 1)
        : configuredName;

    SessionState session = createDefaultSession(fallbackName);
    applyConfig(session, config, false);
    m_sessions.append(std::move(session));
    m_currentIndex = m_sessions.size() - 1;
    emit sessionRuntimeReady(&m_sessions.last());
    emit currentSessionHistoryReloadRequested();
    saveSessions();
    emit sessionsChanged();
    emit currentSessionIndexChanged();
    emit currentSessionChanged();
}

void SessionService::duplicateSessionAt(int index)
{
    if (!isValidIndex(index)) {
        return;
    }

    const SessionState &source = m_sessions.at(index);
    const QVariantMap config = SessionSettingsStore::duplicateConfigFromState(source);

    SessionState session = createDefaultSession(tr("%1 Copy").arg(source.name));
    applyConfig(session, config, false);
    session.outputPaused = source.outputPaused;
    session.runtime.subscriptionFormats = source.runtime.subscriptionFormats;
    session.subscriptions = source.subscriptions;
    for (auto &subscription : session.subscriptions) {
        subscription.runtimeSubscription.clear();
        subscription.runtimeState = QStringLiteral("saved");
        subscription.grantedQos = -1;
        subscription.lastError.clear();
        subscription.recentMessageTimestampsMs.clear();
    }

    m_sessions.append(std::move(session));
    m_currentIndex = m_sessions.size() - 1;
    emit sessionRuntimeReady(&m_sessions.last());
    emit currentSessionHistoryReloadRequested();
    saveSessions();
    emit sessionsChanged();
    emit currentSessionIndexChanged();
    emit currentSessionChanged();
}

void SessionService::removeSessionAt(int index)
{
    if (m_sessions.size() <= 1 || !isValidIndex(index)) {
        return;
    }

    SessionState removed = m_sessions.takeAt(index);
    if (m_preferences.deleteHistoryWithSession()) {
        m_historyStore.clearSessionHistory(removed.id);
    }
    destroySessionRuntime(removed);

    if (m_currentIndex == index) {
        m_currentIndex = (std::min)(index, static_cast<int>(m_sessions.size()) - 1);
    } else if (m_currentIndex > index) {
        --m_currentIndex;
    }

    emit currentSessionHistoryReloadRequested();
    saveSessions();
    emit sessionsChanged();
    emit currentSessionIndexChanged();
    emit currentSessionChanged();
}

void SessionService::setCurrentOutputPaused(bool paused)
{
    auto *session = currentSession();
    if (!session || session->outputPaused == paused) {
        return;
    }

    session->outputPaused = paused;
    saveSessions();
    if (!paused) {
        emit currentSessionHistoryReloadRequested();
    }
    emit sessionsChanged();
    emit currentSessionChanged();
}

bool SessionService::isValidIndex(int index) const
{
    return index >= 0 && index < m_sessions.size();
}

void SessionService::applyConfig(
    SessionState &session,
    const QVariantMap &config,
    bool keepNameFallback) const
{
    auto *client = session.runtime.client;
    if (!client) {
        return;
    }

    QString name = config.value(QStringLiteral("name")).toString().trimmed();
    if (name.isEmpty() && !keepNameFallback) {
        name = session.name;
    }
    if (!name.isEmpty()) {
        session.name = name;
    }

    session.transport = SessionConfig::sanitizeTransport(config.value(QStringLiteral("transport")));
    session.protocolVersion = SessionConfig::sanitizeProtocolVersion(
        config.value(QStringLiteral("protocolVersion"), 5));
    session.sslSecure = config.value(QStringLiteral("sslSecure"), true).toBool();
    session.alpn = config.value(QStringLiteral("alpn")).toString().trimmed();
    session.certificateType = config.value(
                                  QStringLiteral("certificateType"),
                                  QStringLiteral("ca"))
                                      .toString()
            == QStringLiteral("self")
        ? QStringLiteral("self")
        : QStringLiteral("ca");
    session.caFile = config.value(QStringLiteral("caFile")).toString().trimmed();
    session.clientCertificateFile = config.value(
                                            QStringLiteral("clientCertificateFile"))
                                        .toString()
                                        .trimmed();
    session.clientKeyFile = config.value(QStringLiteral("clientKeyFile")).toString().trimmed();
    session.connectTimeoutSeconds = SessionConfig::sanitizeBoundedInt(
        config.value(QStringLiteral("connectTimeoutSeconds"), 10), 10, 1, 300);
    session.sessionExpiryInterval = SessionConfig::sanitizeOptionalUInt32(
        config.value(QStringLiteral("sessionExpiryInterval"), 0));
    session.receiveMaximum = SessionConfig::sanitizeOptionalUInt16(
        config.value(QStringLiteral("receiveMaximum")));
    session.maximumPacketSize = SessionConfig::sanitizeOptionalUInt32(
        config.value(QStringLiteral("maximumPacketSize")));
    session.topicAliasMaximum = SessionConfig::sanitizeOptionalUInt16(
        config.value(QStringLiteral("topicAliasMaximum")));
    session.requestResponseInformation = config.value(
                                                QStringLiteral("requestResponseInformation"),
                                                false)
                                            .toBool();
    session.requestProblemInformation = config.value(
                                               QStringLiteral("requestProblemInformation"),
                                               false)
                                           .toBool();
    session.authenticationMethod = config.value(
                                             QStringLiteral("authenticationMethod"))
                                         .toString()
                                         .trimmed();
    session.authenticationData = config.value(QStringLiteral("authenticationData")).toString();

    QString host = config.value(QStringLiteral("host")).toString().trimmed();
    if (host.isEmpty()) {
        host = QStringLiteral("broker.emqx.io");
    }

    client->setHostname(host);
    client->setPort(SessionConfig::sanitizePort(config.value(QStringLiteral("port")), session.transport));
    client->setProtocolVersion(toProtocolVersion(session.protocolVersion));

    QString clientId = config.value(QStringLiteral("clientId")).toString().trimmed();
    if (clientId.isEmpty()) {
        clientId = SessionConfig::generateClientId();
    }
    client->setClientId(clientId);
    client->setUsername(config.value(QStringLiteral("username")).toString());
    client->setPassword(config.value(QStringLiteral("password")).toString());
    client->setCleanSession(config.value(QStringLiteral("cleanSession"), true).toBool());
    client->setKeepAlive(SessionConfig::sanitizeKeepAlive(
        config.value(QStringLiteral("keepAliveSeconds"), SessionConfig::kDefaultKeepAlive)));
    client->setAutoKeepAlive(true);
    if (session.runtime.connectTimeoutTimer) {
        session.runtime.connectTimeoutTimer->setInterval(session.connectTimeoutSeconds * 1000);
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
    client->setConnectionProperties(connectionProperties);
}

void SessionService::initializeSessionRuntime(SessionState &session)
{
    if (!session.runtime.client) {
        session.runtime.client = new QMqttClient(this);
        session.runtime.client->setAutoKeepAlive(true);
    }
    if (session.runtime.connectTimeoutTimer) {
        return;
    }

    session.runtime.connectTimeoutTimer = new QTimer(this);
    session.runtime.connectTimeoutTimer->setSingleShot(true);
    connect(
        session.runtime.connectTimeoutTimer,
        &QTimer::timeout,
        this,
        [this, sessionId = session.id]() {
            auto *boundSession = sessionById(sessionId);
            auto *client = boundSession ? boundSession->runtime.client : nullptr;
            if (!boundSession || !client || client->state() != QMqttClient::Connecting) {
                return;
            }

            const QString timeoutMessage = QStringLiteral("Connection timed out.");
            boundSession->runtime.lastError = timeoutMessage;
            emit runtimeError(
                boundSession->id,
                QStringLiteral("Error"),
                timeoutMessage);
            client->disconnectFromHost();
            emit sessionsChanged();
        });
}

void SessionService::destroySessionRuntime(SessionState &session)
{
    if (session.runtime.connectTimeoutTimer) {
        session.runtime.connectTimeoutTimer->stop();
        session.runtime.connectTimeoutTimer->deleteLater();
        session.runtime.connectTimeoutTimer = nullptr;
    }
    if (session.runtime.client) {
        session.runtime.client->disconnectFromHost();
        session.runtime.client->deleteLater();
        session.runtime.client = nullptr;
    }
}

SessionState SessionService::createDefaultSession(const QString &name)
{
    SessionState session;
    session.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    session.name = name;
    session.transport = QStringLiteral("tcp");
    session.protocolVersion = 5;
    initializeSessionRuntime(session);

    QVariantMap config = SessionConfig::defaultConfig(1);
    config.insert(QStringLiteral("name"), name);
    applyConfig(session, config, false);
    return session;
}
