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

class WorkbenchBus : public QObject
{
    Q_OBJECT
    Q_PROPERTY(SessionListModel* sessions READ sessions CONSTANT)
    Q_PROPERTY(SubscriptionFilterModel* filteredSubscriptions READ filteredSubscriptions CONSTANT)
    Q_PROPERTY(int currentSessionIndex READ currentSessionIndex WRITE setCurrentSessionIndex NOTIFY currentSessionIndexChanged)
    Q_PROPERTY(QVariantMap currentSession READ currentSession NOTIFY currentSessionChanged)
    Q_PROPERTY(QVariantMap sessionStatus READ sessionStatus NOTIFY currentSessionChanged)
    Q_PROPERTY(QVariantMap publishStatus READ publishStatus NOTIFY currentSessionChanged)
    Q_PROPERTY(QStringList payloadFormats READ payloadFormats CONSTANT)
    Q_PROPERTY(EventStreamModel* messages READ messages CONSTANT)

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

    explicit WorkbenchBus(Dependencies dependencies, QObject *parent = nullptr);

    SessionListModel *sessions();
    SubscriptionFilterModel *filteredSubscriptions();
    int currentSessionIndex() const;
    QVariantMap currentSession() const;
    QVariantMap sessionStatus() const;
    QVariantMap publishStatus() const;
    QStringList payloadFormats() const;
    EventStreamModel *messages();

    void setCurrentSessionIndex(int index);

    Q_INVOKABLE QVariantMap defaultSessionConfig() const;
    Q_INVOKABLE QVariantMap sessionConfigAt(int index) const;
    Q_INVOKABLE bool updateSessionConfigAt(int index, const QVariantMap &config);
    Q_INVOKABLE void addSessionWithConfig(const QVariantMap &config);
    Q_INVOKABLE void duplicateSessionAt(int index);
    Q_INVOKABLE void removeSessionAt(int index);
    Q_INVOKABLE QString showSessionContextMenu(int index, const QPointF &globalPosition);
    Q_INVOKABLE QString showSubscriptionContextMenu(const QString &topic, const QPointF &globalPosition);
    Q_INVOKABLE void connectCurrentSession();
    Q_INVOKABLE void disconnectCurrentSession();
    Q_INVOKABLE void setCurrentOutputPaused(bool paused);
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
    Q_INVOKABLE void publishCurrentSession(
        const QString &topic,
        const QString &payload,
        int format = 0,
        int qos = 0,
        bool retain = false);
    Q_INVOKABLE void copyTextToClipboard(const QString &text) const;
    Q_INVOKABLE void clearCurrentMessages();
    Q_INVOKABLE int loadOlderCurrentSessionMessages();

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
