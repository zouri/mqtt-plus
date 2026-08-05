#pragma once

#include "domain/configurationbundle.h"

namespace MqttxConfigAdapter {

ConfigurationTransfer::ParseResult parse(const QByteArray &content);

} // namespace MqttxConfigAdapter

namespace MqttPlusConfigAdapter {

inline constexpr int kSchemaVersion = 2;

ConfigurationTransfer::ParseResult parse(const QByteArray &content);
ConfigurationTransfer::SerializeResult serialize(
    const ConfigurationTransfer::Bundle &bundle,
    bool sensitiveDataIncluded);

} // namespace MqttPlusConfigAdapter
