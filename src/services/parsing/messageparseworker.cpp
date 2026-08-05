#include "messageparseworker.h"

#include "services/payload/payloadcodec.h"
#include "services/processors/defaultmessageprocessorengine.h"
#include "services/processors/messageprocessorengine.h"
#include "services/processors/processorvaluecodec.h"

#include <QDeadlineTimer>
#include <QMetaObject>
#include <QMutexLocker>
#include <QThread>

#include <algorithm>
#include <utility>

namespace {
qint64 stringBytes(const QString &value)
{
    return qint64(value.size()) * qint64(sizeof(QChar));
}
} // namespace

MessageParseWorker::MessageParseWorker(MessageParseLimits limits, QObject *parent)
    : QObject(parent)
    , m_limits(std::move(limits))
{
}

MessageParseWorker::~MessageParseWorker()
{
    if (QThread::currentThread() == thread()) {
        shutdown();
    }
}

bool MessageParseWorker::enqueueTask(const MessageParseTask &task)
{
    const qint64 bytes = approximateBytes(task);
    bool accepted = false;
    bool queueBecameNonEmpty = false;
    bool stateChanged = false;
    {
        QMutexLocker locker(&m_mutex);
        const bool taskTooLarge = bytes > m_limits.maxBytes;
        const bool queueCountFull = m_limits.maxTasks <= 0
            || m_queue.size() + m_processingTaskCount >= m_limits.maxTasks;
        const bool queueBytesFull = !taskTooLarge
            && (m_limits.maxBytes <= 0
                || m_pendingBytes > m_limits.maxBytes - bytes);
        const bool exceedsCount = queueCountFull;
        const bool exceedsBytes = taskTooLarge || queueBytesFull;
        if (m_accepting && !exceedsCount && !exceedsBytes) {
            queueBecameNonEmpty = m_queue.isEmpty() && m_processingTaskCount == 0;
            m_queue.enqueue(task);
            m_pendingBytes += bytes;
            accepted = true;
            requestWakeLocked();
        } else {
            ++m_droppedTasks;
            if (m_accepting && (queueCountFull || queueBytesFull)) {
                m_queueSaturated = true;
            }
            if (m_started && !m_dropNotificationPending) {
                m_dropNotificationPending = true;
                QMetaObject::invokeMethod(
                    this,
                    &MessageParseWorker::notifyDropped,
                    Qt::QueuedConnection);
            }
        }
        stateChanged = updatePressureStateLocked();
    }

    if (queueBecameNonEmpty || stateChanged) {
        emit queueStateChanged();
    }
    return accepted;
}

int MessageParseWorker::pendingTaskCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_queue.size() + m_processingTaskCount;
}

qint64 MessageParseWorker::pendingBytes() const
{
    QMutexLocker locker(&m_mutex);
    return m_pendingBytes;
}

qint64 MessageParseWorker::droppedTaskCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_droppedTasks;
}

MessageParseWorker::PressureState MessageParseWorker::pressureState() const
{
    QMutexLocker locker(&m_mutex);
    return m_pressureState;
}

bool MessageParseWorker::drain(int timeoutMs)
{
    if (QThread::currentThread() == thread()) {
        while (true) {
            {
                QMutexLocker locker(&m_mutex);
                if (m_queue.isEmpty() && m_processingTaskCount == 0) {
                    return true;
                }
            }
            processBatch();
        }
    }

    QDeadlineTimer deadline((std::max)(0, timeoutMs));
    QMutexLocker locker(&m_mutex);
    requestWakeLocked();
    while (!m_queue.isEmpty() || m_processingTaskCount > 0) {
        if (!m_drainedCondition.wait(&m_mutex, deadline)) {
            return false;
        }
    }
    return true;
}

void MessageParseWorker::stopAccepting()
{
    QMutexLocker locker(&m_mutex);
    m_accepting = false;
}

void MessageParseWorker::start()
{
    QMutexLocker locker(&m_mutex);
    if (m_started) {
        return;
    }
    m_processorEngine = createDefaultMessageProcessorEngine();
    m_started = true;
    if (m_droppedTasks > 0 && !m_dropNotificationPending) {
        m_dropNotificationPending = true;
        QMetaObject::invokeMethod(
            this,
            &MessageParseWorker::notifyDropped,
            Qt::QueuedConnection);
    }
    requestWakeLocked();
}

void MessageParseWorker::shutdown()
{
    QMutexLocker locker(&m_mutex);
    m_started = false;
    m_wakePending = false;
    m_dropNotificationPending = false;
    m_processorEngine.reset();
    m_drainedCondition.wakeAll();
}

void MessageParseWorker::processBatch()
{
    QVector<MessageParseTask> batch;
    {
        QMutexLocker locker(&m_mutex);
        m_wakePending = false;
        if (m_processingTaskCount > 0 || m_queue.isEmpty()) {
            if (m_queue.isEmpty() && m_processingTaskCount == 0) {
                m_drainedCondition.wakeAll();
            }
            return;
        }
        if (!m_processorEngine) {
            m_processorEngine = createDefaultMessageProcessorEngine();
        }

        const int batchSize = (std::min)(m_limits.batchSize, int(m_queue.size()));
        batch.reserve(batchSize);
        for (int index = 0; index < batchSize; ++index) {
            MessageParseTask task = m_queue.dequeue();
            batch.append(std::move(task));
        }
        m_processingTaskCount = batchSize;
    }

    for (const MessageParseTask &task : std::as_const(batch)) {
        emit parseCompleted(parse(task));
    }

    bool hasMore = false;
    {
        QMutexLocker locker(&m_mutex);
        for (const MessageParseTask &task : std::as_const(batch)) {
            m_pendingBytes -= approximateBytes(task);
        }
        m_pendingBytes = (std::max)(qint64(0), m_pendingBytes);
        m_processingTaskCount = 0;
        hasMore = !m_queue.isEmpty();
        updatePressureStateLocked();
        if (hasMore) {
            requestWakeLocked();
        } else {
            m_drainedCondition.wakeAll();
        }
    }
    emit queueStateChanged();
}

void MessageParseWorker::notifyDropped()
{
    qint64 dropped = 0;
    {
        QMutexLocker locker(&m_mutex);
        m_dropNotificationPending = false;
        dropped = m_droppedTasks;
    }
    emit tasksDropped(dropped);
    emit queueStateChanged();
}

qint64 MessageParseWorker::approximateBytes(const MessageParseTask &task)
{
    return task.envelope.payloadBytes.size()
    + stringBytes(task.envelope.sessionId)
        + stringBytes(task.envelope.timestamp)
        + stringBytes(task.envelope.topic)
        + stringBytes(task.processorName)
        + (task.processorRevision
                ? stringBytes(task.processorRevision->id)
                    + stringBytes(task.processorRevision->processorId)
                    + stringBytes(task.processorRevision->contentHash)
                : 0)
        + QCborValue(task.processorParameters).toCbor().size()
        + qint64(sizeof(MessageParseTask));
}

MessageParseResult MessageParseWorker::parse(const MessageParseTask &task)
{
    MessageParseResult result;
    result.messageId = task.envelope.messageId;
    result.sequence = task.envelope.sequence;
    result.sessionId = task.envelope.sessionId;

    const PayloadFormat format = PayloadCodec::formatFromInt(task.envelope.payloadFormat);
    QString decodeError;
    const QString decodedPayload = PayloadCodec::decodeForDisplay(
        format,
        task.envelope.payloadBytes,
        decodeError);

    if (!task.processorRevision) {
        result.displayFormat = PayloadCodec::formatName(format);
        if (!decodeError.isEmpty()) {
            result.state = MessageParseState::Failed;
            result.displayError = decodeError;
            return result;
        }
        result.displayPayload = decodedPayload;
        if (result.displayPayload.size() > m_limits.maxResultCharacters) {
            result.state = MessageParseState::Failed;
            result.displayPayload.clear();
            result.displayError = QStringLiteral(
                "Decoded payload exceeds the maximum display length.");
            return result;
        }
        result.state = MessageParseState::Succeeded;
        return result;
    }

    const ProcessorRevisionSnapshot &revision = *task.processorRevision;
    result.processorId = revision.processorId;
    result.processorRevisionId = revision.id;
    result.processorName = task.processorName;
    result.processorLanguageId = revision.languageId;
    result.processorRuntimeId = revision.runtimeId;
    result.processorContentHash = revision.contentHash;

    MessageProcessorContext context;
    context.topic = task.envelope.topic;
    context.payload = task.envelope.payloadBytes;
    context.receivedAt = task.envelope.timestamp;
    context.format = PayloadCodec::formatName(format);
    context.decoded = decodedPayload;
    context.decodeError = decodeError;
    context.parameters = task.processorParameters;

    ProcessorExecutionLimits executionLimits;
    executionLimits.maxPreviewCharacters = (std::max)(
        0,
        m_limits.maxResultCharacters);
    const ProcessorExecutionResult execution = m_processorEngine->execute(
        revision,
        context,
        executionLimits);
    result.processorExecutionState = processorExecutionStateName(execution.state);
    result.processorExecutionDurationUs = execution.durationMicroseconds;
    result.processorResultPreview = execution.preview;
    if (execution.succeeded()) {
        result.processorResultCbor = ProcessorValueCodec::encodeCanonical(execution.value);
        result.displayPayload = execution.preview;
        const QString language = revision.languageId == QStringLiteral("javascript")
            ? QStringLiteral("JavaScript")
            : revision.languageId == QStringLiteral("lua")
                ? QStringLiteral("Lua")
                : revision.languageId;
        result.displayFormat = task.processorName.isEmpty()
            ? language
            : QStringLiteral("%1: %2").arg(language, task.processorName);
        result.state = MessageParseState::Succeeded;
        return result;
    }

    if (!execution.diagnostics.isEmpty()) {
        result.processorExecutionErrorCode = execution.diagnostics.first().code;
        QStringList messages;
        messages.reserve(execution.diagnostics.size());
        for (const ProcessorDiagnostic &diagnostic : execution.diagnostics) {
            if (!diagnostic.message.isEmpty()) {
                messages.append(diagnostic.message);
            }
        }
        result.processorExecutionError = messages.join(QLatin1Char('\n'));
    }
    if (result.processorExecutionErrorCode.isEmpty()) {
        result.processorExecutionErrorCode = QStringLiteral("internal_runtime_error");
    }
    if (result.processorExecutionError.isEmpty()) {
        result.processorExecutionError = QStringLiteral("Processor execution failed.");
    }
    result.displayError = result.processorExecutionError;
    result.displayFormat = QStringLiteral("Processor Error");
    result.state = MessageParseState::Failed;
    return result;
}

MessageParseWorker::PressureState MessageParseWorker::pressureStateForQueueLocked() const
{
    if (m_queueSaturated) {
        return PressureState::Dropping;
    }

    const int pendingTasks = m_queue.size() + m_processingTaskCount;
    const int highWaterTasks = (std::clamp)(
        m_limits.highWaterTasks,
        0,
        (std::max)(0, m_limits.maxTasks));
    const qint64 highWaterBytes = (std::clamp)(
        m_limits.highWaterBytes,
        qint64(0),
        (std::max)(qint64(0), m_limits.maxBytes));
    const int lowWaterTasks = (std::clamp)(
        m_limits.lowWaterTasks,
        0,
        highWaterTasks);
    const qint64 lowWaterBytes = (std::clamp)(
        m_limits.lowWaterBytes,
        qint64(0),
        highWaterBytes);
    const bool aboveHighWater = (highWaterTasks > 0 && pendingTasks >= highWaterTasks)
        || (highWaterBytes > 0 && m_pendingBytes >= highWaterBytes);
    const bool belowLowWater = pendingTasks <= lowWaterTasks
        && m_pendingBytes <= lowWaterBytes;
    if (aboveHighWater) {
        return PressureState::Elevated;
    }
    if (m_pressureState != PressureState::Normal && !belowLowWater) {
        return PressureState::Elevated;
    }
    return PressureState::Normal;
}

bool MessageParseWorker::updatePressureStateLocked()
{
    const int pendingTasks = m_queue.size() + m_processingTaskCount;
    const int lowWaterTasks = (std::clamp)(
        m_limits.lowWaterTasks,
        0,
        (std::max)(0, m_limits.maxTasks));
    const qint64 lowWaterBytes = (std::clamp)(
        m_limits.lowWaterBytes,
        qint64(0),
        (std::max)(qint64(0), m_limits.maxBytes));
    const bool capacityDisabled = m_limits.maxTasks <= 0 || m_limits.maxBytes <= 0;
    const bool belowHardCapacity = pendingTasks < m_limits.maxTasks
        && m_pendingBytes < m_limits.maxBytes;
    if (m_queueSaturated
        && !capacityDisabled
        && belowHardCapacity
        && pendingTasks <= lowWaterTasks
        && m_pendingBytes <= lowWaterBytes) {
        m_queueSaturated = false;
    }

    const PressureState next = pressureStateForQueueLocked();
    if (next == m_pressureState) {
        return false;
    }
    m_pressureState = next;
    return true;
}

void MessageParseWorker::requestWakeLocked()
{
    if (!m_started || m_wakePending || m_queue.isEmpty()) {
        return;
    }
    m_wakePending = true;
    QMetaObject::invokeMethod(this, &MessageParseWorker::processBatch, Qt::QueuedConnection);
}
