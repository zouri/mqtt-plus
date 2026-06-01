#include "sessionconfig.h"

#include <QRandomGenerator>
#include <QUuid>

#include <algorithm>
#include <limits>

namespace {
QString optionalUInt16ToString(quint16 value)
{
    return value > 0 ? QString::number(value) : QString();
}

QString optionalUInt32ToString(quint32 value)
{
    return value > 0 ? QString::number(value) : QString();
}

QVariantMap baseConfig(
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

void addSessionDetailsFromState(QVariantMap &config, const AppController::SessionState &session)
{
    config.insert(QStringLiteral("sslSecure"), session.sslSecure);
    config.insert(QStringLiteral("alpn"), session.alpn);
    config.insert(QStringLiteral("certificateType"), session.certificateType);
    config.insert(QStringLiteral("caFile"), session.caFile);
    config.insert(QStringLiteral("clientCertificateFile"), session.clientCertificateFile);
    config.insert(QStringLiteral("clientKeyFile"), session.clientKeyFile);
    config.insert(QStringLiteral("clientId"), session.client ? session.client->clientId() : QString());
    config.insert(QStringLiteral("username"), session.client ? session.client->username() : QString());
    config.insert(QStringLiteral("password"), session.client ? session.client->password() : QString());
    config.insert(QStringLiteral("cleanSession"), session.client ? session.client->cleanSession() : true);
    config.insert(
        QStringLiteral("keepAliveSeconds"),
        session.client ? session.client->keepAlive() : SessionConfig::kDefaultKeepAlive);
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

void addDefaultDetails(QVariantMap &config)
{
    config.insert(QStringLiteral("sslSecure"), true);
    config.insert(QStringLiteral("alpn"), QString());
    config.insert(QStringLiteral("certificateType"), QStringLiteral("ca"));
    config.insert(QStringLiteral("caFile"), QString());
    config.insert(QStringLiteral("clientCertificateFile"), QString());
    config.insert(QStringLiteral("clientKeyFile"), QString());
    config.insert(QStringLiteral("clientId"), SessionConfig::generateClientId());
    config.insert(QStringLiteral("username"), QString());
    config.insert(QStringLiteral("password"), QString());
    config.insert(QStringLiteral("cleanSession"), true);
    config.insert(QStringLiteral("keepAliveSeconds"), SessionConfig::kDefaultKeepAlive);
    config.insert(QStringLiteral("connectTimeoutSeconds"), 10);
    config.insert(QStringLiteral("sessionExpiryInterval"), 0);
    config.insert(QStringLiteral("receiveMaximum"), QString());
    config.insert(QStringLiteral("maximumPacketSize"), QString());
    config.insert(QStringLiteral("topicAliasMaximum"), QString());
    config.insert(QStringLiteral("requestResponseInformation"), false);
    config.insert(QStringLiteral("requestProblemInformation"), false);
    config.insert(QStringLiteral("authenticationMethod"), QString());
    config.insert(QStringLiteral("authenticationData"), QString());
}
}

namespace SessionConfig {
QString generateClientId()
{
    return QStringLiteral("mqtt-plus-%1")
        .arg(QRandomGenerator::global()->bounded(100000, 999999));
}

int sanitizePort(const QVariant &value, const QString &transport)
{
    bool ok = false;
    const int parsed = value.toInt(&ok);
    if (!ok) {
        return transport == QStringLiteral("tls") ? kDefaultTlsPort : kDefaultPort;
    }
    return std::clamp(parsed, 1, 65535);
}

int sanitizeKeepAlive(const QVariant &value)
{
    bool ok = false;
    const int parsed = value.toInt(&ok);
    if (!ok) {
        return kDefaultKeepAlive;
    }
    return std::clamp(parsed, 5, 1200);
}

int sanitizeBoundedInt(const QVariant &value, int fallback, int minimum, int maximum)
{
    bool ok = false;
    const int parsed = value.toInt(&ok);
    if (!ok) {
        return fallback;
    }
    return std::clamp(parsed, minimum, maximum);
}

quint16 sanitizeOptionalUInt16(const QVariant &value)
{
    const QString text = value.toString().trimmed();
    if (text.isEmpty()) {
        return 0;
    }

    bool ok = false;
    const uint parsed = text.toUInt(&ok);
    if (!ok) {
        return 0;
    }
    return static_cast<quint16>(std::clamp<uint>(parsed, 0, 65535));
}

quint32 sanitizeOptionalUInt32(const QVariant &value)
{
    const QString text = value.toString().trimmed();
    if (text.isEmpty()) {
        return 0;
    }

    bool ok = false;
    const quint64 parsed = text.toULongLong(&ok);
    if (!ok) {
        return 0;
    }
    return static_cast<quint32>((std::min)(parsed, static_cast<quint64>((std::numeric_limits<quint32>::max)())));
}

int sanitizeQos(int qos)
{
    return std::clamp(qos, 0, 1);
}

QString sanitizeTransport(const QVariant &value)
{
    const QString transport = value.toString().trimmed().toLower();
    return transport == QStringLiteral("tls") ? QStringLiteral("tls") : QStringLiteral("tcp");
}

int sanitizeProtocolVersion(const QVariant &value)
{
    bool ok = false;
    const int parsed = value.toInt(&ok);
    return (ok && parsed == 4) ? 4 : 5;
}

QVariantMap defaultConfig(int sessionNumber)
{
    QVariantMap config = baseConfig(
        QStringLiteral("Session %1").arg(sessionNumber),
        QStringLiteral("broker.emqx.io"),
        kDefaultPort,
        QStringLiteral("tcp"),
        5);
    addDefaultDetails(config);
    return config;
}

QVariantMap configFromSession(const AppController::SessionState &session)
{
    QVariantMap config = baseConfig(
        session.name,
        session.client ? session.client->hostname() : QString(),
        session.client ? session.client->port() : kDefaultPort,
        session.transport,
        session.protocolVersion);
    addSessionDetailsFromState(config, session);
    return config;
}

QVariantMap duplicateConfigFromSession(const AppController::SessionState &session)
{
    QVariantMap config = configFromSession(session);
    config.insert(QStringLiteral("name"), QStringLiteral("%1 Copy").arg(session.name));
    config.insert(QStringLiteral("clientId"), generateClientId());
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
    session.transport = sanitizeTransport(settings.value(QStringLiteral("transport"), QStringLiteral("tcp")));
    session.protocolVersion = sanitizeProtocolVersion(settings.value(QStringLiteral("protocolVersion"), 5));

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
        entry.requestedQos = sanitizeQos(row.value(QStringLiteral("qos"), 0).toInt());
        entry.format = row.value(QStringLiteral("format"), 0).toInt();
        const QString scriptId = row.value(QStringLiteral("scriptId")).toString();
        entry.scriptId = scriptExists(scriptId) ? scriptId : QString();
        entry.paused = row.value(QStringLiteral("paused"), false).toBool();
        session.subscriptions.append(entry);
        session.subscriptionFormats.insert(topic, entry.format);
    }

    loaded.config = baseConfig(
        session.name,
        settings.value(QStringLiteral("host"), QStringLiteral("broker.emqx.io")),
        settings.value(QStringLiteral("port"), sanitizePort(QVariant(), session.transport)),
        session.transport,
        session.protocolVersion);
    loaded.config.insert(QStringLiteral("sslSecure"), settings.value(QStringLiteral("sslSecure"), true).toBool());
    loaded.config.insert(QStringLiteral("alpn"), settings.value(QStringLiteral("alpn")).toString());
    loaded.config.insert(QStringLiteral("certificateType"), settings.value(QStringLiteral("certificateType"), QStringLiteral("ca")).toString());
    loaded.config.insert(QStringLiteral("caFile"), settings.value(QStringLiteral("caFile")).toString());
    loaded.config.insert(QStringLiteral("clientCertificateFile"), settings.value(QStringLiteral("clientCertificateFile")).toString());
    loaded.config.insert(QStringLiteral("clientKeyFile"), settings.value(QStringLiteral("clientKeyFile")).toString());
    loaded.config.insert(QStringLiteral("clientId"), settings.value(QStringLiteral("clientId"), generateClientId()));
    loaded.config.insert(QStringLiteral("username"), settings.value(QStringLiteral("username")).toString());
    loaded.config.insert(QStringLiteral("password"), settings.value(QStringLiteral("password")).toString());
    loaded.config.insert(QStringLiteral("cleanSession"), settings.value(QStringLiteral("cleanSession"), true).toBool());
    loaded.config.insert(QStringLiteral("keepAliveSeconds"), settings.value(QStringLiteral("keepAliveSeconds"), kDefaultKeepAlive));
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

void writeSessionSettings(QSettings &settings, const QVector<AppController::SessionState> &sessions)
{
    settings.beginWriteArray(QStringLiteral("sessions"), sessions.size());
    for (int i = 0; i < sessions.size(); ++i) {
        const auto &session = sessions.at(i);
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("id"), session.id);
        settings.setValue(QStringLiteral("name"), session.name);
        settings.setValue(QStringLiteral("host"), session.client->hostname());
        settings.setValue(QStringLiteral("port"), session.client->port());
        settings.setValue(QStringLiteral("transport"), session.transport);
        settings.setValue(QStringLiteral("protocolVersion"), session.protocolVersion);
        settings.setValue(QStringLiteral("sslSecure"), session.sslSecure);
        settings.setValue(QStringLiteral("alpn"), session.alpn);
        settings.setValue(QStringLiteral("certificateType"), session.certificateType);
        settings.setValue(QStringLiteral("caFile"), session.caFile);
        settings.setValue(QStringLiteral("clientCertificateFile"), session.clientCertificateFile);
        settings.setValue(QStringLiteral("clientKeyFile"), session.clientKeyFile);
        settings.setValue(QStringLiteral("clientId"), session.client->clientId());
        settings.setValue(QStringLiteral("username"), session.client->username());
        settings.setValue(QStringLiteral("password"), session.client->password());
        settings.setValue(QStringLiteral("cleanSession"), session.client->cleanSession());
        settings.setValue(QStringLiteral("keepAliveSeconds"), session.client->keepAlive());
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
}
}
