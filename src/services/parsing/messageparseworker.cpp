#include "messageparseworker.h"

#include "services/payload/payloadcodec.h"
#include "services/scripting/luarunner.h"

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
    m_runtimeCache = std::make_unique<LuaRunner::RuntimeCache>();
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

void MessageParseWorker::clearRuntimeCache()
{
    if (m_runtimeCache) {
        m_runtimeCache->clear();
    }
}

void MessageParseWorker::shutdown()
{
    QMutexLocker locker(&m_mutex);
    m_started = false;
    m_wakePending = false;
    m_dropNotificationPending = false;
    m_runtimeCache.reset();
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
        if (!m_runtimeCache) {
            m_runtimeCache = std::make_unique<LuaRunner::RuntimeCache>();
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
        + stringBytes(task.scriptId)
        + stringBytes(task.scriptName)
        + stringBytes(task.scriptCode)
        + qint64(sizeof(MessageParseTask));
}

MessageParseResult MessageParseWorker::parse(const MessageParseTask &task)
{
    MessageParseResult result;
    result.messageId = task.envelope.messageId;
    result.sequence = task.envelope.sequence;
    result.sessionId = task.envelope.sessionId;
    result.scriptId = task.scriptId;
    result.scriptName = task.scriptName;

    const PayloadFormat format = PayloadCodec::formatFromInt(task.envelope.payloadFormat);
    QString decodeError;
    const QString decodedPayload = PayloadCodec::decodeForDisplay(
        format,
        task.envelope.payloadBytes,
        decodeError);

    if (!task.scriptId.isEmpty()) {
        LuaScriptContext context;
        context.topic = task.envelope.topic;
        context.payloadBytes = task.envelope.payloadBytes;
        context.decodedPayload = decodedPayload;
        context.decodeError = decodeError;
        context.format = format;
        context.timestamp = task.envelope.timestamp;

        const LuaScriptResult scriptResult = m_runtimeCache->run(
            task.scriptId,
            task.scriptCode,
            context);
        if (!scriptResult.success) {
            result.state = MessageParseState::Failed;
            result.parseError = scriptResult.error;
            result.parsedFormat = QStringLiteral("Lua: %1").arg(task.scriptName);
            return result;
        }
        result.parsedPayload = scriptResult.output;
        result.parsedFormat = QStringLiteral("Lua: %1").arg(task.scriptName);
    } else {
        result.parsedFormat = PayloadCodec::formatName(format);
        if (!decodeError.isEmpty()) {
            result.state = MessageParseState::Failed;
            result.parseError = decodeError;
            return result;
        }
        result.parsedPayload = decodedPayload;
    }

    if (result.parsedPayload.size() > m_limits.maxResultCharacters) {
        result.state = MessageParseState::Failed;
        result.parsedPayload.clear();
        result.parseError = QStringLiteral("Parsed result exceeds the maximum display length.");
        return result;
    }

    result.state = MessageParseState::Succeeded;
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
