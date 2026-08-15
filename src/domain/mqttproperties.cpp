#include "mqttproperties.h"

#include <QCborArray>
#include <QCborParserError>
#include <QCborValue>

namespace {
QCborArray userPropertiesToCbor(const MqttUserProperties &properties)
{
    QCborArray rows;
    for (const MqttUserProperty &property : properties) {
        rows.append(QCborArray {property.name, property.value});
    }
    return rows;
}

MqttUserProperties userPropertiesFromCbor(const QCborValue &value)
{
    MqttUserProperties properties;
    for (const QCborValue &rowValue : value.toArray()) {
        const QCborArray row = rowValue.toArray();
        if (row.size() >= 2) {
            properties.append({row.at(0).toString(), row.at(1).toString()});
        }
    }
    return properties;
}
}

bool MqttPublishProperties::isEmpty() const
{
    return !payloadFormatIndicator.has_value()
        && !messageExpiryInterval.has_value()
        && !topicAlias.has_value()
        && responseTopic.isEmpty()
        && correlationData.isEmpty()
        && userProperties.isEmpty()
        && subscriptionIdentifiers.isEmpty()
        && contentType.isEmpty();
}

bool MqttLastWillProperties::isEmpty() const
{
    return delayInterval == 0
        && !payloadFormatIndicator.has_value()
        && !messageExpiryInterval.has_value()
        && contentType.isEmpty()
        && responseTopic.isEmpty()
        && correlationData.isEmpty()
        && userProperties.isEmpty();
}

MqttUserProperties mqttUserPropertiesFromText(const QString &text)
{
    MqttUserProperties properties;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        const qsizetype separator = trimmed.indexOf(QLatin1Char('='));
        const QString name = (separator < 0 ? trimmed : trimmed.left(separator)).trimmed();
        if (name.isEmpty()) {
            continue;
        }
        const QString value = separator < 0 ? QString() : trimmed.mid(separator + 1).trimmed();
        properties.append({name, value});
    }
    return properties;
}

QString mqttUserPropertiesToText(const MqttUserProperties &properties)
{
    QStringList lines;
    lines.reserve(properties.size());
    for (const MqttUserProperty &property : properties) {
        lines.append(QStringLiteral("%1=%2").arg(property.name, property.value));
    }
    return lines.join(QLatin1Char('\n'));
}

QVariantList mqttUserPropertiesToVariantList(const MqttUserProperties &properties)
{
    QVariantList rows;
    rows.reserve(properties.size());
    for (const MqttUserProperty &property : properties) {
        rows.append(QVariantMap {
            {QStringLiteral("name"), property.name},
            {QStringLiteral("value"), property.value},
        });
    }
    return rows;
}

MqttUserProperties mqttUserPropertiesFromVariantList(const QVariantList &rows)
{
    MqttUserProperties properties;
    properties.reserve(rows.size());
    for (const QVariant &rowValue : rows) {
        const QVariantMap row = rowValue.toMap();
        const QString name = row.value(QStringLiteral("name")).toString();
        if (!name.isEmpty()) {
            properties.append({name, row.value(QStringLiteral("value")).toString()});
        }
    }
    return properties;
}

QCborMap mqttPublishPropertiesToCbor(const MqttPublishProperties &properties)
{
    QCborMap map;
    if (properties.payloadFormatIndicator) {
        map.insert(
            QStringLiteral("payloadFormatUtf8"),
            *properties.payloadFormatIndicator == MqttPayloadFormatIndicator::Utf8);
    }
    if (properties.messageExpiryInterval) {
        map.insert(QStringLiteral("messageExpiryInterval"), *properties.messageExpiryInterval);
    }
    if (properties.topicAlias) {
        map.insert(QStringLiteral("topicAlias"), *properties.topicAlias);
    }
    if (!properties.responseTopic.isEmpty()) {
        map.insert(QStringLiteral("responseTopic"), properties.responseTopic);
    }
    if (!properties.correlationData.isEmpty()) {
        map.insert(QStringLiteral("correlationData"), properties.correlationData);
    }
    if (!properties.userProperties.isEmpty()) {
        map.insert(QStringLiteral("userProperties"), userPropertiesToCbor(properties.userProperties));
    }
    if (!properties.subscriptionIdentifiers.isEmpty()) {
        QCborArray identifiers;
        for (quint32 identifier : properties.subscriptionIdentifiers) {
            identifiers.append(identifier);
        }
        map.insert(QStringLiteral("subscriptionIdentifiers"), identifiers);
    }
    if (!properties.contentType.isEmpty()) {
        map.insert(QStringLiteral("contentType"), properties.contentType);
    }
    return map;
}

MqttPublishProperties mqttPublishPropertiesFromCbor(const QCborMap &map)
{
    MqttPublishProperties properties;
    if (map.contains(QStringLiteral("payloadFormatUtf8"))) {
        properties.payloadFormatIndicator = map.value(QStringLiteral("payloadFormatUtf8")).toBool()
            ? MqttPayloadFormatIndicator::Utf8
            : MqttPayloadFormatIndicator::Unspecified;
    }
    if (map.contains(QStringLiteral("messageExpiryInterval"))) {
        properties.messageExpiryInterval = static_cast<quint32>(
            map.value(QStringLiteral("messageExpiryInterval")).toInteger());
    }
    if (map.contains(QStringLiteral("topicAlias"))) {
        properties.topicAlias = static_cast<quint16>(map.value(QStringLiteral("topicAlias")).toInteger());
    }
    properties.responseTopic = map.value(QStringLiteral("responseTopic")).toString();
    properties.correlationData = map.value(QStringLiteral("correlationData")).toByteArray();
    properties.userProperties = userPropertiesFromCbor(map.value(QStringLiteral("userProperties")));
    for (const QCborValue &identifier : map.value(QStringLiteral("subscriptionIdentifiers")).toArray()) {
        properties.subscriptionIdentifiers.append(static_cast<quint32>(identifier.toInteger()));
    }
    properties.contentType = map.value(QStringLiteral("contentType")).toString();
    return properties;
}

QString mqttPublishPropertiesToBase64Cbor(const MqttPublishProperties &properties)
{
    return QString::fromLatin1(
        QCborValue(mqttPublishPropertiesToCbor(properties)).toCbor().toBase64());
}

std::optional<MqttPublishProperties> mqttPublishPropertiesFromBase64Cbor(const QString &encoded)
{
    const QByteArray decoded = QByteArray::fromBase64(
        encoded.toLatin1(),
        QByteArray::AbortOnBase64DecodingErrors);
    if (decoded.isNull()) {
        return std::nullopt;
    }

    QCborParserError parserError;
    const QCborValue value = QCborValue::fromCbor(decoded, &parserError);
    if (parserError.error != QCborError::NoError || !value.isMap()) {
        return std::nullopt;
    }
    return mqttPublishPropertiesFromCbor(value.toMap());
}

QVariantMap mqttPublishPropertiesToVariantMap(const MqttPublishProperties &properties)
{
    QVariantMap map;
    if (properties.payloadFormatIndicator) {
        map.insert(
            QStringLiteral("payloadFormatIndicator"),
            *properties.payloadFormatIndicator == MqttPayloadFormatIndicator::Utf8
                ? QStringLiteral("UTF-8")
                : QStringLiteral("Unspecified"));
    }
    if (properties.messageExpiryInterval) {
        map.insert(QStringLiteral("messageExpiryInterval"), *properties.messageExpiryInterval);
    }
    if (properties.topicAlias) {
        map.insert(QStringLiteral("topicAlias"), *properties.topicAlias);
    }
    if (!properties.responseTopic.isEmpty()) {
        map.insert(QStringLiteral("responseTopic"), properties.responseTopic);
    }
    if (!properties.correlationData.isEmpty()) {
        map.insert(QStringLiteral("correlationDataBase64"), QString::fromLatin1(properties.correlationData.toBase64()));
    }
    if (!properties.userProperties.isEmpty()) {
        map.insert(QStringLiteral("userProperties"), mqttUserPropertiesToVariantList(properties.userProperties));
    }
    if (!properties.subscriptionIdentifiers.isEmpty()) {
        QVariantList identifiers;
        identifiers.reserve(properties.subscriptionIdentifiers.size());
        for (quint32 identifier : properties.subscriptionIdentifiers) {
            identifiers.append(identifier);
        }
        map.insert(QStringLiteral("subscriptionIdentifiers"), identifiers);
    }
    if (!properties.contentType.isEmpty()) {
        map.insert(QStringLiteral("contentType"), properties.contentType);
    }
    return map;
}
