#pragma once

#include "domain/session.h"

#include <QString>
#include <QVariantMap>

class HistoryStore;
class QTimer;
class SubscriptionController;

class SessionControllerContext
{
public:
    virtual ~SessionControllerContext() = default;

    virtual HistoryStore &historyStore() = 0;
    virtual SubscriptionController &subscriptionController() = 0;
    virtual QTimer &subscriptionFpsRefreshTimer() = 0;

    virtual bool deleteHistoryWithSession() const = 0;

    virtual bool saveSessions() = 0;
    virtual void configureSession(SessionState &session, const QVariantMap &config, bool keepNameFallback) = 0;
    virtual void initializeSessionRuntime(SessionState *session) = 0;
    virtual void destroySessionRuntime(SessionState &session) = 0;
    virtual void connectSession(SessionState &session, const QString &eventPrefix) = 0;
    virtual SessionState createDefaultSession(const QString &name) = 0;
    virtual void updatePublishStatus(
        SessionState &session,
        const QString &state,
        const QString &reason = QString(),
        qint32 messageId = -1) = 0;
    virtual void reloadCurrentSessionHistory() = 0;
    virtual void notifyCurrentSessionViewsChanged() = 0;
    virtual void notifyCurrentSessionAndSubscriptionsChanged() = 0;
    virtual void notifySelectedSessionViewsChanged() = 0;
    virtual void notifySessionCollectionViewsChanged() = 0;
    virtual void refreshSessionsModel() = 0;

    virtual void emitSessionsChanged() = 0;
    virtual void emitMessageStreamChanged() = 0;
};
