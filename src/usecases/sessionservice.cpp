#include "sessionservice.h"

#include "domain/sessionconfig.h"
#include "services/apputils.h"
#include "services/storage/historystore.h"
#include "services/storage/historywriterworker.h"
#include "services/parsing/messageparseworker.h"
#include "services/storage/sessionsettingsstore.h"
#include "usecases/preferencescontroller.h"

#include <QMqttClient>
#include <QMqttConnectionProperties>
#include <QMqttTopicFilter>
#include <QSet>
#include <QSettings>
#include <QTimer>
#include <QUuid>

#include <algorithm>
#include <utility>

using namespace AppUtils;

SessionService::SessionService(
    QSettings &settings,
    HistoryStore &historyStore,
    PreferencesController &preferences,
    QObject *parent)
    : QObject(parent)
    , m_settings(settings)
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

bool SessionService::setMessageCapturePolicy(
    const QString &sessionId,
    const MessageCapturePolicy &policy)
{
    SessionState *session = sessionById(sessionId);
    if (!session) {
        return false;
    }

    const MessageCapturePolicy previousPolicy = session->capturePolicy;
    const MessageCapturePolicy normalized = policy.normalized();
    if (previousPolicy == normalized) {
        return true;
    }

    session->capturePolicy = normalized;
    QString errorMessage;
    if (!SessionSettingsStore::writeSessions(m_settings, m_sessions, errorMessage)) {
        session->capturePolicy = previousPolicy;
        QString ignoredError;
        SessionSettingsStore::writeSessions(m_settings, m_sessions, ignoredError);
        emit storageError(
            errorMessage.isEmpty() ? tr("Cannot save sessions.") : errorMessage);
        return false;
    }

    emit sessionsChanged();
    emit messageCapturePolicyChanged(sessionId);
    return true;
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

        initializeSessionRuntime(loaded.session);
        applyConfig(loaded.session, loaded.config);
        m_sessions.append(std::move(loaded.session));
        emit sessionRuntimeReady(&m_sessions.last());
    }
    m_settings.endArray();

    if (m_settings.status() != QSettings::NoError) {
        for (SessionState &session : m_sessions) {
            destroySessionRuntime(session);
        }
        m_sessions.clear();
        m_sessions.append(createDefaultSession(tr("Session 1")));
        emit sessionRuntimeReady(&m_sessions.last());
        emit sessionsChanged();
        emit storageError(
            m_settings.status() == QSettings::AccessError
                ? tr("Cannot read session settings: access denied.")
                : tr("Cannot read session settings: invalid settings format."));
        return false;
    }

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

SessionConnectionConfig SessionService::defaultSessionConfig() const
{
    return SessionConfig::defaultConfig(m_sessions.size() + 1);
}

SessionConnectionConfig SessionService::sessionConfigAt(int index) const
{
    if (!isValidIndex(index)) {
        return defaultSessionConfig();
    }
    return SessionSettingsStore::configFromState(m_sessions.at(index));
}

bool SessionService::updateSessionConfigAt(
    int index,
    const SessionConnectionConfig &config)
{
    if (!isValidIndex(index)) {
        return false;
    }

    auto &session = m_sessions[index];
    auto *client = session.runtime.client;
    if (!client) {
        return false;
    }

    const SessionConnectionConfig previousConfig = SessionSettingsStore::configFromState(session);
    const bool reconnect = client->state() != QMqttClient::Disconnected;
    if (reconnect) {
        session.runtime.reconnectPending = false;
        session.runtime.disconnectRequested = true;
        client->disconnectFromHost();
    }

    applyConfig(session, config);

    QString errorMessage;
    if (!SessionSettingsStore::writeSessions(m_settings, m_sessions, errorMessage)) {
        applyConfig(session, previousConfig);

        QString ignoredError;
        SessionSettingsStore::writeSessions(m_settings, m_sessions, ignoredError);

        if (reconnect) {
            requestReconnect(session);
        }

        emit storageError(
            errorMessage.isEmpty() ? tr("Cannot save sessions.") : errorMessage);
        return false;
    }

    session.runtime.lastError.clear();
    session.runtime.sessionRestored = false;
    session.runtime.publishStatus.state = QStringLiteral("idle");
    session.runtime.publishStatus.reason.clear();
    session.runtime.publishStatus.updatedAt = timestampNow();

    if (reconnect) {
        requestReconnect(session);
    }

    emit sessionsChanged();
    if (index == m_currentIndex) {
        emit currentSessionChanged();
    }
    return true;
}

bool SessionService::addSessionWithConfig(const SessionConnectionConfig &config)
{
    const QString configuredName = config.name.trimmed();
    const QString fallbackName = configuredName.isEmpty()
        ? tr("Session %1").arg(m_sessions.size() + 1)
        : configuredName;

    SessionState session = createDefaultSession(fallbackName);
    applyConfig(session, config);
    m_sessions.append(std::move(session));

    QString errorMessage;
    if (!SessionSettingsStore::writeSessions(m_settings, m_sessions, errorMessage)) {
        SessionState failedSession = m_sessions.takeLast();
        QString ignoredError;
        SessionSettingsStore::writeSessions(m_settings, m_sessions, ignoredError);
        destroySessionRuntime(failedSession);
        emit storageError(
            errorMessage.isEmpty() ? tr("Cannot save sessions.") : errorMessage);
        return false;
    }

    m_currentIndex = m_sessions.size() - 1;
    emit sessionRuntimeReady(&m_sessions.last());
    emit currentSessionHistoryReloadRequested();
    emit sessionsChanged();
    emit currentSessionIndexChanged();
    emit currentSessionChanged();
    return true;
}

bool SessionService::importSessions(
    const QVector<SessionImportRequest> &requests,
    QStringList &importedSessionIds,
    QString &errorMessage)
{
    importedSessionIds.clear();
    errorMessage.clear();
    if (requests.isEmpty()) {
        return true;
    }

    QSet<QString> ids;
    QSet<QString> names;
    for (const SessionState &session : std::as_const(m_sessions)) {
        ids.insert(session.id);
        names.insert(session.name.trimmed().toCaseFolded());
    }

    QVector<SessionState> imported;
    imported.reserve(requests.size());
    for (const SessionImportRequest &request : requests) {
        QString name = request.config.name.trimmed();
        if (name.isEmpty()) {
            name = tr("Imported connection");
        }
        const QString baseName = name;
        int suffix = 2;
        while (names.contains(name.toCaseFolded())) {
            name = tr("%1 (Imported %2)").arg(baseName).arg(suffix++);
        }
        names.insert(name.toCaseFolded());

        SessionState session = createDefaultSession(name);
        QString sessionId = request.id.trimmed();
        if (sessionId.isEmpty() || ids.contains(sessionId)) {
            sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        ids.insert(sessionId);
        session.id = sessionId;

        SessionConnectionConfig config = request.config;
        config.name = name;
        applyConfig(session, config);
        session.outputPaused = request.outputPaused;
        session.capturePolicy = request.capturePolicy.normalized();

        QSet<QString> topics;
        for (SubscriptionEntry subscription : request.subscriptions) {
            subscription.topic = subscription.topic.trimmed();
            if (subscription.topic.isEmpty()
                || !QMqttTopicFilter(subscription.topic).isValid()
                || topics.contains(subscription.topic)) {
                continue;
            }
            topics.insert(subscription.topic);
            subscription.alias = subscription.alias.trimmed();
            subscription.requestedQos = SessionConfig::sanitizeQos(subscription.requestedQos);
            subscription.format = std::clamp(subscription.format, 0, 5);
            subscription.color = subscription.color.trimmed();
            subscription.runtimeSubscription.clear();
            subscription.runtimeState = subscription.paused
                ? QStringLiteral("paused")
                : QStringLiteral("saved");
            subscription.grantedQos = -1;
            subscription.lastError.clear();
            subscription.recentMessages.clear();
            session.runtime.subscriptionFormats.insert(
                subscription.topic,
                subscription.format);
            session.subscriptions.append(std::move(subscription));
        }
        importedSessionIds.append(session.id);
        imported.append(std::move(session));
    }

    for (SessionState &session : imported) {
        m_sessions.append(std::move(session));
    }
    if (!SessionSettingsStore::writeSessions(m_settings, m_sessions, errorMessage)) {
        for (qsizetype index = 0; index < importedSessionIds.size(); ++index) {
            SessionState failed = m_sessions.takeLast();
            destroySessionRuntime(failed);
        }
        QString ignoredError;
        SessionSettingsStore::writeSessions(m_settings, m_sessions, ignoredError);
        importedSessionIds.clear();
        if (errorMessage.isEmpty()) {
            errorMessage = tr("Cannot save imported sessions.");
        }
        return false;
    }

    for (const QString &sessionId : std::as_const(importedSessionIds)) {
        if (SessionState *session = sessionById(sessionId)) {
            emit sessionRuntimeReady(session);
        }
    }
    emit sessionsChanged();
    return true;
}

bool SessionService::rollbackImportedSessions(
    const QStringList &sessionIds,
    QString &errorMessage)
{
    errorMessage.clear();
    if (sessionIds.isEmpty()) {
        return true;
    }
    const QSet<QString> rollbackIds(sessionIds.cbegin(), sessionIds.cend());
    QVector<SessionState> retained;
    retained.reserve(m_sessions.size());
    for (const SessionState &session : std::as_const(m_sessions)) {
        if (!rollbackIds.contains(session.id)) {
            retained.append(session);
        }
    }
    if (retained.size() == m_sessions.size()) {
        return true;
    }
    if (!SessionSettingsStore::writeSessions(m_settings, retained, errorMessage)) {
        if (errorMessage.isEmpty()) {
            errorMessage = tr("Cannot roll back imported sessions.");
        }
        return false;
    }

    const int previousCurrentIndex = m_currentIndex;
    const QString previousCurrentSessionId = currentSession()
        ? currentSession()->id
        : QString();
    for (qsizetype index = m_sessions.size(); index > 0; --index) {
        if (!rollbackIds.contains(m_sessions.at(index - 1).id)) {
            continue;
        }
        SessionState removed = m_sessions.takeAt(index - 1);
        destroySessionRuntime(removed);
    }
    if (m_sessions.isEmpty()) {
        m_currentIndex = -1;
    } else if (!previousCurrentSessionId.isEmpty()
        && sessionById(previousCurrentSessionId)) {
        for (int index = 0; index < m_sessions.size(); ++index) {
            if (m_sessions.at(index).id == previousCurrentSessionId) {
                m_currentIndex = index;
                break;
            }
        }
    } else {
        m_currentIndex = (std::clamp)(
            previousCurrentIndex,
            0,
            static_cast<int>(m_sessions.size() - 1));
    }
    const QString currentSessionId = currentSession() ? currentSession()->id : QString();
    const bool selectedSessionChanged = currentSessionId != previousCurrentSessionId;
    if (selectedSessionChanged) {
        emit currentSessionHistoryReloadRequested();
    }
    emit sessionsChanged();
    if (m_currentIndex != previousCurrentIndex) {
        emit currentSessionIndexChanged();
    }
    if (selectedSessionChanged) {
        emit currentSessionChanged();
    }
    return true;
}

void SessionService::duplicateSessionAt(int index)
{
    if (!isValidIndex(index)) {
        return;
    }

    const SessionState &source = m_sessions.at(index);
    const SessionConnectionConfig config = SessionSettingsStore::duplicateConfigFromState(source);

    SessionState session = createDefaultSession(tr("%1 Copy").arg(source.name));
    applyConfig(session, config);
    session.outputPaused = source.outputPaused;
    session.capturePolicy = source.capturePolicy;
    session.runtime.subscriptionFormats = source.runtime.subscriptionFormats;
    session.subscriptions = source.subscriptions;
    for (auto &subscription : session.subscriptions) {
        subscription.runtimeSubscription.clear();
        subscription.runtimeState = QStringLiteral("saved");
        subscription.grantedQos = -1;
        subscription.lastError.clear();
        subscription.recentMessages.clear();
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

    if (m_preferences.deleteHistoryWithSession()
        && m_messageParser
        && !m_messageParser->drain()) {
        emit storageError(tr("Cannot delete session history: queued message parsing timed out."));
        return;
    }

    if (m_preferences.deleteHistoryWithSession()
        && m_historyWriter
        && !m_historyWriter->drain()) {
        const QString error = m_historyWriter->lastError().isEmpty()
            ? tr("Timed out while saving queued messages.")
            : m_historyWriter->lastError();
        emit storageError(tr("Cannot delete session history: %1").arg(error));
        return;
    }

    SessionState removed = m_sessions.takeAt(index);
    if (m_preferences.deleteHistoryWithSession()
        && !m_historyStore.clearSessionHistory(removed.id)) {
        emit storageError(
            tr("Cannot delete session history: %1").arg(m_historyStore.lastError()));
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

void SessionService::setHistoryWriter(HistoryWriterWorker *historyWriter)
{
    m_historyWriter = historyWriter;
}

void SessionService::setMessageParser(MessageParseWorker *messageParser)
{
    m_messageParser = messageParser;
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

void SessionService::requestReconnect(SessionState &session)
{
    auto *client = session.runtime.client;
    if (!client) {
        return;
    }
    if (client->state() != QMqttClient::Disconnected) {
        session.runtime.reconnectPending = true;
        return;
    }

    session.runtime.reconnectPending = false;
    session.runtime.disconnectRequested = false;
    emit reconnectRequested(&session);
}

void SessionService::applyConfig(
    SessionState &session,
    const SessionConnectionConfig &config) const
{
    auto *client = session.runtime.client;
    if (!client) {
        return;
    }

    const QString name = config.name.trimmed();
    if (!name.isEmpty()) {
        session.name = name;
    }

    session.transport = SessionConfig::sanitizeTransport(config.transport);
    session.protocolVersion = SessionConfig::sanitizeProtocolVersion(config.protocolVersion);
    session.sslSecure = config.sslSecure;
    session.alpn = config.alpn.trimmed();
    session.certificateType = config.certificateType == QStringLiteral("self")
        ? QStringLiteral("self")
        : QStringLiteral("ca");
    session.caFile = config.caFile.trimmed();
    session.clientCertificateFile = config.clientCertificateFile.trimmed();
    session.clientKeyFile = config.clientKeyFile.trimmed();
    session.connectTimeoutSeconds = SessionConfig::sanitizeBoundedInt(
        config.connectTimeoutSeconds,
        10,
        1,
        300);
    session.sessionExpiryInterval = config.sessionExpiryInterval;
    session.receiveMaximum = config.receiveMaximum;
    session.maximumPacketSize = config.maximumPacketSize;
    session.topicAliasMaximum = config.topicAliasMaximum;
    session.requestResponseInformation = config.requestResponseInformation;
    session.requestProblemInformation = config.requestProblemInformation;
    session.authenticationMethod = config.authenticationMethod.trimmed();
    session.authenticationData = config.authenticationData;

    QString host = config.host.trimmed();
    if (host.isEmpty()) {
        host = QStringLiteral("broker.emqx.io");
    }

    client->setHostname(host);
    client->setPort(SessionConfig::sanitizePort(config.port, session.transport));
    client->setProtocolVersion(toProtocolVersion(session.protocolVersion));

    QString clientId = config.clientId.trimmed();
    if (clientId.isEmpty()) {
        clientId = SessionConfig::generateClientId();
    }
    client->setClientId(clientId);
    client->setUsername(config.username);
    client->setPassword(config.password);
    client->setCleanSession(config.cleanSession);
    client->setKeepAlive(SessionConfig::sanitizeKeepAlive(config.keepAliveSeconds));
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

    SessionConnectionConfig config = SessionConfig::defaultConfig(1);
    config.name = name;
    applyConfig(session, config);
    return session;
}
