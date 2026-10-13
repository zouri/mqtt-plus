#pragma once

#include "services/storage/editordraftstore.h"
#include <QMutex>
#include <QMutexLocker>
#include <QUuid>
#include <chrono>
#include <functional>

class EditorWindowLease
{
public:
    static constexpr qint64 durationMs = 30000;
    using Clock = std::function<qint64()>;

    explicit EditorWindowLease(Clock clock = []() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }) : m_clock(std::move(clock)) {}

    QJsonObject acquire(const QString &session, const QString &window)
    {
        QMutexLocker lock(&m_mutex);
        const auto now = m_clock();
        if (m_expires > now && (m_session != session || m_window != window))
            return {{"editable", false}, {"leaseMs", durationMs}};
        if (m_expires <= now || m_session != session || m_window != window)
            m_token = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_session = session;
        m_window = window;
        m_expires = now + durationMs;
        return {{"editable", true}, {"leaseToken", m_token}, {"leaseMs", durationMs}};
    }

    bool allows(const QString &session, const QString &window, const QString &token)
    {
        QMutexLocker lock(&m_mutex);
        return owns(session, window, token);
    }

    bool release(const QString &session, const QString &window, const QString &token)
    {
        QMutexLocker lock(&m_mutex);
        if (!owns(session, window, token)) return false;
        m_expires = 0;
        return true;
    }

    EditorStoreReply write(const QString &session, const QString &window, const QString &token,
        const std::function<EditorStoreReply()> &operation)
    {
        // A handover cannot race a transaction already admitted for the current window.
        QMutexLocker lock(&m_mutex);
        if (!owns(session, window, token))
            return {423, {{"error", "Another window owns editing. This window is read-only."}}, {}, {}};
        return operation();
    }

private:
    bool owns(const QString &session, const QString &window, const QString &token) const
    {
        return !token.isEmpty() && m_expires > m_clock()
            && m_session == session && m_window == window && m_token == token;
    }

    Clock m_clock;
    QMutex m_mutex;
    QString m_session;
    QString m_window;
    QString m_token;
    qint64 m_expires = 0;
};
