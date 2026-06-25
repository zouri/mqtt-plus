#pragma once

#include <QObject>
#include <QPointF>
#include <QStringList>

class MqttWorkspaceCoordinator;
class SubscriptionFilterModel;

class SubscriptionsBus : public QObject
{
    Q_OBJECT
    Q_PROPERTY(SubscriptionFilterModel* filteredSubscriptions READ filteredSubscriptions CONSTANT)
    Q_PROPERTY(QStringList payloadFormats READ payloadFormats CONSTANT)

public:
    explicit SubscriptionsBus(MqttWorkspaceCoordinator *coordinator, QObject *parent = nullptr);

    SubscriptionFilterModel *filteredSubscriptions();
    QStringList payloadFormats() const;

    Q_INVOKABLE bool upsertCurrentSubscription(
        const QString &topic,
        int qos = 0,
        int format = 0,
        const QString &scriptId = QString(),
        const QString &alias = QString());
    Q_INVOKABLE bool updateCurrentSubscription(
        const QString &topic,
        const QString &newTopic,
        const QString &alias,
        const QString &scriptId);
    Q_INVOKABLE void removeCurrentSubscription(const QString &topic);
    Q_INVOKABLE void setCurrentSubscriptionPaused(const QString &topic, bool paused);
    Q_INVOKABLE QString showSubscriptionContextMenu(const QString &topic, const QPointF &globalPosition);

signals:
    void subscriptionsChanged();

private:
    MqttWorkspaceCoordinator *m_coordinator = nullptr;
};
