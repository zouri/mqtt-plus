#pragma once

#include <QObject>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <functional>

#include "models/eventstreammodel.h"
#include "models/scriptlibrarymodel.h"
#include "models/sessionlistmodel.h"
#include "models/subscriptionfiltermodel.h"
#include "platform/platformactions.h"
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
    Q_PROPERTY(EventStreamModel* messages READ messages CONSTANT)
    Q_PROPERTY(SessionEditorViewModel* sessionEditor READ sessionEditor CONSTANT)
    Q_PROPERTY(SubscriptionEditorViewModel* subscriptionEditor READ subscriptionEditor CONSTANT)
    Q_PROPERTY(int currentSessionIndex READ currentSessionIndex WRITE setCurrentSessionIndex NOTIFY currentSessionIndexChanged)
    Q_PROPERTY(QVariantMap currentSession READ currentSession NOTIFY currentSessionChanged)
    Q_PROPERTY(QVariantMap sessionStatus READ sessionStatus NOTIFY currentSessionChanged)
    Q_PROPERTY(QVariantMap publishStatus READ publishStatus NOTIFY currentSessionChanged)
    Q_PROPERTY(QStringList payloadFormats READ payloadFormats CONSTANT)
    Q_PROPERTY(QString publishTopic READ publishTopic WRITE setPublishTopic NOTIFY publishTopicChanged)
    Q_PROPERTY(QString publishPayload READ publishPayload WRITE setPublishPayload NOTIFY publishPayloadChanged)
    Q_PROPERTY(int publishFormat READ publishFormat WRITE setPublishFormat NOTIFY publishFormatChanged)
    Q_PROPERTY(int publishQos READ publishQos WRITE setPublishQos NOTIFY publishQosChanged)
    Q_PROPERTY(bool publishRetain READ publishRetain WRITE setPublishRetain NOTIFY publishRetainChanged)
    Q_PROPERTY(bool canPublish READ canPublish NOTIFY canPublishChanged)
    Q_PROPERTY(QString subscriptionFilterText READ subscriptionFilterText WRITE setSubscriptionFilterText NOTIFY subscriptionFilterTextChanged)
    Q_PROPERTY(QString subscriptionFilterMode READ subscriptionFilterMode WRITE setSubscriptionFilterMode NOTIFY subscriptionFilterModeChanged)
    Q_PROPERTY(int subscriptionFilterModeIndex READ subscriptionFilterModeIndex WRITE setSubscriptionFilterModeIndex NOTIFY subscriptionFilterModeIndexChanged)
    Q_PROPERTY(bool hasSubscriptionFilter READ hasSubscriptionFilter NOTIFY subscriptionFilterChanged)
    Q_PROPERTY(QString pendingSubscriptionDeleteTopic READ pendingSubscriptionDeleteTopic NOTIFY pendingSubscriptionDeleteChanged)
    Q_PROPERTY(QString pendingSubscriptionDeleteDisplayName READ pendingSubscriptionDeleteDisplayName NOTIFY pendingSubscriptionDeleteChanged)

public:
    struct Dependencies {
        std::function<void(QObject *, std::function<void()>)> bindCurrentSessionIndexChanged;
        std::function<void(QObject *, std::function<void()>)> bindCurrentSessionChanged;
        std::function<void(QObject *, std::function<void()>)> bindMessageStreamChanged;
        std::function<void(QObject *, std::function<void(const QVariantMap &)>)> bindMessageStreamRowAppended;
        std::function<void(QObject *, std::function<void()>)> bindScriptLibraryChanged;
        SessionService *sessionController = nullptr;
        MqttSessionService *mqttController = nullptr;
        SubscriptionService *subscriptionController = nullptr;
        EventHistoryService *eventController = nullptr;
        SessionListModel *sessions = nullptr;
        SubscriptionFilterModel *filteredSubscriptions = nullptr;
        EventStreamModel *messages = nullptr;
        ScriptLibraryModel *scripts = nullptr;
    };

    explicit WorkbenchViewModel(QObject *parent = nullptr);
    explicit WorkbenchViewModel(const Dependencies &dependencies, QObject *parent = nullptr);

    SessionListModel *sessions() const;
    SubscriptionFilterModel *filteredSubscriptions() const;
    EventStreamModel *messages() const;
    SessionEditorViewModel *sessionEditor();
    SubscriptionEditorViewModel *subscriptionEditor();
    int currentSessionIndex() const;
    QVariantMap currentSession() const;
    QVariantMap sessionStatus() const;
    QVariantMap publishStatus() const;
    QStringList payloadFormats() const;
    QString publishTopic() const;
    QString publishPayload() const;
    int publishFormat() const;
    int publishQos() const;
    bool publishRetain() const;
    bool canPublish() const;
    QString subscriptionFilterText() const;
    QString subscriptionFilterMode() const;
    int subscriptionFilterModeIndex() const;
    bool hasSubscriptionFilter() const;
    QString pendingSubscriptionDeleteTopic() const;
    QString pendingSubscriptionDeleteDisplayName() const;

    void setCurrentSessionIndex(int index);
    void setPublishTopic(const QString &topic);
    void setPublishPayload(const QString &payload);
    void setPublishFormat(int format);
    void setPublishQos(int qos);
    void setPublishRetain(bool retain);
    void setSubscriptionFilterText(const QString &filterText);
    void setSubscriptionFilterMode(const QString &filterMode);
    void setSubscriptionFilterModeIndex(int index);

    Q_INVOKABLE void openSessionEditorForCreate();
    Q_INVOKABLE void openSessionEditorForEdit(int index);
    Q_INVOKABLE bool submitSessionEditor();
    Q_INVOKABLE void handleSessionContextMenu(int index, const QPointF &globalPosition);
    Q_INVOKABLE void handleSubscriptionContextMenu(int filteredIndex, const QString &topic, const QPointF &globalPosition);
    Q_INVOKABLE void toggleCurrentSessionConnection();
    Q_INVOKABLE void toggleCurrentOutputPaused(bool currentlyPaused);
    Q_INVOKABLE void openSubscriptionEditorForCreate();
    Q_INVOKABLE bool openSubscriptionEditorForEdit(int filteredIndex);
    Q_INVOKABLE bool submitSubscriptionEditor();
    Q_INVOKABLE void toggleCurrentSubscriptionPaused(const QString &topic, bool currentlyPaused);
    Q_INVOKABLE void requestSubscriptionDelete(const QString &topic, const QString &displayName);
    Q_INVOKABLE void cancelPendingSubscriptionDelete();
    Q_INVOKABLE bool confirmPendingSubscriptionDelete();
    Q_INVOKABLE void useMessageAsPublishDraft(const QString &topic, const QString &payload, const QString &testPayload, int format);
    Q_INVOKABLE bool publishDraft();
    Q_INVOKABLE void copyMessageTopic(const QString &topic) const;
    Q_INVOKABLE void copyMessagePayload(const QString &payload, const QString &testPayload) const;
    Q_INVOKABLE void clearMessages();
    Q_INVOKABLE int loadOlderMessages();

signals:
    void currentSessionIndexChanged();
    void currentSessionChanged();
    void messageStreamChanged();
    void messageStreamRowAppended();
    void publishTopicChanged();
    void publishPayloadChanged();
    void publishFormatChanged();
    void publishQosChanged();
    void publishRetainChanged();
    void canPublishChanged();
    void subscriptionFilterTextChanged();
    void subscriptionFilterModeChanged();
    void subscriptionFilterModeIndexChanged();
    void subscriptionFilterChanged();
    void pendingSubscriptionDeleteChanged();
    void sessionEditRequested(int index);
    void subscriptionEditRequested(int index);
    void subscriptionDeleteRequested(const QString &topic, const QString &displayName);

private:
    static QString normalizedSubscriptionFilterMode(const QString &filterMode);
    static int subscriptionFilterModeIndexForMode(const QString &filterMode);
    ScriptLibraryModel *scriptLibrary() const;
    void refreshSubscriptionEditorScriptOptions();
    void syncSubscriptionFilterModel();
    void emitSubscriptionFilterSignals(const QString &oldText, const QString &oldMode);
    void clearPendingSubscriptionDelete();

    Dependencies m_dependencies;
    PlatformActions m_platformActions;
    QString m_publishTopic;
    QString m_publishPayload;
    int m_publishFormat = 1;
    int m_publishQos = 0;
    bool m_publishRetain = false;
    QString m_subscriptionFilterText;
    QString m_subscriptionFilterMode = QStringLiteral("all");
    QString m_pendingSubscriptionDeleteTopic;
    QString m_pendingSubscriptionDeleteDisplayName;
    SessionEditorViewModel m_sessionEditor;
    SubscriptionEditorViewModel m_subscriptionEditor;
};
