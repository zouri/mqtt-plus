#include "domain/mqtttopicfilter.h"

#include <QStringList>

namespace MqttTopicFilter {

bool matches(const QString &filter, const QString &topic)
{
    if (filter.isEmpty()) {
        return false;
    }
    if (filter == topic) {
        return true;
    }

    const QStringList filterLevels = filter.split('/');
    const QStringList topicLevels = topic.split('/');

    int filterIndex = 0;
    int topicIndex = 0;
    while (filterIndex < filterLevels.size() && topicIndex < topicLevels.size()) {
        const QString &level = filterLevels.at(filterIndex);
        if (level == QStringLiteral("#")) {
            return filterIndex == filterLevels.size() - 1;
        }
        if (level != QStringLiteral("+") && level != topicLevels.at(topicIndex)) {
            return false;
        }
        ++filterIndex;
        ++topicIndex;
    }

    if (filterIndex == filterLevels.size() && topicIndex == topicLevels.size()) {
        return true;
    }
    return filterIndex == filterLevels.size() - 1
        && filterLevels.at(filterIndex) == QStringLiteral("#");
}

int specificityScore(const QString &filter)
{
    int score = filter.count('/');
    for (const QChar character : filter) {
        if (character != QLatin1Char('#')
            && character != QLatin1Char('+')
            && character != QLatin1Char('/')) {
            ++score;
        }
    }
    return score;
}

} // namespace MqttTopicFilter
