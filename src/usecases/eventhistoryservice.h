#pragma once

#include "domain/session.h"
#include "domain/subscription.h"
#include "domain/messageparsing.h"
#include "presentation/eventrow.h"
#include "domain/messagecapturepolicy.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QSharedPointer>
#include <QThread>
#include <QTimer>
#include <QVariantMap>
#include <QVector>

#include <optional>

class EventStreamModel;
class HistoryStore;
class HistoryWriterWorker;
class MessageAdmissionWorker;
class MessageParseWorker;
class PreferencesController;
class ProcessorLibrary;
class SessionService;
struct MessageRecord;
struct MessageAdmissionContext;
struct SubscriptionRenderContext;

class EventHistoryService : public QObject
{
    Q_OBJECT

public:
    explicit EventHistoryService(
        SessionService &sessionService,
        HistoryStore &historyStore,
        HistoryWriterWorker &historyWriter,
        MessageParseWorker &messageParser,
        EventStreamModel &messages,
        EventStreamModel &logs,
        ProcessorLibrary &processorLibrary,
        QString launchTimestamp,
        PreferencesController &preferencesController,
        QObject *parent = nullptr);
    ~EventHistoryService() override;

    Q_INVOKABLE bool clearCurrentMessages();
    Q_INVOKABLE bool clearCurrentLogs();
    Q_INVOKABLE bool clearAllMessages();
    Q_INVOKABLE bool clearAllLogs();
    Q_INVOKABLE bool clearAllHistory();
    Q_INVOKABLE int loadOlderCurrentSessionMessages();
    Q_INVOKABLE int loadOlderCurrentSessionLogs();
    Q_INVOKABLE void setMessageStreamFrozen(bool frozen);
    void appendEvent(SessionState &session, const QString &channel, const QString &message);
    void appendPublishedMessage(
        const QString &sessionId,
        const QString &topic,
        const QByteArray &payloadBytes,
        int format,
        int qos = -1,
        bool retain = false);
    void queueIncomingMessage(const QString &sessionId, const QString &topic, const QByteArray &payloadBytes);
    QString messagePayloadForReuse(
        qint64 messageId,
        const QString &fallbackPayload,
        const QString &fallbackTestPayload,
        int format) const;
    QString messagePayloadForDisplay(qint64 messageId, const QString &fallbackPayload, int format) const;
    bool requestExpandedMessage(qint64 messageId);
    QVariantMap messageDetails(qint64 messageId) const;
    void reloadCurrentSessionHistory();
    void invalidateMessageContexts();
    void stopAcceptingIncomingMessages();
    void shutdownIncomingMessageAdmission();
    void stopAcceptingMessageParsing();
    bool flushPendingIncomingMessages(int timeoutMs = 5000);
    bool flushPendingMessageHistory(int timeoutMs = 5000);
    int messageWriterBacklog() const;
    qint64 messageWriterBacklogBytes() const;
    qint64 droppedMessageCount() const;
    qint64 droppedParseResultCount() const;
    int messageParserBacklog() const;
    qint64 messageParserBacklogBytes() const;
    qint64 droppedParseTaskCount() const;
    qint64 captureFilteredMessageCount() const;
    qint64 pressureSkippedParseCount() const;
    QString messagePressureState() const;
    QString messageCaptureMode() const;
    QString messageWriterPressureState() const;
    QString messageParserPressureState() const;
    bool messageStorageDegraded() const;
    QString messageStorageError() const;

signals:
    void messageStreamChanged();
    void totalMessageCountChanged();
    void logStreamChanged();
    void messageRowsAppended(const QVector<EventRow> &rows);
    void messageParseResultChanged(qint64 historyId);
    void logAppended(const EventRow &row);
    void subscriptionActivityChanged();
    void messageWriterStateChanged();

private:
    enum class Stream { Message, Log };

    struct VisibleStreamState {
        QVector<EventRow> messageRows;
        QVector<EventRow> logRows;
        qint64 oldestLoadedMessageId = 0;
        qint64 oldestLoadedLogId = 0;
        bool loadedAllMessageHistory = false;
        bool loadedAllLogHistory = false;
    };

    VisibleStreamState &visibleStreamState(const SessionState &session);
    QVector<EventRow> &streamRows(const SessionState &session, Stream kind);
    qint64 &oldestLoadedId(const SessionState &session, Stream kind);
    bool &loadedAllHistory(const SessionState &session, Stream kind);

    void appendRenderedMessageRow(SessionState &session, const EventRow &row);
    void appendRenderedMessageRows(SessionState &session, const QVector<EventRow> &rows);
    void appendRenderedLogRow(SessionState &session, const EventRow &row);
    void enqueueMessageParsing(
        const MessageRecord &record,
        qint64 sequence,
        const QSharedPointer<const ProcessorRevisionSnapshot> &processorRevision = {},
        const QCborMap &processorParameters = {});
    void handleParseOutcome(const ParseOutcome &result);
    void updateRenderedParseResult(SessionState &session, const ParseOutcome &result);
    bool clearStream(Stream kind, bool allSessions);
    void resetMessageStreamTransientState(bool allSessions, const SessionState *current);
    int loadOlderCurrentSession(Stream kind);
    std::optional<QString> decodedStoredPayload(qint64 messageId, int format, QString &parseErrorOut) const;
    void trimVisibleRows(SessionState &session, Stream kind);
    void flushPendingVisibleMessageRows();
    void applyPreparedIncomingMessages();
    QSharedPointer<const MessageAdmissionContext> messageAdmissionContext(
        const SessionState &session);
    QSharedPointer<const SubscriptionRenderContext> subscriptionRenderContext(
        const SessionState &session) const;
    void reportMessageStorageError(SessionState &session, const QString &message);
    void scheduleVisibleMessageRowsFlush();
    void scheduleMessagePressureNotification();
    void scheduleMessageActivityNotification(
        bool totalCountChanged,
        bool subscriptionActivityDirty);
    void flushPendingMessageActivityNotifications();
    bool shouldCaptureMessage(
        const SessionState &session,
        MessageDirection direction,
        const QString &topic) const;
    bool shouldSkipParsingForPressure() const;
    void recordCaptureFiltered();
    void recordPressureSkippedParse();

    SessionService &m_sessionService;
    HistoryStore &m_historyStore;
    HistoryWriterWorker &m_historyWriter;
    MessageParseWorker &m_messageParser;
    EventStreamModel &m_messages;
    EventStreamModel &m_logs;
    ProcessorLibrary &m_processorLibrary;
    const QString m_launchTimestamp;
    PreferencesController &m_preferencesController;
    QThread m_messageAdmissionThread;
    MessageAdmissionWorker *m_messageAdmissionWorker = nullptr;
    QHash<QString, QSharedPointer<const MessageAdmissionContext>> m_messageAdmissionContexts;
    mutable QHash<QString, QSharedPointer<const SubscriptionRenderContext>>
        m_subscriptionRenderContexts;
    VisibleStreamState m_visibleStreamState;
    QString m_visibleStreamSessionId;
    QTimer m_visibleMessageRowsFlushTimer;
    QTimer m_messagePressureNotificationTimer;
    QTimer m_messageActivityNotificationTimer;
    QVector<EventRow> m_pendingVisibleMessageRows;
    QString m_pendingVisibleMessageSessionId;
    qint64 m_frozenOldestLoadedMessageId = 0;
    QSet<QString> m_reportedPayloadStorageStates;
    QHash<QString, qint64> m_nextMessageSequence;
    QString m_lastMessageStorageError;
    qint64 m_captureFilteredMessages = 0;
    qint64 m_pressureSkippedParses = 0;
    bool m_messageStreamFrozen = false;
    bool m_totalMessageCountNotificationPending = false;
    bool m_subscriptionActivityNotificationPending = false;
};
