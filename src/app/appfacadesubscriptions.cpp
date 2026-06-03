#include "app/appfacade.h"

bool AppFacade::upsertCurrentSubscription(
    const QString &topic,
    int qos,
    int format,
    const QString &scriptId,
    const QString &alias)
{
    return m_subscriptionController.upsertCurrentSubscription(topic, qos, format, scriptId, alias);
}

bool AppFacade::updateCurrentSubscription(const QString &topic, const QString &alias, const QString &scriptId)
{
    return m_subscriptionController.updateCurrentSubscription(topic, alias, scriptId);
}

void AppFacade::removeCurrentSubscription(const QString &topic)
{
    m_subscriptionController.removeCurrentSubscription(topic);
}

void AppFacade::setCurrentSubscriptionPaused(const QString &topic, bool paused)
{
    m_subscriptionController.setCurrentSubscriptionPaused(topic, paused);
}

SubscriptionEntry *AppFacade::subscriptionByTopic(SessionState *session, const QString &topic)
{
    return m_subscriptionController.subscriptionByTopic(session, topic);
}

const SubscriptionEntry *AppFacade::subscriptionByTopic(const SessionState *session, const QString &topic) const
{
    return m_subscriptionController.subscriptionByTopic(session, topic);
}

const SubscriptionEntry *AppFacade::bestSubscriptionForTopic(const SessionState &session, const QString &topic) const
{
    return m_subscriptionController.bestSubscriptionForTopic(session, topic);
}

void AppFacade::restoreActiveSubscriptions(SessionState &session, bool emitEvents)
{
    m_subscriptionController.restoreActiveSubscriptions(session, emitEvents);
}

void AppFacade::ensureSubscriptionActive(SessionState &session, SubscriptionEntry &entry, bool emitEvents)
{
    m_subscriptionController.ensureSubscriptionActive(session, entry, emitEvents);
}

qreal AppFacade::subscriptionFps(const SubscriptionEntry &entry, qint64 nowMs) const
{
    return m_subscriptionController.subscriptionFps(entry, nowMs);
}

bool AppFacade::currentSessionHasActiveSubscriptionFps(qint64 nowMs) const
{
    return m_subscriptionController.currentSessionHasActiveSubscriptionFps(nowMs);
}

void AppFacade::refreshSubscriptionFps()
{
    m_subscriptionController.refreshSubscriptionFps();
}
