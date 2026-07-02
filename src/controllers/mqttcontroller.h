#pragma once

#include <QObject>
#include <QSslConfiguration>
#include <QString>

#include <functional>

#include "domain/session.h"

class EventController;
class SubscriptionController;

class MqttController : public QObject
{
    Q_OBJECT

public:
    struct Dependencies
    {
        SubscriptionController *subscriptionController = nullptr;
        EventController *eventController = nullptr;
        std::function<SessionState *()> currentSessionState;
        std::function<SessionState *(const QString &)> sessionById;
        std::function<void(SessionState &, const QString &, const QString &)> appendEvent;
        std::function<void()> refreshModels;
        std::function<void()> refreshCurrentSessionModels;
    };

    explicit MqttController(QObject *parent = nullptr);

    void setDependencies(const Dependencies &dependencies);

    void connectCurrentSession();
    void disconnectCurrentSession();
    void publishCurrentSession(
        const QString &topic,
        const QString &payload,
        int format,
        int qos,
        bool retain);
    void bindSessionSignals(SessionState *session);
    void connectSession(SessionState &session, const QString &eventPrefix);
    QSslConfiguration sslConfigurationForSession(const SessionState &session, QString &errorMessage) const;
    void updatePublishStatus(
        SessionState &session,
        const QString &state,
        const QString &reason = QString(),
        qint32 messageId = -1);

signals:
    void sessionStateChanged();

private:
    Dependencies m_dependencies;
};
