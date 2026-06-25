#pragma once

#include <QObject>
#include <QPointF>
#include <QVariantMap>

class SessionListModel;
class MqttWorkspaceCoordinator;

class SessionsBus : public QObject
{
    Q_OBJECT
    Q_PROPERTY(SessionListModel* sessions READ sessions CONSTANT)
    Q_PROPERTY(int currentSessionIndex READ currentSessionIndex WRITE setCurrentSessionIndex NOTIFY currentSessionIndexChanged)
    Q_PROPERTY(QVariantMap currentSession READ currentSession NOTIFY currentSessionChanged)

public:
    explicit SessionsBus(MqttWorkspaceCoordinator *coordinator, QObject *parent = nullptr);

    SessionListModel *sessions();
    int currentSessionIndex() const;
    QVariantMap currentSession() const;

    void setCurrentSessionIndex(int index);

    Q_INVOKABLE QVariantMap defaultSessionConfig() const;
    Q_INVOKABLE QVariantMap sessionConfigAt(int index) const;
    Q_INVOKABLE bool updateSessionConfigAt(int index, const QVariantMap &config);
    Q_INVOKABLE void addSessionWithConfig(const QVariantMap &config);
    Q_INVOKABLE void duplicateSessionAt(int index);
    Q_INVOKABLE void removeSessionAt(int index);
    Q_INVOKABLE QString showSessionContextMenu(int index, const QPointF &globalPosition);

signals:
    void sessionsChanged();
    void currentSessionIndexChanged();
    void currentSessionChanged();

private:
    MqttWorkspaceCoordinator *m_coordinator = nullptr;
};
