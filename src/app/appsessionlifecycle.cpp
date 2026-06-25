#include "app/appsessionlifecycle.h"

#include <utility>

AppSessionLifecycle::AppSessionLifecycle(Dependencies dependencies)
{
    QObject *parent = dependencies.runtimeParent;
    m_service = std::make_unique<SessionLifecycleService>(std::move(dependencies), parent);
}

AppSessionLifecycle::~AppSessionLifecycle() = default;

void AppSessionLifecycle::configureSession(
    SessionState &session,
    const QVariantMap &config,
    bool keepNameFallback)
{
    m_service->configureSession(session, config, keepNameFallback);
}

void AppSessionLifecycle::initializeSessionRuntime(SessionState *session)
{
    m_service->initializeSessionRuntime(session);
}

void AppSessionLifecycle::destroySessionRuntime(SessionState &session)
{
    m_service->destroySessionRuntime(session);
}

void AppSessionLifecycle::loadSessions()
{
    m_service->loadSessions();
}

bool AppSessionLifecycle::saveSessions()
{
    return m_service->saveSessions();
}

SessionState AppSessionLifecycle::createDefaultSession(const QString &name)
{
    return m_service->createDefaultSession(name);
}
