#include "app/applicationcore.h"

bool ApplicationCore::upsertCurrentSubscription(
    const QString &topic,
    int qos,
    int format,
    const QString &scriptId,
    const QString &alias)
{
    return m_subscriptionController.upsertCurrentSubscription(topic, qos, format, scriptId, alias);
}

bool ApplicationCore::updateCurrentSubscription(
    const QString &topic,
    const QString &newTopic,
    const QString &alias,
    const QString &scriptId)
{
    return m_subscriptionController.updateCurrentSubscription(topic, newTopic, alias, scriptId);
}

void ApplicationCore::removeCurrentSubscription(const QString &topic)
{
    m_subscriptionController.removeCurrentSubscription(topic);
}

void ApplicationCore::setCurrentSubscriptionPaused(const QString &topic, bool paused)
{
    m_subscriptionController.setCurrentSubscriptionPaused(topic, paused);
}

SubscriptionEntry *ApplicationCore::subscriptionByTopic(SessionState *session, const QString &topic)
{
    return m_subscriptionController.subscriptionByTopic(session, topic);
}

const SubscriptionEntry *ApplicationCore::subscriptionByTopic(const SessionState *session, const QString &topic) const
{
    return m_subscriptionController.subscriptionByTopic(session, topic);
}

const SubscriptionEntry *ApplicationCore::bestSubscriptionForTopic(const SessionState &session, const QString &topic) const
{
    return m_subscriptionController.bestSubscriptionForTopic(session, topic);
}

void ApplicationCore::restoreActiveSubscriptions(SessionState &session, bool emitEvents)
{
    m_subscriptionController.restoreActiveSubscriptions(session, emitEvents);
}

void ApplicationCore::ensureSubscriptionActive(SessionState &session, SubscriptionEntry &entry, bool emitEvents)
{
    m_subscriptionController.ensureSubscriptionActive(session, entry, emitEvents);
}

qreal ApplicationCore::subscriptionFps(const SubscriptionEntry &entry, qint64 nowMs) const
{
    return m_subscriptionController.subscriptionFps(entry, nowMs);
}

bool ApplicationCore::currentSessionHasActiveSubscriptionFps(qint64 nowMs) const
{
    return m_subscriptionController.currentSessionHasActiveSubscriptionFps(nowMs);
}

void ApplicationCore::refreshSubscriptionFps()
{
    m_subscriptionController.refreshSubscriptionFps();
}
