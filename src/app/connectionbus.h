#pragma once

#include <QObject>
#include <QVariantMap>

class MqttWorkspaceCoordinator;

class ConnectionBus : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap sessionStatus READ sessionStatus NOTIFY sessionStatusChanged)
    Q_PROPERTY(QVariantMap publishStatus READ publishStatus NOTIFY publishStatusChanged)

public:
    explicit ConnectionBus(MqttWorkspaceCoordinator *coordinator, QObject *parent = nullptr);

    QVariantMap sessionStatus() const;
    QVariantMap publishStatus() const;

    Q_INVOKABLE void connectCurrentSession();
    Q_INVOKABLE void disconnectCurrentSession();
    Q_INVOKABLE void setCurrentOutputPaused(bool paused);
    Q_INVOKABLE void publishCurrentSession(
        const QString &topic,
        const QString &payload,
        int format = 0,
        int qos = 0,
        bool retain = false);

signals:
    void currentSessionChanged();
    void sessionStatusChanged();
    void publishStatusChanged();

private:
    MqttWorkspaceCoordinator *m_coordinator = nullptr;
};
