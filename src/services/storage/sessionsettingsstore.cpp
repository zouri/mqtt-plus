#include "sessionsettingsstore.h"

#include "domain/sessionconfig.h"

#include <QCoreApplication>
#include <QCborParserError>
#include <QCborValue>
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
        row.insert(QStringLiteral("noLocal"), entry.options.noLocal);
        row.insert(QStringLiteral("subscriptionIdentifier"), entry.options.subscriptionIdentifier);
        row.insert(
            QStringLiteral("userProperties"),
            mqttUserPropertiesToVariantList(entry.options.userProperties));
        QVariantMap processor;
        processor.insert(QStringLiteral("processorId"), entry.processor.processorId);
        processor.insert(
            QStringLiteral("parametersCborBase64"),
            QString::fromLatin1(QCborValue(entry.processor.parameters).toCbor().toBase64()));
        row.insert(QStringLiteral("processor"), processor);
        row.insert(QStringLiteral("color"), entry.color);
        row.insert(QStringLiteral("paused"), entry.paused);
        rows.append(row);
    }
    return rows;
}

QVariantMap lastWillToVariantMap(const MqttLastWillConfig &will)
{
    return {
        {QStringLiteral("enabled"), will.enabled},
        {QStringLiteral("topic"), will.topic},
        {QStringLiteral("payload"), will.payload},
        {QStringLiteral("payloadFormat"), will.payloadFormat},
        {QStringLiteral("qos"), will.qos},
        {QStringLiteral("retain"), will.retain},
        {QStringLiteral("delayInterval"), will.properties.delayInterval},
        {QStringLiteral("payloadFormatIndicatorSet"), will.properties.payloadFormatIndicator.has_value()},
        {QStringLiteral("payloadFormatUtf8"),
         will.properties.payloadFormatIndicator == MqttPayloadFormatIndicator::Utf8},
        {QStringLiteral("messageExpiryIntervalSet"), will.properties.messageExpiryInterval.has_value()},
        {QStringLiteral("messageExpiryInterval"), will.properties.messageExpiryInterval.value_or(0)},
        {QStringLiteral("contentType"), will.properties.contentType},
        {QStringLiteral("responseTopic"), will.properties.responseTopic},
        {QStringLiteral("correlationDataBase64"), QString::fromLatin1(will.properties.correlationData.toBase64())},
        {QStringLiteral("userProperties"), mqttUserPropertiesToVariantList(will.properties.userProperties)},
    };
}

MqttLastWillConfig lastWillFromVariantMap(const QVariantMap &row)
{
    MqttLastWillConfig will;
    will.enabled = row.value(QStringLiteral("enabled"), false).toBool();
    will.topic = row.value(QStringLiteral("topic")).toString();
    will.payload = row.value(QStringLiteral("payload")).toString();
    will.payloadFormat = row.value(QStringLiteral("payloadFormat"), 0).toInt();
    will.qos = SessionConfig::sanitizeQos(row.value(QStringLiteral("qos"), 0).toInt());
    will.retain = row.value(QStringLiteral("retain"), false).toBool();
    will.properties.delayInterval = row.value(QStringLiteral("delayInterval"), 0).toUInt();
    if (row.value(QStringLiteral("payloadFormatIndicatorSet"), false).toBool()) {
        will.properties.payloadFormatIndicator = row.value(
                                                         QStringLiteral("payloadFormatUtf8"),
                                                         false)
                                                         .toBool()
            ? MqttPayloadFormatIndicator::Utf8
            : MqttPayloadFormatIndicator::Unspecified;
    }
    if (row.value(QStringLiteral("messageExpiryIntervalSet"), false).toBool()) {
        will.properties.messageExpiryInterval = row.value(
            QStringLiteral("messageExpiryInterval"), 0).toUInt();
    }
    will.properties.contentType = row.value(QStringLiteral("contentType")).toString();
    will.properties.responseTopic = row.value(QStringLiteral("responseTopic")).toString();
    will.properties.correlationData = QByteArray::fromBase64(
        row.value(QStringLiteral("correlationDataBase64")).toString().toLatin1());
    will.properties.userProperties = mqttUserPropertiesFromVariantList(
        row.value(QStringLiteral("userProperties")).toList());
    return will;
}

} // namespace

namespace SessionSettingsStore {

SessionConnectionConfig configFromState(const SessionState &session)
{
    const auto *client = session.runtime.client;
    SessionConnectionConfig config;
    config.name = session.name;
    config.host = client ? client->hostname() : QString();
    config.port = client ? client->port() : SessionConfig::kDefaultPort;
    config.transport = session.transport;
    config.webSocketPath = session.webSocketPath;
    config.protocolVersion = session.protocolVersion;
    config.sslSecure = session.sslSecure;
    config.alpn = session.alpn;
    config.certificateType = session.certificateType;
    config.caFile = session.caFile;
    config.clientCertificateFile = session.clientCertificateFile;
    config.clientKeyFile = session.clientKeyFile;
    config.clientId = client ? client->clientId() : QString();
    config.username = client ? client->username() : QString();
    config.password = client ? client->password() : QString();
    config.cleanSession = client ? client->cleanSession() : true;
    config.keepAliveSeconds = client ? client->keepAlive() : SessionConfig::kDefaultKeepAlive;
    config.connectTimeoutSeconds = session.connectTimeoutSeconds;
    config.sessionExpiryInterval = session.sessionExpiryInterval;
    config.receiveMaximum = session.receiveMaximum;
    config.maximumPacketSize = session.maximumPacketSize;
    config.topicAliasMaximum = session.topicAliasMaximum;
    config.requestResponseInformation = session.requestResponseInformation;
    config.requestProblemInformation = session.requestProblemInformation;
    config.authenticationMethod = session.authenticationMethod;
    config.authenticationData = session.authenticationData;
    config.userProperties = session.userProperties;
    config.lastWill = session.lastWill;
    return config;
}

SessionConnectionConfig duplicateConfigFromState(const SessionState &session)
{
    SessionConnectionConfig config = configFromState(session);
    config.name = QCoreApplication::translate(
        "SessionSettingsStore",
        "%1 Copy").arg(session.name);
    config.clientId = SessionConfig::generateClientId();
    return config;
}

LoadedSession readSession(QSettings &settings, int index)
{
    settings.setArrayIndex(index);

    LoadedSession loaded;
    auto &session = loaded.session;
    session.id = settings.value(QStringLiteral("id")).toString();
    session.name = settings.value(QStringLiteral("name")).toString();

    session.outputPaused = settings.value(QStringLiteral("outputPaused"), false).toBool();
    session.capturePolicy.captureIncoming = settings.value(
                                                        QStringLiteral("captureIncoming"),
                                                        true)
                                                .toBool();
    session.capturePolicy.captureOutgoing = settings.value(
                                                        QStringLiteral("captureOutgoing"),
                                                        true)
                                                .toBool();
    session.capturePolicy.includeTopicFilters = settings.value(
                                                           QStringLiteral("captureIncludeTopicFilters"))
                                                       .toStringList();
    session.capturePolicy.excludeTopicFilters = settings.value(
                                                           QStringLiteral("captureExcludeTopicFilters"))
                                                       .toStringList();
    session.capturePolicy = session.capturePolicy.normalized();
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
        entry.options.noLocal = row.value(QStringLiteral("noLocal"), false).toBool();
        entry.options.subscriptionIdentifier = SessionConfig::sanitizeSubscriptionIdentifier(
            row.value(QStringLiteral("subscriptionIdentifier"), 0));
        entry.options.userProperties = mqttUserPropertiesFromVariantList(
            row.value(QStringLiteral("userProperties")).toList());
        const QVariantMap processor = row.value(QStringLiteral("processor")).toMap();
        entry.processor.processorId = processor.value(QStringLiteral("processorId")).toString().trimmed();
        QCborParserError parserError;
        const QByteArray parameterBytes = QByteArray::fromBase64(
            processor.value(QStringLiteral("parametersCborBase64")).toString().toLatin1());
        const QCborValue parameters = QCborValue::fromCbor(parameterBytes, &parserError);
        if (parserError.error == QCborError::NoError && parameters.isMap()) {
            entry.processor.parameters = parameters.toMap();
        }
        entry.color = row.value(QStringLiteral("color")).toString().trimmed();
        entry.paused = row.value(QStringLiteral("paused"), false).toBool();
        session.subscriptions.append(entry);
        session.runtime.subscriptionFormats.insert(topic, entry.format);
    }

    loaded.config.name = session.name;
    loaded.config.host = settings.value(
                                     QStringLiteral("host"),
                                     QStringLiteral("broker.emqx.io"))
                             .toString();
    loaded.config.port = SessionConfig::sanitizePort(
        settings.value(QStringLiteral("port")),
        session.transport);
    loaded.config.transport = session.transport;
    loaded.config.webSocketPath = SessionConfig::sanitizeWebSocketPath(
        settings.value(QStringLiteral("webSocketPath"), QStringLiteral("/mqtt")));
    loaded.config.protocolVersion = session.protocolVersion;
    loaded.config.sslSecure = settings.value(QStringLiteral("sslSecure"), true).toBool();
    loaded.config.alpn = settings.value(QStringLiteral("alpn")).toString();
    loaded.config.certificateType = settings.value(
                                                QStringLiteral("certificateType"),
                                                QStringLiteral("ca"))
                                        .toString();
    loaded.config.caFile = settings.value(QStringLiteral("caFile")).toString();
    loaded.config.clientCertificateFile = settings.value(
                                                      QStringLiteral("clientCertificateFile"))
                                              .toString();
    loaded.config.clientKeyFile = settings.value(QStringLiteral("clientKeyFile")).toString();
    loaded.config.clientId = settings.value(
                                         QStringLiteral("clientId"),
                                         SessionConfig::generateClientId())
                                 .toString();
    loaded.config.username = settings.value(QStringLiteral("username")).toString();
    loaded.config.password = settings.value(QStringLiteral("password")).toString();
    loaded.config.cleanSession = settings.value(QStringLiteral("cleanSession"), true).toBool();
    loaded.config.keepAliveSeconds = SessionConfig::sanitizeKeepAlive(
        settings.value(
            QStringLiteral("keepAliveSeconds"),
            SessionConfig::kDefaultKeepAlive));
    loaded.config.connectTimeoutSeconds = SessionConfig::sanitizeBoundedInt(
        settings.value(QStringLiteral("connectTimeoutSeconds"), 10),
        10,
        1,
        300);
    loaded.config.sessionExpiryInterval = SessionConfig::sanitizeOptionalUInt32(
        settings.value(QStringLiteral("sessionExpiryInterval"), 0));
    loaded.config.receiveMaximum = SessionConfig::sanitizeOptionalUInt16(
        settings.value(QStringLiteral("receiveMaximum")));
    loaded.config.maximumPacketSize = SessionConfig::sanitizeOptionalUInt32(
        settings.value(QStringLiteral("maximumPacketSize")));
    loaded.config.topicAliasMaximum = SessionConfig::sanitizeOptionalUInt16(
        settings.value(QStringLiteral("topicAliasMaximum")));
    loaded.config.requestResponseInformation = settings.value(
                                                            QStringLiteral("requestResponseInformation"),
                                                            false)
                                                    .toBool();
    loaded.config.requestProblemInformation = settings.value(
                                                           QStringLiteral("requestProblemInformation"),
                                                           false)
                                                   .toBool();
    loaded.config.authenticationMethod = settings.value(
                                                       QStringLiteral("authenticationMethod"))
                                               .toString();
    loaded.config.authenticationData = settings.value(
                                                     QStringLiteral("authenticationData"))
                                             .toString();
    loaded.config.userProperties = mqttUserPropertiesFromVariantList(
        settings.value(QStringLiteral("userProperties")).toList());
    loaded.config.lastWill = lastWillFromVariantMap(
        settings.value(QStringLiteral("lastWill")).toMap());
    return loaded;
}

bool writeSessions(QSettings &settings, const QVector<SessionState> &sessions, QString &errorMessage)
{
    errorMessage.clear();
    settings.remove(QStringLiteral("sessions"));
    settings.beginWriteArray(QStringLiteral("sessions"), sessions.size());
    for (int i = 0; i < sessions.size(); ++i) {
        const auto &session = sessions.at(i);
        const QMqttClient *client = session.runtime.client;
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("id"), session.id);
        settings.setValue(QStringLiteral("name"), session.name);
        settings.setValue(QStringLiteral("host"), client ? client->hostname() : QString());
        settings.setValue(QStringLiteral("port"), client ? client->port() : SessionConfig::kDefaultPort);
        settings.setValue(QStringLiteral("transport"), session.transport);
        settings.setValue(QStringLiteral("webSocketPath"), session.webSocketPath);
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
        settings.setValue(
            QStringLiteral("userProperties"),
            mqttUserPropertiesToVariantList(session.userProperties));
        settings.setValue(QStringLiteral("lastWill"), lastWillToVariantMap(session.lastWill));
        settings.setValue(QStringLiteral("outputPaused"), session.outputPaused);
        settings.setValue(
            QStringLiteral("captureIncoming"),
            session.capturePolicy.captureIncoming);
        settings.setValue(
            QStringLiteral("captureOutgoing"),
            session.capturePolicy.captureOutgoing);
        settings.setValue(
            QStringLiteral("captureIncludeTopicFilters"),
            session.capturePolicy.includeTopicFilters);
        settings.setValue(
            QStringLiteral("captureExcludeTopicFilters"),
            session.capturePolicy.excludeTopicFilters);
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
