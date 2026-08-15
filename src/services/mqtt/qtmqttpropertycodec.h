#pragma once

#include "domain/mqttproperties.h"

#include <QMqttConnectionProperties>
#include <QMqttPublishProperties>
#include <QMqttSubscriptionProperties>

namespace QtMqttPropertyCodec {

QMqttUserProperties toQtUserProperties(const MqttUserProperties &properties);
MqttUserProperties fromQtUserProperties(const QMqttUserProperties &properties);

QMqttPublishProperties toQtPublishProperties(const MqttPublishProperties &properties);
MqttPublishProperties fromQtPublishProperties(const QMqttPublishProperties &properties);

QMqttLastWillProperties toQtLastWillProperties(const MqttLastWillProperties &properties);
QMqttSubscriptionProperties toQtSubscriptionProperties(const MqttSubscriptionOptions &options);

} // namespace QtMqttPropertyCodec

