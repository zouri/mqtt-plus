#pragma once

#include "domain/session.h"

#include <QString>
#include <QVector>

#include <functional>

class HistoryStore;

class MessageRetentionLifecycle
{
public:
    explicit MessageRetentionLifecycle(HistoryStore &historyStore);

    void applyRetention(const QVector<SessionState> &sessions, int limit);
    void applyExit(
        const QVector<SessionState> &sessions,
        int limit,
        const QString &clearMessagesMode,
        const QString &currentSessionId,
        const std::function<void()> &flushPendingMessages);

private:
    HistoryStore &m_historyStore;
};
