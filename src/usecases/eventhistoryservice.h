#pragma once

#include "domain/session.h"
#include "domain/subscription.h"
#include "domain/messageenvelope.h"
#include "services/messaging/messagecapturepolicy.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <optional>

class EventStreamModel;
class HistoryStore;
class HistoryWriterWorker;
class MessageParseWorker;
class PreferencesController;
class ScriptService;
class SessionService;
struct MessageRecord;

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
        ScriptService &scriptService,
        QString launchTimestamp,
        PreferencesController &preferencesController,
        QObject *parent = nullptr);

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
    void appendIncomingMessage(const QString &sessionId, const QString &topic, const QByteArray &payloadBytes);
    QString messagePayloadForReuse(
        qint64 messageId,
        const QString &fallbackPayload,
        const QString &fallbackTestPayload,
        int format) const;
    QString messagePayloadForDisplay(qint64 messageId, const QString &fallbackPayload, int format) const;
    QVariantMap messageDetails(qint64 messageId) const;
    void reloadCurrentSessionHistory();
    void stopAcceptingMessageParsing();
    bool flushPendingMessageHistory(int timeoutMs = 5000);
    int messageWriterBacklog() const;
    qint64 messageWriterBacklogBytes() const;
    qint64 droppedMessageCount() const;
    qint64 droppedParseResultCount() const;
    int messageParserBacklog() const;
    qint64 messageParserBacklogBytes() const;
    qint64 droppedParseTaskCount() const;
    void setMessageCapturePolicy(const QString &sessionId, const MessageCapturePolicy &policy);
    MessageCapturePolicy messageCapturePolicy(const QString &sessionId) const;
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
    void messageRowsAppended(const QVariantList &rows);
    void messageParseResultChanged(qint64 historyId);
    void logAppended(const QVariantMap &row);
    void subscriptionActivityChanged();
    void messageWriterStateChanged();

private:
    enum class Stream { Message, Log };

    static QVariantList &streamRows(SessionState &session, Stream kind);
    static qint64 &oldestLoadedId(SessionState &session, Stream kind);
    static bool &loadedAllHistory(SessionState &session, Stream kind);

    void appendRenderedMessageRow(SessionState &session, const QVariantMap &row);
    void appendRenderedLogRow(SessionState &session, const QVariantMap &row);
    void enqueueMessageParsing(
        const MessageRecord &record,
        qint64 sequence,
        const QString &scriptCode);
    void handleMessageParseResult(const MessageParseResult &result);
    void updateRenderedParseResult(SessionState &session, const MessageParseResult &result);
    bool clearStream(Stream kind, bool allSessions);
    void resetMessageStreamTransientState(bool allSessions, const SessionState *current);
    int loadOlderCurrentSession(Stream kind);
    std::optional<QString> decodedStoredPayload(qint64 messageId, int format, QString &parseErrorOut) const;
    void trimVisibleRows(SessionState &session, Stream kind);
    void flushPendingVisibleMessageRows();
    void reportMessageStorageError(SessionState &session, const QString &message);
    void scheduleVisibleMessageRowsFlush();
    void scheduleMessagePressureNotification();
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
    ScriptService &m_scriptService;
    const QString m_launchTimestamp;
    PreferencesController &m_preferencesController;
    QTimer m_visibleMessageRowsFlushTimer;
    QTimer m_messagePressureNotificationTimer;
    QVariantList m_pendingVisibleMessageRows;
    QString m_pendingVisibleMessageSessionId;
    qint64 m_frozenOldestLoadedMessageId = 0;
    QSet<QString> m_reportedPayloadStorageStates;
    QHash<QString, qint64> m_nextMessageSequence;
    QHash<QString, MessageCapturePolicy> m_capturePolicies;
    QString m_lastMessageStorageError;
    qint64 m_captureFilteredMessages = 0;
    qint64 m_pressureSkippedParses = 0;
    bool m_messageStreamFrozen = false;
};
