#pragma once

#include "domain/session.h"

#include <QObject>
#include <QSettings>
#include <QVariantMap>

#include <functional>

class SessionController;

class SessionLifecycleService : public QObject
{
    Q_OBJECT

public:
    struct Dependencies
    {
        QSettings *settings = nullptr;
        QObject *runtimeParent = nullptr;
        SessionController *sessionController = nullptr;
        std::function<bool(const QString &)> scriptExists;
        std::function<SessionState *(const QString &)> sessionById;
        std::function<void(SessionState *)> bindSessionSignals;
        std::function<void(SessionState &, const QString &, const QString &)> appendEvent;
        std::function<void()> reloadCurrentSessionHistory;
        std::function<void()> publishSessionViewsChanged;
        std::function<void()> publishSessionCollectionViewsChanged;
        std::function<void(const QString &)> reportStorageError;
    };

    explicit SessionLifecycleService(Dependencies dependencies, QObject *parent = nullptr);

    void configureSession(SessionState &session, const QVariantMap &config, bool keepNameFallback);
    void initializeSessionRuntime(SessionState *session);
    void destroySessionRuntime(SessionState &session);
    void loadSessions();
    bool saveSessions();
    SessionState createDefaultSession(const QString &name);

private:
    Dependencies m_dependencies;
};
