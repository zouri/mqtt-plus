#include "app/applicationsessionconfigurator.h"

#include "services/apputils.h"
#include "domain/sessionconfig.h"

#include <QMqttConnectionProperties>

using namespace AppUtils;

void ApplicationSessionConfigurator::applyConfig(SessionState &session, const QVariantMap &config, bool keepNameFallback)
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
