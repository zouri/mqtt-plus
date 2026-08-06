#include "viewmodels/draftsviewmodel.h"

#include "models/draftlibrarymodel.h"
#include "services/payload/payloadcodec.h"
#include "usecases/draftlibraryservice.h"

DraftsViewModel::DraftsViewModel(
    DraftLibraryService &draftService,
    DraftLibraryModel &draftsModel,
    QObject *parent)
    : QObject(parent)
    , m_draftService(draftService)
    , m_filteredDrafts(this)
    , m_editor(this)
{
    m_filteredDrafts.setSourceModel(&draftsModel);
    m_filteredDrafts.setSortMode(QStringLiteral("name"));
    connect(&m_draftService, &DraftLibraryService::stateChanged,
            this, &DraftsViewModel::libraryStateChanged);
    connect(&m_draftService, &DraftLibraryService::draftsChanged,
            this, &DraftsViewModel::ensureEditorSelection);
    connect(&draftsModel, &QAbstractItemModel::modelReset,
            this, &DraftsViewModel::ensureEditorSelection);
    connect(&m_draftService, &DraftLibraryService::operationSucceeded,
            this, &DraftsViewModel::handleOperationSucceeded);
    connect(&m_draftService, &DraftLibraryService::storageError,
            this, [this]() {
                m_waitingForEditorSave = false;
                m_pendingDeleteId.clear();
            });
}

DraftFilterModel *DraftsViewModel::filteredDrafts() { return &m_filteredDrafts; }
DraftEditorViewModel *DraftsViewModel::editor() { return &m_editor; }
QStringList DraftsViewModel::payloadFormats() const { return PayloadCodec::formatNames(); }
bool DraftsViewModel::loading() const { return m_draftService.loading(); }
bool DraftsViewModel::busy() const { return m_draftService.busy(); }
bool DraftsViewModel::ready() const { return m_draftService.ready(); }
bool DraftsViewModel::readOnly() const { return m_draftService.readOnly(); }
bool DraftsViewModel::canRecover() const { return m_draftService.canRecover(); }
QString DraftsViewModel::storageError() const { return m_draftService.errorMessage(); }

void DraftsViewModel::setFilterText(const QString &text)
{
    m_filteredDrafts.setFilterText(text);
}

void DraftsViewModel::ensureEditorSelection()
{
    if (!m_draftService.ready() || m_editor.hasUnsavedChanges()) return;
    if (!m_editor.currentDraftId().isEmpty()
        && m_draftService.draftById(m_editor.currentDraftId())) {
        return;
    }
    if (m_filteredDrafts.rowCount() > 0) {
        m_editor.loadDraft(m_filteredDrafts.rowAt(0));
    } else {
        m_editor.newDraft();
    }
}

bool DraftsViewModel::selectFilteredDraftAt(int index)
{
    const QVariantMap row = m_filteredDrafts.rowAt(index);
    if (row.isEmpty()) return false;
    m_editor.loadDraft(row);
    return true;
}

bool DraftsViewModel::selectDraftById(const QString &id)
{
    const PublishDraft *draft = m_draftService.draftById(id);
    if (!draft) return false;
    m_editor.loadDraft(draftMap(*draft));
    return true;
}

void DraftsViewModel::newDraft()
{
    m_editor.newDraft();
}

void DraftsViewModel::discardEditorChanges()
{
    if (const PublishDraft *draft = m_draftService.draftById(m_editor.currentDraftId())) {
        m_editor.loadDraft(draftMap(*draft));
        return;
    }
    m_editor.newDraft();
}

bool DraftsViewModel::duplicateCurrentDraft()
{
    const QString id = m_editor.currentDraftId();
    const PublishDraft *draft = m_draftService.draftById(id);
    if (!draft) return false;
    m_editor.duplicateDraft(draftMap(*draft), m_draftService.suggestCopyName(draft->name));
    return true;
}

bool DraftsViewModel::saveEditor()
{
    PublishDraft draft = m_editor.draft();
    QString error;
    if (!m_draftService.validateDraft(draft, error, draft.id)) {
        m_editor.setValidationError(error);
        return false;
    }
    m_editor.setValidationError(QString());
    const bool accepted = draft.id.isEmpty()
        ? m_draftService.createDraft(draft)
        : m_draftService.updateDraft(draft);
    m_waitingForEditorSave = accepted;
    if (!accepted && !m_draftService.errorMessage().isEmpty()) {
        m_editor.setValidationError(m_draftService.errorMessage());
    }
    return accepted;
}

bool DraftsViewModel::deleteCurrentDraft()
{
    m_pendingDeleteId = m_editor.currentDraftId();
    if (m_pendingDeleteId.isEmpty()) return false;
    if (!m_draftService.removeDraft(m_pendingDeleteId)) {
        m_pendingDeleteId.clear();
        return false;
    }
    return true;
}

bool DraftsViewModel::recoverBackup()
{
    return m_draftService.recoverBackup();
}

QVariantMap DraftsViewModel::draftMap(const PublishDraft &draft)
{
    bool ok = false;
    const PayloadFormat format = PayloadCodec::formatFromId(draft.formatId, &ok);
    return {
        {QStringLiteral("id"), draft.id},
        {QStringLiteral("name"), draft.name},
        {QStringLiteral("description"), draft.description},
        {QStringLiteral("defaultTopic"), draft.defaultTopic},
        {QStringLiteral("payload"), draft.payload},
        {QStringLiteral("format"), static_cast<int>(ok ? format : PayloadFormat::Plaintext)},
        {QStringLiteral("qos"), draft.qos},
        {QStringLiteral("retain"), draft.retain},
    };
}

void DraftsViewModel::handleOperationSucceeded(const QString &operation, const QString &draftId)
{
    if ((operation == QStringLiteral("create") || operation == QStringLiteral("update"))
        && m_waitingForEditorSave) {
        m_waitingForEditorSave = false;
        if (const PublishDraft *draft = m_draftService.draftById(draftId)) {
            m_editor.loadDraft(draftMap(*draft));
        }
        emit editorSaveSucceeded();
        return;
    }
    if (operation == QStringLiteral("delete") && draftId == m_pendingDeleteId) {
        m_pendingDeleteId.clear();
        m_editor.newDraft();
        ensureEditorSelection();
        emit editorDeleteSucceeded();
    }
}
