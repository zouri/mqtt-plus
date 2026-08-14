#pragma once

#include "domain/session.h"
#include "models/processorfiltermodel.h"
#include "viewmodels/processoreditorviewmodel.h"

#include <QObject>
#include <QStringList>

#include <memory>

class MessageProcessorEngine;
class ProcessorLibrary;
class ProcessorLibraryModel;

class ProcessorsViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ProcessorLibraryModel* processors READ processors CONSTANT)
    Q_PROPERTY(ProcessorFilterModel* filteredProcessors READ filteredProcessors CONSTANT)
    Q_PROPERTY(ProcessorEditorViewModel* editor READ editor CONSTANT)

public:
    explicit ProcessorsViewModel(
        ProcessorLibrary &library,
        ProcessorLibraryModel &processors,
        const QVector<SessionState> &sessions,
        QObject *parent = nullptr);
    ~ProcessorsViewModel() override;

    ProcessorLibraryModel *processors() const;
    ProcessorFilterModel *filteredProcessors();
    ProcessorEditorViewModel *editor();

    Q_INVOKABLE void ensureEditorSelection();
    Q_INVOKABLE bool selectFilteredProcessorAt(int index);
    Q_INVOKABLE void setProcessorFilterText(const QString &filterText);
    Q_INVOKABLE void newProcessor(const QString &languageId);
    Q_INVOKABLE bool validateEditor();
    Q_INVOKABLE bool saveEditor();
    Q_INVOKABLE bool deleteCurrent();

signals:
    void processorLibraryChanged();
    void editorSaveSucceeded();

private:
    void refreshLibrary();

    ProcessorLibrary &m_library;
    ProcessorLibraryModel &m_processors;
    const QVector<SessionState> &m_sessions;
    std::unique_ptr<MessageProcessorEngine> m_engine;
    ProcessorFilterModel m_filteredProcessors;
    ProcessorEditorViewModel m_editor;
};
