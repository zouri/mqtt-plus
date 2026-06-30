#pragma once

#include "domain/session.h"
#include "domain/subscription.h"
#include "services/scripting/luarunner.h"

#include <QByteArray>
#include <QSslConfiguration>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

class EventController;
class EventStreamModel;
class HistoryStore;
class ScriptController;
class ScriptTestSamplesModel;
class SubscriptionController;
class SubscriptionListModel;

class SessionControllerContext
{
public:
    virtual ~SessionControllerContext() = default;

    virtual HistoryStore &historyStore() = 0;
    virtual SubscriptionController &subscriptionController() = 0;
    virtual QTimer &subscriptionFpsRefreshTimer() = 0;

    virtual bool deleteHistoryWithSession() const = 0;

    virtual bool saveSessions() = 0;
    virtual void configureSession(SessionState &session, const QVariantMap &config, bool keepNameFallback) = 0;
    virtual void initializeSessionRuntime(SessionState *session) = 0;
    virtual void destroySessionRuntime(SessionState &session) = 0;
    virtual void connectSession(SessionState &session, const QString &eventPrefix) = 0;
    virtual SessionState createDefaultSession(const QString &name) = 0;
    virtual void updatePublishStatus(
        SessionState &session,
        const QString &state,
        const QString &reason = QString(),
        qint32 messageId = -1) = 0;
    virtual void reloadCurrentSessionHistory() = 0;
    virtual void notifyCurrentSessionViewsChanged() = 0;
    virtual void notifyCurrentSessionAndSubscriptionsChanged() = 0;
    virtual void notifySelectedSessionViewsChanged() = 0;
    virtual void notifySessionCollectionViewsChanged() = 0;
    virtual void refreshSessionsModel() = 0;

    virtual void emitSessionsChanged() = 0;
    virtual void emitMessageStreamChanged() = 0;
};

class MqttControllerContext
{
public:
    virtual ~MqttControllerContext() = default;

    virtual SessionState *currentSessionState() = 0;
    virtual SessionState *sessionById(const QString &sessionId) = 0;
    virtual const SessionState *sessionById(const QString &sessionId) const = 0;
    virtual SubscriptionController &subscriptionController() = 0;
    virtual EventController &eventController() = 0;

    virtual void updatePublishStatus(
        SessionState &session,
        const QString &state,
        const QString &reason = QString(),
        qint32 messageId = -1) = 0;
    virtual void appendEvent(SessionState &session, const QString &channel, const QString &message) = 0;
    virtual QSslConfiguration sslConfigurationForSession(const SessionState &session, QString &errorMessage) const = 0;
    virtual void notifyCurrentSessionViewsChanged() = 0;
    virtual void notifySessionViewsChanged() = 0;
    virtual void notifySessionAndSubscriptionViewsChanged() = 0;
};

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
    virtual void emitScriptTestSamplesChanged() = 0;
};

class SubscriptionControllerContext
{
public:
    virtual ~SubscriptionControllerContext() = default;

    virtual SessionState *currentSessionState() = 0;
    virtual const SessionState *currentSessionState() const = 0;
    virtual SessionState *sessionById(const QString &sessionId) = 0;
    virtual SubscriptionListModel &subscriptionsModel() = 0;
    virtual ScriptController &scriptController() = 0;
    virtual QTimer &subscriptionFpsRefreshTimer() = 0;

    virtual bool saveSessions() = 0;
    virtual void appendEvent(SessionState &session, const QString &channel, const QString &message) = 0;
    virtual qreal subscriptionFps(const SubscriptionEntry &entry, qint64 nowMs) const = 0;
    virtual bool currentSessionHasActiveSubscriptionFps(qint64 nowMs) const = 0;
    virtual void refreshSubscriptionsModel() = 0;
    virtual void notifyCurrentSessionAndSubscriptionsChanged() = 0;
    virtual void notifySessionAndSubscriptionViewsChanged() = 0;

    virtual void emitSubscriptionsChanged() = 0;
};
