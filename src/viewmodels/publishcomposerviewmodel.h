#pragma once

#include "models/draftfiltermodel.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

class DraftLibraryModel;
class DraftLibraryService;
class MqttSessionService;
class SessionService;

class PublishComposerViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList payloadFormats READ payloadFormats CONSTANT)
    Q_PROPERTY(QString topic READ topic WRITE setTopic NOTIFY topicChanged)
    Q_PROPERTY(QString payload READ payload WRITE setPayload NOTIFY payloadChanged)
    Q_PROPERTY(int format READ format WRITE setFormat NOTIFY formatChanged)
    Q_PROPERTY(int qos READ qos WRITE setQos NOTIFY qosChanged)
    Q_PROPERTY(bool retain READ retain WRITE setRetain NOTIFY retainChanged)
    Q_PROPERTY(bool canPublish READ canPublish NOTIFY canPublishChanged)
    Q_PROPERTY(bool hasContent READ hasContent NOTIFY composerStateChanged)
    Q_PROPERTY(QVariantList recentPublishes READ recentPublishes NOTIFY recentPublishesChanged)
    Q_PROPERTY(DraftFilterModel* drafts READ drafts CONSTANT)
    Q_PROPERTY(bool draftsLoading READ draftsLoading NOTIFY draftLibraryStateChanged)
    Q_PROPERTY(bool draftsReady READ draftsReady NOTIFY draftLibraryStateChanged)
    Q_PROPERTY(bool draftsBusy READ draftsBusy NOTIFY draftLibraryStateChanged)
    Q_PROPERTY(QString draftError READ draftError NOTIFY draftLibraryStateChanged)

public:
    explicit PublishComposerViewModel(
        SessionService &sessionService,
        MqttSessionService &mqttService,
        DraftLibraryService &draftService,
        DraftLibraryModel &draftsModel,
        QObject *parent = nullptr);

    QStringList payloadFormats() const;
    QString topic() const;
    QString payload() const;
    int format() const;
    int qos() const;
    bool retain() const;
    bool canPublish() const;
    bool hasContent() const;
    QVariantList recentPublishes() const;
    DraftFilterModel *drafts();
    bool draftsLoading() const;
    bool draftsReady() const;
    bool draftsBusy() const;
    QString draftError() const;

    void setTopic(const QString &topic);
    void setPayload(const QString &payload);
    void setFormat(int format);
    void setQos(int qos);
    void setRetain(bool retain);

    Q_INVOKABLE void useMessageAsDraft(const QString &topic, const QString &payload, const QString &testPayload, int format);
    Q_INVOKABLE bool useRecentPublish(int index);
    Q_INVOKABLE bool quickPublishRecent(int index);
    Q_INVOKABLE void clearRecentPublishes();
    Q_INVOKABLE bool publishDraft();
    Q_INVOKABLE void setDraftFilterText(const QString &text);
    Q_INVOKABLE int draftIndexOfId(const QString &id) const;
    Q_INVOKABLE bool useSavedDraft(int index);
    Q_INVOKABLE bool quickPublishDraft(int index, const QString &temporaryTopic = QString());
    Q_INVOKABLE bool draftNeedsTopic(int index) const;
    Q_INVOKABLE bool wouldReplaceWithDraft(int index) const;
    Q_INVOKABLE bool saveAsDraft(const QString &name);

signals:
    void topicChanged();
    void payloadChanged();
    void formatChanged();
    void qosChanged();
    void retainChanged();
    void canPublishChanged();
    void composerStateChanged();
    void recentPublishesChanged();
    void draftLibraryStateChanged();

private:
    SessionService &m_sessionService;
    MqttSessionService &m_mqttService;
    DraftLibraryService &m_draftService;
    DraftFilterModel m_drafts;
    QString m_topic;
    QString m_payload;
    int m_format = 1;
    int m_qos = 0;
    bool m_retain = false;
};
