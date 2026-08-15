#pragma once

#include "domain/messageprocessor.h"
#include "domain/messageparsing.h"
#include "domain/messagerecord.h"
#include "presentation/eventrow.h"
#include "domain/messagecapturepolicy.h"

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QSharedPointer>
#include <QStringList>
#include <QVector>
#include <QWaitCondition>

struct MessageAdmissionSubscription
{
    QString topic;
    QString alias;
    QString color;
    int format = -1;
    bool paused = false;
    ProcessorReference processor;
    QSharedPointer<const ProcessorRevisionSnapshot> processorRevision;
    QString processorName;
    QString processorResolutionError;
};

struct MessageAdmissionContext
{
    MessageCapturePolicy capturePolicy;
    QVector<MessageAdmissionSubscription> subscriptions;
    QHash<QString, int> subscriptionFormats;
    int maxPayloadBytes = 0;
    bool outputPaused = false;
    bool saveMessagesWhenOutputPaused = true;
};

struct IncomingMessageAdmissionTask
{
    QString sessionId;
    QString topic;
    QByteArray payloadBytes;
    int qos = -1;
    bool retain = false;
    MqttPublishProperties publishProperties;
    qint64 receivedAtMs = 0;
    bool pressureSkipsParsing = false;
    QSharedPointer<const MessageAdmissionContext> context;
};

struct PreparedIncomingMessage
{
    QString sessionId;
    QString topic;
    qint64 receivedAtMs = 0;
    qint64 payloadBytes = 0;
    qint64 sequence = 0;
    bool captured = false;
    bool parsingRequired = false;
    bool parsingSkippedForPressure = false;
    QStringList activeSubscriptionTopics;
    QString reportKey;
    QString reportMessage;
    MessageRecord record;
    EventRow renderedRow;
    QSharedPointer<const ProcessorRevisionSnapshot> processorRevision;
    QCborMap processorParameters;
};

struct MessageAdmissionLimits
{
    int maxMessages = 5000;
    qint64 maxBytes = 128 * 1024 * 1024;
    int batchSize = 200;
};

class MessageAdmissionWorker : public QObject
{
    Q_OBJECT

public:
    enum class PressureState {
        Normal,
        Elevated,
        Dropping,
    };
    Q_ENUM(PressureState)

    explicit MessageAdmissionWorker(
        MessageAdmissionLimits limits = {},
        QObject *parent = nullptr);

    bool enqueue(IncomingMessageAdmissionTask task);
    bool drain(int timeoutMs = 5000);
    void stopAccepting();
    QVector<PreparedIncomingMessage> takePrepared();
    int pendingMessageCount() const;
    qint64 pendingBytes() const;
    qint64 droppedMessageCount() const;
    PressureState pressureState() const;

    static PreparedIncomingMessage prepare(const IncomingMessageAdmissionTask &task);

signals:
    void preparedAvailable();
    void queueStateChanged();

public slots:
    void start();
    void shutdown();

private slots:
    void processBatch();

private:
    static qint64 approximateBytes(const IncomingMessageAdmissionTask &task);
    void requestWakeLocked();
    void updatePressureStateLocked();

    const MessageAdmissionLimits m_limits;
    mutable QMutex m_mutex;
    QWaitCondition m_drainedCondition;
    QQueue<IncomingMessageAdmissionTask> m_queue;
    QVector<PreparedIncomingMessage> m_prepared;
    qint64 m_pendingBytes = 0;
    qint64 m_preparedBytes = 0;
    qint64 m_droppedMessages = 0;
    PressureState m_pressureState = PressureState::Normal;
    bool m_accepting = true;
    bool m_started = false;
    bool m_wakePending = false;
    int m_processingCount = 0;
};
