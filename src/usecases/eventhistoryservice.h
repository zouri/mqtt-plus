#pragma once

#include "domain/session.h"
#include "domain/subscription.h"
#include "services/scripting/luarunner.h"

#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <optional>

class EventStreamModel;
class HistoryStore;
class PreferencesController;
class ScriptService;
class SessionService;

class EventHistoryService : public QObject
{
    Q_OBJECT

public:
    explicit EventHistoryService(
        SessionService &sessionService,
        HistoryStore &historyStore,
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
    void flushPendingMessageHistory();

signals:
    void messageStreamChanged();
    void totalMessageCountChanged();
    void logStreamChanged();
    void messageRowsAppended(int count);
    void logAppended(const QVariantMap &row);
    void subscriptionActivityChanged();

private:
    enum class Stream { Message, Log };

    static QVariantList &streamRows(SessionState &session, Stream kind);
    static qint64 &oldestLoadedId(SessionState &session, Stream kind);
    static bool &loadedAllHistory(SessionState &session, Stream kind);

    void appendRenderedMessageRow(SessionState &session, const QVariantMap &row);
    void appendRenderedLogRow(SessionState &session, const QVariantMap &row);
    LuaScriptResult parseIncomingPayload(
        const SessionState &session,
        const SubscriptionEntry *subscription,
        const QString &topic,
        const QByteArray &payloadBytes,
        const QString &timestamp,
        QString &scriptNameOut) const;
    bool clearStream(Stream kind, bool allSessions);
    void resetMessageStreamTransientState(bool allSessions, const SessionState *current);
    int loadOlderCurrentSession(Stream kind);
    std::optional<QString> decodedStoredPayload(qint64 messageId, int format, QString &parseErrorOut) const;
    void trimVisibleRows(SessionState &session, Stream kind);
    void flushPendingVisibleMessageRows();
    void reportMessageStorageError(SessionState &session, const QString &message);
    void scheduleMessageHistoryFlush();
    void scheduleVisibleMessageRowsFlush();

    SessionService &m_sessionService;
    HistoryStore &m_historyStore;
    EventStreamModel &m_messages;
    EventStreamModel &m_logs;
    ScriptService &m_scriptService;
    const QString m_launchTimestamp;
    PreferencesController &m_preferencesController;
    QTimer m_messageHistoryFlushTimer;
    QTimer m_visibleMessageRowsFlushTimer;
    QVariantList m_pendingVisibleMessageRows;
    QString m_pendingVisibleMessageSessionId;
    qint64 m_frozenOldestLoadedMessageId = 0;
    QSet<QString> m_reportedPayloadStorageStates;
    QString m_lastMessageStorageError;
    bool m_messageStreamFrozen = false;
};
