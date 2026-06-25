#pragma once

#include <QObject>
#include <QPointF>
#include <QStringList>
#include <QVariantMap>
#include <functional>

class EventController;
class EventStreamModel;
class MqttController;
class SessionController;
class SessionListModel;
struct SessionState;
struct SubscriptionEntry;
class SubscriptionController;
class SubscriptionFilterModel;

class MqttWorkspaceCoordinator : public QObject
{
    Q_OBJECT

public:
    struct Dependencies
    {
        SessionListModel *sessionsModel = nullptr;
        SubscriptionFilterModel *filteredSubscriptionsModel = nullptr;
        EventStreamModel *messagesModel = nullptr;
        SessionController *sessionController = nullptr;
        SubscriptionController *subscriptionController = nullptr;
        MqttController *mqttController = nullptr;
        EventController *eventController = nullptr;
        std::function<SessionState *()> currentSession;
        std::function<const SubscriptionEntry *(const SessionState *, const QString &)> subscriptionByTopic;
    };

    explicit MqttWorkspaceCoordinator(Dependencies dependencies, QObject *parent = nullptr);

    SessionListModel *sessions();
    SubscriptionFilterModel *filteredSubscriptions();
    int currentSessionIndex() const;
    QVariantMap currentSession() const;
    QVariantMap sessionStatus() const;
    QVariantMap publishStatus() const;
    QStringList payloadFormats() const;
    EventStreamModel *messages();

    void setCurrentSessionIndex(int index);

    QVariantMap defaultSessionConfig() const;
    QVariantMap sessionConfigAt(int index) const;
    bool updateSessionConfigAt(int index, const QVariantMap &config);
    void addSessionWithConfig(const QVariantMap &config);
    void duplicateSessionAt(int index);
    void removeSessionAt(int index);
    QString showSessionContextMenu(int index, const QPointF &globalPosition);
    QString showSubscriptionContextMenu(const QString &topic, const QPointF &globalPosition);
    void connectCurrentSession();
    void disconnectCurrentSession();
    void setCurrentOutputPaused(bool paused);
    bool upsertCurrentSubscription(
        const QString &topic,
        int qos = 0,
        int format = 0,
        const QString &scriptId = QString(),
        const QString &alias = QString());
    bool updateCurrentSubscription(
        const QString &topic,
        const QString &newTopic,
        const QString &alias,
        const QString &scriptId);
    void removeCurrentSubscription(const QString &topic);
    void setCurrentSubscriptionPaused(const QString &topic, bool paused);
    void publishCurrentSession(
        const QString &topic,
        const QString &payload,
        int format = 0,
        int qos = 0,
        bool retain = false);
    void copyTextToClipboard(const QString &text) const;
    void clearCurrentMessages();
    int loadOlderCurrentSessionMessages();

signals:
    void sessionsChanged();
    void currentSessionIndexChanged();
    void currentSessionChanged();
    void subscriptionsChanged();
    void messageStreamChanged();
    void messageStreamRowAppended(const QVariantMap &row);

private:
    Dependencies m_dependencies;
};
