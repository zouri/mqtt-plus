#pragma once

#include "domain/session.h"

#include <QObject>
#include <QVariantMap>
#include <QVector>
#include <functional>

class SessionController : public QObject
{
    Q_OBJECT

public:
    struct Dependencies
    {
        std::function<void()> reloadCurrentSessionHistory;
        std::function<bool()> currentSessionHasActiveSubscriptionFps;
        std::function<void(bool)> setSubscriptionFpsRefreshActive;
        std::function<void(SessionState &, const QVariantMap &, bool)> configureSession;
        std::function<void(SessionState &, const QString &, const QString &, qint32)> updatePublishStatus;
        std::function<bool()> saveSessions;
        std::function<void(SessionState &, const QString &)> connectSession;
        std::function<SessionState(const QString &)> createDefaultSession;
        std::function<bool()> deleteHistoryWithSession;
        std::function<void(const QString &)> clearSessionHistory;
        std::function<void(SessionState &)> destroySessionRuntime;
        std::function<void()> notifySelectedSessionViewsChanged;
        std::function<void()> notifyCurrentSessionViewsChanged;
        std::function<void()> notifyCurrentSessionAndSubscriptionsChanged;
        std::function<void()> notifySessionCollectionViewsChanged;
        std::function<void()> emitSessionsChanged;
        std::function<void()> emitMessageStreamChanged;
    };

    explicit SessionController(QObject *parent = nullptr);

    void setDependencies(Dependencies dependencies);

    QVector<SessionState> &sessions();
    const QVector<SessionState> &sessions() const;
    int currentIndex() const;
    void setCurrentIndex(int index);

    SessionState *currentSession();
    const SessionState *currentSession() const;
    SessionState *sessionById(const QString &sessionId);
    const SessionState *sessionById(const QString &sessionId) const;

    void appendSession(const SessionState &session);
    SessionState takeSessionAt(int index);
    void clear();
    bool isValidIndex(int index) const;

    void setCurrentSessionIndex(int index);
    QVariantMap defaultSessionConfig() const;
    QVariantMap sessionConfigAt(int index) const;
    bool updateSessionConfigAt(int index, const QVariantMap &config);
    void addSessionWithConfig(const QVariantMap &config);
    void duplicateSessionAt(int index);
    void removeSessionAt(int index);
    void setCurrentOutputPaused(bool paused);

private:
    Dependencies m_dependencies;
    QVector<SessionState> m_sessions;
    int m_currentIndex = -1;
};
