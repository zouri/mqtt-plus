#pragma once

#include "domain/messageparsing.h"

#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QWaitCondition>

#include <memory>

class MessageProcessorEngine;

struct MessageParseLimits
{
    int highWaterTasks = 500;
    qint64 highWaterBytes = 32 * 1024 * 1024;
    int lowWaterTasks = 250;
    qint64 lowWaterBytes = 16 * 1024 * 1024;
    int maxTasks = 1000;
    qint64 maxBytes = 64 * 1024 * 1024;
    int batchSize = 20;
    int maxResultCharacters = 256 * 1024;
};

class MessageParseWorker : public QObject
{
    Q_OBJECT

public:
    enum class PressureState {
        Normal,
        Elevated,
        Dropping,
    };
    Q_ENUM(PressureState)

    explicit MessageParseWorker(MessageParseLimits limits = {}, QObject *parent = nullptr);
    ~MessageParseWorker() override;

    bool enqueueTask(const MessageParseTask &task);
    int pendingTaskCount() const;
    qint64 pendingBytes() const;
    qint64 droppedTaskCount() const;
    PressureState pressureState() const;
    bool drain(int timeoutMs = 5000);
    void stopAccepting();

signals:
    void parseCompleted(const ParseOutcome &result);
    void queueStateChanged();
    void tasksDropped(qint64 totalDropped);

public slots:
    void start();
    void shutdown();

private slots:
    void processBatch();
    void notifyDropped();

private:
    static qint64 approximateBytes(const MessageParseTask &task);
    ParseOutcome parse(const MessageParseTask &task);
    PressureState pressureStateForQueueLocked() const;
    bool updatePressureStateLocked();
    void requestWakeLocked();

    const MessageParseLimits m_limits;
    mutable QMutex m_mutex;
    QWaitCondition m_drainedCondition;
    QQueue<MessageParseTask> m_queue;
    qint64 m_pendingBytes = 0;
    qint64 m_droppedTasks = 0;
    PressureState m_pressureState = PressureState::Normal;
    bool m_queueSaturated = false;
    bool m_accepting = true;
    bool m_started = false;
    int m_processingTaskCount = 0;
    bool m_wakePending = false;
    bool m_dropNotificationPending = false;
    std::unique_ptr<MessageProcessorEngine> m_processorEngine;
};

Q_DECLARE_METATYPE(MessageParseWorker::PressureState)
