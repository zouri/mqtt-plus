#pragma once

#include "domain/session.h"
#include "domain/subscription.h"

#include <QObject>
#include <QMqttSubscription>

class EventHistoryService;
class ScriptService;
class SessionService;

class SubscriptionService : public QObject
{
    Q_OBJECT

public:
    explicit SubscriptionService(
        SessionService &sessionService,
        ScriptService &scriptService,
        EventHistoryService &eventHistoryService,
        QObject *parent = nullptr);

    bool upsertCurrentSubscription(
        const QString &topic,
        int qos,
        int format,
        const QString &scriptId,
        const QString &color,
        const QString &alias);
    bool updateCurrentSubscription(
        const QString &topic,
        const QString &newTopic,
        const QString &alias,
        int qos,
        int format,
        const QString &scriptId,
        const QString &color);
    void removeCurrentSubscription(const QString &topic);
    void setCurrentSubscriptionPaused(const QString &topic, bool paused);
    void setAllCurrentSubscriptionsPaused(bool paused);

    void resetRuntimeSubscriptions(SessionState &session);
    void restoreActiveSubscriptions(SessionState &session, bool emitEvents);
    bool currentSessionHasActiveSubscriptionFps(qint64 nowMs) const;

signals:
    void subscriptionsChanged();

private:
    SubscriptionEntry *subscriptionByTopic(SessionState *session, const QString &topic);
    void ensureSubscriptionActive(SessionState &session, SubscriptionEntry &entry, bool emitEvents);
    void observeSubscription(SessionState &session, SubscriptionEntry &entry, QMqttSubscription *subscription);
    void updateSubscriptionState(
        const QString &sessionId,
        const QString &topic,
        const QPointer<QMqttSubscription> &subscription,
        QMqttSubscription::SubscriptionState state);

    SessionService &m_sessionService;
    ScriptService &m_scriptService;
    EventHistoryService &m_eventHistoryService;
};
