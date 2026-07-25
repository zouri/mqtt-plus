#pragma once

#include <QObject>
#include <QSslConfiguration>
#include <QString>

#include "domain/session.h"

class EventHistoryService;
class SessionService;
class SubscriptionService;

class MqttSessionService : public QObject
{
    Q_OBJECT

public:
    explicit MqttSessionService(
        SessionService &sessionService,
        SubscriptionService &subscriptionService,
        EventHistoryService &eventHistoryService,
        QObject *parent = nullptr);

    void connectCurrentSession();
    void disconnectCurrentSession();
    bool publishCurrentSession(
        const QString &topic,
        const QString &payload,
        int format,
        int qos,
        bool retain);
    void bindSessionSignals(SessionState *session);
    void connectSession(SessionState &session, const QString &eventPrefix);

signals:
    void sessionStateChanged();

private:
    QSslConfiguration sslConfigurationForSession(
        const SessionState &session,
        QString &errorMessage) const;
    void updatePublishStatus(
        SessionState &session,
        const QString &state,
        const QString &reason = QString(),
        qint32 messageId = -1);

    SessionService &m_sessionService;
    SubscriptionService &m_subscriptionService;
    EventHistoryService &m_eventHistoryService;
};
