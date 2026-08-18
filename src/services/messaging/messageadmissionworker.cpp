#include "messageadmissionworker.h"

#include "domain/mqtttopicfilter.h"
#include "presentation/eventrenderer.h"
#include "services/messaging/messagepayloadplan.h"
#include "services/payload/payloadcodec.h"

#include <QDateTime>
#include <QDeadlineTimer>
#include <QMetaObject>
#include <QMutexLocker>
#include <QThread>
#include <QTimeZone>

#include <algorithm>
#include <utility>

namespace {
struct SubscriptionMatch {
    const MessageAdmissionSubscription *displaySubscription = nullptr;
    QString alias;
    QString color;
    int payloadFormat = -1;
    QStringList activeTopics;
};

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
        if (!MqttTopicFilter::matches(subscription.topic, topic)) {
            continue;
        }
        if (!subscription.paused) {
            match.activeTopics.append(subscription.topic);
        }

        const int score = MqttTopicFilter::specificityScore(subscription.topic);
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
    const MessagePayload::Plan payloadPlan = MessagePayload::planStorage(
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
    record.qos = task.qos;
    record.retain = task.retain;
    record.retainKnown = task.qos >= 0;
    record.payloadBytes = payloadPlan.storedBytes;
    record.payloadPreview = payloadPlan.preview;
    record.payloadState = payloadPlan.state;
    record.payloadSize = payloadPlan.originalSize;
    record.payloadHash = payloadPlan.hash;
    record.payloadFormat = payloadFormat;
    record.publishProperties = task.publishProperties;

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

    result.parsingRequired = MessagePayload::requiresBackgroundParse(
                                 PayloadCodec::formatFromInt(payloadFormat))
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

    result.renderedRow = EventRenderer::renderMessageRow(
        record,
        {},
        {},
        {},
        match.color,
        match.alias);
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
