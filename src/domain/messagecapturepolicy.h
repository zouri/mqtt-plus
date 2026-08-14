#pragma once

#include <QStringList>

enum class MessageDirection;

struct MessageCapturePolicy
{
    bool captureIncoming = true;
    bool captureOutgoing = true;
    QStringList includeTopicFilters;
    QStringList excludeTopicFilters;

    bool operator==(const MessageCapturePolicy &other) const = default;
    MessageCapturePolicy normalized() const;
    bool accepts(MessageDirection direction, const QString &topic) const;
};
