#include "app/appfacade.h"

void AppFacade::clearCurrentMessages()
{
    m_eventController.clearCurrentMessages();
}

void AppFacade::clearCurrentLogs()
{
    m_eventController.clearCurrentLogs();
}

int AppFacade::loadOlderCurrentSessionMessages()
{
    return m_eventController.loadOlderCurrentSessionMessages();
}

int AppFacade::loadOlderCurrentSessionLogs()
{
    return m_eventController.loadOlderCurrentSessionLogs();
}

void AppFacade::appendRenderedMessageRow(SessionState &session, const QVariantMap &row)
{
    m_eventController.appendRenderedMessageRow(session, row);
}

void AppFacade::appendRenderedLogRow(SessionState &session, const QVariantMap &row)
{
    m_eventController.appendRenderedLogRow(session, row);
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

void AppFacade::trimVisibleMessageRows(SessionState &session)
{
    m_eventController.trimVisibleMessageRows(session);
}

void AppFacade::trimVisibleLogRows(SessionState &session)
{
    m_eventController.trimVisibleLogRows(session);
}

void AppFacade::reloadCurrentSessionHistory()
{
    m_eventController.reloadCurrentSessionHistory();
}
