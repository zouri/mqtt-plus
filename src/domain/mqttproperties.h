#pragma once

#include <QByteArray>
#include <QCborMap>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include <optional>

struct MqttUserProperty
{
    QString name;
    QString value;

    bool operator==(const MqttUserProperty &) const = default;
};

using MqttUserProperties = QVector<MqttUserProperty>;

enum class MqttPayloadFormatIndicator
{
    Unspecified,
    Utf8,
};

struct MqttPublishProperties
{
    std::optional<MqttPayloadFormatIndicator> payloadFormatIndicator = std::nullopt;
    std::optional<quint32> messageExpiryInterval = std::nullopt;
    std::optional<quint16> topicAlias = std::nullopt;
    QString responseTopic;
    QByteArray correlationData;
    MqttUserProperties userProperties;
    QVector<quint32> subscriptionIdentifiers;
    QString contentType;

    bool isEmpty() const;
    bool operator==(const MqttPublishProperties &) const = default;
};

struct MqttLastWillProperties
{
    quint32 delayInterval = 0;
    std::optional<MqttPayloadFormatIndicator> payloadFormatIndicator = std::nullopt;
    std::optional<quint32> messageExpiryInterval = std::nullopt;
    QString contentType;
    QString responseTopic;
    QByteArray correlationData;
    MqttUserProperties userProperties;

    bool isEmpty() const;
    bool operator==(const MqttLastWillProperties &) const = default;
};

struct MqttLastWillConfig
{
    bool enabled = false;
    QString topic;
    QString payload;
    int payloadFormat = 0;
    int qos = 0;
    bool retain = false;
    MqttLastWillProperties properties;

    bool operator==(const MqttLastWillConfig &) const = default;
};

struct MqttSubscriptionOptions
{
    bool noLocal = false;
    quint32 subscriptionIdentifier = 0;
    MqttUserProperties userProperties;

    bool operator==(const MqttSubscriptionOptions &) const = default;
};

MqttUserProperties mqttUserPropertiesFromText(const QString &text);
QString mqttUserPropertiesToText(const MqttUserProperties &properties);
QVariantList mqttUserPropertiesToVariantList(const MqttUserProperties &properties);
MqttUserProperties mqttUserPropertiesFromVariantList(const QVariantList &rows);

QCborMap mqttPublishPropertiesToCbor(const MqttPublishProperties &properties);
MqttPublishProperties mqttPublishPropertiesFromCbor(const QCborMap &map);
QString mqttPublishPropertiesToBase64Cbor(const MqttPublishProperties &properties);
std::optional<MqttPublishProperties> mqttPublishPropertiesFromBase64Cbor(const QString &encoded);
QVariantMap mqttPublishPropertiesToVariantMap(const MqttPublishProperties &properties);
