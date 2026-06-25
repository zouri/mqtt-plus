#include "app/appworkspacesurface.h"

#include "app/appsurfacedependencies.h"
#include "app/appruntimestate.h"
#include "app/connectionbus.h"
#include "app/messagesbus.h"
#include "app/mqttworkspacecoordinator.h"
#include "app/sessionsbus.h"
#include "app/subscriptionsbus.h"

AppWorkspaceSurface::AppWorkspaceSurface(AppRuntimeState *state)
    : m_coordinator(std::make_unique<MqttWorkspaceCoordinator>(state->surfaceDependencies()->workspaceCoordinatorDependencies(), state))
    , m_sessionsBus(std::make_unique<SessionsBus>(m_coordinator.get()))
    , m_subscriptionsBus(std::make_unique<SubscriptionsBus>(m_coordinator.get()))
    , m_connectionBus(std::make_unique<ConnectionBus>(m_coordinator.get()))
    , m_messagesBus(std::make_unique<MessagesBus>(m_coordinator.get()))
{
    state->surfaceDependencies()->bindWorkspaceSurfaceSignals(m_coordinator.get());
}

AppWorkspaceSurface::~AppWorkspaceSurface() = default;

SessionsBus *AppWorkspaceSurface::sessions()
{
    return m_sessionsBus.get();
}

SubscriptionsBus *AppWorkspaceSurface::subscriptions()
{
    return m_subscriptionsBus.get();
}

ConnectionBus *AppWorkspaceSurface::connection()
{
    return m_connectionBus.get();
}

MessagesBus *AppWorkspaceSurface::messages()
{
    return m_messagesBus.get();
}
