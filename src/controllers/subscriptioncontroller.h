#pragma once

#include "domain/session.h"
#include "domain/subscription.h"

#include <QObject>
#include <QMqttSubscription>

class AppFacade;

class SubscriptionController : public QObject
{
    Q_OBJECT

public:
    explicit SubscriptionController(AppFacade *app, QObject *parent = nullptr);

    bool upsertCurrentSubscription(
        const QString &topic,
        int qos,
        int format,
        const QString &scriptId,
        const QString &alias);
    bool updateCurrentSubscription(
        const QString &topic,
        const QString &newTopic,
        const QString &alias,
        const QString &scriptId);
    void removeCurrentSubscription(const QString &topic);
    void setCurrentSubscriptionPaused(const QString &topic, bool paused);

    SubscriptionEntry *subscriptionByTopic(SessionState *session, const QString &topic);
    const SubscriptionEntry *subscriptionByTopic(const SessionState *session, const QString &topic) const;
    const SubscriptionEntry *bestSubscriptionForTopic(const SessionState &session, const QString &topic) const;
    void resetRuntimeSubscriptions(SessionState &session);
    void restoreActiveSubscriptions(SessionState &session, bool emitEvents);
    void ensureSubscriptionActive(SessionState &session, SubscriptionEntry &entry, bool emitEvents);
    qreal subscriptionFps(const SubscriptionEntry &entry, qint64 nowMs) const;
    bool currentSessionHasActiveSubscriptionFps(qint64 nowMs) const;
    void refreshSubscriptionFps();

private:
    void observeSubscription(SessionState &session, SubscriptionEntry &entry, QMqttSubscription *subscription);
    void updateSubscriptionState(
        const QString &sessionId,
        const QString &topic,
        const QPointer<QMqttSubscription> &subscription,
        QMqttSubscription::SubscriptionState state);

    AppFacade &m_app;
};
