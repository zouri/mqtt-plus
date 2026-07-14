#pragma once

#include "domain/session.h"
#include "domain/subscription.h"

#include <QObject>
#include <QMqttSubscription>

#include <functional>

class EventHistoryService;
class QTimer;
class ScriptService;
class SubscriptionListModel;

class SubscriptionService : public QObject
{
    Q_OBJECT

public:
    struct Dependencies
    {
        SubscriptionListModel *subscriptionsModel = nullptr;
        ScriptService *scriptController = nullptr;
        EventHistoryService *eventController = nullptr;
        QTimer *subscriptionFpsRefreshTimer = nullptr;
        std::function<SessionState *()> currentSessionState;
        std::function<SessionState *(const QString &)> sessionById;
        std::function<bool()> saveSessions;
        std::function<void()> refreshSubscriptionsModel;
    };

    explicit SubscriptionService(QObject *parent = nullptr);

    void setDependencies(const Dependencies &dependencies);

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

    SubscriptionEntry *subscriptionByTopic(SessionState *session, const QString &topic);
    const SubscriptionEntry *subscriptionByTopic(const SessionState *session, const QString &topic) const;
    const SubscriptionEntry *bestSubscriptionForTopic(const SessionState &session, const QString &topic) const;
    void resetRuntimeSubscriptions(SessionState &session);
    void restoreActiveSubscriptions(SessionState &session, bool emitEvents);
    void ensureSubscriptionActive(SessionState &session, SubscriptionEntry &entry, bool emitEvents);
    qreal subscriptionFps(const SubscriptionEntry &entry, qint64 nowMs) const;
    bool currentSessionHasActiveSubscriptionFps(qint64 nowMs) const;
    void refreshSubscriptionFps();

signals:
    void subscriptionsChanged();

private:
    void observeSubscription(SessionState &session, SubscriptionEntry &entry, QMqttSubscription *subscription);
    void updateSubscriptionState(
        const QString &sessionId,
        const QString &topic,
        const QPointer<QMqttSubscription> &subscription,
        QMqttSubscription::SubscriptionState state);

    Dependencies m_dependencies;
};
