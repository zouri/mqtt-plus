#pragma once

#include "domain/publishdraft.h"
#include "services/storage/draftstore.h"

#include <QFutureWatcher>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

class DraftLibraryService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(bool readOnly READ readOnly NOTIFY stateChanged)
    Q_PROPERTY(bool canRecover READ canRecover NOTIFY stateChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY stateChanged)

public:
    explicit DraftLibraryService(const QString &storageRoot = QString(), QObject *parent = nullptr);
    ~DraftLibraryService() override;

    const QVector<PublishDraft> &drafts() const;
    const PublishDraft *draftById(const QString &id) const;
    bool loading() const;
    bool busy() const;
    bool ready() const;
    bool readOnly() const;
    bool canRecover() const;
    QString errorMessage() const;
    QString storageDirectory() const;

    void load();
    bool validateDraft(const PublishDraft &draft, QString &errorMessage, const QString &excludeId = QString()) const;
    QString suggestCopyName(const QString &name) const;
    bool createDraft(PublishDraft draft);
    bool updateDraft(PublishDraft draft);
    bool removeDraft(const QString &id);
    bool importDrafts(QVector<PublishDraft> drafts);
    void markUsed(const QString &id);
    Q_INVOKABLE bool recoverBackup();

signals:
    void draftsChanged();
    void stateChanged();
    void operationSucceeded(const QString &operation, const QString &draftId);
    void storageError(const QString &message);

private:
    bool beginSave(const QVector<PublishDraft> &candidate, const QString &operation, const QString &draftId);
    void finishLoad();
    void finishSave();
    void flushQueuedUsage();
    bool nameExists(const QString &name, const QString &excludeId = QString()) const;

    QString m_storageRoot;
    QVector<PublishDraft> m_drafts;
    QVector<PublishDraft> m_pendingDrafts;
    QString m_pendingOperation;
    QString m_pendingDraftId;
    QSet<QString> m_queuedUsageIds;
    QFutureWatcher<DraftStore::LoadResult> m_loadWatcher;
    QFutureWatcher<DraftStore::SaveResult> m_saveWatcher;
    bool m_loading = false;
    bool m_busy = false;
    bool m_ready = false;
    bool m_readOnly = false;
    bool m_canRecover = false;
    QString m_errorMessage;
};
