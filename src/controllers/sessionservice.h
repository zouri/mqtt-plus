#pragma once

#include "domain/session.h"

#include <QObject>
#include <QVariantMap>
#include <QVector>

#include <functional>

class HistoryStore;
class MqttSessionService;
class QTimer;
class SubscriptionService;

class SessionService : public QObject
{
    Q_OBJECT

public:
    struct Dependencies
    {
        HistoryStore *historyStore = nullptr;
        SubscriptionService *subscriptionController = nullptr;
        MqttSessionService *mqttController = nullptr;
        QTimer *subscriptionFpsRefreshTimer = nullptr;
        std::function<bool()> deleteHistoryWithSession;
        std::function<bool()> saveSessions;
        std::function<void(SessionState &, const QVariantMap &, bool)> configureSession;
        std::function<void(SessionState *)> initializeSessionRuntime;
        std::function<void(SessionState &)> destroySessionRuntime;
        std::function<SessionState(const QString &)> createDefaultSession;
        std::function<void()> reloadCurrentSessionHistory;
        std::function<void()> refreshSessionsModel;
        std::function<void()> refreshAllModels;
        std::function<void()> refreshSessionAndSubscriptionModels;
        std::function<void()> refreshCurrentSessionModels;
        std::function<void()> refreshScriptsModel;
    };

    explicit SessionService(QObject *parent = nullptr);

    void setDependencies(const Dependencies &dependencies);

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

signals:
    void currentSessionIndexChanged();
    void currentSessionChanged();

private:
    Dependencies m_dependencies;
    QVector<SessionState> m_sessions;
    int m_currentIndex = -1;
};
