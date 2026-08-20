#pragma once

#include <QMetaType>
#include <QString>

struct TopicObservation
{
    QString topic;
    qint64 historyId = 0;
    qint64 observedAtMs = 0;
    QString payloadPreview;
};

Q_DECLARE_METATYPE(TopicObservation)

