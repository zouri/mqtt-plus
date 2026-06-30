#include "app/applicationcore.h"

void ApplicationCore::connectCurrentSession()
{
    m_mqttController.connectCurrentSession();
}

void ApplicationCore::disconnectCurrentSession()
{
    m_mqttController.disconnectCurrentSession();
}

void ApplicationCore::publishCurrentSession(
    const QString &topic,
    const QString &payload,
    int format,
    int qos,
    bool retain)
{
    m_mqttController.publishCurrentSession(topic, payload, format, qos, retain);
}

void ApplicationCore::bindSessionSignals(SessionState *session)
{
    m_mqttController.bindSessionSignals(session);
}

void ApplicationCore::connectSession(SessionState &session, const QString &eventPrefix)
{
    m_mqttController.connectSession(session, eventPrefix);
}

QSslConfiguration ApplicationCore::sslConfigurationForSession(const SessionState &session, QString &errorMessage) const
{
    return m_mqttController.sslConfigurationForSession(session, errorMessage);
}

void ApplicationCore::updatePublishStatus(
    SessionState &session,
    const QString &state,
    const QString &reason,
    qint32 messageId)
{
    m_mqttController.updatePublishStatus(session, state, reason, messageId);
}
