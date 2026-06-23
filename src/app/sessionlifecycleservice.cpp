#include "app/sessionlifecycleservice.h"

#include "app/appfacadeutils.h"
#include "controllers/sessioncontroller.h"
#include "domain/sessionconfig.h"
#include "services/storage/sessionsettingsstore.h"

#include <QMqttConnectionProperties>
#include <QUuid>

#include <utility>

using namespace AppFacadeUtils;

SessionLifecycleService::SessionLifecycleService(Dependencies dependencies, QObject *parent)
    : QObject(parent)
    , m_dependencies(std::move(dependencies))
{
}

void SessionLifecycleService::configureSession(
    SessionState &session,
    const QVariantMap &config,
    bool keepNameFallback)
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

void SessionLifecycleService::initializeSessionRuntime(SessionState *session)
{
    if (!session) {
        return;
    }

    if (!session->client) {
        session->client = new QMqttClient(m_dependencies.runtimeParent ? m_dependencies.runtimeParent : this);
        session->client->setAutoKeepAlive(true);
    }
    if (!session->connectTimeoutTimer) {
        session->connectTimeoutTimer = new QTimer(m_dependencies.runtimeParent ? m_dependencies.runtimeParent : this);
        session->connectTimeoutTimer->setSingleShot(true);
        connect(session->connectTimeoutTimer, &QTimer::timeout, this, [this, sessionId = session->id]() {
            auto *boundSession = m_dependencies.sessionById ? m_dependencies.sessionById(sessionId) : nullptr;
            auto *client = boundSession ? boundSession->client : nullptr;
            if (!boundSession || !client || client->state() != QMqttClient::Connecting) {
                return;
            }

            boundSession->lastError = tr("Connection timed out.");
            if (m_dependencies.appendEvent) {
                m_dependencies.appendEvent(
                    *boundSession,
                    QStringLiteral("Error"),
                    QStringLiteral("Connection timed out."));
            }
            client->disconnectFromHost();
            if (m_dependencies.publishSessionViewsChanged) {
                m_dependencies.publishSessionViewsChanged();
            }
        });
    }
}

void SessionLifecycleService::destroySessionRuntime(SessionState &session)
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

void SessionLifecycleService::loadSessions()
{
    auto *settings = m_dependencies.settings;
    auto *sessionController = m_dependencies.sessionController;
    if (!settings || !sessionController) {
        return;
    }

    const int count = settings->beginReadArray(QStringLiteral("sessions"));
    for (int i = 0; i < count; ++i) {
        SessionSettingsStore::LoadedSession loaded = SessionSettingsStore::readSession(
            *settings,
            i,
            [this](const QString &scriptId) {
                return m_dependencies.scriptExists ? m_dependencies.scriptExists(scriptId) : false;
            });
        SessionState session = loaded.session;

        initializeSessionRuntime(&session);
        configureSession(session, loaded.config, false);
        session.publishStatus = defaultPublishStatus();
        if (m_dependencies.bindSessionSignals) {
            m_dependencies.bindSessionSignals(&session);
        }
        sessionController->appendSession(session);
    }
    settings->endArray();

    if (sessionController->sessions().isEmpty()) {
        sessionController->appendSession(createDefaultSession(tr("Session 1")));
        saveSessions();
    }

    sessionController->setCurrentIndex(0);
    if (m_dependencies.reloadCurrentSessionHistory) {
        m_dependencies.reloadCurrentSessionHistory();
    }
    if (m_dependencies.publishSessionCollectionViewsChanged) {
        m_dependencies.publishSessionCollectionViewsChanged();
    }
}

bool SessionLifecycleService::saveSessions()
{
    auto *settings = m_dependencies.settings;
    auto *sessionController = m_dependencies.sessionController;
    if (!settings || !sessionController) {
        return false;
    }

    QString errorMessage;
    if (SessionSettingsStore::writeSessions(*settings, sessionController->sessions(), errorMessage)) {
        return true;
    }
    if (m_dependencies.reportStorageError) {
        m_dependencies.reportStorageError(errorMessage.isEmpty() ? QStringLiteral("Cannot save sessions.") : errorMessage);
    }
    return false;
}

SessionState SessionLifecycleService::createDefaultSession(const QString &name)
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
    if (m_dependencies.bindSessionSignals) {
        m_dependencies.bindSessionSignals(&session);
    }
    return session;
}
