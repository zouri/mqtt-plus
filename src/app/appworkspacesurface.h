#pragma once

#include <memory>

class AppRuntimeState;
class ConnectionBus;
class MessagesBus;
class MqttWorkspaceCoordinator;
class SessionsBus;
class SubscriptionsBus;

class AppWorkspaceSurface
{
public:
    explicit AppWorkspaceSurface(AppRuntimeState *state);
    ~AppWorkspaceSurface();

    SessionsBus *sessions();
    SubscriptionsBus *subscriptions();
    ConnectionBus *connection();
    MessagesBus *messages();

private:
    std::unique_ptr<MqttWorkspaceCoordinator> m_coordinator;
    std::unique_ptr<SessionsBus> m_sessionsBus;
    std::unique_ptr<SubscriptionsBus> m_subscriptionsBus;
    std::unique_ptr<ConnectionBus> m_connectionBus;
    std::unique_ptr<MessagesBus> m_messagesBus;
};
