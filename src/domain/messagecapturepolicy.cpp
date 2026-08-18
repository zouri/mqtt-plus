#include "domain/messagecapturepolicy.h"

#include "domain/messagerecord.h"
#include "domain/mqtttopicfilter.h"

#include <algorithm>

namespace {
QStringList normalizedFilters(const QStringList &filters)
{
    QStringList normalized;
    normalized.reserve(filters.size());
    for (const QString &filter : filters) {
        const QString trimmed = filter.trimmed();
        if (!trimmed.isEmpty()) {
            normalized.append(trimmed);
        }
    }
    normalized.removeDuplicates();
    return normalized;
}

bool matchesAny(const QStringList &filters, const QString &topic)
{
    return std::any_of(
        filters.cbegin(),
        filters.cend(),
        [&topic](const QString &filter) {
            return MqttTopicFilter::matches(filter, topic);
        });
}
} // namespace

MessageCapturePolicy MessageCapturePolicy::normalized() const
{
    MessageCapturePolicy result = *this;
    result.includeTopicFilters = normalizedFilters(includeTopicFilters);
    result.excludeTopicFilters = normalizedFilters(excludeTopicFilters);
    return result;
}

bool MessageCapturePolicy::accepts(MessageDirection direction, const QString &topic) const
{
    if ((direction == MessageDirection::Incoming && !captureIncoming)
        || (direction == MessageDirection::Outgoing && !captureOutgoing)) {
        return false;
    }

    if (matchesAny(excludeTopicFilters, topic)) {
        return false;
    }

    return includeTopicFilters.isEmpty() || matchesAny(includeTopicFilters, topic);
}
