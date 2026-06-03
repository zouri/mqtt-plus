#include "appcontroller.h"

#include "appcontrollerutils.h"
#include "sessionconfig.h"

#include <QMqttConnectionProperties>
#include <QUuid>

#include <functional>

namespace {
struct LoadedSession {
    AppController::SessionState session;
    QVariantMap config;
};

QString optionalUInt16ToString(quint16 value)
{
    return value > 0 ? QString::number(value) : QString();
}

QString optionalUInt32ToString(quint32 value)
{
    return value > 0 ? QString::number(value) : QString();
}

QVariantMap baseSessionConfig(
    const QString &name,
    const QVariant &host,
    const QVariant &port,
    const QString &transport,
    int protocolVersion)
{
    QVariantMap config;
    config.insert(QStringLiteral("name"), name);
    config.insert(QStringLiteral("host"), host);
    config.insert(QStringLiteral("port"), port);
    config.insert(QStringLiteral("transport"), transport);
    config.insert(QStringLiteral("protocolVersion"), protocolVersion);
    return config;
}

QVariantList subscriptionsToVariantList(const QVector<AppController::SubscriptionEntry> &subscriptions)
{
    QVariantList rows;
    rows.reserve(subscriptions.size());
    for (const auto &entry : subscriptions) {
        QVariantMap row;
        row.insert(QStringLiteral("topic"), entry.topic);
        row.insert(QStringLiteral("alias"), entry.alias);
        row.insert(QStringLiteral("qos"), entry.requestedQos);
        row.insert(QStringLiteral("format"), entry.format);
        row.insert(QStringLiteral("scriptId"), entry.scriptId);
        row.insert(QStringLiteral("paused"), entry.paused);
        rows.append(row);
    }
    return rows;
}

void addSessionDetailsToConfig(
    QVariantMap &config,
    const AppController::SessionState &session,
    const QMqttClient *client)
{
    config.insert(QStringLiteral("sslSecure"), session.sslSecure);
    config.insert(QStringLiteral("alpn"), session.alpn);
    config.insert(QStringLiteral("certificateType"), session.certificateType);
    config.insert(QStringLiteral("caFile"), session.caFile);
    config.insert(QStringLiteral("clientCertificateFile"), session.clientCertificateFile);
    config.insert(QStringLiteral("clientKeyFile"), session.clientKeyFile);
    config.insert(QStringLiteral("clientId"), client ? client->clientId() : QString());
    config.insert(QStringLiteral("username"), client ? client->username() : QString());
    config.insert(QStringLiteral("password"), client ? client->password() : QString());
    config.insert(QStringLiteral("cleanSession"), client ? client->cleanSession() : true);
    config.insert(
        QStringLiteral("keepAliveSeconds"),
        client ? client->keepAlive() : SessionConfig::kDefaultKeepAlive);
    config.insert(QStringLiteral("connectTimeoutSeconds"), session.connectTimeoutSeconds);
    config.insert(QStringLiteral("sessionExpiryInterval"), session.sessionExpiryInterval);
    config.insert(QStringLiteral("receiveMaximum"), optionalUInt16ToString(session.receiveMaximum));
    config.insert(QStringLiteral("maximumPacketSize"), optionalUInt32ToString(session.maximumPacketSize));
    config.insert(QStringLiteral("topicAliasMaximum"), optionalUInt16ToString(session.topicAliasMaximum));
    config.insert(QStringLiteral("requestResponseInformation"), session.requestResponseInformation);
    config.insert(QStringLiteral("requestProblemInformation"), session.requestProblemInformation);
    config.insert(QStringLiteral("authenticationMethod"), session.authenticationMethod);
    config.insert(QStringLiteral("authenticationData"), session.authenticationData);
}

QVariantMap sessionConfigFromState(const AppController::SessionState &session)
{
    const auto *client = session.client;
    QVariantMap config = baseSessionConfig(
        session.name,
        client ? client->hostname() : QString(),
        client ? client->port() : SessionConfig::kDefaultPort,
        session.transport,
        session.protocolVersion);
    addSessionDetailsToConfig(config, session, client);
    return config;
}

QVariantMap duplicateSessionConfigFromState(const AppController::SessionState &session)
{
    QVariantMap config = sessionConfigFromState(session);
    config.insert(QStringLiteral("name"), QStringLiteral("%1 Copy").arg(session.name));
    config.insert(QStringLiteral("clientId"), SessionConfig::generateClientId());
    return config;
}

LoadedSession readSessionSettings(
    QSettings &settings,
    int index,
    const std::function<bool(const QString &)> &scriptExists)
{
    settings.setArrayIndex(index);

    LoadedSession loaded;
    auto &session = loaded.session;
    session.id = settings.value(QStringLiteral("id")).toString();
    session.name = settings.value(QStringLiteral("name")).toString();
    if (session.id.isEmpty()) {
        session.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    if (session.name.isEmpty()) {
        session.name = QStringLiteral("Session %1").arg(index + 1);
    }

    session.outputPaused = settings.value(QStringLiteral("outputPaused"), false).toBool();
    session.transport = SessionConfig::sanitizeTransport(settings.value(QStringLiteral("transport"), QStringLiteral("tcp")));
    session.protocolVersion = SessionConfig::sanitizeProtocolVersion(settings.value(QStringLiteral("protocolVersion"), 5));

    const QVariantList subscriptions = settings.value(QStringLiteral("subscriptions")).toList();
    for (const QVariant &item : subscriptions) {
        const QVariantMap row = item.toMap();
        const QString topic = row.value(QStringLiteral("topic")).toString().trimmed();
        if (topic.isEmpty()) {
            continue;
        }

        AppController::SubscriptionEntry entry;
        entry.topic = topic;
        entry.alias = row.value(QStringLiteral("alias")).toString().trimmed();
        entry.requestedQos = SessionConfig::sanitizeQos(row.value(QStringLiteral("qos"), 0).toInt());
        entry.format = row.value(QStringLiteral("format"), 0).toInt();
        const QString scriptId = row.value(QStringLiteral("scriptId")).toString();
        entry.scriptId = scriptExists(scriptId) ? scriptId : QString();
        entry.paused = row.value(QStringLiteral("paused"), false).toBool();
        session.subscriptions.append(entry);
        session.subscriptionFormats.insert(topic, entry.format);
    }

    loaded.config = baseSessionConfig(
        session.name,
        settings.value(QStringLiteral("host"), QStringLiteral("broker.emqx.io")),
        settings.value(QStringLiteral("port"), SessionConfig::sanitizePort(QVariant(), session.transport)),
        session.transport,
        session.protocolVersion);
    loaded.config.insert(QStringLiteral("sslSecure"), settings.value(QStringLiteral("sslSecure"), true).toBool());
    loaded.config.insert(QStringLiteral("alpn"), settings.value(QStringLiteral("alpn")).toString());
    loaded.config.insert(QStringLiteral("certificateType"), settings.value(QStringLiteral("certificateType"), QStringLiteral("ca")).toString());
    loaded.config.insert(QStringLiteral("caFile"), settings.value(QStringLiteral("caFile")).toString());
    loaded.config.insert(QStringLiteral("clientCertificateFile"), settings.value(QStringLiteral("clientCertificateFile")).toString());
    loaded.config.insert(QStringLiteral("clientKeyFile"), settings.value(QStringLiteral("clientKeyFile")).toString());
    loaded.config.insert(QStringLiteral("clientId"), settings.value(QStringLiteral("clientId"), SessionConfig::generateClientId()));
    loaded.config.insert(QStringLiteral("username"), settings.value(QStringLiteral("username")).toString());
    loaded.config.insert(QStringLiteral("password"), settings.value(QStringLiteral("password")).toString());
    loaded.config.insert(QStringLiteral("cleanSession"), settings.value(QStringLiteral("cleanSession"), true).toBool());
    loaded.config.insert(QStringLiteral("keepAliveSeconds"), settings.value(QStringLiteral("keepAliveSeconds"), SessionConfig::kDefaultKeepAlive));
    loaded.config.insert(QStringLiteral("connectTimeoutSeconds"), settings.value(QStringLiteral("connectTimeoutSeconds"), 10));
    loaded.config.insert(QStringLiteral("sessionExpiryInterval"), settings.value(QStringLiteral("sessionExpiryInterval"), 0));
    loaded.config.insert(QStringLiteral("receiveMaximum"), settings.value(QStringLiteral("receiveMaximum")).toString());
    loaded.config.insert(QStringLiteral("maximumPacketSize"), settings.value(QStringLiteral("maximumPacketSize")).toString());
    loaded.config.insert(QStringLiteral("topicAliasMaximum"), settings.value(QStringLiteral("topicAliasMaximum")).toString());
    loaded.config.insert(QStringLiteral("requestResponseInformation"), settings.value(QStringLiteral("requestResponseInformation"), false).toBool());
    loaded.config.insert(QStringLiteral("requestProblemInformation"), settings.value(QStringLiteral("requestProblemInformation"), false).toBool());
    loaded.config.insert(QStringLiteral("authenticationMethod"), settings.value(QStringLiteral("authenticationMethod")).toString());
    loaded.config.insert(QStringLiteral("authenticationData"), settings.value(QStringLiteral("authenticationData")).toString());
    return loaded;
}

bool writeSessionSettings(
    QSettings &settings,
    const QVector<AppController::SessionState> &sessions,
    QString &errorMessage)
{
    errorMessage.clear();
    settings.beginWriteArray(QStringLiteral("sessions"), sessions.size());
    for (int i = 0; i < sessions.size(); ++i) {
        const auto &session = sessions.at(i);
        const QMqttClient *client = session.client;
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("id"), session.id);
        settings.setValue(QStringLiteral("name"), session.name);
        settings.setValue(QStringLiteral("host"), client ? client->hostname() : QString());
        settings.setValue(QStringLiteral("port"), client ? client->port() : SessionConfig::kDefaultPort);
        settings.setValue(QStringLiteral("transport"), session.transport);
        settings.setValue(QStringLiteral("protocolVersion"), session.protocolVersion);
        settings.setValue(QStringLiteral("sslSecure"), session.sslSecure);
        settings.setValue(QStringLiteral("alpn"), session.alpn);
        settings.setValue(QStringLiteral("certificateType"), session.certificateType);
        settings.setValue(QStringLiteral("caFile"), session.caFile);
        settings.setValue(QStringLiteral("clientCertificateFile"), session.clientCertificateFile);
        settings.setValue(QStringLiteral("clientKeyFile"), session.clientKeyFile);
        settings.setValue(QStringLiteral("clientId"), client ? client->clientId() : QString());
        settings.setValue(QStringLiteral("username"), client ? client->username() : QString());
        settings.setValue(QStringLiteral("password"), client ? client->password() : QString());
        settings.setValue(QStringLiteral("cleanSession"), client ? client->cleanSession() : true);
        settings.setValue(QStringLiteral("keepAliveSeconds"), client ? client->keepAlive() : SessionConfig::kDefaultKeepAlive);
        settings.setValue(QStringLiteral("connectTimeoutSeconds"), session.connectTimeoutSeconds);
        settings.setValue(QStringLiteral("sessionExpiryInterval"), session.sessionExpiryInterval);
        settings.setValue(QStringLiteral("receiveMaximum"), optionalUInt16ToString(session.receiveMaximum));
        settings.setValue(QStringLiteral("maximumPacketSize"), optionalUInt32ToString(session.maximumPacketSize));
        settings.setValue(QStringLiteral("topicAliasMaximum"), optionalUInt16ToString(session.topicAliasMaximum));
        settings.setValue(QStringLiteral("requestResponseInformation"), session.requestResponseInformation);
        settings.setValue(QStringLiteral("requestProblemInformation"), session.requestProblemInformation);
        settings.setValue(QStringLiteral("authenticationMethod"), session.authenticationMethod);
        settings.setValue(QStringLiteral("authenticationData"), session.authenticationData);
        settings.setValue(QStringLiteral("outputPaused"), session.outputPaused);
        settings.setValue(QStringLiteral("subscriptions"), subscriptionsToVariantList(session.subscriptions));
    }
    settings.endArray();
    settings.sync();
    if (settings.status() == QSettings::NoError) {
        return true;
    }

    errorMessage = settings.status() == QSettings::AccessError
        ? QStringLiteral("Cannot write session settings: access denied.")
        : QStringLiteral("Cannot write session settings: invalid settings format.");
    return false;
}
}

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
    notifySelectedSessionViewsChanged();
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

    const auto &session = m_sessions.at(index);
    return sessionConfigFromState(session);
}

bool AppController::updateSessionConfigAt(int index, const QVariantMap &config)
{
    if (index < 0 || index >= m_sessions.size()) {
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

    configureSession(*session, config, true);
    session->lastError.clear();
    session->sessionRestored = false;
    updatePublishStatus(*session, QStringLiteral("idle"));
    const bool saved = saveSessions();

    if (reconnect) {
        session->disconnectRequested = false;
        connectSession(*session, QStringLiteral("Connecting to"));
    }

    emit sessionsChanged();
    if (index == m_currentSessionIndex) {
        notifyCurrentSessionAndSubscriptionsChanged();
    }
    return saved;
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
    notifySessionCollectionViewsChanged();
}

void AppController::duplicateSessionAt(int index)
{
    if (index < 0 || index >= m_sessions.size()) {
        return;
    }

    const auto &source = m_sessions.at(index);
    const QVariantMap config = duplicateSessionConfigFromState(source);

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
    notifySessionCollectionViewsChanged();
}

void AppController::removeSessionAt(int index)
{
    if (m_sessions.size() <= 1 || index < 0 || index >= m_sessions.size()) {
        return;
    }

    SessionState removed = m_sessions.takeAt(index);
    destroySessionRuntime(removed);

    if (m_currentSessionIndex >= m_sessions.size()) {
        m_currentSessionIndex = m_sessions.size() - 1;
    }
    if (m_currentSessionIndex > index) {
        --m_currentSessionIndex;
    }

    reloadCurrentSessionHistory();
    saveSessions();
    notifySessionCollectionViewsChanged();
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
    notifyCurrentSessionViewsChanged();
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
    auto *client = session.client;
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
    client->setConnectionProperties(connectionProperties);
}

void AppController::initializeSessionRuntime(SessionState *session)
{
    if (!session) {
        return;
    }

    if (!session->client) {
        session->client = new QMqttClient(this);
        session->client->setAutoKeepAlive(true);
    }
    if (!session->connectTimeoutTimer) {
        session->connectTimeoutTimer = new QTimer(this);
        session->connectTimeoutTimer->setSingleShot(true);
        connect(session->connectTimeoutTimer, &QTimer::timeout, this, [this, sessionId = session->id]() {
            auto *boundSession = sessionById(sessionId);
            auto *client = boundSession ? boundSession->client : nullptr;
            if (!boundSession || !client || client->state() != QMqttClient::Connecting) {
                return;
            }

            boundSession->lastError = QStringLiteral("Connection timed out.");
            appendEvent(*boundSession, QStringLiteral("Error"), boundSession->lastError);
            client->disconnectFromHost();
            notifySessionViewsChanged();
        });
    }
}

void AppController::destroySessionRuntime(SessionState &session)
{
    if (session.connectTimeoutTimer) {
        session.connectTimeoutTimer->stop();
        session.connectTimeoutTimer->deleteLater();
        session.connectTimeoutTimer = nullptr;
    }
    if (session.client) {
        session.client->disconnectFromHost();
        session.client->deleteLater();
        session.client = nullptr;
    }
}

void AppController::loadSessions()
{
    const int count = m_settings.beginReadArray(QStringLiteral("sessions"));
    for (int i = 0; i < count; ++i) {
        LoadedSession loaded = readSessionSettings(
            m_settings,
            i,
            [this](const QString &scriptId) { return scriptById(scriptId) != nullptr; });
        SessionState session = loaded.session;

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
    notifySessionCollectionViewsChanged();
}

bool AppController::saveSessions()
{
    QString errorMessage;
    if (writeSessionSettings(m_settings, m_sessions, errorMessage)) {
        return true;
    }
    reportStorageError(errorMessage.isEmpty() ? QStringLiteral("Cannot save sessions.") : errorMessage);
    return false;
}

void AppController::reportStorageError(const QString &message)
{
    if (message.isEmpty()) {
        return;
    }

    if (auto *session = currentSessionState()) {
        session->lastError = message;
        appendEvent(*session, QStringLiteral("Storage"), message);
    }

    notifySessionViewsChanged();
}

AppController::SessionState AppController::createDefaultSession(const QString &name)
{
    SessionState session;
    session.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    session.name = name;
    session.transport = QStringLiteral("tcp");
    session.protocolVersion = 5;
    session.publishStatus = defaultPublishStatus();
    initializeSessionRuntime(&session);
    QVariantMap config = SessionConfig::defaultConfig(1);
    config.insert(QStringLiteral("name"), name);
    configureSession(session, config, false);
    bindSessionSignals(&session);
    return session;
}
