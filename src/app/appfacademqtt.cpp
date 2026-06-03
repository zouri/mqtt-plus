#include "app/appfacade.h"

void AppFacade::connectCurrentSession()
{
    m_mqttController.connectCurrentSession();
}

void AppFacade::disconnectCurrentSession()
{
    m_mqttController.disconnectCurrentSession();
}

void AppFacade::publishCurrentSession(
    const QString &topic,
    const QString &payload,
    int format,
    int qos,
    bool retain)
{
    m_mqttController.publishCurrentSession(topic, payload, format, qos, retain);
}

void AppFacade::bindSessionSignals(SessionState *session)
{
    m_mqttController.bindSessionSignals(session);
}

void AppFacade::connectSession(SessionState &session, const QString &eventPrefix)
{
    m_mqttController.connectSession(session, eventPrefix);
}

QSslConfiguration AppFacade::sslConfigurationForSession(const SessionState &session, QString &errorMessage) const
{
    return m_mqttController.sslConfigurationForSession(session, errorMessage);
}

void AppFacade::updatePublishStatus(
    SessionState &session,
    const QString &state,
    const QString &reason,
    qint32 messageId)
{
    m_mqttController.updatePublishStatus(session, state, reason, messageId);
}
