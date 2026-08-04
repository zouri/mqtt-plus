#include "usecases/draftlibraryservice.h"

#include "services/apputils.h"
#include "services/payload/payloadcodec.h"

#include <QMqttTopicName>
#include <QtConcurrentRun>
#include <QUuid>

#include <algorithm>
#include <utility>

namespace {
constexpr qsizetype kMaxNameLength = 80;
constexpr qsizetype kMaxDescriptionLength = 500;
constexpr qsizetype kMaxPayloadBytes = 16 * 1024 * 1024;

bool storedPayloadExceedsLimit(const QString &payload)
{
    return payload.size() > kMaxPayloadBytes
        || payload.toUtf8().size() > kMaxPayloadBytes;
}
}

DraftLibraryService::DraftLibraryService(const QString &storageRoot, QObject *parent)
    : QObject(parent)
    , m_storageRoot(storageRoot)
{
    connect(&m_loadWatcher, &QFutureWatcher<DraftStore::LoadResult>::finished,
            this, &DraftLibraryService::finishLoad);
    connect(&m_saveWatcher, &QFutureWatcher<DraftStore::SaveResult>::finished,
            this, &DraftLibraryService::finishSave);
}

DraftLibraryService::~DraftLibraryService()
{
    m_loadWatcher.waitForFinished();
    m_saveWatcher.waitForFinished();
}

const QVector<PublishDraft> &DraftLibraryService::drafts() const { return m_drafts; }

const PublishDraft *DraftLibraryService::draftById(const QString &id) const
{
    for (const PublishDraft &draft : m_drafts) {
        if (draft.id == id) {
            return &draft;
        }
    }
    return nullptr;
}

bool DraftLibraryService::loading() const { return m_loading; }
bool DraftLibraryService::busy() const { return m_busy; }
bool DraftLibraryService::ready() const { return m_ready; }
bool DraftLibraryService::readOnly() const { return m_readOnly; }
bool DraftLibraryService::canRecover() const { return m_canRecover; }
QString DraftLibraryService::errorMessage() const { return m_errorMessage; }

void DraftLibraryService::load()
{
    if (m_loading || m_busy) {
        return;
    }
    m_loading = true;
    m_ready = false;
    m_errorMessage.clear();
    emit stateChanged();
    const QString root = m_storageRoot;
    m_loadWatcher.setFuture(QtConcurrent::run([root]() { return DraftStore::loadDrafts(root); }));
}

bool DraftLibraryService::validateDraft(
    const PublishDraft &draft,
    QString &errorMessage,
    const QString &excludeId) const
{
    errorMessage.clear();
    const QString name = draft.name.trimmed();
    if (name.isEmpty()) {
        errorMessage = tr("Draft name is required.");
        return false;
    }
    if (name.size() > kMaxNameLength) {
        errorMessage = tr("Draft name must be 80 characters or fewer.");
        return false;
    }
    if (draft.description.size() > kMaxDescriptionLength) {
        errorMessage = tr("Draft description must be 500 characters or fewer.");
        return false;
    }
    if (nameExists(name, excludeId)) {
        errorMessage = tr("A draft with this name already exists.");
        return false;
    }
    const QString topic = draft.defaultTopic.trimmed();
    if (!topic.isEmpty() && !QMqttTopicName(topic).isValid()) {
        errorMessage = tr("Default topic is not a valid MQTT topic name.");
        return false;
    }
    bool formatOk = false;
    const PayloadFormat format = PayloadCodec::formatFromId(draft.formatId, &formatOk);
    if (!formatOk) {
        errorMessage = tr("Unsupported payload format.");
        return false;
    }
    if (storedPayloadExceedsLimit(draft.payload)) {
        errorMessage = tr("Encoded payload exceeds the 16 MiB draft limit.");
        return false;
    }
    QByteArray payloadBytes;
    QString payloadError;
    if (!PayloadCodec::encodeForPublish(format, draft.payload, payloadBytes, payloadError)) {
        errorMessage = payloadError;
        return false;
    }
    if (payloadBytes.size() > kMaxPayloadBytes) {
        errorMessage = tr("Encoded payload exceeds the 16 MiB draft limit.");
        return false;
    }
    if (draft.qos < 0 || draft.qos > 1) {
        errorMessage = tr("QoS must be 0 or 1.");
        return false;
    }
    return true;
}

QString DraftLibraryService::suggestCopyName(const QString &name) const
{
    const QString base = name.trimmed().isEmpty() ? tr("Draft") : name.trimmed();
    const auto boundedCandidate = [&base](const QString &pattern, int suffix = 0) {
        QString fittedBase = base;
        QString candidate = suffix > 0
            ? pattern.arg(fittedBase).arg(suffix)
            : pattern.arg(fittedBase);
        while (candidate.size() > kMaxNameLength && !fittedBase.isEmpty()) {
            fittedBase.chop(1);
            candidate = suffix > 0
                ? pattern.arg(fittedBase).arg(suffix)
                : pattern.arg(fittedBase);
        }
        return candidate.left(kMaxNameLength);
    };

    QString candidate = boundedCandidate(tr("%1 Copy"));
    int suffix = 2;
    while (nameExists(candidate)) {
        candidate = boundedCandidate(tr("%1 Copy %2"), suffix++);
    }
    return candidate;
}

bool DraftLibraryService::createDraft(PublishDraft draft)
{
    if (!m_ready || m_readOnly || m_busy) {
        return false;
    }
    draft.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    draft.name = draft.name.trimmed();
    draft.description = draft.description.trimmed();
    draft.defaultTopic = draft.defaultTopic.trimmed();
    const QString now = AppUtils::timestampNow();
    draft.createdAt = now;
    draft.updatedAt = now;
    draft.lastUsedAt.clear();
    QString error;
    if (!validateDraft(draft, error)) {
        m_errorMessage = error;
        emit stateChanged();
        return false;
    }
    QVector<PublishDraft> candidate = m_drafts;
    candidate.append(draft);
    return beginSave(candidate, QStringLiteral("create"), draft.id);
}

bool DraftLibraryService::updateDraft(PublishDraft draft)
{
    if (!m_ready || m_readOnly || m_busy || draft.id.trimmed().isEmpty()) {
        return false;
    }
    QVector<PublishDraft> candidate = m_drafts;
    auto it = std::find_if(candidate.begin(), candidate.end(), [&draft](const PublishDraft &entry) {
        return entry.id == draft.id;
    });
    if (it == candidate.end()) {
        return false;
    }
    draft.name = draft.name.trimmed();
    draft.description = draft.description.trimmed();
    draft.defaultTopic = draft.defaultTopic.trimmed();
    draft.createdAt = it->createdAt;
    draft.lastUsedAt = it->lastUsedAt;
    draft.updatedAt = AppUtils::timestampNow();
    QString error;
    if (!validateDraft(draft, error, draft.id)) {
        m_errorMessage = error;
        emit stateChanged();
        return false;
    }
    *it = draft;
    return beginSave(candidate, QStringLiteral("update"), draft.id);
}

bool DraftLibraryService::removeDraft(const QString &id)
{
    if (!m_ready || m_readOnly || m_busy || id.trimmed().isEmpty()) {
        return false;
    }
    QVector<PublishDraft> candidate = m_drafts;
    const auto it = std::remove_if(candidate.begin(), candidate.end(), [&id](const PublishDraft &draft) {
        return draft.id == id;
    });
    if (it == candidate.end()) {
        return false;
    }
    candidate.erase(it, candidate.end());
    return beginSave(candidate, QStringLiteral("delete"), id);
}

bool DraftLibraryService::importDrafts(QVector<PublishDraft> drafts)
{
    if (!m_ready || m_readOnly || m_busy || drafts.isEmpty()) {
        return false;
    }

    QVector<PublishDraft> candidate = m_drafts;
    QSet<QString> ids;
    QSet<QString> names;
    for (const PublishDraft &draft : std::as_const(candidate)) {
        ids.insert(draft.id);
        names.insert(draft.name.trimmed().toCaseFolded());
    }

    const QString now = AppUtils::timestampNow();
    for (PublishDraft &draft : drafts) {
        draft.id = draft.id.trimmed();
        if (draft.id.isEmpty()) {
            draft.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        draft.name = draft.name.trimmed();
        draft.description = draft.description.trimmed();
        draft.defaultTopic = draft.defaultTopic.trimmed();
        if (draft.createdAt.isEmpty()) {
            draft.createdAt = now;
        }
        if (draft.updatedAt.isEmpty()) {
            draft.updatedAt = now;
        }

        const QString normalizedName = draft.name.toCaseFolded();
        if (ids.contains(draft.id) || names.contains(normalizedName)) {
            m_errorMessage = tr("Cannot import drafts with duplicate IDs or names.");
            emit stateChanged();
            return false;
        }
        QString error;
        if (!validateDraft(draft, error)) {
            m_errorMessage = error;
            emit stateChanged();
            return false;
        }
        ids.insert(draft.id);
        names.insert(normalizedName);
        candidate.append(draft);
    }

    return beginSave(candidate, QStringLiteral("import"), QString());
}

void DraftLibraryService::markUsed(const QString &id)
{
    if (!m_ready || m_readOnly || id.trimmed().isEmpty()) {
        return;
    }
    if (m_busy) {
        m_queuedUsageIds.insert(id);
        return;
    }
    QVector<PublishDraft> candidate = m_drafts;
    for (PublishDraft &draft : candidate) {
        if (draft.id == id) {
            draft.lastUsedAt = AppUtils::timestampNow();
            beginSave(candidate, QStringLiteral("touch"), id);
            return;
        }
    }
}

bool DraftLibraryService::recoverBackup()
{
    if (m_loading || m_busy || !m_canRecover) {
        return false;
    }
    m_busy = true;
    m_pendingOperation = QStringLiteral("recover");
    m_pendingDraftId.clear();
    emit stateChanged();
    const QString root = m_storageRoot;
    m_saveWatcher.setFuture(QtConcurrent::run([root]() { return DraftStore::recoverBackup(root); }));
    return true;
}

bool DraftLibraryService::beginSave(
    const QVector<PublishDraft> &candidate,
    const QString &operation,
    const QString &draftId)
{
    if (m_busy || m_readOnly) {
        return false;
    }
    m_pendingDrafts = candidate;
    m_pendingOperation = operation;
    m_pendingDraftId = draftId;
    m_busy = true;
    m_errorMessage.clear();
    emit stateChanged();
    const QString root = m_storageRoot;
    m_saveWatcher.setFuture(QtConcurrent::run([candidate, root]() {
        return DraftStore::saveDrafts(candidate, root);
    }));
    return true;
}

void DraftLibraryService::finishLoad()
{
    const DraftStore::LoadResult result = m_loadWatcher.result();
    m_loading = false;
    m_ready = result.state == DraftStore::LoadState::Ready;
    m_readOnly = result.state != DraftStore::LoadState::Ready;
    m_canRecover = result.canRecover;
    m_errorMessage = result.errorMessage;
    if (m_ready) {
        m_drafts = result.drafts;
        QSet<QString> ids;
        for (const PublishDraft &draft : std::as_const(m_drafts)) {
            QString validationError;
            if (ids.contains(draft.id) || !validateDraft(draft, validationError, draft.id)) {
                m_ready = false;
                m_readOnly = true;
                m_errorMessage = ids.contains(draft.id)
                    ? tr("Draft library contains duplicate IDs.")
                    : validationError;
                m_drafts.clear();
                break;
            }
            ids.insert(draft.id);
        }
    }
    emit draftsChanged();
    emit stateChanged();
    if (!m_errorMessage.isEmpty()) {
        emit storageError(m_errorMessage);
    }
}

void DraftLibraryService::finishSave()
{
    const DraftStore::SaveResult result = m_saveWatcher.result();
    const QString operation = m_pendingOperation;
    const QString draftId = m_pendingDraftId;
    m_busy = false;
    if (!result.ok) {
        m_errorMessage = result.errorMessage;
        emit stateChanged();
        emit storageError(m_errorMessage);
        return;
    }
    if (operation == QStringLiteral("recover")) {
        m_canRecover = false;
        m_readOnly = false;
        m_errorMessage.clear();
        emit stateChanged();
        emit operationSucceeded(operation, QString());
        load();
        return;
    }
    m_drafts = m_pendingDrafts;
    m_errorMessage.clear();
    emit draftsChanged();
    emit stateChanged();
    emit operationSucceeded(operation, draftId);
    flushQueuedUsage();
}

void DraftLibraryService::flushQueuedUsage()
{
    if (m_busy || m_queuedUsageIds.isEmpty()) {
        return;
    }
    QVector<PublishDraft> candidate = m_drafts;
    const QString now = AppUtils::timestampNow();
    bool changed = false;
    for (PublishDraft &draft : candidate) {
        if (m_queuedUsageIds.contains(draft.id)) {
            draft.lastUsedAt = now;
            changed = true;
        }
    }
    m_queuedUsageIds.clear();
    if (changed) {
        beginSave(candidate, QStringLiteral("touch"), QString());
    }
}

bool DraftLibraryService::nameExists(const QString &name, const QString &excludeId) const
{
    const QString normalized = name.trimmed();
    for (const PublishDraft &draft : m_drafts) {
        if (draft.id != excludeId && draft.name.trimmed().compare(normalized, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}
