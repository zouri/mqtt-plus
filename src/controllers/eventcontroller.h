#pragma once

#include "domain/session.h"
#include "domain/subscription.h"
#include "services/scripting/luarunner.h"

#include <QObject>
#include <QTimer>
#include <QVariantMap>

#include <functional>

class EventStreamModel;
class HistoryStore;
class PreferencesController;
class ScriptController;
class ScriptTestSamplesModel;
class SubscriptionController;

class EventController : public QObject
{
    Q_OBJECT

public:
    struct Dependencies
    {
        HistoryStore *historyStore = nullptr;
        EventStreamModel *messagesModel = nullptr;
        EventStreamModel *logsModel = nullptr;
        ScriptTestSamplesModel *scriptTestSamplesModel = nullptr;
        ScriptController *scriptController = nullptr;
        SubscriptionController *subscriptionController = nullptr;
        QTimer *subscriptionFpsRefreshTimer = nullptr;
        QString *launchTimestamp = nullptr;
        PreferencesController *preferencesController = nullptr;
        std::function<SessionState *()> currentSessionState;
        std::function<SessionState *(const QString &)> sessionById;
        std::function<void()> refreshSubscriptionsModel;
        std::function<void()> refreshScriptTestSamplesModel;
        std::function<void()> emitSubscriptionsChanged;
        std::function<void()> emitMessageStreamChanged;
        std::function<void()> emitLogStreamChanged;
        std::function<void(const QVariantMap &)> emitMessageStreamRowAppended;
        std::function<void(const QVariantMap &)> emitLogStreamRowAppended;
    };

    explicit EventController(QObject *parent = nullptr);

    void setDependencies(const Dependencies &dependencies);

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
    void reportMessageStorageError(SessionState &session, const QString &message);
    void scheduleMessageHistoryFlush();

    Dependencies m_dependencies;
    QTimer m_messageHistoryFlushTimer;
    QString m_lastMessageStorageError;
};
