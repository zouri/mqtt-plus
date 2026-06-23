#pragma once

#include <QObject>
#include <QByteArray>
#include <QSslConfiguration>
#include <QString>
#include <functional>

#include "domain/session.h"

class MqttController : public QObject
{
    Q_OBJECT

public:
    struct Dependencies
    {
        std::function<SessionState *()> currentSession;
        std::function<SessionState *(const QString &)> sessionById;
        std::function<void(SessionState &, const QString &, const QString &)> appendEvent;
        std::function<void(SessionState &, bool)> restoreActiveSubscriptions;
        std::function<void(SessionState &)> resetRuntimeSubscriptions;
        std::function<void(const QString &, const QString &, const QByteArray &)> appendIncomingMessage;
        std::function<void()> publishSessionViewsChanged;
        std::function<void()> publishCurrentSessionViewsChanged;
        std::function<void()> publishSessionAndSubscriptionViewsChanged;
    };

    explicit MqttController(QObject *parent = nullptr);

    void setDependencies(Dependencies dependencies);

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

private:
    Dependencies m_dependencies;
};
