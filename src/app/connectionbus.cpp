#include "app/connectionbus.h"

#include "app/mqttworkspacecoordinator.h"

ConnectionBus::ConnectionBus(MqttWorkspaceCoordinator *coordinator, QObject *parent)
    : QObject(parent)
    , m_coordinator(coordinator)
{
    if (!m_coordinator) {
        return;
    }

    connect(m_coordinator, &MqttWorkspaceCoordinator::currentSessionChanged, this, [this]() {
        emit currentSessionChanged();
        emit sessionStatusChanged();
        emit publishStatusChanged();
    });
}

QVariantMap ConnectionBus::sessionStatus() const
{
    return m_coordinator ? m_coordinator->sessionStatus() : QVariantMap {};
}

QVariantMap ConnectionBus::publishStatus() const
{
    return m_coordinator ? m_coordinator->publishStatus() : QVariantMap {};
}

void ConnectionBus::connectCurrentSession()
{
    if (m_coordinator) {
        m_coordinator->connectCurrentSession();
    }
}

void ConnectionBus::disconnectCurrentSession()
{
    if (m_coordinator) {
        m_coordinator->disconnectCurrentSession();
    }
}

void ConnectionBus::setCurrentOutputPaused(bool paused)
{
    if (m_coordinator) {
        m_coordinator->setCurrentOutputPaused(paused);
    }
}

void ConnectionBus::publishCurrentSession(
    const QString &topic,
    const QString &payload,
    int format,
    int qos,
    bool retain)
{
    if (m_coordinator) {
        m_coordinator->publishCurrentSession(topic, payload, format, qos, retain);
    }
}
