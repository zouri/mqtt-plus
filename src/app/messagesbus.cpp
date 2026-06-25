#include "app/messagesbus.h"

#include "app/mqttworkspacecoordinator.h"

MessagesBus::MessagesBus(MqttWorkspaceCoordinator *coordinator, QObject *parent)
    : QObject(parent)
    , m_coordinator(coordinator)
{
    if (!m_coordinator) {
        return;
    }

    connect(m_coordinator, &MqttWorkspaceCoordinator::messageStreamChanged, this, &MessagesBus::messageStreamChanged);
    connect(m_coordinator, &MqttWorkspaceCoordinator::messageStreamRowAppended, this, &MessagesBus::messageStreamRowAppended);
}

EventStreamModel *MessagesBus::messages()
{
    return m_coordinator ? m_coordinator->messages() : nullptr;
}

int MessagesBus::loadOlderCurrentSessionMessages()
{
    return m_coordinator ? m_coordinator->loadOlderCurrentSessionMessages() : 0;
}

void MessagesBus::clearCurrentMessages()
{
    if (m_coordinator) {
        m_coordinator->clearCurrentMessages();
    }
}

void MessagesBus::copyTextToClipboard(const QString &text) const
{
    if (m_coordinator) {
        m_coordinator->copyTextToClipboard(text);
    }
}
