#pragma once

#include "domain/session.h"
#include "domain/subscription.h"
#include "services/scripting/luarunner.h"

#include <QByteArray>
#include <QString>
#include <QVariantMap>

class EventStreamModel;
class HistoryStore;
class QTimer;
class ScriptController;
class ScriptTestSamplesModel;
class SubscriptionController;

class EventControllerContext
{
public:
    virtual ~EventControllerContext() = default;

    virtual SessionState *currentSessionState() = 0;
    virtual SessionState *sessionById(const QString &sessionId) = 0;
    virtual HistoryStore &historyStore() = 0;
    virtual EventStreamModel &messagesModel() = 0;
    virtual EventStreamModel &logsModel() = 0;
    virtual ScriptTestSamplesModel &scriptTestSamplesModel() = 0;
    virtual ScriptController &scriptController() = 0;
    virtual SubscriptionController &subscriptionController() = 0;
    virtual QTimer &subscriptionFpsRefreshTimer() = 0;
    virtual QString launchTimestamp() const = 0;

    virtual int historyPageSize() const = 0;
    virtual int messageRetentionLimit() const = 0;
    virtual int logRetentionLimit() const = 0;
    virtual int maxIncomingPayloadBytes() const = 0;
    virtual bool saveMessagesWhenOutputPaused() const = 0;

    virtual void appendEvent(SessionState &session, const QString &channel, const QString &message) = 0;
    virtual void appendIncomingMessage(const QString &sessionId, const QString &topic, const QByteArray &payloadBytes) = 0;
    virtual LuaScriptResult parseIncomingPayload(
        const SessionState &session,
        const SubscriptionEntry *subscription,
        const QString &topic,
        const QByteArray &payloadBytes,
        const QString &timestamp,
        QString &scriptNameOut,
        QString &decodedPayloadOut) const = 0;
    virtual QString scriptName(const QString &id) const = 0;
    virtual void refreshSubscriptionsModel() = 0;
    virtual void refreshScriptTestSamplesModel() = 0;

    virtual void emitSubscriptionsChanged() = 0;
    virtual void emitMessageStreamChanged() = 0;
    virtual void emitLogStreamChanged() = 0;
    virtual void emitMessageStreamRowAppended(const QVariantMap &row) = 0;
    virtual void emitLogStreamRowAppended(const QVariantMap &row) = 0;
};
