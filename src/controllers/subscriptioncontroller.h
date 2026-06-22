#pragma once

#include "domain/session.h"
#include "domain/subscription.h"
#include "models/subscriptionlistmodel.h"

#include <QObject>
#include <QMqttSubscription>
#include <QVector>
#include <functional>

class SubscriptionController : public QObject
{
    Q_OBJECT

public:
    struct Dependencies
    {
        std::function<SessionState *()> currentSession;
        std::function<SessionState *(const QString &)> sessionById;
        std::function<void(SessionState &, const QString &, const QString &)> appendEvent;
        std::function<bool(const QString &)> scriptExists;
        std::function<bool()> saveSessions;
        std::function<void()> notifySessionAndSubscriptionViewsChanged;
        std::function<void()> notifyCurrentSessionAndSubscriptionsChanged;
        std::function<void()> refreshSubscriptionsModel;
        std::function<void()> emitSubscriptionsChanged;
        std::function<void(bool)> setSubscriptionFpsRefreshActive;
        std::function<void(const QVector<SubscriptionFpsRow> &)> setTopicFpsRows;
    };

    explicit SubscriptionController(QObject *parent = nullptr);

    void setDependencies(Dependencies dependencies);

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

    Dependencies m_dependencies;
};
