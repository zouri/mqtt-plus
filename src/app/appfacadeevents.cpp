#include "app/appfacade.h"

void AppFacade::clearCurrentMessages()
{
    m_eventController.clearCurrentMessages();
}

int AppFacade::loadOlderCurrentSessionEvents()
{
    return m_eventController.loadOlderCurrentSessionEvents();
}

void AppFacade::appendRenderedEventRow(SessionState &session, const QVariantMap &row)
{
    m_eventController.appendRenderedEventRow(session, row);
}

void AppFacade::appendEvent(SessionState &session, const QString &channel, const QString &message)
{
    m_eventController.appendEvent(session, channel, message);
}

LuaScriptResult AppFacade::parseIncomingPayload(
    const SessionState &session,
    const SubscriptionEntry *subscription,
    const QString &topic,
    const QByteArray &payloadBytes,
    const QString &timestamp,
    QString &scriptNameOut,
    QString &decodedPayloadOut) const
{
    return m_eventController.parseIncomingPayload(
        session,
        subscription,
        topic,
        payloadBytes,
        timestamp,
        scriptNameOut,
        decodedPayloadOut);
}

void AppFacade::appendIncomingMessage(const QString &sessionId, const QString &topic, const QByteArray &payloadBytes)
{
    m_eventController.appendIncomingMessage(sessionId, topic, payloadBytes);
}

void AppFacade::trimVisibleEventRows(SessionState &session)
{
    m_eventController.trimVisibleEventRows(session);
}

void AppFacade::reloadCurrentSessionHistory()
{
    m_eventController.reloadCurrentSessionHistory();
}
