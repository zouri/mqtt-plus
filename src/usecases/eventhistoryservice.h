#pragma once

#include "domain/session.h"
#include "domain/subscription.h"
#include "services/scripting/luarunner.h"

#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

class EventStreamModel;
class HistoryStore;
class PreferencesController;
class ScriptService;
class ScriptTestSamplesModel;
class SubscriptionService;

class EventHistoryService : public QObject
{
    Q_OBJECT

public:
    struct Dependencies
    {
        HistoryStore *historyStore = nullptr;
        EventStreamModel *messagesModel = nullptr;
        EventStreamModel *logsModel = nullptr;
        ScriptTestSamplesModel *scriptTestSamplesModel = nullptr;
        ScriptService *scriptController = nullptr;
        SubscriptionService *subscriptionController = nullptr;
        QTimer *subscriptionFpsRefreshTimer = nullptr;
        QString *launchTimestamp = nullptr;
        PreferencesController *preferencesController = nullptr;
        std::function<SessionState *()> currentSessionState;
        std::function<SessionState *(const QString &)> sessionById;
        std::function<void()> refreshSubscriptionsModel;
        std::function<void()> refreshScriptTestSamplesModel;
    };

    explicit EventHistoryService(QObject *parent = nullptr);

    void setDependencies(const Dependencies &dependencies);

    void clearCurrentMessages();
    void clearCurrentLogs();
    int loadOlderCurrentSessionMessages();
    int loadOlderCurrentSessionLogs();
    bool messageStreamFrozen() const;
    void setMessageStreamFrozen(bool frozen);
    void appendRenderedMessageRow(SessionState &session, const QVariantMap &row);
    void appendRenderedLogRow(SessionState &session, const QVariantMap &row);
    void appendEvent(SessionState &session, const QString &channel, const QString &message);
    LuaScriptResult parseIncomingPayload(
        const SessionState &session,
        const SubscriptionEntry *subscription,
        const QString &topic,
        const QByteArray &payloadBytes,
        const QString &timestamp,
        QString &scriptNameOut,
        QString &decodedPayloadOut) const;
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
    QVariantMap messageDetails(qint64 messageId) const;
    void trimVisibleMessageRows(SessionState &session);
    void trimVisibleLogRows(SessionState &session);
    void reloadCurrentSessionHistory();
    void flushPendingMessageHistory();

signals:
    void messageStreamChanged();
    void totalMessageCountChanged();
    void logStreamChanged();
    void messageAppended(const QVariantMap &row);
    void messageRowsAppended(int count);
    void logAppended(const QVariantMap &row);

private:
    void flushPendingVisibleMessageRows();
    void reportMessageStorageError(SessionState &session, const QString &message);
    void scheduleMessageHistoryFlush();
    void scheduleVisibleMessageRowsFlush();

    Dependencies m_dependencies;
    QTimer m_messageHistoryFlushTimer;
    QTimer m_visibleMessageRowsFlushTimer;
    QVariantList m_pendingVisibleMessageRows;
    QString m_pendingVisibleMessageSessionId;
    qint64 m_frozenOldestLoadedMessageId = 0;
    QSet<QString> m_reportedPayloadStorageStates;
    QString m_lastMessageStorageError;
    bool m_messageStreamFrozen = false;
};
