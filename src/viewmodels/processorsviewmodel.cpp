#include "processorsviewmodel.h"
#include "models/processorlibrarymodel.h"
#include "services/processors/editorcodec.h"
#include "usecases/browsereditorservice.h"

ProcessorsViewModel::ProcessorsViewModel(ProcessorLibrary &library, ProcessorLibraryModel &processors,
    const QVector<SessionState> &sessions, QObject *parent)
    : QObject(parent), m_library(library), m_processors(processors), m_sessions(sessions), m_filteredProcessors(this)
{
    m_processors.refreshSnapshots(m_library);
    m_filteredProcessors.setSourceModel(&m_processors);
}

QString ProcessorsViewModel::name() const
{
    const auto processor = m_library.processorById(m_currentId);
    return processor ? processor->name : QString();
}

QString ProcessorsViewModel::runtimeName() const
{
    const auto processor = m_library.processorById(m_currentId);
    const auto revision = processor ? m_library.revisionById(processor->currentRevisionId) : QSharedPointer<const ProcessorRevisionSnapshot>();
    if (!revision) return {};
    return revision->languageId == QStringLiteral("lua") ? QStringLiteral("Lua 5.5") : QStringLiteral("JavaScript (Qt QJSEngine)");
}

void ProcessorsViewModel::ensureEditorSelection()
{
    if (m_processors.indexOfId(m_currentId) >= 0) return;
    m_currentId = m_processors.rowAt(0).value(QStringLiteral("id")).toString();
    emit detailsChanged();
}

bool ProcessorsViewModel::selectFilteredProcessorAt(int index)
{
    const auto row = m_filteredProcessors.rowAt(index);
    if (row.isEmpty()) return false;
    m_currentId = row.value(QStringLiteral("id")).toString();
    m_error.clear();
    emit detailsChanged();
    return true;
}

void ProcessorsViewModel::setProcessorFilterText(const QString &text) { m_filteredProcessors.setFilterText(text); }
void ProcessorsViewModel::newProcessor(const QString &languageId)
{
    if (m_browser) m_browser->openEditor({}, languageId);
}
void ProcessorsViewModel::openBrowserEditor()
{
    if (m_browser && !m_currentId.isEmpty()) m_browser->openEditor(m_currentId);
}

bool ProcessorsViewModel::deleteCurrent()
{
    if (m_currentId.isEmpty()) return false;
    QStringList usage;
    for (const auto &session : m_sessions) {
        for (const auto &subscription : session.subscriptions) {
            if (subscription.processor.processorId == m_currentId) { usage.append(session.name); break; }
        }
    }
    if (!usage.isEmpty()) {
        m_error = tr("This processor is used by subscriptions in: %1. Remove those bindings before deleting it.").arg(usage.join(QStringLiteral(", ")));
        emit detailsChanged();
        return false;
    }
    if (!m_browser) return false;
    m_browser->deleteProcessor(m_currentId);
    return true;
}

void ProcessorsViewModel::setBrowserEditor(BrowserEditorService *service)
{
    m_browser = service;
    connect(service, &BrowserEditorService::libraryChanged, this, [this]() {
        m_processors.refreshSnapshots(m_library);
        ensureEditorSelection();
        emit detailsChanged();
        emit processorLibraryChanged();
        m_browser->requestDraftCounts();
    });
    connect(service, &BrowserEditorService::openingLink, this, [this](const QString &url) {
        m_editorLink = url;
        emit detailsChanged();
    });
    connect(service, &BrowserEditorService::errorOccurred, this, [this](const QString &message) {
        m_error = message;
        emit detailsChanged();
    });
    connect(service, &BrowserEditorService::draftCountsChanged, this, [this](const QJsonObject &counts) {
        m_draftCounts = counts;
        emit detailsChanged();
    });
    connect(service, &BrowserEditorService::readinessChanged, this,
        [this](const QString &id, const QString &revisionId, bool ready, const QString &detail) {
            const auto processor = m_library.processorById(id);
            if (!processor || processor->currentRevisionId != revisionId) return;
            m_processors.setReadiness(id, ready ? QStringLiteral("ready") : QStringLiteral("invalid_source"), detail);
            emit detailsChanged();
        });
    connect(&m_draftTimer, &QTimer::timeout, service, &BrowserEditorService::requestDraftCounts);
    m_draftTimer.start(5000);
}

QVariantMap ProcessorsViewModel::selectedDetails() const
{
    const auto processor = m_library.processorById(m_currentId);
    if (!processor) return {};
    const auto revision = m_library.revisionById(processor->currentRevisionId);
    auto data = EditorCodec::processor(*processor);
    if (revision) data.insert("revisionNumber", QString::number(revision->revisionNumber));
    QStringList usage;
    for (const auto &session : m_sessions) {
        for (const auto &subscription : session.subscriptions) {
            if (subscription.processor.processorId == processor->id) { usage.append(session.name); break; }
        }
    }
    data.insert("usage", usage.join(QStringLiteral(", ")));
    data.insert("draftCount", m_draftCounts.value(processor->id).toInt());
    data.insert("readiness", m_processors.rowAt(m_processors.indexOfId(processor->id)).value(QStringLiteral("readinessState")).toString());
    return data.toVariantMap();
}
