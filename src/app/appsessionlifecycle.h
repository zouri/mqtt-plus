#pragma once

#include "app/sessionlifecycleservice.h"

#include <memory>

class AppSessionLifecycle
{
public:
    using Dependencies = SessionLifecycleService::Dependencies;

    explicit AppSessionLifecycle(Dependencies dependencies);
    ~AppSessionLifecycle();

    void configureSession(SessionState &session, const QVariantMap &config, bool keepNameFallback);
    void initializeSessionRuntime(SessionState *session);
    void destroySessionRuntime(SessionState &session);
    void loadSessions();
    bool saveSessions();
    SessionState createDefaultSession(const QString &name);

private:
    std::unique_ptr<SessionLifecycleService> m_service;
};
