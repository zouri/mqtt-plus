#pragma once

#include "services/processors/processorlibrary.h"
#include <QObject>
#include <QThread>
#include <QJsonObject>
#include <functional>

class EditorHttpWorker;
class EditorStorageWorker;

class BrowserEditorService : public QObject
{
    Q_OBJECT
public:
    explicit BrowserEditorService(ProcessorLibrary &snapshot, QObject *parent = nullptr);
    ~BrowserEditorService() override;
    void openEditor(const QString &processorId, const QString &language = QStringLiteral("lua"), bool launchBrowser = true);
    void setAppearance(const QString &language, const QString &theme, const QString &font);
    void deleteProcessor(const QString &processorId);
    void applyMutation(const SaveProcessorRevisionResult &saved, const QString &deletedId,
        std::function<void()> acknowledge);
    void requestDraftCounts();

signals:
    void libraryChanged();
    void errorOccurred(const QString &message);
    void openingLink(const QString &url);
    void draftCountsChanged(const QJsonObject &counts);
    void readinessChanged(const QString &processorId, const QString &revisionId, bool ready, const QString &detail);

private:
    ProcessorLibrary &m_snapshot;
    QThread m_httpThread;
    QThread m_storageThread;
    EditorStorageWorker *m_storage = nullptr;
    EditorHttpWorker *m_http = nullptr;
};
