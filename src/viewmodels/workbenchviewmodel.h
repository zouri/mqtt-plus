#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "models/eventstreammodel.h"
#include "models/messagefiltermodel.h"
#include "models/scriptlibrarymodel.h"
#include "models/sessionlistmodel.h"
#include "models/subscriptionfiltermodel.h"
#include "viewmodels/publishdraftviewmodel.h"
#include "viewmodels/sessioneditorviewmodel.h"
#include "viewmodels/subscriptioneditorviewmodel.h"

class EventHistoryService;
class MqttSessionService;
class SessionService;
class SubscriptionService;

class WorkbenchViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(SessionListModel* sessions READ sessions CONSTANT)
    Q_PROPERTY(SubscriptionFilterModel* filteredSubscriptions READ filteredSubscriptions CONSTANT)
    Q_PROPERTY(SubscriptionFilterModel* messageFilterSubscriptions READ messageFilterSubscriptions CONSTANT)
    Q_PROPERTY(EventStreamModel* messages READ messages CONSTANT)
    Q_PROPERTY(MessageFilterModel* filteredMessages READ filteredMessages CONSTANT)
    Q_PROPERTY(PublishDraftViewModel* publisher READ publisher CONSTANT)
    Q_PROPERTY(SessionEditorViewModel* sessionEditor READ sessionEditor CONSTANT)
    Q_PROPERTY(SubscriptionEditorViewModel* subscriptionEditor READ subscriptionEditor CONSTANT)
    Q_PROPERTY(int currentSessionIndex READ currentSessionIndex WRITE setCurrentSessionIndex NOTIFY currentSessionIndexChanged)
    Q_PROPERTY(QVariantMap currentSession READ currentSession NOTIFY currentSessionChanged)
    Q_PROPERTY(QVariantMap sessionStatus READ sessionStatus NOTIFY sessionStatusChanged)
    Q_PROPERTY(QVariantMap publishStatus READ publishStatus NOTIFY publishStatusChanged)
    Q_PROPERTY(QStringList payloadFormats READ payloadFormats CONSTANT)
    Q_PROPERTY(QString pendingSubscriptionDeleteTopic READ pendingSubscriptionDeleteTopic NOTIFY pendingSubscriptionDeleteChanged)
    Q_PROPERTY(QString pendingSubscriptionDeleteDisplayName READ pendingSubscriptionDeleteDisplayName NOTIFY pendingSubscriptionDeleteChanged)
    Q_PROPERTY(bool allSubscriptionsPaused READ allSubscriptionsPaused NOTIFY subscriptionsStateChanged)
    Q_PROPERTY(QVariantMap messageTopicFilterState READ messageTopicFilterState NOTIFY messageTopicFilterStateChanged)
    Q_PROPERTY(qint64 totalMessageCount READ totalMessageCount NOTIFY totalMessageCountChanged)

public:
    explicit WorkbenchViewModel(
        SessionService &sessionService,
        MqttSessionService &mqttService,
        SubscriptionService &subscriptionService,
        EventHistoryService &eventHistoryService,
        SessionListModel &sessionsModel,
        SubscriptionFilterModel &filteredSubscriptionsModel,
        SubscriptionFilterModel &messageFilterSubscriptionsModel,
        EventStreamModel &messagesModel,
        MessageFilterModel &filteredMessagesModel,
        ScriptLibraryModel &scriptsModel,
        QObject *parent = nullptr);

    SessionListModel *sessions() const;
    SubscriptionFilterModel *filteredSubscriptions() const;
    SubscriptionFilterModel *messageFilterSubscriptions() const;
    EventStreamModel *messages() const;
    MessageFilterModel *filteredMessages() const;
    PublishDraftViewModel *publisher();
    SessionEditorViewModel *sessionEditor();
    SubscriptionEditorViewModel *subscriptionEditor();
    int currentSessionIndex() const;
    QVariantMap currentSession() const;
    QVariantMap sessionStatus() const;
    QVariantMap publishStatus() const;
    QStringList payloadFormats() const;
    QString pendingSubscriptionDeleteTopic() const;
    QString pendingSubscriptionDeleteDisplayName() const;
    bool allSubscriptionsPaused() const;
    QVariantMap messageTopicFilterState() const;
    qint64 totalMessageCount() const;

    void setCurrentSessionIndex(int index);

    Q_INVOKABLE void openSessionEditorForCreate();
    Q_INVOKABLE void openSessionEditorForEdit(int index);
    Q_INVOKABLE bool submitSessionEditor();
    Q_INVOKABLE void requestSessionDuplicate(int index);
    Q_INVOKABLE void requestSessionDelete(int index);
    Q_INVOKABLE void toggleCurrentSessionConnection();
    Q_INVOKABLE void openSubscriptionEditorForCreate();
    Q_INVOKABLE bool openSubscriptionEditorForEdit(int filteredIndex);
    Q_INVOKABLE bool submitSubscriptionEditor();
    Q_INVOKABLE void requestSubscriptionDelete(const QString &topic, const QString &displayName);
    Q_INVOKABLE void cancelPendingSubscriptionDelete();
    Q_INVOKABLE bool confirmPendingSubscriptionDelete();
    Q_INVOKABLE void copyMessageTopic(const QString &topic) const;
    Q_INVOKABLE QString messagePayloadForDisplay(
        const QString &historyId,
        const QString &fallbackPayload,
        int format) const;
    Q_INVOKABLE void copyMessagePayload(
        const QString &historyId,
        const QString &payload,
        const QString &testPayload,
        int format) const;
    Q_INVOKABLE void useMessageAsDraft(
        const QString &historyId,
        const QString &topic,
        const QString &payload,
        const QString &testPayload,
        int format);
    Q_INVOKABLE void setMessageTopicFilter(const QString &topic);
    Q_INVOKABLE void setMessageSearchText(const QString &text);
    Q_INVOKABLE void addMessageTopicFilter(const QString &topic);
    Q_INVOKABLE void clearMessageFilters();
    Q_INVOKABLE QVariantMap messageDetails(const QString &historyId) const;
    Q_INVOKABLE qreal currentIncomingMessageRate() const;
    Q_INVOKABLE qreal currentOutgoingMessageRate() const;
    Q_INVOKABLE qint64 currentIncomingByteRate() const;
    Q_INVOKABLE qint64 currentOutgoingByteRate() const;

signals:
    void currentSessionIndexChanged();
    void currentSessionChanged();
    void sessionStatusChanged();
    void publishStatusChanged();
    void messageStreamChanged();
    void totalMessageCountChanged();
    void messageStreamRowsAppended(int count);
    void pendingSubscriptionDeleteChanged();
    void subscriptionDeleteRequested(const QString &topic, const QString &displayName);
    void subscriptionsStateChanged();
    void messageTopicFilterStateChanged();

private:
    QString reusableMessagePayload(
        const QString &historyId,
        const QString &payload,
        const QString &testPayload,
        int format) const;
    void refreshSubscriptionEditorScriptOptions();
    void clearPendingSubscriptionDelete();

    SessionService &m_sessionService;
    MqttSessionService &m_mqttService;
    SubscriptionService &m_subscriptionService;
    EventHistoryService &m_eventHistoryService;
    SessionListModel &m_sessionsModel;
    SubscriptionFilterModel &m_filteredSubscriptionsModel;
    SubscriptionFilterModel &m_messageFilterSubscriptionsModel;
    EventStreamModel &m_messagesModel;
    MessageFilterModel &m_filteredMessagesModel;
    ScriptLibraryModel &m_scriptsModel;
    PublishDraftViewModel m_publisher;
    QString m_pendingSubscriptionDeleteTopic;
    QString m_pendingSubscriptionDeleteDisplayName;
    SessionEditorViewModel m_sessionEditor;
    SubscriptionEditorViewModel m_subscriptionEditor;
};
