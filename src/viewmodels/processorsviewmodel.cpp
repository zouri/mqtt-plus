#include "processorsviewmodel.h"

#include "models/processorlibrarymodel.h"
#include "services/processors/defaultmessageprocessorengine.h"
#include "services/processors/messageprocessorengine.h"
#include "services/processors/processorlibrary.h"

#include <algorithm>

ProcessorsViewModel::ProcessorsViewModel(
    ProcessorLibrary &library,
    ProcessorLibraryModel &processors,
    const QVector<SessionState> &sessions,
    QObject *parent)
    : QObject(parent)
    , m_library(library)
    , m_processors(processors)
    , m_sessions(sessions)
    , m_engine(createDefaultMessageProcessorEngine())
    , m_filteredProcessors(this)
    , m_editor(library, *m_engine, this)
{
    refreshLibrary();
    m_filteredProcessors.setSourceModel(&m_processors);
}

ProcessorsViewModel::~ProcessorsViewModel() = default;

ProcessorLibraryModel *ProcessorsViewModel::processors() const { return &m_processors; }
ProcessorFilterModel *ProcessorsViewModel::filteredProcessors() { return &m_filteredProcessors; }
ProcessorEditorViewModel *ProcessorsViewModel::editor() { return &m_editor; }

void ProcessorsViewModel::ensureEditorSelection()
{
    const QString currentId = m_editor.currentProcessorId();
    if (!currentId.isEmpty() && m_processors.indexOfId(currentId) >= 0) {
        return;
    }
    if (m_editor.hasUnsavedChanges()) {
        return;
    }
    if (m_processors.rowCount() > 0) {
        m_editor.loadProcessor(m_processors.rowAt(0).value(QStringLiteral("id")).toString());
    } else if (m_editor.name().isEmpty()) {
        m_editor.newProcessor(QStringLiteral("lua"));
    }
}

bool ProcessorsViewModel::selectFilteredProcessorAt(int index)
{
    const QVariantMap row = m_filteredProcessors.rowAt(index);
    return !row.isEmpty()
        && m_editor.loadProcessor(row.value(QStringLiteral("id")).toString());
}

void ProcessorsViewModel::setProcessorFilterText(const QString &filterText)
{
    m_filteredProcessors.setFilterText(filterText);
}

void ProcessorsViewModel::newProcessor(const QString &languageId)
{
    m_editor.newProcessor(languageId);
}

bool ProcessorsViewModel::validateEditor()
{
    return m_editor.validateDraft();
}

bool ProcessorsViewModel::saveEditor()
{
    if (!m_editor.canSave() || !m_editor.validateDraft()) {
        return false;
    }
    const SaveProcessorRevisionResult result = m_library.saveRevision(
        m_editor.saveCommand());
    if (!result.ok) {
        m_editor.setOperationError(result.error);
        return false;
    }
    refreshLibrary();
    m_editor.loadProcessor(result.processor.id);
    emit processorLibraryChanged();
    emit editorSaveSucceeded();
    return true;
}

bool ProcessorsViewModel::deleteCurrent()
{
    const QString processorId = m_editor.currentProcessorId();
    if (processorId.isEmpty()) {
        return false;
    }
    QStringList usage;
    for (const SessionState &session : m_sessions) {
        const bool used = std::any_of(
            session.subscriptions.cbegin(),
            session.subscriptions.cend(),
            [&processorId](const SubscriptionEntry &subscription) {
                return subscription.processor.processorId == processorId;
            });
        if (used) {
            usage.append(session.name);
        }
    }
    if (!usage.isEmpty()) {
        m_editor.setOperationError(tr(
            "This processor is used by subscriptions in: %1. Remove those bindings before deleting it.")
                                       .arg(usage.join(QStringLiteral(", "))));
        return false;
    }

    const QString languageId = m_editor.languageId();
    if (!m_library.deleteProcessor(processorId)) {
        m_editor.setOperationError(m_library.lastError());
        return false;
    }

    refreshLibrary();
    if (m_filteredProcessors.rowCount() > 0) {
        m_editor.loadProcessor(
            m_filteredProcessors.rowAt(0).value(QStringLiteral("id")).toString());
    } else {
        m_editor.newProcessor(languageId);
    }
    emit processorLibraryChanged();
    return true;
}

void ProcessorsViewModel::refreshLibrary()
{
    m_processors.refresh(m_library, *m_engine);
}
