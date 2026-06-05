#include "sessionsettingsstore.h"

#include "domain/sessionconfig.h"

#include <QCoreApplication>
#include <QMqttClient>
#include <QUuid>

namespace {
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

QVariantList subscriptionsToVariantList(const QVector<SubscriptionEntry> &subscriptions)
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
    const SessionState &session,
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
} // namespace

namespace SessionSettingsStore {

QVariantMap configFromState(const SessionState &session)
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

QVariantMap duplicateConfigFromState(const SessionState &session)
{
    QVariantMap config = configFromState(session);
    config.insert(
        QStringLiteral("name"),
        QCoreApplication::translate("SessionSettingsStore", "%1 Copy").arg(session.name));
    config.insert(QStringLiteral("clientId"), SessionConfig::generateClientId());
    return config;
}

LoadedSession readSession(
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
        session.name = QCoreApplication::translate("SessionSettingsStore", "Session %1").arg(index + 1);
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

        SubscriptionEntry entry;
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

bool writeSessions(QSettings &settings, const QVector<SessionState> &sessions, QString &errorMessage)
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
        ? QCoreApplication::translate("SessionSettingsStore", "Cannot write session settings: access denied.")
        : QCoreApplication::translate("SessionSettingsStore", "Cannot write session settings: invalid settings format.");
    return false;
}

} // namespace SessionSettingsStore
