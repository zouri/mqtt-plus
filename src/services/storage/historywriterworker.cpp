#include "historywriterworker.h"

#include "services/payload/payloadcodec.h"
#include "services/storage/historystore.h"

#include <QDeadlineTimer>
#include <QHash>
#include <QMetaObject>
#include <QMutexLocker>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <utility>

namespace {
constexpr qsizetype kMaxInlineExpandedCharacters = 256 * 1024;
constexpr qsizetype kMaxInlineExpandedLines = 4096;

qint64 stringBytes(const QString &value)
{
    return qint64(value.size()) * qint64(sizeof(QChar));
}

bool fitsInlineText(const QString &text)
{
    return text.size() <= kMaxInlineExpandedCharacters
        && text.count(QLatin1Char('\n')) < kMaxInlineExpandedLines;
}
} // namespace

HistoryWriterWorker::HistoryWriterWorker(
    QString dataPath,
    qint64 nextMessageId,
    HistoryWriterLimits limits,
    QObject *parent)
    : QObject(parent)
    , m_dataPath(std::move(dataPath))
    , m_limits(std::move(limits))
    , m_nextMessageId(nextMessageId > 0 ? nextMessageId : 0)
    , m_retryDelayMs((std::max)(1, m_limits.initialRetryMs))
{
}

HistoryWriterWorker::~HistoryWriterWorker()
{
    if (QThread::currentThread() == thread()) {
        shutdown();
    }
}

qint64 HistoryWriterWorker::enqueueMessage(const MessageRecord &message)
{
    const qint64 bytes = approximateBytes(message);
    bool stateChanged = false;
    bool queueBecameNonEmpty = false;
    qint64 messageId = 0;
    {
        QMutexLocker locker(&m_mutex);
        const bool messageTooLarge = bytes > m_limits.maxBytes;
        const bool queueCountFull = m_limits.maxMessages <= 0
            || m_pendingCaptureCount >= m_limits.maxMessages;
        const bool queueBytesFull = !messageTooLarge
            && (m_limits.maxBytes <= 0
                || m_pendingBytes > m_limits.maxBytes - bytes);
        const bool exceedsCount = queueCountFull;
        const bool exceedsBytes = messageTooLarge || queueBytesFull;
        if (!m_accepting || m_nextMessageId <= 0 || exceedsCount || exceedsBytes) {
            ++m_droppedMessages;
            if (m_accepting && (queueCountFull || queueBytesFull)) {
                m_captureQueueSaturated = true;
            }
            if (m_started && !m_dropNotificationPending) {
                m_dropNotificationPending = true;
                QMetaObject::invokeMethod(this, &HistoryWriterWorker::notifyDropped, Qt::QueuedConnection);
            }
            stateChanged = updatePressureStateLocked();
        } else {
            queueBecameNonEmpty = m_queue.isEmpty();
            MessageRecord pending = message;
            pending.id = m_nextMessageId++;
            if (pending.payloadState.isEmpty()) {
                pending.payloadState = QStringLiteral("full");
            }
            if (pending.payloadSize < 0) {
                pending.payloadSize = pending.payloadBytes.size();
            }
            messageId = pending.id;
            m_pendingBytes += bytes;
            WriteOperation operation;
            operation.type = WriteOperation::Type::Capture;
            operation.message = std::move(pending);
            m_queue.enqueue(std::move(operation));
            ++m_pendingCaptureCount;
            stateChanged = updatePressureStateLocked();
            requestWakeLocked();
        }
    }

    if (stateChanged || queueBecameNonEmpty) {
        emit queueStateChanged();
    }
    return messageId;
}

bool HistoryWriterWorker::enqueueParseResult(const ParseOutcome &result)
{
    if (result.messageId <= 0) {
        return false;
    }

    ParseOutcome pending = result;
    qint64 bytes = approximateBytes(pending);
    if (bytes > m_limits.maxBytes) {
        const QString error = QStringLiteral(
            "Processor result discarded because the history writer is overloaded.");
        pending.displayPayload.clear();
        pending.displayFormat = QStringLiteral("Processor Error");
        pending.displayError = error;
        pending.processorResultCbor.clear();
        pending.processorResultPreview.clear();
        pending.processorExecutionState = QStringLiteral("skipped_overload");
        pending.processorExecutionErrorCode = QStringLiteral("history_writer_overloaded");
        pending.processorExecutionError = error;
        pending.state = MessageParseState::SkippedOverload;
        bytes = approximateBytes(pending);
    }

    bool stateChanged = false;
    bool queueBecameNonEmpty = false;
    bool accepted = false;
    {
        QMutexLocker locker(&m_mutex);
        const bool exceedsCount = m_queue.size() >= m_limits.maxMessages * 2;
        const bool exceedsBytes = bytes > m_limits.maxBytes
            || m_pendingBytes > m_limits.maxBytes - bytes;
        if (!m_accepting || exceedsCount || exceedsBytes) {
            ++m_droppedParseResults;
            if (m_started && !m_dropNotificationPending) {
                m_dropNotificationPending = true;
                QMetaObject::invokeMethod(this, &HistoryWriterWorker::notifyDropped, Qt::QueuedConnection);
            }
            stateChanged = updatePressureStateLocked();
        } else {
            queueBecameNonEmpty = m_queue.isEmpty();
            WriteOperation operation;
            operation.type = WriteOperation::Type::ParseUpdate;
            operation.parseResult = std::move(pending);
            m_pendingBytes += bytes;
            m_queue.enqueue(std::move(operation));
            accepted = true;
            stateChanged = updatePressureStateLocked();
            requestWakeLocked();
        }
    }

    if (stateChanged || queueBecameNonEmpty) {
        emit queueStateChanged();
    }
    return accepted;
}

int HistoryWriterWorker::pendingMessageCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_queue.size();
}

qint64 HistoryWriterWorker::pendingBytes() const
{
    QMutexLocker locker(&m_mutex);
    return m_pendingBytes;
}

qint64 HistoryWriterWorker::droppedMessageCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_droppedMessages;
}

qint64 HistoryWriterWorker::droppedParseResultCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_droppedParseResults;
}

QString HistoryWriterWorker::lastError() const
{
    QMutexLocker locker(&m_mutex);
    return m_lastError;
}

HistoryWriterWorker::PressureState HistoryWriterWorker::pressureState() const
{
    QMutexLocker locker(&m_mutex);
    return m_pressureState;
}

std::optional<MessageRecord> HistoryWriterWorker::pendingMessage(qint64 messageId) const
{
    std::optional<MessageRecord> message;
    QMutexLocker locker(&m_mutex);
    for (const WriteOperation &operation : m_queue) {
        if (operationMessageId(operation) != messageId) {
            continue;
        }
        if (operation.type == WriteOperation::Type::Capture) {
            message = operation.message;
        } else if (message) {
            applyParseOutcome(*message, operation.parseResult);
        }
    }
    return message;
}

std::optional<ParseOutcome> HistoryWriterWorker::pendingParseResult(qint64 messageId) const
{
    std::optional<ParseOutcome> result;
    QMutexLocker locker(&m_mutex);
    for (const WriteOperation &operation : m_queue) {
        if (operation.type == WriteOperation::Type::ParseUpdate
            && operation.parseResult.messageId == messageId) {
            result = operation.parseResult;
        }
    }
    return result;
}

QVector<MessageRecord> HistoryWriterWorker::pendingMessages(const QString &sessionId) const
{
    QVector<MessageRecord> messages;
    QMutexLocker locker(&m_mutex);
    messages.reserve(m_queue.size());
    QHash<qint64, int> indexes;
    for (const WriteOperation &operation : m_queue) {
        if (operationSessionId(operation) != sessionId) {
            continue;
        }
        if (operation.type == WriteOperation::Type::Capture) {
            indexes.insert(operation.message.id, messages.size());
            messages.append(operation.message);
        } else {
            const auto index = indexes.constFind(operation.parseResult.messageId);
            if (index != indexes.cend()) {
                applyParseOutcome(messages[*index], operation.parseResult);
            }
        }
    }
    return messages;
}

QVector<ParseOutcome> HistoryWriterWorker::pendingParseResults(const QString &sessionId) const
{
    QVector<ParseOutcome> results;
    QMutexLocker locker(&m_mutex);
    for (const WriteOperation &operation : m_queue) {
        if (operation.type == WriteOperation::Type::ParseUpdate
            && operation.parseResult.sessionId == sessionId) {
            results.append(operation.parseResult);
        }
    }
    return results;
}

bool HistoryWriterWorker::drain(int timeoutMs)
{
    if (QThread::currentThread() == thread()) {
        while (pendingMessageCount() > 0) {
            const int countBeforeFlush = pendingMessageCount();
            flushBatch();
            if (pendingMessageCount() >= countBeforeFlush) {
                return false;
            }
        }
        return true;
    }

    QDeadlineTimer deadline((std::max)(0, timeoutMs));
    QMutexLocker locker(&m_mutex);
    if (m_queue.isEmpty()) {
        return true;
    }

    m_drainRequested = true;
    requestWakeLocked();
    while (!m_queue.isEmpty()) {
        if (!m_drainedCondition.wait(&m_mutex, deadline)) {
            m_drainRequested = false;
            return false;
        }
    }
    m_drainRequested = false;
    return true;
}

void HistoryWriterWorker::stopAccepting()
{
    QMutexLocker locker(&m_mutex);
    m_accepting = false;
}

void HistoryWriterWorker::start()
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_started) {
            return;
        }
    }

    m_flushTimer = new QTimer(this);
    m_flushTimer->setSingleShot(true);
    connect(m_flushTimer, &QTimer::timeout, this, &HistoryWriterWorker::flushBatch);
    if (!ensureStore()) {
        scheduleRetry(m_store ? m_store->lastError() : QString());
    }

    QMutexLocker locker(&m_mutex);
    m_started = true;
    if ((m_droppedMessages > 0 || m_droppedParseResults > 0)
        && !m_dropNotificationPending) {
        m_dropNotificationPending = true;
        QMetaObject::invokeMethod(this, &HistoryWriterWorker::notifyDropped, Qt::QueuedConnection);
    }
    if (!m_queue.isEmpty()) {
        requestWakeLocked();
    }
}

void HistoryWriterWorker::shutdown()
{
    {
        QMutexLocker locker(&m_mutex);
        m_started = false;
        m_wakePending = false;
        m_dropNotificationPending = false;
    }
    if (m_flushTimer) {
        m_flushTimer->stop();
        delete m_flushTimer;
        m_flushTimer = nullptr;
    }
    m_store.reset();

    QMutexLocker locker(&m_mutex);
    m_drainedCondition.wakeAll();
}

void HistoryWriterWorker::loadExpandedMessage(qint64 messageId)
{
    if (messageId <= 0) {
        emit expandedMessageLoaded(messageId, QString(), QStringLiteral("unavailable"));
        return;
    }

    QByteArray payloadBytes;
    QString parsedPayload;
    QString parseState;
    int payloadFormat = -1;

    if (const auto pending = pendingMessage(messageId)) {
        payloadBytes = pending->payloadBytes;
        parsedPayload = pending->displayPayload;
        parseState = pending->displayState;
        payloadFormat = pending->payloadFormat;
    } else {
        if (!ensureStore()) {
            emit expandedMessageLoaded(messageId, QString(), QStringLiteral("unavailable"));
            return;
        }
        const auto stored = m_store->loadMessage(messageId);
        if (!stored) {
            emit expandedMessageLoaded(messageId, QString(), QStringLiteral("unavailable"));
            return;
        }
        payloadBytes = stored->payloadBytes;
        parsedPayload = stored->displayPayload;
        parseState = stored->displayState;
        payloadFormat = stored->payloadFormat;
        if (const auto pendingResult = pendingParseResult(messageId)) {
            parsedPayload = pendingResult->displayPayload;
            parseState = messageParseStateName(pendingResult->state);
        }
    }

    QString expandedPayload;
    if (parseState == QStringLiteral("succeeded")) {
        expandedPayload = parsedPayload;
    } else if (!payloadBytes.isEmpty()) {
        QString decodeError;
        expandedPayload = PayloadCodec::decodeForDisplay(
            PayloadCodec::formatFromInt(payloadFormat),
            payloadBytes,
            decodeError);
        if (!decodeError.isEmpty()) {
            emit expandedMessageLoaded(messageId, QString(), QStringLiteral("unavailable"));
            return;
        }
    }

    if (!fitsInlineText(expandedPayload)) {
        emit expandedMessageLoaded(messageId, QString(), QStringLiteral("too_large"));
        return;
    }
    emit expandedMessageLoaded(messageId, expandedPayload, QStringLiteral("ready"));
}

void HistoryWriterWorker::wake()
{
    bool flushImmediately = false;
    {
        QMutexLocker locker(&m_mutex);
        m_wakePending = false;
        if (m_queue.isEmpty()) {
            m_drainedCondition.wakeAll();
            return;
        }
        flushImmediately = m_drainRequested
            || (!m_storageDegraded && m_queue.size() >= m_limits.batchSize);
    }

    if (flushImmediately) {
        flushBatch();
    } else if (m_flushTimer && !m_flushTimer->isActive()) {
        m_flushTimer->start(m_storageDegraded ? m_retryDelayMs : m_limits.flushIntervalMs);
    }
}

void HistoryWriterWorker::flushBatch()
{
    if (!ensureStore()) {
        scheduleRetry(m_store ? m_store->lastError() : lastError());
        return;
    }

    QVector<WriteOperation> batch;
    {
        QMutexLocker locker(&m_mutex);
        if (m_queue.isEmpty()) {
            m_drainedCondition.wakeAll();
            return;
        }
        const int batchSize = (std::min)(m_limits.batchSize, int(m_queue.size()));
        batch.reserve(batchSize);
        for (int index = 0; index < batchSize; ++index) {
            batch.append(m_queue.at(index));
        }
    }

    QVector<MessageRecord> captures;
    QVector<ParseOutcome> parseResults;
    captures.reserve(batch.size());
    parseResults.reserve(batch.size());
    for (const WriteOperation &operation : std::as_const(batch)) {
        if (operation.type == WriteOperation::Type::Capture) {
            captures.append(operation.message);
        } else {
            parseResults.append(operation.parseResult);
        }
    }

    const HistoryWriteResult result = m_store->writeMessageBatch(captures, parseResults);
    if (!result.ok) {
        scheduleRetry(result.error);
        return;
    }

    bool stateChanged = false;
    bool errorCleared = false;
    {
        QMutexLocker locker(&m_mutex);
        for (const WriteOperation &operation : batch) {
            if (m_queue.isEmpty()
                || m_queue.head().type != operation.type
                || operationMessageId(m_queue.head()) != operationMessageId(operation)) {
                break;
            }
            m_pendingBytes -= approximateBytes(m_queue.head());
            if (m_queue.head().type == WriteOperation::Type::Capture) {
                --m_pendingCaptureCount;
            }
            m_queue.dequeue();
        }
        m_pendingBytes = (std::max)(qint64(0), m_pendingBytes);
        errorCleared = !m_lastError.isEmpty();
        m_lastError.clear();
        m_storageDegraded = false;
        m_retryDelayMs = (std::max)(1, m_limits.initialRetryMs);
        stateChanged = updatePressureStateLocked();
        if (m_queue.isEmpty()) {
            m_drainedCondition.wakeAll();
        }
    }

    if (errorCleared) {
        emit storageErrorChanged(QString());
    }
    if (stateChanged || !batch.isEmpty()) {
        emit queueStateChanged();
    }
    emit messagesPersisted(result.sessionIds, result.messageCount);
    scheduleNextFlush();
}

void HistoryWriterWorker::notifyDropped()
{
    qint64 droppedMessages = 0;
    qint64 droppedParseResults = 0;
    {
        QMutexLocker locker(&m_mutex);
        m_dropNotificationPending = false;
        droppedMessages = m_droppedMessages;
        droppedParseResults = m_droppedParseResults;
    }
    emit messagesDropped(droppedMessages);
    emit parseResultsDropped(droppedParseResults);
    emit queueStateChanged();
}

qint64 HistoryWriterWorker::approximateBytes(const MessageRecord &message)
{
    return message.payloadBytes.size()
        + message.processorResultCbor.size()
        + stringBytes(message.sessionId)
        + stringBytes(message.timestamp)
        + stringBytes(message.topic)
        + stringBytes(message.displayPayload)
        + stringBytes(message.displayFormat)
        + stringBytes(message.displayError)
        + stringBytes(message.displayState)
        + stringBytes(message.processorId)
        + stringBytes(message.processorRevisionId)
        + stringBytes(message.processorName)
        + stringBytes(message.processorLanguageId)
        + stringBytes(message.processorRuntimeId)
        + stringBytes(message.processorContentHash)
        + stringBytes(message.processorResultPreview)
        + stringBytes(message.processorExecutionState)
        + stringBytes(message.processorExecutionErrorCode)
        + stringBytes(message.processorExecutionError)
        + stringBytes(message.payloadPreview)
        + stringBytes(message.payloadState)
        + stringBytes(message.payloadHash)
        + qint64(sizeof(MessageRecord));
}

qint64 HistoryWriterWorker::approximateBytes(const ParseOutcome &result)
{
    return result.processorResultCbor.size()
        + stringBytes(result.sessionId)
        + stringBytes(result.displayPayload)
        + stringBytes(result.displayFormat)
        + stringBytes(result.displayError)
        + stringBytes(result.processorId)
        + stringBytes(result.processorRevisionId)
        + stringBytes(result.processorName)
        + stringBytes(result.processorLanguageId)
        + stringBytes(result.processorRuntimeId)
        + stringBytes(result.processorContentHash)
        + stringBytes(result.processorResultPreview)
        + stringBytes(result.processorExecutionState)
        + stringBytes(result.processorExecutionErrorCode)
        + stringBytes(result.processorExecutionError)
        + qint64(sizeof(ParseOutcome));
}

qint64 HistoryWriterWorker::approximateBytes(const WriteOperation &operation)
{
    return operation.type == WriteOperation::Type::Capture
        ? approximateBytes(operation.message)
        : approximateBytes(operation.parseResult);
}

qint64 HistoryWriterWorker::operationMessageId(const WriteOperation &operation)
{
    return operation.type == WriteOperation::Type::Capture
        ? operation.message.id
        : operation.parseResult.messageId;
}

QString HistoryWriterWorker::operationSessionId(const WriteOperation &operation)
{
    return operation.type == WriteOperation::Type::Capture
        ? operation.message.sessionId
        : operation.parseResult.sessionId;
}

HistoryWriterWorker::PressureState HistoryWriterWorker::pressureStateForQueueLocked() const
{
    if (m_captureQueueSaturated) {
        return PressureState::Dropping;
    }
    if (m_storageDegraded) {
        return PressureState::Degraded;
    }

    const bool aboveHighWater = m_pendingCaptureCount >= m_limits.highWaterMessages
        || m_pendingBytes >= m_limits.highWaterBytes;
    const bool belowLowWater = m_pendingCaptureCount <= m_limits.lowWaterMessages
        && m_pendingBytes <= m_limits.lowWaterBytes;
    if (aboveHighWater) {
        return PressureState::Elevated;
    }
    if (m_pressureState != PressureState::Normal && !belowLowWater) {
        return PressureState::Elevated;
    }
    return PressureState::Normal;
}

bool HistoryWriterWorker::updatePressureStateLocked()
{
    const bool capacityDisabled = m_limits.maxMessages <= 0 || m_limits.maxBytes <= 0;
    const bool belowHardCapacity = m_pendingCaptureCount < m_limits.maxMessages
        && m_pendingBytes < m_limits.maxBytes;
    const bool belowLowWater = m_pendingCaptureCount <= m_limits.lowWaterMessages
        && m_pendingBytes <= m_limits.lowWaterBytes;
    if (m_captureQueueSaturated
        && !capacityDisabled
        && belowHardCapacity
        && belowLowWater) {
        m_captureQueueSaturated = false;
    }

    const PressureState next = pressureStateForQueueLocked();
    if (next == m_pressureState) {
        return false;
    }
    m_pressureState = next;
    return true;
}

void HistoryWriterWorker::requestWakeLocked()
{
    if (!m_started || m_wakePending) {
        return;
    }
    m_wakePending = true;
    QMetaObject::invokeMethod(this, &HistoryWriterWorker::wake, Qt::QueuedConnection);
}

void HistoryWriterWorker::scheduleRetry(const QString &error)
{
    const QString normalizedError = error.isEmpty()
        ? QStringLiteral("Cannot write message history.")
        : error;
    bool errorChanged = false;
    bool stateChanged = false;
    int retryDelay = 0;
    {
        QMutexLocker locker(&m_mutex);
        errorChanged = m_lastError != normalizedError;
        m_lastError = normalizedError;
        m_storageDegraded = true;
        stateChanged = updatePressureStateLocked();
        retryDelay = m_retryDelayMs;
        m_retryDelayMs = (std::min)(m_limits.maxRetryMs, m_retryDelayMs * 2);
    }

    if (errorChanged) {
        emit storageErrorChanged(normalizedError);
    }
    if (stateChanged) {
        emit queueStateChanged();
    }
    if (m_flushTimer) {
        m_flushTimer->start(retryDelay);
    }
}

void HistoryWriterWorker::scheduleNextFlush()
{
    bool hasPending = false;
    bool drainRequested = false;
    {
        QMutexLocker locker(&m_mutex);
        hasPending = !m_queue.isEmpty();
        drainRequested = m_drainRequested;
    }
    if (!hasPending) {
        return;
    }
    if (drainRequested) {
        QMetaObject::invokeMethod(this, &HistoryWriterWorker::flushBatch, Qt::QueuedConnection);
    } else if (m_flushTimer) {
        m_flushTimer->start(m_limits.flushIntervalMs);
    }
}

bool HistoryWriterWorker::ensureStore()
{
    if (!m_store || !m_store->isReady()) {
        m_store = std::make_unique<HistoryStore>(m_dataPath, m_limits.busyTimeoutMs);
        if (!m_store->isReady()) {
            return false;
        }
    }

    {
        QMutexLocker locker(&m_mutex);
        if (m_nextMessageId > 0) {
            return true;
        }
    }

    const qint64 nextMessageId = m_store->nextMessageId();
    if (nextMessageId <= 0) {
        return false;
    }

    QMutexLocker locker(&m_mutex);
    if (m_nextMessageId <= 0) {
        m_nextMessageId = nextMessageId;
    }
    return true;
}
