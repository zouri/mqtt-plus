#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <functional>

#include "models/eventstreammodel.h"
#include "models/scriptlibrarymodel.h"
#include "models/sessionlistmodel.h"
#include "models/subscriptionfiltermodel.h"
#include "platform/platformactions.h"
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
    Q_PROPERTY(EventStreamModel* messages READ messages CONSTANT)
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

public:
    struct Dependencies {
        std::function<void(QObject *, std::function<void()>)> bindCurrentSessionIndexChanged;
        std::function<void(QObject *, std::function<void()>)> bindCurrentSessionChanged;
        std::function<void(QObject *, std::function<void()>)> bindSessionRuntimeStateChanged;
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

    void setCurrentSessionIndex(int index);

    Q_INVOKABLE void openSessionEditorForCreate();
    Q_INVOKABLE void openSessionEditorForEdit(int index);
    Q_INVOKABLE bool submitSessionEditor();
    Q_INVOKABLE void requestSessionDuplicate(int index);
    Q_INVOKABLE void requestSessionDelete(int index);
    Q_INVOKABLE void toggleCurrentSessionConnection();
    Q_INVOKABLE void toggleCurrentOutputPaused(bool currentlyPaused);
    Q_INVOKABLE void openSubscriptionEditorForCreate();
    Q_INVOKABLE bool openSubscriptionEditorForEdit(int filteredIndex);
    Q_INVOKABLE bool submitSubscriptionEditor();
    Q_INVOKABLE void toggleCurrentSubscriptionPaused(const QString &topic, bool currentlyPaused);
    Q_INVOKABLE void requestSubscriptionDelete(const QString &topic, const QString &displayName);
    Q_INVOKABLE void cancelPendingSubscriptionDelete();
    Q_INVOKABLE bool confirmPendingSubscriptionDelete();
    Q_INVOKABLE void copyMessageTopic(const QString &topic) const;
    Q_INVOKABLE void copyMessagePayload(const QString &payload, const QString &testPayload) const;
    Q_INVOKABLE void clearMessages();
    Q_INVOKABLE int loadOlderMessages();

signals:
    void currentSessionIndexChanged();
    void currentSessionChanged();
    void sessionStatusChanged();
    void publishStatusChanged();
    void messageStreamChanged();
    void messageStreamRowAppended();
    void pendingSubscriptionDeleteChanged();
    void sessionEditRequested(int index);
    void subscriptionDeleteRequested(const QString &topic, const QString &displayName);

private:
    ScriptLibraryModel *scriptLibrary() const;
    void refreshSubscriptionEditorScriptOptions();
    void clearPendingSubscriptionDelete();

    Dependencies m_dependencies;
    PlatformActions m_platformActions;
    PublishDraftViewModel m_publisher;
    QString m_pendingSubscriptionDeleteTopic;
    QString m_pendingSubscriptionDeleteDisplayName;
    SessionEditorViewModel m_sessionEditor;
    SubscriptionEditorViewModel m_subscriptionEditor;
};
