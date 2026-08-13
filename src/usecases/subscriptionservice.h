#pragma once

#include "domain/session.h"
#include "domain/subscription.h"

#include <QObject>
#include <QMqttSubscription>
#include <QStringList>

class EventHistoryService;
class SessionService;

class SubscriptionService : public QObject
{
    Q_OBJECT

public:
    explicit SubscriptionService(
        SessionService &sessionService,
        EventHistoryService &eventHistoryService,
        QObject *parent = nullptr);

    bool upsertCurrentSubscription(
        const QString &topic,
        int qos,
        int format,
        const ProcessorReference &processor,
        const QString &color,
        const QString &alias);
    bool upsertCurrentSubscriptions(
        const QStringList &topics,
        int qos,
        int format,
        const ProcessorReference &processor,
        const QString &color,
        const QString &alias);
    bool updateCurrentSubscription(
        const QString &topic,
        const QString &newTopic,
        const QString &alias,
        int qos,
        int format,
        const ProcessorReference &processor,
        const QString &color);
    void removeCurrentSubscription(const QString &topic);
    Q_INVOKABLE void setCurrentSubscriptionPaused(const QString &topic, bool paused);
    Q_INVOKABLE void setAllCurrentSubscriptionsPaused(bool paused);

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
    EventHistoryService &m_eventHistoryService;
};
