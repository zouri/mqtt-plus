#include "qtmqttpropertycodec.h"

#include "domain/sessionconfig.h"

namespace QtMqttPropertyCodec {

QMqttUserProperties toQtUserProperties(const MqttUserProperties &properties)
{
    QMqttUserProperties result;
    result.reserve(properties.size());
    for (const MqttUserProperty &property : properties) {
        result.append(QMqttStringPair(property.name, property.value));
    }
    return result;
}

MqttUserProperties fromQtUserProperties(const QMqttUserProperties &properties)
{
    MqttUserProperties result;
    result.reserve(properties.size());
    for (const QMqttStringPair &property : properties) {
        result.append({property.name(), property.value()});
    }
    return result;
}

QMqttPublishProperties toQtPublishProperties(const MqttPublishProperties &properties)
{
    QMqttPublishProperties result;
    if (properties.payloadFormatIndicator) {
        result.setPayloadFormatIndicator(
            *properties.payloadFormatIndicator == MqttPayloadFormatIndicator::Utf8
                ? QMqtt::PayloadFormatIndicator::UTF8Encoded
                : QMqtt::PayloadFormatIndicator::Unspecified);
    }
    if (properties.messageExpiryInterval) {
        result.setMessageExpiryInterval(*properties.messageExpiryInterval);
    }
    if (properties.topicAlias) {
        result.setTopicAlias(*properties.topicAlias);
    }
    if (!properties.responseTopic.isEmpty()) {
        result.setResponseTopic(properties.responseTopic);
    }
    if (!properties.correlationData.isEmpty()) {
        result.setCorrelationData(properties.correlationData);
    }
    if (!properties.userProperties.isEmpty()) {
        result.setUserProperties(toQtUserProperties(properties.userProperties));
    }
    if (!properties.contentType.isEmpty()) {
        result.setContentType(properties.contentType);
    }
    return result;
}

MqttPublishProperties fromQtPublishProperties(const QMqttPublishProperties &properties)
{
    MqttPublishProperties result;
    const auto available = properties.availableProperties();
    if (available.testFlag(QMqttPublishProperties::PayloadFormatIndicator)) {
        result.payloadFormatIndicator = properties.payloadFormatIndicator()
                == QMqtt::PayloadFormatIndicator::UTF8Encoded
            ? MqttPayloadFormatIndicator::Utf8
            : MqttPayloadFormatIndicator::Unspecified;
    }
    if (available.testFlag(QMqttPublishProperties::MessageExpiryInterval)) {
        result.messageExpiryInterval = properties.messageExpiryInterval();
    }
    if (available.testFlag(QMqttPublishProperties::TopicAlias)) {
        result.topicAlias = properties.topicAlias();
    }
    if (available.testFlag(QMqttPublishProperties::ResponseTopic)) {
        result.responseTopic = properties.responseTopic();
    }
    if (available.testFlag(QMqttPublishProperties::CorrelationData)) {
        result.correlationData = properties.correlationData();
    }
    if (available.testFlag(QMqttPublishProperties::UserProperty)) {
        result.userProperties = fromQtUserProperties(properties.userProperties());
    }
    if (available.testFlag(QMqttPublishProperties::SubscriptionIdentifier)) {
        const QList<quint32> identifiers = properties.subscriptionIdentifiers();
        result.subscriptionIdentifiers = QVector<quint32>(identifiers.cbegin(), identifiers.cend());
    }
    if (available.testFlag(QMqttPublishProperties::ContentType)) {
        result.contentType = properties.contentType();
    }
    return result;
}

QMqttLastWillProperties toQtLastWillProperties(const MqttLastWillProperties &properties)
{
    QMqttLastWillProperties result;
    result.setWillDelayInterval(properties.delayInterval);
    if (properties.payloadFormatIndicator) {
        result.setPayloadFormatIndicator(
            *properties.payloadFormatIndicator == MqttPayloadFormatIndicator::Utf8
                ? QMqtt::PayloadFormatIndicator::UTF8Encoded
                : QMqtt::PayloadFormatIndicator::Unspecified);
    }
    if (properties.messageExpiryInterval) {
        result.setMessageExpiryInterval(*properties.messageExpiryInterval);
    }
    if (!properties.contentType.isEmpty()) {
        result.setContentType(properties.contentType);
    }
    if (!properties.responseTopic.isEmpty()) {
        result.setResponseTopic(properties.responseTopic);
    }
    if (!properties.correlationData.isEmpty()) {
        result.setCorrelationData(properties.correlationData);
    }
    if (!properties.userProperties.isEmpty()) {
        result.setUserProperties(toQtUserProperties(properties.userProperties));
    }
    return result;
}

QMqttSubscriptionProperties toQtSubscriptionProperties(const MqttSubscriptionOptions &options)
{
    QMqttSubscriptionProperties result;
    result.setNoLocal(options.noLocal);
    if (options.subscriptionIdentifier > 0
        && options.subscriptionIdentifier <= SessionConfig::kMaximumSubscriptionIdentifier) {
        result.setSubscriptionIdentifier(options.subscriptionIdentifier);
    }
    if (!options.userProperties.isEmpty()) {
        result.setUserProperties(toQtUserProperties(options.userProperties));
    }
    return result;
}

} // namespace QtMqttPropertyCodec
