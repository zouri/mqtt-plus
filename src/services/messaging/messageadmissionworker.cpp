#include "messageadmissionworker.h"

#include "presentation/eventrenderer.h"
#include "services/payload/payloadcodec.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDeadlineTimer>
#include <QMetaObject>
#include <QMutexLocker>
#include <QThread>
#include <QTimeZone>

#include <algorithm>
#include <utility>

namespace {
constexpr qint64 kPayloadPreviewBytes = 64 * 1024;
constexpr qint64 kPressurePayloadPreviewBytes = 4 * 1024;
constexpr qint64 kHardPayloadLimitBytes = 16 * 1024 * 1024;

struct PayloadStoragePlan {
    QByteArray storedBytes;
    QString preview;
    QString state = QStringLiteral("full");
    QString hash;
    qint64 originalSize = 0;
    bool allowFullProcessing = true;
    bool shouldReport = false;
    QString reportMessage;
};

struct SubscriptionMatch {
    const MessageAdmissionSubscription *displaySubscription = nullptr;
    QString alias;
    QString color;
    int payloadFormat = -1;
    QStringList activeTopics;
};

bool requiresBackgroundParse(PayloadFormat format)
{
    return format == PayloadFormat::Json
        || format == PayloadFormat::Cbor
        || format == PayloadFormat::MsgPack;
}

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
    for (qsizetype i = 0; i < sampleSize; ++i) {
        const uchar ch = static_cast<uchar>(bytes.at(i));
        if (ch == 0 || (ch < 0x20 && ch != '\n' && ch != '\r' && ch != '\t')) {
            ++suspicious;
        }
    }
    return suspicious > 0 || suspicious * 100 > sampleSize * 15;
}

PayloadStoragePlan makePayloadStoragePlan(
    const QString &topic,
    const QByteArray &payloadBytes,
    int configuredLimit,
    bool compactPreview)
{
    PayloadStoragePlan plan;
    plan.originalSize = payloadBytes.size();
    const qint64 maxBytes = configuredLimit > 0 ? configuredLimit : kHardPayloadLimitBytes;
    const qint64 previewLimit = compactPreview ? kPressurePayloadPreviewBytes : kPayloadPreviewBytes;
    const bool binary = looksBinary(payloadBytes, compactPreview ? qsizetype(1024) : qsizetype(4096));
    const QByteArray previewBytes = payloadBytes.left((std::min)(payloadBytes.size(), qsizetype(previewLimit)));
    if (binary) {
        const QByteArray hexBytes = payloadBytes.left((std::min)(payloadBytes.size(), qsizetype(64)));
        plan.preview = QString::fromLatin1(hexBytes.toHex(' ').toUpper());
        if (payloadBytes.size() > hexBytes.size()) {
            plan.preview.append(QStringLiteral(" ..."));
        }
    } else {
        plan.preview = QString::fromUtf8(previewBytes);
    }

    if (plan.originalSize > maxBytes) {
        plan.state = QStringLiteral("skipped");
        plan.allowFullProcessing = false;
        plan.hash = QString::fromLatin1(
            QCryptographicHash::hash(payloadBytes, QCryptographicHash::Sha256).toHex());
        plan.shouldReport = true;
        plan.reportMessage = QStringLiteral(
            "Payload skipped on %1: %2 exceeds the configured limit of %3. SHA-256: %4")
                                 .arg(topic, formatByteCount(plan.originalSize), formatByteCount(maxBytes), plan.hash);
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
        plan.reportMessage = QStringLiteral("Payload truncated for display on %1: showing %2 of %3.")
                                 .arg(topic, formatByteCount(kPayloadPreviewBytes), formatByteCount(plan.originalSize));
    }
    return plan;
}

SubscriptionMatch matchSubscriptions(
    const QVector<MessageAdmissionSubscription> &subscriptions,
    const QString &topic)
{
    SubscriptionMatch match;
    int bestDisplayScore = -1;
    int bestAliasScore = -1;
    int bestColorScore = -1;
    QString bestDisplayFilter;
    QString bestAliasFilter;
    QString bestColorFilter;

    for (const MessageAdmissionSubscription &subscription : subscriptions) {
        if (!PayloadCodec::topicFilterMatches(subscription.topic, topic)) {
            continue;
        }
        if (!subscription.paused) {
            match.activeTopics.append(subscription.topic);
        }

        const int score = PayloadCodec::topicSpecificityScore(subscription.topic);
        if (score > bestDisplayScore
            || (score == bestDisplayScore
                && (bestDisplayFilter.isEmpty() || subscription.topic < bestDisplayFilter))) {
            bestDisplayScore = score;
            bestDisplayFilter = subscription.topic;
            match.displaySubscription = &subscription;
            match.payloadFormat = subscription.format;
        }
        if (!subscription.alias.isEmpty()
            && (score > bestAliasScore
                || (score == bestAliasScore
                    && (bestAliasFilter.isEmpty() || subscription.topic < bestAliasFilter)))) {
            bestAliasScore = score;
            bestAliasFilter = subscription.topic;
            match.alias = subscription.alias;
        }
        if (!subscription.color.isEmpty()
            && (score > bestColorScore
                || (score == bestColorScore
                    && (bestColorFilter.isEmpty() || subscription.topic < bestColorFilter)))) {
            bestColorScore = score;
            bestColorFilter = subscription.topic;
            match.color = subscription.color;
        }
    }
    return match;
}

QVariantMap messageRecordRow(
    const MessageRecord &record,
    const QString &alias,
    const QString &color)
{
    return {
        {QStringLiteral("id"), record.id},
        {QStringLiteral("timestamp"), record.timestamp},
        {QStringLiteral("entry_type"), QStringLiteral("message")},
        {QStringLiteral("direction"), messageDirectionName(record.direction)},
        {QStringLiteral("topic"), record.topic},
        {QStringLiteral("topic_alias"), alias},
        {QStringLiteral("topic_color"), color},
        {QStringLiteral("qos"), record.qos},
        {QStringLiteral("retain"), record.retain},
        {QStringLiteral("retain_known"), record.retainKnown},
        {QStringLiteral("payload_bytes"), record.payloadBytes},
        {QStringLiteral("payload_size"), record.payloadSize},
        {QStringLiteral("payload_state"), record.payloadState},
        {QStringLiteral("payload_preview"), record.payloadPreview},
        {QStringLiteral("payload_hash"), record.payloadHash},
        {QStringLiteral("payload_format"), record.payloadFormat},
        {QStringLiteral("display_payload"), record.displayPayload},
        {QStringLiteral("display_format"), record.displayFormat},
        {QStringLiteral("display_error"), record.displayError},
        {QStringLiteral("display_state"), record.displayState},
        {QStringLiteral("processor_id"), record.processorId},
    };
}
} // namespace

MessageAdmissionWorker::MessageAdmissionWorker(
    MessageAdmissionLimits limits,
    QObject *parent)
    : QObject(parent)
    , m_limits(std::move(limits))
{
}

bool MessageAdmissionWorker::enqueue(IncomingMessageAdmissionTask task)
{
    const qint64 bytes = approximateBytes(task);
    bool accepted = false;
    {
        QMutexLocker locker(&m_mutex);
        const bool full = m_limits.maxMessages <= 0
            || m_queue.size() + m_processingCount + m_prepared.size()
                >= m_limits.maxMessages
            || bytes > m_limits.maxBytes
            || m_pendingBytes > m_limits.maxBytes - bytes;
        if (!m_accepting || full) {
            ++m_droppedMessages;
        } else {
            m_pendingBytes += bytes;
            m_queue.enqueue(std::move(task));
            accepted = true;
            requestWakeLocked();
        }
        updatePressureStateLocked();
    }
    emit queueStateChanged();
    return accepted;
}

bool MessageAdmissionWorker::drain(int timeoutMs)
{
    QDeadlineTimer deadline((std::max)(0, timeoutMs));
    QMutexLocker locker(&m_mutex);
    requestWakeLocked();
    while (!m_queue.isEmpty() || m_processingCount > 0) {
        if (!m_drainedCondition.wait(&m_mutex, deadline)) {
            return false;
        }
    }
    return true;
}

void MessageAdmissionWorker::stopAccepting()
{
    QMutexLocker locker(&m_mutex);
    m_accepting = false;
}

QVector<PreparedIncomingMessage> MessageAdmissionWorker::takePrepared()
{
    QMutexLocker locker(&m_mutex);
    QVector<PreparedIncomingMessage> prepared = std::move(m_prepared);
    m_prepared.clear();
    m_pendingBytes -= m_preparedBytes;
    m_pendingBytes = (std::max)(qint64(0), m_pendingBytes);
    m_preparedBytes = 0;
    updatePressureStateLocked();
    return prepared;
}

int MessageAdmissionWorker::pendingMessageCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_queue.size() + m_processingCount + m_prepared.size();
}

qint64 MessageAdmissionWorker::pendingBytes() const
{
    QMutexLocker locker(&m_mutex);
    return m_pendingBytes;
}

qint64 MessageAdmissionWorker::droppedMessageCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_droppedMessages;
}

MessageAdmissionWorker::PressureState MessageAdmissionWorker::pressureState() const
{
    QMutexLocker locker(&m_mutex);
    return m_pressureState;
}

void MessageAdmissionWorker::start()
{
    QMutexLocker locker(&m_mutex);
    m_started = true;
    requestWakeLocked();
}

void MessageAdmissionWorker::shutdown()
{
    QMutexLocker locker(&m_mutex);
    m_started = false;
    m_wakePending = false;
    m_drainedCondition.wakeAll();
}

void MessageAdmissionWorker::processBatch()
{
    QVector<IncomingMessageAdmissionTask> batch;
    {
        QMutexLocker locker(&m_mutex);
        m_wakePending = false;
        const int count = (std::min)(m_limits.batchSize, int(m_queue.size()));
        batch.reserve(count);
        for (int i = 0; i < count; ++i) {
            batch.append(m_queue.dequeue());
        }
        m_processingCount = count;
    }

    QVector<PreparedIncomingMessage> prepared;
    prepared.reserve(batch.size());
    for (const IncomingMessageAdmissionTask &task : std::as_const(batch)) {
        prepared.append(prepare(task));
    }

    bool hasMore = false;
    {
        QMutexLocker locker(&m_mutex);
        for (const IncomingMessageAdmissionTask &task : std::as_const(batch)) {
            m_preparedBytes += approximateBytes(task);
        }
        m_prepared.append(std::move(prepared));
        m_processingCount = 0;
        hasMore = !m_queue.isEmpty();
        updatePressureStateLocked();
        if (hasMore) {
            requestWakeLocked();
        } else {
            m_drainedCondition.wakeAll();
        }
    }
    emit preparedAvailable();
    emit queueStateChanged();
}

PreparedIncomingMessage MessageAdmissionWorker::prepare(
    const IncomingMessageAdmissionTask &task)
{
    PreparedIncomingMessage result;
    result.sessionId = task.sessionId;
    result.topic = task.topic;
    result.receivedAtMs = task.receivedAtMs;
    result.payloadBytes = task.payloadBytes.size();
    if (!task.context
        || !task.context->capturePolicy.accepts(MessageDirection::Incoming, task.topic)
        || (task.context->outputPaused && !task.context->saveMessagesWhenOutputPaused)) {
        return result;
    }

    result.captured = true;
    const SubscriptionMatch match = matchSubscriptions(task.context->subscriptions, task.topic);
    result.activeSubscriptionTopics = match.activeTopics;
    const PayloadStoragePlan payloadPlan = makePayloadStoragePlan(
        task.topic,
        task.payloadBytes,
        task.context->maxPayloadBytes,
        task.pressureSkipsParsing);
    if (payloadPlan.shouldReport) {
        result.reportKey = QStringLiteral("%1|%2|%3")
                               .arg(task.sessionId, task.topic, payloadPlan.state);
        result.reportMessage = payloadPlan.reportMessage;
    }

    const MessageAdmissionSubscription *displaySubscription = match.displaySubscription;
    const ProcessorReference *processorReference = displaySubscription
            && !displaySubscription->processor.processorId.isEmpty()
        ? &displaySubscription->processor
        : nullptr;
    const int payloadFormat = match.payloadFormat >= 0
        ? match.payloadFormat
        : static_cast<int>(PayloadCodec::resolveTopicFormat(
            task.context->subscriptionFormats,
            task.topic));

    MessageRecord &record = result.record;
    record.sessionId = task.sessionId;
    record.timestamp = QDateTime::fromMSecsSinceEpoch(
                           task.receivedAtMs,
                           QTimeZone::UTC)
                           .toString(Qt::ISODateWithMs);
    record.direction = MessageDirection::Incoming;
    record.topic = task.topic;
    record.payloadBytes = payloadPlan.storedBytes;
    record.payloadPreview = payloadPlan.preview;
    record.payloadState = payloadPlan.state;
    record.payloadSize = payloadPlan.originalSize;
    record.payloadHash = payloadPlan.hash;
    record.payloadFormat = payloadFormat;

    if (processorReference) {
        record.processorId = processorReference->processorId;
        result.processorParameters = processorReference->parameters;
        if (displaySubscription->processorRevision) {
            result.processorRevision = displaySubscription->processorRevision;
            record.processorRevisionId = result.processorRevision->id;
            record.processorName = displaySubscription->processorName;
            record.processorLanguageId = result.processorRevision->languageId;
            record.processorRuntimeId = result.processorRevision->runtimeId;
            record.processorContentHash = result.processorRevision->contentHash;
        } else {
            record.processorName = displaySubscription->processorName;
        }
    }

    result.parsingRequired = requiresBackgroundParse(PayloadCodec::formatFromInt(payloadFormat))
        || processorReference;
    if (!result.parsingRequired) {
        record.displayState = messageParseStateName(MessageParseState::NotRequired);
    } else if (processorReference && !result.processorRevision) {
        record.displayState = messageParseStateName(MessageParseState::Failed);
        record.displayError = displaySubscription->processorResolutionError.isEmpty()
            ? QStringLiteral("Selected Processor is unavailable.")
            : displaySubscription->processorResolutionError;
        record.displayFormat = QStringLiteral("Processor Error");
        record.processorExecutionState = QStringLiteral("preparation_failed");
        record.processorExecutionErrorCode = QStringLiteral("processor_not_found");
        record.processorExecutionError = record.displayError;
        result.parsingRequired = false;
    } else if (!payloadPlan.allowFullProcessing) {
        record.displayState = messageParseStateName(MessageParseState::Failed);
        record.displayError = processorReference
            ? QStringLiteral("Processor skipped because the payload exceeds the configured size limit.")
            : QStringLiteral("Payload parsing skipped because the payload exceeds the configured size limit.");
        record.displayFormat = processorReference
            ? QStringLiteral("Processor Error")
            : PayloadCodec::formatName(PayloadCodec::formatFromInt(payloadFormat));
        result.parsingRequired = false;
    } else if (task.pressureSkipsParsing) {
        record.displayState = messageParseStateName(MessageParseState::SkippedOverload);
        record.displayError = QStringLiteral(
            "Payload parsing skipped while the capture pipeline is under pressure.");
        record.displayFormat = processorReference
            ? QStringLiteral("Processor skipped")
            : PayloadCodec::formatName(PayloadCodec::formatFromInt(payloadFormat));
        result.parsingRequired = false;
        result.parsingSkippedForPressure = true;
    } else {
        record.displayState = messageParseStateName(MessageParseState::Pending);
        if (processorReference) {
            record.processorExecutionState = QStringLiteral("pending");
        }
    }

    result.renderedRow = EventRenderer::renderHistoryRow(
        messageRecordRow(record, match.alias, match.color),
        {},
        {},
        {});
    return result;
}

qint64 MessageAdmissionWorker::approximateBytes(
    const IncomingMessageAdmissionTask &task)
{
    return task.payloadBytes.size()
        + qint64(task.sessionId.size() + task.topic.size()) * qint64(sizeof(QChar))
        + qint64(sizeof(IncomingMessageAdmissionTask));
}

void MessageAdmissionWorker::requestWakeLocked()
{
    if (!m_started || m_wakePending || m_queue.isEmpty()) {
        return;
    }
    m_wakePending = true;
    QMetaObject::invokeMethod(this, &MessageAdmissionWorker::processBatch, Qt::QueuedConnection);
}

void MessageAdmissionWorker::updatePressureStateLocked()
{
    const int pendingMessages = m_queue.size() + m_processingCount + m_prepared.size();
    if (m_droppedMessages > 0
        && (pendingMessages >= m_limits.maxMessages
            || m_pendingBytes >= m_limits.maxBytes)) {
        m_pressureState = PressureState::Dropping;
        return;
    }
    if (pendingMessages >= m_limits.maxMessages / 2
        || m_pendingBytes >= m_limits.maxBytes / 2) {
        m_pressureState = PressureState::Elevated;
        return;
    }
    m_pressureState = PressureState::Normal;
}
