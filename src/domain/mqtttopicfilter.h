#pragma once

#include <QString>

namespace MqttTopicFilter {

bool matches(const QString &filter, const QString &topic);
int specificityScore(const QString &filter);

} // namespace MqttTopicFilter
