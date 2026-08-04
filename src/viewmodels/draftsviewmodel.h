#pragma once

#include "models/draftfiltermodel.h"
#include "viewmodels/drafteditorviewmodel.h"

#include <QObject>
#include <QStringList>
#include <QVariantMap>

class DraftLibraryModel;
class DraftLibraryService;
class MqttSessionService;
class SessionService;

class DraftsViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(DraftFilterModel* filteredDrafts READ filteredDrafts CONSTANT)
    Q_PROPERTY(DraftEditorViewModel* editor READ editor CONSTANT)
    Q_PROPERTY(QStringList payloadFormats READ payloadFormats CONSTANT)
    Q_PROPERTY(bool loading READ loading NOTIFY libraryStateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY libraryStateChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY libraryStateChanged)
    Q_PROPERTY(bool readOnly READ readOnly NOTIFY libraryStateChanged)
    Q_PROPERTY(bool canRecover READ canRecover NOTIFY libraryStateChanged)
    Q_PROPERTY(QString storageError READ storageError NOTIFY libraryStateChanged)
    Q_PROPERTY(QStringList sessionNames READ sessionNames NOTIFY currentSessionChanged)
    Q_PROPERTY(int currentSessionIndex READ currentSessionIndex WRITE setCurrentSessionIndex NOTIFY currentSessionChanged)
    Q_PROPERTY(QVariantMap currentSession READ currentSession NOTIFY currentSessionChanged)
    Q_PROPERTY(QVariantMap sessionStatus READ sessionStatus NOTIFY currentSessionChanged)

public:
    explicit DraftsViewModel(
        DraftLibraryService &draftService,
        DraftLibraryModel &draftsModel,
        SessionService &sessionService,
        MqttSessionService &mqttService,
        QObject *parent = nullptr);

    DraftFilterModel *filteredDrafts();
    DraftEditorViewModel *editor();
    QStringList payloadFormats() const;
    bool loading() const;
    bool busy() const;
    bool ready() const;
    bool readOnly() const;
    bool canRecover() const;
    QString storageError() const;
    QStringList sessionNames() const;
    int currentSessionIndex() const;
    QVariantMap currentSession() const;
    QVariantMap sessionStatus() const;
    void setCurrentSessionIndex(int index);

    Q_INVOKABLE void setFilterText(const QString &text);
    Q_INVOKABLE void ensureEditorSelection();
    Q_INVOKABLE bool selectFilteredDraftAt(int index);
    Q_INVOKABLE bool selectDraftById(const QString &id);
    Q_INVOKABLE void newDraft();
    Q_INVOKABLE void discardEditorChanges();
    Q_INVOKABLE bool duplicateCurrentDraft();
    Q_INVOKABLE bool saveEditor();
    Q_INVOKABLE bool deleteCurrentDraft();
    Q_INVOKABLE bool sendCurrent(const QString &temporaryTopic = QString());
    Q_INVOKABLE bool currentNeedsTopic() const;
    Q_INVOKABLE bool recoverBackup();

signals:
    void libraryStateChanged();
    void currentSessionChanged();
    void editorSaveSucceeded();
    void editorDeleteSucceeded();

private:
    static QVariantMap draftMap(const PublishDraft &draft);
    void handleOperationSucceeded(const QString &operation, const QString &draftId);

    DraftLibraryService &m_draftService;
    SessionService &m_sessionService;
    MqttSessionService &m_mqttService;
    DraftFilterModel m_filteredDrafts;
    DraftEditorViewModel m_editor;
    bool m_waitingForEditorSave = false;
    QString m_pendingDeleteId;
};
