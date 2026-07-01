#pragma once

#include "domain/session.h"

#include <QSslConfiguration>
#include <QString>

class EventController;
class SubscriptionController;

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
