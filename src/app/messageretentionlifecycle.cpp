#include "app/messageretentionlifecycle.h"

#include "services/storage/historystore.h"

MessageRetentionLifecycle::MessageRetentionLifecycle(HistoryStore &historyStore)
    : m_historyStore(historyStore)
{
}

void MessageRetentionLifecycle::applyRetention(const QVector<SessionState> &sessions, int limit)
{
    if (limit <= 0) {
        return;
    }

    for (const SessionState &session : sessions) {
        m_historyStore.pruneMessages(session.id, limit);
    }
}

void MessageRetentionLifecycle::applyExit(
    const QVector<SessionState> &sessions,
    int limit,
    const QString &clearMessagesMode,
    const QString &currentSessionId,
    const std::function<void()> &flushPendingMessages)
{
    if (flushPendingMessages) {
        flushPendingMessages();
    }

    applyRetention(sessions, limit);

    if (clearMessagesMode == QStringLiteral("all")) {
        m_historyStore.clearAllMessages();
    } else if (clearMessagesMode == QStringLiteral("current") && !currentSessionId.isEmpty()) {
        m_historyStore.clearMessages(currentSessionId);
    }
}
