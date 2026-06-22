#include "app/appfacade.h"

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
