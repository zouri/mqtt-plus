#include "app/sessionsbus.h"

#include "app/mqttworkspacecoordinator.h"

SessionsBus::SessionsBus(MqttWorkspaceCoordinator *coordinator, QObject *parent)
    : QObject(parent)
    , m_coordinator(coordinator)
{
    if (!m_coordinator) {
        return;
    }

    connect(m_coordinator, &MqttWorkspaceCoordinator::sessionsChanged, this, &SessionsBus::sessionsChanged);
    connect(m_coordinator, &MqttWorkspaceCoordinator::currentSessionIndexChanged, this, &SessionsBus::currentSessionIndexChanged);
    connect(m_coordinator, &MqttWorkspaceCoordinator::currentSessionChanged, this, &SessionsBus::currentSessionChanged);
}

SessionListModel *SessionsBus::sessions()
{
    return m_coordinator ? m_coordinator->sessions() : nullptr;
}

int SessionsBus::currentSessionIndex() const
{
    return m_coordinator ? m_coordinator->currentSessionIndex() : -1;
}

QVariantMap SessionsBus::currentSession() const
{
    return m_coordinator ? m_coordinator->currentSession() : QVariantMap {};
}

void SessionsBus::setCurrentSessionIndex(int index)
{
    if (m_coordinator) {
        m_coordinator->setCurrentSessionIndex(index);
    }
}

QVariantMap SessionsBus::defaultSessionConfig() const
{
    return m_coordinator ? m_coordinator->defaultSessionConfig() : QVariantMap {};
}

QVariantMap SessionsBus::sessionConfigAt(int index) const
{
    return m_coordinator ? m_coordinator->sessionConfigAt(index) : QVariantMap {};
}

bool SessionsBus::updateSessionConfigAt(int index, const QVariantMap &config)
{
    return m_coordinator && m_coordinator->updateSessionConfigAt(index, config);
}

void SessionsBus::addSessionWithConfig(const QVariantMap &config)
{
    if (m_coordinator) {
        m_coordinator->addSessionWithConfig(config);
    }
}

void SessionsBus::duplicateSessionAt(int index)
{
    if (m_coordinator) {
        m_coordinator->duplicateSessionAt(index);
    }
}

void SessionsBus::removeSessionAt(int index)
{
    if (m_coordinator) {
        m_coordinator->removeSessionAt(index);
    }
}

QString SessionsBus::showSessionContextMenu(int index, const QPointF &globalPosition)
{
    return m_coordinator ? m_coordinator->showSessionContextMenu(index, globalPosition) : QString {};
}
