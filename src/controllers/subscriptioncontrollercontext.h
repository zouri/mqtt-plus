#pragma once

#include "domain/session.h"
#include "domain/subscription.h"

#include <QString>

class QTimer;
class ScriptController;
class SubscriptionListModel;

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
