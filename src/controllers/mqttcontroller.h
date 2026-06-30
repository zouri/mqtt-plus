#pragma once

#include "controllers/applicationcontext.h"

#include <QObject>
#include <QSslConfiguration>
#include <QString>

#include "domain/session.h"

class MqttController : public QObject
{
    Q_OBJECT

public:
    explicit MqttController(MqttControllerContext *app, QObject *parent = nullptr);

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
    MqttControllerContext &m_app;
};
