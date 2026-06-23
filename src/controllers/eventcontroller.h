#pragma once

#include "domain/session.h"
#include "domain/subscription.h"
#include "models/eventstreammodel.h"
#include "services/scripting/luarunner.h"
#include "services/storage/historystore.h"

#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <functional>

struct ScriptEntry;

class EventController : public QObject
{
    Q_OBJECT

public:
    struct Dependencies
    {
        std::function<SessionState *()> currentSession;
        std::function<SessionState *(const QString &)> sessionById;
        std::function<int()> historyPageSize;
        std::function<int()> messageRetentionLimit;
        std::function<int()> logRetentionLimit;
        std::function<bool()> saveMessagesWhenOutputPaused;
        std::function<const ScriptEntry *(const QString &)> scriptById;
        std::function<QString(const QString &)> scriptName;
        std::function<const SubscriptionEntry *(const SessionState &, const QString &)> bestSubscriptionForTopic;
        std::function<void()> syncScriptTestSamplesModel;
        std::function<void()> syncSubscriptionsModel;
        std::function<bool()> subscriptionFpsRefreshActive;
        std::function<void()> startSubscriptionFpsRefresh;
        std::function<void()> publishMessageStreamChanged;
        std::function<void()> publishLogsChanged;
        std::function<void(const QVariantMap &)> publishMessageStreamRowAppended;
        std::function<void(const QVariantMap &)> publishLogsRowAppended;
        std::function<void()> publishScriptTestSamplesChanged;
        std::function<void()> publishSubscriptionsChanged;
        HistoryStore *historyStore = nullptr;
        EventStreamModel *messagesModel = nullptr;
        EventStreamModel *logsModel = nullptr;
        QString launchTimestamp;
    };

    explicit EventController(QObject *parent = nullptr);

    void setDependencies(Dependencies dependencies);

    void clearCurrentMessages();
    void clearCurrentLogs();
    int loadOlderCurrentSessionMessages();
    int loadOlderCurrentSessionLogs();
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
    void appendIncomingMessage(const QString &sessionId, const QString &topic, const QByteArray &payloadBytes);
    void trimVisibleMessageRows(SessionState &session);
    void trimVisibleLogRows(SessionState &session);
    void reloadCurrentSessionHistory();
    void flushPendingMessageHistory();

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
    QString m_lastMessageStorageError;
};
