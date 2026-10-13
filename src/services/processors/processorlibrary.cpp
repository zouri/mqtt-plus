#include "processorlibrary.h"
#include "processorlibrary_p.h"

#include <algorithm>
#include <memory>
#include <QDir>
#include <QStandardPaths>

ProcessorLibrary::ProcessorLibrary(const QString &storageDirectory, bool snapshotOnly)
    : m_storageDirectory(storageDirectory)
{
    if (snapshotOnly && m_storageDirectory.isEmpty()) {
        m_storageDirectory = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
            .filePath(QStringLiteral("mqtt_plus/processors"));
    }
    if (!snapshotOnly) {
        m_impl = std::make_unique<Impl>(storageDirectory);
        reload();
    }
}

ProcessorLibrary::~ProcessorLibrary() = default;

bool ProcessorLibrary::isReady() const
{
    return !m_impl || m_impl->isReady();
}

QString ProcessorLibrary::lastError() const
{
    return m_lastError;
}

QString ProcessorLibrary::storageDirectory() const
{
    return m_impl ? m_impl->storageDirectory() : m_storageDirectory;
}

bool ProcessorLibrary::reload()
{
    if (!m_impl) {
        return false;
    }
    if (!m_impl->isReady()) {
        m_lastError = m_impl->lastError();
        return false;
    }

    QHash<QString, ProcessorDefinition> processors;
    QHash<QString, QSharedPointer<const ProcessorRevisionSnapshot>> revisions;
    const QVector<ProcessorDefinition> definitions = m_impl->processors();
    if (!m_impl->lastError().isEmpty()) {
        m_lastError = m_impl->lastError();
        return false;
    }
    for (const ProcessorDefinition &processor : definitions) {
        processors.insert(processor.id, processor);
        const auto processorRevisions = m_impl->revisions(processor.id);
        if (!m_impl->lastError().isEmpty()) {
            m_lastError = m_impl->lastError();
            return false;
        }
        for (const auto &revision : processorRevisions) {
            revisions.insert(revision->id, revision);
        }
    }

    m_processors = std::move(processors);
    m_revisions = std::move(revisions);
    m_lastError.clear();
    return true;
}

QVector<ProcessorDefinition> ProcessorLibrary::processors() const
{
    QVector<ProcessorDefinition> result;
    result.reserve(m_processors.size());
    for (const ProcessorDefinition &processor : m_processors) {
        result.append(processor);
    }
    std::sort(
        result.begin(),
        result.end(),
        [](const ProcessorDefinition &left, const ProcessorDefinition &right) {
            const int nameOrder = left.name.compare(right.name, Qt::CaseInsensitive);
            return nameOrder == 0 ? left.id < right.id : nameOrder < 0;
        });
    return result;
}

std::optional<ProcessorDefinition> ProcessorLibrary::processorById(
    const QString &processorId) const
{
    const auto it = m_processors.constFind(processorId.trimmed());
    return it == m_processors.cend()
        ? std::nullopt
        : std::optional<ProcessorDefinition>(*it);
}

QVector<QSharedPointer<const ProcessorRevisionSnapshot>> ProcessorLibrary::revisions(
    const QString &processorId) const
{
    QVector<QSharedPointer<const ProcessorRevisionSnapshot>> result;
    const QString normalizedId = processorId.trimmed();
    for (const auto &revision : m_revisions) {
        if (revision->processorId == normalizedId) {
            result.append(revision);
        }
    }
    std::sort(
        result.begin(),
        result.end(),
        [](const auto &left, const auto &right) {
            return left->revisionNumber < right->revisionNumber;
        });
    return result;
}

QSharedPointer<const ProcessorRevisionSnapshot> ProcessorLibrary::revisionById(
    const QString &revisionId) const
{
    return m_revisions.value(revisionId.trimmed());
}

std::optional<ResolvedProcessor> ProcessorLibrary::resolve(
    const ProcessorReference &reference,
    QString *error) const
{
    const QString processorId = reference.processorId.trimmed();
    const auto processorIt = m_processors.constFind(processorId);
    if (processorId.isEmpty() || processorIt == m_processors.cend()) {
        if (error) {
            *error = processorId.isEmpty()
                ? QStringLiteral("Processor binding is empty.")
                : QStringLiteral("Processor was not found: %1").arg(processorId);
        }
        return std::nullopt;
    }

    const QString revisionId = processorIt->currentRevisionId;
    if (revisionId.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Processor has no content: %1").arg(processorId);
        }
        return std::nullopt;
    }

    const auto revisionIt = m_revisions.constFind(revisionId);
    if (revisionIt == m_revisions.cend()) {
        if (error) {
            *error = QStringLiteral("Processor content was not found: %1").arg(revisionId);
        }
        return std::nullopt;
    }
    if ((*revisionIt)->processorId != processorId) {
        if (error) {
            *error = QStringLiteral("Processor content does not belong to Processor %1.")
                         .arg(processorId);
        }
        return std::nullopt;
    }

    if (error) {
        error->clear();
    }
    return ResolvedProcessor {*processorIt, *revisionIt};
}

SaveProcessorRevisionResult ProcessorLibrary::saveRevision(
    const SaveProcessorRevisionCommand &command)
{
    if (!m_impl) {
        SaveProcessorRevisionResult result;
        result.error = QStringLiteral("Read-only Processor snapshot.");
        return result;
    }
    SaveProcessorRevisionResult result = m_impl->saveRevision(command);
    if (!result.ok) {
        m_lastError = result.error;
        return result;
    }

    if (!result.replayed) applySavedSnapshot(result);
    m_lastError.clear();
    return result;
}

bool ProcessorLibrary::deleteProcessor(const QString &processorId)
{
    if (!m_impl) {
        return false;
    }
    if (!m_impl->deleteProcessor(processorId)) {
        m_lastError = m_impl->lastError();
        return false;
    }
    return reload();
}

void ProcessorLibrary::replaceSnapshot(const QVector<ProcessorDefinition> &processors,
    const QVector<QSharedPointer<const ProcessorRevisionSnapshot>> &revisions)
{
    m_processors.clear();
    m_revisions.clear();
    for (const auto &processor : processors) m_processors.insert(processor.id, processor);
    for (const auto &revision : revisions) m_revisions.insert(revision->id, revision);
}

void ProcessorLibrary::applySavedSnapshot(const SaveProcessorRevisionResult &result)
{
    m_processors.insert(result.processor.id, result.processor);
    m_revisions.insert(result.revision->id, result.revision);
}

void ProcessorLibrary::applyDeletedSnapshot(const QString &processorId)
{
    m_processors.remove(processorId);
    for (auto it = m_revisions.begin(); it != m_revisions.end();) {
        if ((*it)->processorId == processorId) it = m_revisions.erase(it);
        else ++it;
    }
}

SaveProcessorRevisionResult ProcessorLibrary::savedRequest(const QString &requestId) const
{
    return m_impl ? m_impl->savedRequest(requestId) : SaveProcessorRevisionResult{};
}
