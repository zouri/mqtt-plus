#pragma once

#include "domain/messagerecord.h"

#include <QStringList>

struct MessageCapturePolicy
{
    bool captureIncoming = true;
    bool captureOutgoing = true;
    QStringList includeTopicFilters;
    QStringList excludeTopicFilters;

    MessageCapturePolicy normalized() const;
    bool accepts(MessageDirection direction, const QString &topic) const;
};
