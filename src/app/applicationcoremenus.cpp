#include "app/applicationcore.h"

QString ApplicationCore::showSessionContextMenu(int index, const QPointF &globalPosition)
{
    if (!m_sessionController.isValidIndex(index)) {
        return {};
    }

    return m_platformActions.showSessionContextMenu(m_sessionController.sessions().size() > 1, globalPosition);
}

QString ApplicationCore::showSubscriptionContextMenu(const QString &topic, const QPointF &globalPosition)
{
    const SessionState *session = currentSessionState();
    if (!session || !subscriptionByTopic(session, topic.trimmed())) {
        return {};
    }

    return m_platformActions.showSubscriptionContextMenu(globalPosition);
}
