#pragma once

#include "domain/session.h"
#include "models/processorfiltermodel.h"
#include <QJsonObject>
#include <QObject>
#include <QTimer>

class ProcessorLibrary;
class ProcessorLibraryModel;
class BrowserEditorService;

class ProcessorsViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ProcessorLibraryModel* processors READ processors CONSTANT)
    Q_PROPERTY(ProcessorFilterModel* filteredProcessors READ filteredProcessors CONSTANT)
    Q_PROPERTY(QString currentProcessorId READ currentProcessorId NOTIFY detailsChanged)
    Q_PROPERTY(QString name READ name NOTIFY detailsChanged)
    Q_PROPERTY(QString runtimeName READ runtimeName NOTIFY detailsChanged)
    Q_PROPERTY(QString diagnostics READ diagnostics NOTIFY detailsChanged)
    Q_PROPERTY(QString editorLink READ editorLink NOTIFY detailsChanged)
    Q_PROPERTY(QVariantMap selectedDetails READ selectedDetails NOTIFY detailsChanged)
public:
    explicit ProcessorsViewModel(ProcessorLibrary &library, ProcessorLibraryModel &processors,
        const QVector<SessionState> &sessions, QObject *parent = nullptr);
    ProcessorLibraryModel *processors() const { return &m_processors; }
    ProcessorFilterModel *filteredProcessors() { return &m_filteredProcessors; }
    QString currentProcessorId() const { return m_currentId; }
    QString name() const;
    QString runtimeName() const;
    QString diagnostics() const { return m_error; }
    QString editorLink() const { return m_editorLink; }
    QVariantMap selectedDetails() const;
    void setBrowserEditor(BrowserEditorService *service);
    Q_INVOKABLE void ensureEditorSelection();
    Q_INVOKABLE bool selectFilteredProcessorAt(int index);
    Q_INVOKABLE void setProcessorFilterText(const QString &text);
    Q_INVOKABLE void newProcessor(const QString &languageId);
    Q_INVOKABLE void openBrowserEditor();
    Q_INVOKABLE bool deleteCurrent();
signals:
    void processorLibraryChanged();
    void detailsChanged();
private:
    ProcessorLibrary &m_library;
    ProcessorLibraryModel &m_processors;
    const QVector<SessionState> &m_sessions;
    ProcessorFilterModel m_filteredProcessors;
    BrowserEditorService *m_browser = nullptr;
    QString m_currentId;
    QString m_error;
    QString m_editorLink;
    QJsonObject m_draftCounts;
    QTimer m_draftTimer;
};
