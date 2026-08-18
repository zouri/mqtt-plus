#pragma once

#include "services/payload/payloadcodec.h"

#include <QByteArray>
#include <QString>

namespace MessagePayload {

struct Plan
{
    QByteArray storedBytes;
    QString preview;
    QString state = QStringLiteral("full");
    QString hash;
    qint64 originalSize = 0;
    bool allowFullProcessing = true;
    bool shouldReport = false;
    QString reportMessage;
};

Plan planStorage(
    const QString &topic,
    const QByteArray &payloadBytes,
    int configuredLimit,
    bool compactPreview = false);
bool requiresBackgroundParse(PayloadFormat format);

} // namespace MessagePayload
