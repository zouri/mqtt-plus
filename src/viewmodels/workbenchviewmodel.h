#pragma once

#include <QObject>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "models/eventstreammodel.h"
#include "models/scriptlibrarymodel.h"
#include "models/sessionlistmodel.h"
#include "models/subscriptionfiltermodel.h"
#include "models/subscriptionlistmodel.h"
#include "viewmodels/sessioneditorviewmodel.h"
#include "viewmodels/subscriptioneditorviewmodel.h"

class ApplicationCore;

class WorkbenchViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(SessionListModel* sessions READ sessions CONSTANT)
    Q_PROPERTY(SubscriptionListModel* subscriptions READ subscriptions CONSTANT)
    Q_PROPERTY(SubscriptionFilterModel* filteredSubscriptions READ filteredSubscriptions CONSTANT)
    Q_PROPERTY(EventStreamModel* messages READ messages CONSTANT)
    Q_PROPERTY(ScriptLibraryModel* scripts READ scripts CONSTANT)
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

public:
    explicit WorkbenchViewModel(ApplicationCore *core = nullptr, QObject *parent = nullptr);

    SessionListModel *sessions() const;
    SubscriptionListModel *subscriptions() const;
    SubscriptionFilterModel *filteredSubscriptions() const;
    EventStreamModel *messages() const;
    ScriptLibraryModel *scripts() const;
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

    void setCurrentSessionIndex(int index);
    void setPublishTopic(const QString &topic);
    void setPublishPayload(const QString &payload);
    void setPublishFormat(int format);
    void setPublishQos(int qos);
    void setPublishRetain(bool retain);

    Q_INVOKABLE void openSessionEditorForCreate();
    Q_INVOKABLE void openSessionEditorForEdit(int index);
    Q_INVOKABLE bool submitSessionEditor();
    Q_INVOKABLE void duplicateSessionAt(int index);
    Q_INVOKABLE void removeSessionAt(int index);
    Q_INVOKABLE QString showSessionContextMenu(int index, const QPointF &globalPosition);
    Q_INVOKABLE QString showSubscriptionContextMenu(const QString &topic, const QPointF &globalPosition);
    Q_INVOKABLE void connectCurrentSession();
    Q_INVOKABLE void disconnectCurrentSession();
    Q_INVOKABLE void setCurrentOutputPaused(bool paused);
    Q_INVOKABLE void refreshSubscriptionEditorScriptOptions();
    Q_INVOKABLE bool submitSubscriptionEditor();
    Q_INVOKABLE void removeCurrentSubscription(const QString &topic);
    Q_INVOKABLE void setCurrentSubscriptionPaused(const QString &topic, bool paused);
    Q_INVOKABLE void setPublishDraft(const QString &topic, const QString &payload, int format);
    Q_INVOKABLE bool publishDraft();
    Q_INVOKABLE void copyTextToClipboard(const QString &text) const;
    Q_INVOKABLE void clearCurrentMessages();
    Q_INVOKABLE int loadOlderCurrentSessionMessages();

signals:
    void currentSessionIndexChanged();
    void currentSessionChanged();
    void subscriptionsChanged();
    void messageStreamChanged();
    void logStreamChanged();
    void messageStreamRowAppended(const QVariantMap &row);
    void logStreamRowAppended(const QVariantMap &row);
    void scriptLibraryChanged();
    void publishTopicChanged();
    void publishPayloadChanged();
    void publishFormatChanged();
    void publishQosChanged();
    void publishRetainChanged();
    void canPublishChanged();

private:
    ApplicationCore *m_core = nullptr;
    QString m_publishTopic;
    QString m_publishPayload;
    int m_publishFormat = 1;
    int m_publishQos = 0;
    bool m_publishRetain = false;
    SessionEditorViewModel m_sessionEditor;
    SubscriptionEditorViewModel m_subscriptionEditor;
};
