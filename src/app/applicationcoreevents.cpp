#include "app/applicationcore.h"

void ApplicationCore::clearCurrentMessages()
{
    m_eventController.clearCurrentMessages();
}

void ApplicationCore::clearCurrentLogs()
{
    m_eventController.clearCurrentLogs();
}

void ApplicationCore::clearAllMessages()
{
    m_historyStore.clearAllMessages();
    for (auto &session : m_sessionController.sessions()) {
        session.messageRows.clear();
        session.oldestLoadedMessageId = 0;
        session.loadedAllMessageHistory = true;
    }
    m_messagesModel.clear();
    refreshScriptTestSamplesModel();
    emit messageStreamChanged();
    emit scriptTestSamplesChanged();
}

void ApplicationCore::clearAllLogs()
{
    m_historyStore.clearAllLogs();
    for (auto &session : m_sessionController.sessions()) {
        session.logRows.clear();
        session.oldestLoadedLogId = 0;
        session.loadedAllLogHistory = true;
    }
    m_logsModel.clear();
    emit logStreamChanged();
}

void ApplicationCore::clearAllHistory()
{
    m_historyStore.clearAllMessages();
    m_historyStore.clearAllLogs();
    for (auto &session : m_sessionController.sessions()) {
        session.messageRows.clear();
        session.oldestLoadedMessageId = 0;
        session.loadedAllMessageHistory = true;
        session.logRows.clear();
        session.oldestLoadedLogId = 0;
        session.loadedAllLogHistory = true;
    }
    m_messagesModel.clear();
    m_logsModel.clear();
    refreshScriptTestSamplesModel();
    emit messageStreamChanged();
    emit logStreamChanged();
    emit scriptTestSamplesChanged();
}

int ApplicationCore::loadOlderCurrentSessionMessages()
{
    return m_eventController.loadOlderCurrentSessionMessages();
}

int ApplicationCore::loadOlderCurrentSessionLogs()
{
    return m_eventController.loadOlderCurrentSessionLogs();
}

void ApplicationCore::appendRenderedMessageRow(SessionState &session, const QVariantMap &row)
{
    m_eventController.appendRenderedMessageRow(session, row);
}

void ApplicationCore::appendRenderedLogRow(SessionState &session, const QVariantMap &row)
{
    m_eventController.appendRenderedLogRow(session, row);
}

void ApplicationCore::appendEvent(SessionState &session, const QString &channel, const QString &message)
{
    m_eventController.appendEvent(session, channel, message);
}

LuaScriptResult ApplicationCore::parseIncomingPayload(
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

void ApplicationCore::appendIncomingMessage(const QString &sessionId, const QString &topic, const QByteArray &payloadBytes)
{
    m_eventController.appendIncomingMessage(sessionId, topic, payloadBytes);
}

void ApplicationCore::trimVisibleMessageRows(SessionState &session)
{
    m_eventController.trimVisibleMessageRows(session);
}

void ApplicationCore::trimVisibleLogRows(SessionState &session)
{
    m_eventController.trimVisibleLogRows(session);
}

void ApplicationCore::reloadCurrentSessionHistory()
{
    m_eventController.reloadCurrentSessionHistory();
}
