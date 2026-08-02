#pragma once

#include "domain/messagerecord.h"
#include "domain/messageenvelope.h"

#include <QObject>
#include <QMutex>
#include <QQueue>
#include <QString>
#include <QVector>
#include <QWaitCondition>

#include <memory>
#include <optional>

class HistoryStore;
class QTimer;

struct HistoryWriterLimits
{
    int highWaterMessages = 2000;
    qint64 highWaterBytes = 64 * 1024 * 1024;
    int lowWaterMessages = 1000;
    qint64 lowWaterBytes = 32 * 1024 * 1024;
    int maxMessages = 5000;
    qint64 maxBytes = 128 * 1024 * 1024;
    int batchSize = 200;
    int flushIntervalMs = 250;
    int initialRetryMs = 250;
    int maxRetryMs = 8000;
    int busyTimeoutMs = 5000;
};

class HistoryWriterWorker : public QObject
{
    Q_OBJECT

public:
    enum class PressureState {
        Normal,
        Elevated,
        Degraded,
        Dropping,
    };
    Q_ENUM(PressureState)

    explicit HistoryWriterWorker(
        QString dataPath,
        qint64 nextMessageId,
        HistoryWriterLimits limits = {},
        QObject *parent = nullptr);
    ~HistoryWriterWorker() override;

    qint64 enqueueMessage(const MessageRecord &message);
    bool enqueueParseResult(const MessageParseResult &result);
    int pendingMessageCount() const;
    qint64 pendingBytes() const;
    qint64 pendingMessageCountForSession(const QString &sessionId) const;
    qint64 droppedMessageCount() const;
    qint64 droppedParseResultCount() const;
    QString lastError() const;
    PressureState pressureState() const;
    std::optional<MessageRecord> pendingMessage(qint64 messageId) const;
    std::optional<MessageParseResult> pendingParseResult(qint64 messageId) const;
    QVector<MessageRecord> pendingMessages(const QString &sessionId) const;
    QVector<MessageParseResult> pendingParseResults(const QString &sessionId) const;
    bool drain(int timeoutMs = 5000);
    void stopAccepting();

signals:
    void queueStateChanged();
    void storageErrorChanged(const QString &error);
    void messagesPersisted(const QStringList &sessionIds, int messageCount);
    void messagesDropped(qint64 totalDropped);
    void parseResultsDropped(qint64 totalDropped);

public slots:
    void start();
    void shutdown();

private slots:
    void wake();
    void flushBatch();
    void notifyDropped();

private:
    struct WriteOperation {
        enum class Type { Capture, ParseUpdate };

        Type type = Type::Capture;
        MessageRecord message;
        MessageParseResult parseResult;
    };

    static qint64 approximateBytes(const MessageRecord &message);
    static qint64 approximateBytes(const MessageParseResult &result);
    static qint64 approximateBytes(const WriteOperation &operation);
    static qint64 operationMessageId(const WriteOperation &operation);
    static QString operationSessionId(const WriteOperation &operation);
    static void applyParseResult(MessageRecord &message, const MessageParseResult &result);
    PressureState pressureStateForQueueLocked() const;
    bool updatePressureStateLocked();
    void requestWakeLocked();
    void scheduleRetry(const QString &error);
    void scheduleNextFlush();
    bool ensureStore();

    const QString m_dataPath;
    const HistoryWriterLimits m_limits;
    mutable QMutex m_mutex;
    QWaitCondition m_drainedCondition;
    QQueue<WriteOperation> m_queue;
    int m_pendingCaptureCount = 0;
    qint64 m_pendingBytes = 0;
    qint64 m_nextMessageId = 1;
    qint64 m_droppedMessages = 0;
    qint64 m_droppedParseResults = 0;
    QString m_lastError;
    PressureState m_pressureState = PressureState::Normal;
    bool m_accepting = true;
    bool m_started = false;
    bool m_wakePending = false;
    bool m_dropNotificationPending = false;
    bool m_drainRequested = false;
    bool m_storageDegraded = false;
    bool m_captureQueueSaturated = false;
    int m_retryDelayMs = 0;
    QTimer *m_flushTimer = nullptr;
    std::unique_ptr<HistoryStore> m_store;
};

Q_DECLARE_METATYPE(HistoryWriterWorker::PressureState)
