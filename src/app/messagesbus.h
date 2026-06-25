#pragma once

#include <QObject>
#include <QVariantMap>

class EventStreamModel;
class MqttWorkspaceCoordinator;

class MessagesBus : public QObject
{
    Q_OBJECT
    Q_PROPERTY(EventStreamModel* messages READ messages CONSTANT)

public:
    explicit MessagesBus(MqttWorkspaceCoordinator *coordinator, QObject *parent = nullptr);

    EventStreamModel *messages();

    Q_INVOKABLE int loadOlderCurrentSessionMessages();
    Q_INVOKABLE void clearCurrentMessages();
    Q_INVOKABLE void copyTextToClipboard(const QString &text) const;

signals:
    void messageStreamChanged();
    void messageStreamRowAppended(const QVariantMap &row);

private:
    MqttWorkspaceCoordinator *m_coordinator = nullptr;
};
