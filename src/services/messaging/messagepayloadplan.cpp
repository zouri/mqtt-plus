#include "services/messaging/messagepayloadplan.h"

#include <QCryptographicHash>

#include <algorithm>

namespace {
constexpr qint64 kPayloadPreviewBytes = 64 * 1024;
constexpr qint64 kPressurePayloadPreviewBytes = 4 * 1024;
constexpr qint64 kHardPayloadLimitBytes = 16 * 1024 * 1024;

QString formatByteCount(qint64 bytes)
{
    if (bytes >= 1024 * 1024) {
        return QStringLiteral("%1 MiB").arg(QString::number(bytes / 1024.0 / 1024.0, 'f', 1));
    }
    if (bytes >= 1024) {
        return QStringLiteral("%1 KiB").arg(QString::number(bytes / 1024.0, 'f', 1));
    }
    return QStringLiteral("%1 bytes").arg(bytes);
}

bool looksBinary(const QByteArray &bytes, qsizetype sampleLimit)
{
    if (bytes.isEmpty()) {
        return false;
    }

    const qsizetype sampleSize = (std::min)(bytes.size(), sampleLimit);
    qsizetype suspicious = 0;
    for (qsizetype index = 0; index < sampleSize; ++index) {
        const uchar character = static_cast<uchar>(bytes.at(index));
        if (character == 0
            || (character < 0x20
                && character != '\n'
                && character != '\r'
                && character != '\t')) {
            ++suspicious;
        }
    }
    return suspicious > 0 || suspicious * 100 > sampleSize * 15;
}
} // namespace

namespace MessagePayload {

Plan planStorage(
    const QString &topic,
    const QByteArray &payloadBytes,
    int configuredLimit,
    bool compactPreview)
{
    Plan plan;
    plan.originalSize = payloadBytes.size();

    const qint64 maxBytes = configuredLimit > 0
        ? configuredLimit
        : kHardPayloadLimitBytes;
    const qint64 previewLimit = compactPreview
        ? kPressurePayloadPreviewBytes
        : kPayloadPreviewBytes;
    const bool binary = looksBinary(
        payloadBytes,
        compactPreview ? qsizetype(1024) : qsizetype(4096));
    const QByteArray previewBytes = payloadBytes.left(
        (std::min)(payloadBytes.size(), qsizetype(previewLimit)));
    plan.preview = binary
        ? QString::fromLatin1(
              payloadBytes.left((std::min)(payloadBytes.size(), qsizetype(64)))
                  .toHex(' ')
                  .toUpper())
        : QString::fromUtf8(previewBytes);

    if (plan.originalSize > maxBytes) {
        plan.state = QStringLiteral("skipped");
        plan.allowFullProcessing = false;
        plan.hash = QString::fromLatin1(
            QCryptographicHash::hash(payloadBytes, QCryptographicHash::Sha256).toHex());
        plan.shouldReport = true;
        plan.reportMessage = QStringLiteral(
            "Payload skipped on %1: %2 exceeds the configured limit of %3. SHA-256: %4")
                                 .arg(
                                     topic,
                                     formatByteCount(plan.originalSize),
                                     formatByteCount(maxBytes),
                                     plan.hash);
        return plan;
    }

    plan.storedBytes = payloadBytes;
    if (binary) {
        plan.state = QStringLiteral("raw_only");
        plan.shouldReport = true;
        plan.reportMessage = QStringLiteral("Payload stored as raw bytes on %1: %2.")
                                 .arg(topic, formatByteCount(plan.originalSize));
    } else if (plan.originalSize > kPayloadPreviewBytes) {
        plan.state = QStringLiteral("truncated");
        plan.shouldReport = true;
        plan.reportMessage = QStringLiteral(
            "Payload truncated for display on %1: showing %2 of %3.")
                                 .arg(
                                     topic,
                                     formatByteCount(kPayloadPreviewBytes),
                                     formatByteCount(plan.originalSize));
    }
    return plan;
}

bool requiresBackgroundParse(PayloadFormat format)
{
    return format == PayloadFormat::Json
        || format == PayloadFormat::Cbor
        || format == PayloadFormat::MsgPack;
}

} // namespace MessagePayload
