#include "app/subscriptionsbus.h"

#include "app/mqttworkspacecoordinator.h"

SubscriptionsBus::SubscriptionsBus(MqttWorkspaceCoordinator *coordinator, QObject *parent)
    : QObject(parent)
    , m_coordinator(coordinator)
{
    if (m_coordinator) {
        connect(m_coordinator, &MqttWorkspaceCoordinator::subscriptionsChanged, this, &SubscriptionsBus::subscriptionsChanged);
    }
}

SubscriptionFilterModel *SubscriptionsBus::filteredSubscriptions()
{
    return m_coordinator ? m_coordinator->filteredSubscriptions() : nullptr;
}

QStringList SubscriptionsBus::payloadFormats() const
{
    return m_coordinator ? m_coordinator->payloadFormats() : QStringList {};
}

bool SubscriptionsBus::upsertCurrentSubscription(
    const QString &topic,
    int qos,
    int format,
    const QString &scriptId,
    const QString &alias)
{
    return m_coordinator && m_coordinator->upsertCurrentSubscription(topic, qos, format, scriptId, alias);
}

bool SubscriptionsBus::updateCurrentSubscription(
    const QString &topic,
    const QString &newTopic,
    const QString &alias,
    const QString &scriptId)
{
    return m_coordinator && m_coordinator->updateCurrentSubscription(topic, newTopic, alias, scriptId);
}

void SubscriptionsBus::removeCurrentSubscription(const QString &topic)
{
    if (m_coordinator) {
        m_coordinator->removeCurrentSubscription(topic);
    }
}

void SubscriptionsBus::setCurrentSubscriptionPaused(const QString &topic, bool paused)
{
    if (m_coordinator) {
        m_coordinator->setCurrentSubscriptionPaused(topic, paused);
    }
}

QString SubscriptionsBus::showSubscriptionContextMenu(const QString &topic, const QPointF &globalPosition)
{
    return m_coordinator ? m_coordinator->showSubscriptionContextMenu(topic, globalPosition) : QString {};
}
