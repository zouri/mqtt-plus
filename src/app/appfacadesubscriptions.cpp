#include "app/appfacade.h"

SubscriptionEntry *AppFacade::subscriptionByTopic(SessionState *session, const QString &topic)
{
    return m_subscriptionController.subscriptionByTopic(session, topic);
}

const SubscriptionEntry *AppFacade::subscriptionByTopic(const SessionState *session, const QString &topic) const
{
    return m_subscriptionController.subscriptionByTopic(session, topic);
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
