#include "services/configuration/configurationadapters.h"

#include "domain/sessionconfig.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMqttTopicFilter>
#include <QSet>

#include <algorithm>
#include <limits>

namespace {

QString text(const char *source)
{
    return QCoreApplication::translate("ConfigurationAdapters", source);
}

int boundedInt(const QJsonValue &value, int fallback, int minimum, int maximum)
{
    if (!value.isDouble() && !value.isString()) {
        return fallback;
    }
    bool ok = false;
    const int parsed = value.toVariant().toInt(&ok);
    return ok ? std::clamp(parsed, minimum, maximum) : fallback;
}

quint16 optionalUInt16(const QJsonValue &value)
{
    if (value.isNull() || value.isUndefined()) {
        return 0;
    }
    bool ok = false;
    const uint parsed = value.toVariant().toUInt(&ok);
    return ok ? static_cast<quint16>(std::min(parsed, 65535u)) : 0;
}

quint32 optionalUInt32(const QJsonValue &value)
{
    if (value.isNull() || value.isUndefined()) {
        return 0;
    }
    bool ok = false;
    const quint64 parsed = value.toVariant().toULongLong(&ok);
    return ok
        ? static_cast<quint32>(std::min<quint64>(
              parsed,
              std::numeric_limits<quint32>::max()))
        : 0;
}

QJsonValue connectionProperty(const QJsonObject &object, const QString &name)
{
    const QJsonObject properties = object.value(QStringLiteral("properties")).toObject();
    const QJsonValue nested = properties.value(name);
    if (!nested.isUndefined() && !nested.isNull()) {
        return nested;
    }
    return object.value(name);
}

QString alpnString(const QJsonValue &value)
{
    if (value.isArray()) {
        QStringList protocols;
        for (const QJsonValue &entry : value.toArray()) {
            const QString protocol = entry.toString().trimmed();
            if (!protocol.isEmpty()) {
                protocols.append(protocol);
            }
        }
        return protocols.join(QStringLiteral(","));
    }
    return value.toString().trimmed();
}

bool jsonCollectionHasValues(const QJsonValue &value)
{
    if (value.isArray()) {
        return !value.toArray().isEmpty();
    }
    if (value.isObject()) {
        return !value.toObject().isEmpty();
    }
    return !value.isNull() && !value.isUndefined() && !value.toString().isEmpty();
}

bool mqttxWillHasValues(const QJsonObject &will)
{
    return !will.value(QStringLiteral("lastWillTopic")).toString().trimmed().isEmpty()
        || !will.value(QStringLiteral("lastWillPayload")).toString().isEmpty()
        || will.value(QStringLiteral("lastWillRetain")).toBool(false)
        || jsonCollectionHasValues(will.value(QStringLiteral("userProperties")));
}

QByteArray nativeAsset(const QJsonObject &assets, const QString &name, bool &ok)
{
    const QJsonValue value = assets.value(name);
    if (value.isUndefined() || value.isNull() || value.toString().isEmpty()) {
        return {};
    }
    const QByteArray encoded = value.toString().toLatin1();
    const QByteArray decoded = QByteArray::fromBase64(encoded, QByteArray::AbortOnBase64DecodingErrors);
    if (decoded.isNull()) {
        ok = false;
        return {};
    }
    return decoded;
}

QJsonArray stringListToJson(const QStringList &values)
{
    QJsonArray array;
    for (const QString &value : values) {
        array.append(value);
    }
    return array;
}

QStringList stringListFromJson(const QJsonValue &value)
{
    QStringList values;
    for (const QJsonValue &entry : value.toArray()) {
        const QString item = entry.toString().trimmed();
        if (!item.isEmpty()) {
            values.append(item);
        }
    }
    return values;
}

QJsonObject serializeSubscription(const ConfigurationTransfer::SubscriptionData &subscription)
{
    return {
        {QStringLiteral("topic"), subscription.topic},
        {QStringLiteral("alias"), subscription.alias},
        {QStringLiteral("qos"), subscription.qos},
        {QStringLiteral("format"), subscription.format},
        {QStringLiteral("color"), subscription.color},
        {QStringLiteral("paused"), subscription.paused},
    };
}

QJsonObject serializeSession(const ConfigurationTransfer::SessionData &session)
{
    QJsonArray subscriptions;
    for (const auto &subscription : session.subscriptions) {
        subscriptions.append(serializeSubscription(subscription));
    }

    QJsonObject assets;
    if (!session.caCertificate.isEmpty()) {
        assets.insert(
            QStringLiteral("caCertificate"),
            QString::fromLatin1(session.caCertificate.toBase64()));
    }
    if (!session.clientCertificate.isEmpty()) {
        assets.insert(
            QStringLiteral("clientCertificate"),
            QString::fromLatin1(session.clientCertificate.toBase64()));
    }
    if (!session.clientKey.isEmpty()) {
        assets.insert(
            QStringLiteral("clientKey"),
            QString::fromLatin1(session.clientKey.toBase64()));
    }

    QJsonObject object {
        {QStringLiteral("id"), session.sourceId},
        {QStringLiteral("name"), session.name},
        {QStringLiteral("host"), session.host},
        {QStringLiteral("port"), session.port},
        {QStringLiteral("transport"), session.transport},
        {QStringLiteral("protocolVersion"), session.protocolVersion},
        {QStringLiteral("sslSecure"), session.sslSecure},
        {QStringLiteral("alpn"), session.alpn},
        {QStringLiteral("certificateType"), session.certificateType},
        {QStringLiteral("clientId"), session.clientId},
        {QStringLiteral("username"), session.username},
        {QStringLiteral("password"), session.password},
        {QStringLiteral("cleanSession"), session.cleanSession},
        {QStringLiteral("keepAliveSeconds"), session.keepAliveSeconds},
        {QStringLiteral("connectTimeoutSeconds"), session.connectTimeoutSeconds},
        {QStringLiteral("sessionExpiryInterval"), static_cast<double>(session.sessionExpiryInterval)},
        {QStringLiteral("receiveMaximum"), session.receiveMaximum},
        {QStringLiteral("maximumPacketSize"), static_cast<double>(session.maximumPacketSize)},
        {QStringLiteral("topicAliasMaximum"), session.topicAliasMaximum},
        {QStringLiteral("requestResponseInformation"), session.requestResponseInformation},
        {QStringLiteral("requestProblemInformation"), session.requestProblemInformation},
        {QStringLiteral("authenticationMethod"), session.authenticationMethod},
        {QStringLiteral("authenticationData"), session.authenticationData},
        {QStringLiteral("outputPaused"), session.outputPaused},
        {QStringLiteral("captureIncoming"), session.capturePolicy.captureIncoming},
        {QStringLiteral("captureOutgoing"), session.capturePolicy.captureOutgoing},
        {QStringLiteral("captureIncludeTopicFilters"), stringListToJson(session.capturePolicy.includeTopicFilters)},
        {QStringLiteral("captureExcludeTopicFilters"), stringListToJson(session.capturePolicy.excludeTopicFilters)},
        {QStringLiteral("subscriptions"), subscriptions},
    };
    if (!assets.isEmpty()) {
        object.insert(QStringLiteral("tlsAssets"), assets);
    }
    return object;
}

ConfigurationTransfer::SubscriptionData parseNativeSubscription(const QJsonObject &object)
{
    ConfigurationTransfer::SubscriptionData subscription;
    subscription.topic = object.value(QStringLiteral("topic")).toString().trimmed();
    subscription.alias = object.value(QStringLiteral("alias")).toString().trimmed();
    subscription.qos = boundedInt(
        object.value(QStringLiteral("qos")),
        0,
        0,
        SessionConfig::kMaximumQos);
    subscription.format = boundedInt(object.value(QStringLiteral("format")), 0, 0, 5);
    subscription.color = object.value(QStringLiteral("color")).toString().trimmed();
    subscription.paused = object.value(QStringLiteral("paused")).toBool(false);
    return subscription;
}

} // namespace

namespace MqttxConfigAdapter {

ConfigurationTransfer::ParseResult parse(const QByteArray &content)
{
    using namespace ConfigurationTransfer;

    ParseResult result;
    result.format = QStringLiteral("mqttx");
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(content, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        result.errorMessage = text(QT_TRANSLATE_NOOP(
            "ConfigurationAdapters",
            "The file is not a supported MQTTX connection export."));
        return result;
    }

    int skippedConnections = 0;
    int skippedSubscriptions = 0;
    int autoReconnectConnections = 0;
    int ignoredWillConnections = 0;
    int ignoredMessages = 0;
    int advancedSubscriptionCount = 0;
    int nonPemAssetCount = 0;

    const QJsonArray connections = document.array();
    result.bundle.sessions.reserve(connections.size());
    for (qsizetype connectionIndex = 0; connectionIndex < connections.size(); ++connectionIndex) {
        const QJsonObject object = connections.at(connectionIndex).toObject();
        if (object.isEmpty()) {
            ++skippedConnections;
            continue;
        }

        const QString protocol = object.value(QStringLiteral("protocol")).toString().trimmed().toLower();
        QString transport;
        if (protocol == QStringLiteral("mqtt")) {
            transport = QStringLiteral("tcp");
        } else if (protocol == QStringLiteral("mqtts")) {
            transport = QStringLiteral("tls");
        } else {
            ++skippedConnections;
            continue;
        }

        const QString mqttVersion = object.value(QStringLiteral("mqttVersion")).toVariant().toString().trimmed();
        int protocolVersion = 0;
        if (mqttVersion == QStringLiteral("5") || mqttVersion == QStringLiteral("5.0")) {
            protocolVersion = 5;
        } else if (mqttVersion == QStringLiteral("4") || mqttVersion == QStringLiteral("3.1.1")) {
            protocolVersion = 4;
        } else {
            ++skippedConnections;
            continue;
        }

        SessionData session;
        session.sourceId = object.value(QStringLiteral("id")).toString().trimmed();
        session.name = object.value(QStringLiteral("name")).toString().trimmed();
        if (session.name.isEmpty()) {
            session.name = text(QT_TRANSLATE_NOOP(
                "ConfigurationAdapters",
                "Imported connection %1")).arg(connectionIndex + 1);
        }
        session.host = object.value(QStringLiteral("host")).toString().trimmed();
        if (session.host.isEmpty()) {
            ++skippedConnections;
            continue;
        }
        session.transport = transport;
        session.protocolVersion = protocolVersion;
        session.port = boundedInt(
            object.value(QStringLiteral("port")),
            transport == QStringLiteral("tls") ? 8883 : 1883,
            1,
            65535);
        session.sslSecure = object.value(QStringLiteral("rejectUnauthorized")).isBool()
            ? object.value(QStringLiteral("rejectUnauthorized")).toBool()
            : true;
        session.alpn = alpnString(object.value(QStringLiteral("ALPNProtocols")));
        const QString certificateType = object.value(QStringLiteral("certType")).toString().trimmed().toLower();
        session.certificateType = certificateType.contains(QStringLiteral("self"))
            ? QStringLiteral("self")
            : QStringLiteral("ca");
        session.caCertificate = object.value(QStringLiteral("ca")).toString().toUtf8();
        session.clientCertificate = object.value(QStringLiteral("cert")).toString().toUtf8();
        session.clientKey = object.value(QStringLiteral("key")).toString().toUtf8();
        for (const QByteArray &asset : {session.caCertificate, session.clientCertificate, session.clientKey}) {
            if (!asset.isEmpty() && !asset.contains("-----BEGIN")) {
                ++nonPemAssetCount;
            }
        }
        session.clientId = object.value(QStringLiteral("clientId")).toString().trimmed();
        session.username = object.value(QStringLiteral("username")).toString();
        session.password = object.value(QStringLiteral("password")).toString();
        session.cleanSession = object.value(QStringLiteral("clean")).toBool(true);
        session.keepAliveSeconds = boundedInt(
            object.value(QStringLiteral("keepalive")), 30, 5, 1200);
        int connectTimeout = boundedInt(
            object.value(QStringLiteral("connectTimeout")), 10, 1, 300000);
        if (connectTimeout > 300) {
            connectTimeout = std::clamp(connectTimeout / 1000, 1, 300);
        }
        session.connectTimeoutSeconds = connectTimeout;
        session.sessionExpiryInterval = optionalUInt32(
            connectionProperty(object, QStringLiteral("sessionExpiryInterval")));
        session.receiveMaximum = optionalUInt16(
            connectionProperty(object, QStringLiteral("receiveMaximum")));
        session.maximumPacketSize = optionalUInt32(
            connectionProperty(object, QStringLiteral("maximumPacketSize")));
        session.topicAliasMaximum = optionalUInt16(
            connectionProperty(object, QStringLiteral("topicAliasMaximum")));
        session.requestResponseInformation = connectionProperty(
                                                 object,
                                                 QStringLiteral("requestResponseInformation"))
                                                 .toBool(false);
        session.requestProblemInformation = connectionProperty(
                                                object,
                                                QStringLiteral("requestProblemInformation"))
                                                .toBool(false);
        session.authenticationMethod = connectionProperty(
                                           object,
                                           QStringLiteral("authenticationMethod"))
                                           .toString()
                                           .trimmed();
        session.authenticationData = connectionProperty(
                                         object,
                                         QStringLiteral("authenticationData"))
                                         .toString();

        if (!session.password.isEmpty()) {
            ++result.sensitiveFieldCount;
        }
        if (!session.authenticationData.isEmpty()) {
            ++result.sensitiveFieldCount;
        }
        if (!session.clientKey.isEmpty()) {
            ++result.sensitiveFieldCount;
        }

        QSet<QString> topics;
        const QJsonArray subscriptions = object.value(QStringLiteral("subscriptions")).toArray();
        session.subscriptions.reserve(subscriptions.size());
        for (const QJsonValue &value : subscriptions) {
            const QJsonObject row = value.toObject();
            const QString topic = row.value(QStringLiteral("topic")).toString().trimmed();
            if (topic.isEmpty() || !QMqttTopicFilter(topic).isValid() || topics.contains(topic)) {
                ++skippedSubscriptions;
                continue;
            }
            topics.insert(topic);

            SubscriptionData subscription;
            subscription.topic = topic;
            subscription.alias = row.value(QStringLiteral("alias")).toString().trimmed();
            subscription.qos = boundedInt(
                row.value(QStringLiteral("qos")),
                0,
                0,
                SessionConfig::kMaximumQos);
            subscription.color = row.value(QStringLiteral("color")).toString().trimmed();
            subscription.paused = row.value(QStringLiteral("disabled")).toBool(false);
            const QJsonValue subscriptionIdentifier =
                row.value(QStringLiteral("subscriptionIdentifier"));
            if (row.value(QStringLiteral("nl")).toBool(false)
                || row.value(QStringLiteral("rap")).toBool(false)
                || boundedInt(row.value(QStringLiteral("rh")), 0, 0, 2) != 0
                || jsonCollectionHasValues(row.value(QStringLiteral("userProperties")))
                || (!subscriptionIdentifier.isUndefined()
                    && !subscriptionIdentifier.isNull())) {
                ++advancedSubscriptionCount;
            }
            session.subscriptions.append(subscription);
        }

        if (object.value(QStringLiteral("reconnect")).toBool(false)
            || boundedInt(object.value(QStringLiteral("reconnectPeriod")), 0, 0, 3600000) > 0) {
            ++autoReconnectConnections;
        }
        if (mqttxWillHasValues(object.value(QStringLiteral("will")).toObject())) {
            ++ignoredWillConnections;
        }
        ignoredMessages += object.value(QStringLiteral("messages")).toArray().size();
        if (jsonCollectionHasValues(object.value(QStringLiteral("userProperties")))) {
            result.warnings.append(text(QT_TRANSLATE_NOOP(
                "ConfigurationAdapters",
                "MQTT 5 connection user properties are not supported and will be ignored.")));
        }
        result.bundle.sessions.append(std::move(session));
    }

    if (result.bundle.sessions.isEmpty()) {
        result.errorMessage = text(QT_TRANSLATE_NOOP(
            "ConfigurationAdapters",
            "The MQTTX export contains no supported connections."));
        return result;
    }
    if (skippedConnections > 0) {
        result.warnings.append(
            text(QT_TRANSLATE_NOOP(
                     "ConfigurationAdapters",
                     "%1 unsupported or incomplete connections will be skipped."))
                .arg(skippedConnections));
    }
    if (skippedSubscriptions > 0) {
        result.warnings.append(
            text(QT_TRANSLATE_NOOP(
                     "ConfigurationAdapters",
                     "%1 invalid or duplicate subscriptions will be skipped."))
                .arg(skippedSubscriptions));
    }
    if (autoReconnectConnections > 0) {
        result.warnings.append(
            text(QT_TRANSLATE_NOOP(
                     "ConfigurationAdapters",
                     "Automatic reconnect settings from %1 connections are not supported."))
                .arg(autoReconnectConnections));
    }
    if (ignoredWillConnections > 0) {
        result.warnings.append(
            text(QT_TRANSLATE_NOOP(
                     "ConfigurationAdapters",
                     "Last-will settings from %1 connections are not supported."))
                .arg(ignoredWillConnections));
    }
    if (ignoredMessages > 0) {
        result.warnings.append(
            text(QT_TRANSLATE_NOOP(
                     "ConfigurationAdapters",
                     "%1 MQTTX message records will not be imported as drafts or history."))
                .arg(ignoredMessages));
    }
    if (advancedSubscriptionCount > 0) {
        result.warnings.append(
            text(QT_TRANSLATE_NOOP(
                     "ConfigurationAdapters",
                     "Advanced MQTT 5 options on %1 subscriptions are not supported."))
                .arg(advancedSubscriptionCount));
    }
    if (nonPemAssetCount > 0) {
        result.warnings.append(
            text(QT_TRANSLATE_NOOP(
                     "ConfigurationAdapters",
                     "%1 certificate fields do not look like PEM data and may require manual repair."))
                .arg(nonPemAssetCount));
    }
    result.warnings.removeDuplicates();
    result.ok = true;
    return result;
}

} // namespace MqttxConfigAdapter

namespace MqttPlusConfigAdapter {

ConfigurationTransfer::ParseResult parse(const QByteArray &content)
{
    using namespace ConfigurationTransfer;

    ParseResult result;
    result.format = QStringLiteral("mqtt-plus");
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(content, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        result.errorMessage = text(QT_TRANSLATE_NOOP(
            "ConfigurationAdapters",
            "The file is not a valid MQTT Plus configuration export."));
        return result;
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("format")).toString() != QStringLiteral("mqtt-plus-config")) {
        result.errorMessage = text(QT_TRANSLATE_NOOP(
            "ConfigurationAdapters",
            "The file is not an MQTT Plus configuration export."));
        return result;
    }
    const int version = root.value(QStringLiteral("version")).toInt(-1);
    if (version > kSchemaVersion) {
        result.errorMessage = text(QT_TRANSLATE_NOOP(
            "ConfigurationAdapters",
            "This configuration export requires a newer application version."));
        return result;
    }
    if (version != kSchemaVersion) {
        result.errorMessage = text(QT_TRANSLATE_NOOP(
            "ConfigurationAdapters",
            "The configuration export version is not supported."));
        return result;
    }

    const QJsonValue sessionsValue = root.value(QStringLiteral("sessions"));
    const QJsonValue draftsValue = root.value(QStringLiteral("drafts"));
    const QJsonValue preferencesValue = root.value(QStringLiteral("preferences"));
    if (!sessionsValue.isArray()
        || !draftsValue.isArray()
        || !preferencesValue.isObject()) {
        result.errorMessage = text(QT_TRANSLATE_NOOP(
            "ConfigurationAdapters",
            "The configuration export has an invalid structure."));
        return result;
    }
    for (const QJsonValue &value : sessionsValue.toArray()) {
        if (!value.isObject()) {
            result.errorMessage = text(QT_TRANSLATE_NOOP(
                "ConfigurationAdapters",
                "The configuration export has an invalid structure."));
            return result;
        }
    }
    for (const QJsonValue &value : draftsValue.toArray()) {
        if (!value.isObject()) {
            result.errorMessage = text(QT_TRANSLATE_NOOP(
                "ConfigurationAdapters",
                "The configuration export has an invalid structure."));
            return result;
        }
    }

    int skippedSessions = 0;
    int skippedSubscriptions = 0;
    for (const QJsonValue &value : sessionsValue.toArray()) {
        const QJsonObject object = value.toObject();
        SessionData session;
        session.sourceId = object.value(QStringLiteral("id")).toString().trimmed();
        session.name = object.value(QStringLiteral("name")).toString().trimmed();
        session.host = object.value(QStringLiteral("host")).toString().trimmed();
        if (session.name.isEmpty() || session.host.isEmpty()) {
            ++skippedSessions;
            continue;
        }
        session.transport = object.value(QStringLiteral("transport")).toString() == QStringLiteral("tls")
            ? QStringLiteral("tls")
            : QStringLiteral("tcp");
        session.protocolVersion = object.value(QStringLiteral("protocolVersion")).toInt(5) == 4 ? 4 : 5;
        session.port = boundedInt(
            object.value(QStringLiteral("port")),
            session.transport == QStringLiteral("tls") ? 8883 : 1883,
            1,
            65535);
        session.sslSecure = object.value(QStringLiteral("sslSecure")).toBool(true);
        session.alpn = object.value(QStringLiteral("alpn")).toString().trimmed();
        session.certificateType = object.value(QStringLiteral("certificateType")).toString() == QStringLiteral("self")
            ? QStringLiteral("self")
            : QStringLiteral("ca");
        bool assetsOk = true;
        const QJsonObject assets = object.value(QStringLiteral("tlsAssets")).toObject();
        session.caCertificate = nativeAsset(assets, QStringLiteral("caCertificate"), assetsOk);
        session.clientCertificate = nativeAsset(assets, QStringLiteral("clientCertificate"), assetsOk);
        session.clientKey = nativeAsset(assets, QStringLiteral("clientKey"), assetsOk);
        if (!assetsOk) {
            ++skippedSessions;
            continue;
        }
        session.clientId = object.value(QStringLiteral("clientId")).toString().trimmed();
        session.username = object.value(QStringLiteral("username")).toString();
        session.password = object.value(QStringLiteral("password")).toString();
        session.cleanSession = object.value(QStringLiteral("cleanSession")).toBool(true);
        session.keepAliveSeconds = boundedInt(
            object.value(QStringLiteral("keepAliveSeconds")), 30, 5, 1200);
        session.connectTimeoutSeconds = boundedInt(
            object.value(QStringLiteral("connectTimeoutSeconds")), 10, 1, 300);
        session.sessionExpiryInterval = optionalUInt32(object.value(QStringLiteral("sessionExpiryInterval")));
        session.receiveMaximum = optionalUInt16(object.value(QStringLiteral("receiveMaximum")));
        session.maximumPacketSize = optionalUInt32(object.value(QStringLiteral("maximumPacketSize")));
        session.topicAliasMaximum = optionalUInt16(object.value(QStringLiteral("topicAliasMaximum")));
        session.requestResponseInformation = object.value(QStringLiteral("requestResponseInformation")).toBool(false);
        session.requestProblemInformation = object.value(QStringLiteral("requestProblemInformation")).toBool(false);
        session.authenticationMethod = object.value(QStringLiteral("authenticationMethod")).toString().trimmed();
        session.authenticationData = object.value(QStringLiteral("authenticationData")).toString();
        session.outputPaused = object.value(QStringLiteral("outputPaused")).toBool(false);
        session.capturePolicy.captureIncoming = object.value(
                                                          QStringLiteral("captureIncoming"))
                                                  .toBool(true);
        session.capturePolicy.captureOutgoing = object.value(
                                                          QStringLiteral("captureOutgoing"))
                                                  .toBool(true);
        session.capturePolicy.includeTopicFilters = stringListFromJson(
            object.value(QStringLiteral("captureIncludeTopicFilters")));
        session.capturePolicy.excludeTopicFilters = stringListFromJson(
            object.value(QStringLiteral("captureExcludeTopicFilters")));
        if (!session.password.isEmpty()) {
            ++result.sensitiveFieldCount;
        }
        if (!session.authenticationData.isEmpty()) {
            ++result.sensitiveFieldCount;
        }
        if (!session.clientKey.isEmpty()) {
            ++result.sensitiveFieldCount;
        }

        QSet<QString> topics;
        for (const QJsonValue &subscriptionValue : object.value(QStringLiteral("subscriptions")).toArray()) {
            const SubscriptionData subscription = parseNativeSubscription(subscriptionValue.toObject());
            if (subscription.topic.isEmpty()
                || !QMqttTopicFilter(subscription.topic).isValid()
                || topics.contains(subscription.topic)) {
                ++skippedSubscriptions;
                continue;
            }
            topics.insert(subscription.topic);
            session.subscriptions.append(subscription);
        }
        result.bundle.sessions.append(std::move(session));
    }

    const int ignoredScripts = root.value(QStringLiteral("scripts")).toArray().size();
    if (ignoredScripts > 0) {
        result.warnings.append(
            text(QT_TRANSLATE_NOOP(
                     "ConfigurationAdapters",
                     "%1 scripts are present but script import is not supported yet."))
                .arg(ignoredScripts));
    }

    for (const QJsonValue &value : draftsValue.toArray()) {
        const QJsonObject object = value.toObject();
        PublishDraft draft;
        draft.id = object.value(QStringLiteral("id")).toString().trimmed();
        draft.name = object.value(QStringLiteral("name")).toString().trimmed();
        draft.description = object.value(QStringLiteral("description")).toString();
        draft.defaultTopic = object.value(QStringLiteral("defaultTopic")).toString();
        draft.payload = object.value(QStringLiteral("payload")).toString();
        draft.formatId = object.value(QStringLiteral("format")).toString();
        draft.qos = object.value(QStringLiteral("qos")).toInt(-1);
        draft.retain = object.value(QStringLiteral("retain")).toBool(false);
        draft.createdAt = object.value(QStringLiteral("createdAt")).toString();
        draft.updatedAt = object.value(QStringLiteral("updatedAt")).toString();
        draft.lastUsedAt = object.value(QStringLiteral("lastUsedAt")).toString();
        if (!draft.id.isEmpty() && !draft.name.isEmpty()) {
            result.bundle.drafts.append(draft);
        }
    }

    result.bundle.preferences = preferencesValue.toObject().toVariantMap();
    if (skippedSessions > 0) {
        result.warnings.append(
            text(QT_TRANSLATE_NOOP(
                     "ConfigurationAdapters",
                     "%1 incomplete sessions will be skipped."))
                .arg(skippedSessions));
    }
    if (skippedSubscriptions > 0) {
        result.warnings.append(
            text(QT_TRANSLATE_NOOP(
                     "ConfigurationAdapters",
                     "%1 invalid or duplicate subscriptions will be skipped."))
                .arg(skippedSubscriptions));
    }
    result.ok = true;
    return result;
}

ConfigurationTransfer::SerializeResult serialize(
    const ConfigurationTransfer::Bundle &bundle,
    bool sensitiveDataIncluded)
{
    using namespace ConfigurationTransfer;

    QJsonArray sessions;
    for (const SessionData &session : bundle.sessions) {
        sessions.append(serializeSession(session));
    }

    QJsonArray drafts;
    for (const PublishDraft &draft : bundle.drafts) {
        drafts.append(QJsonObject {
            {QStringLiteral("id"), draft.id},
            {QStringLiteral("name"), draft.name},
            {QStringLiteral("description"), draft.description},
            {QStringLiteral("defaultTopic"), draft.defaultTopic},
            {QStringLiteral("payload"), draft.payload},
            {QStringLiteral("format"), draft.formatId},
            {QStringLiteral("qos"), draft.qos},
            {QStringLiteral("retain"), draft.retain},
            {QStringLiteral("createdAt"), draft.createdAt},
            {QStringLiteral("updatedAt"), draft.updatedAt},
            {QStringLiteral("lastUsedAt"), draft.lastUsedAt},
        });
    }

    QJsonObject root {
        {QStringLiteral("format"), QStringLiteral("mqtt-plus-config")},
        {QStringLiteral("version"), kSchemaVersion},
        {QStringLiteral("sensitiveDataIncluded"), sensitiveDataIncluded},
        {QStringLiteral("sessions"), sessions},
        {QStringLiteral("drafts"), drafts},
        {QStringLiteral("preferences"), QJsonObject::fromVariantMap(bundle.preferences)},
    };

    SerializeResult result;
    result.ok = true;
    result.content = QJsonDocument(root).toJson(QJsonDocument::Indented);
    return result;
}

} // namespace MqttPlusConfigAdapter
