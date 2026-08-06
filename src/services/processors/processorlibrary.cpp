#include "processorlibrary.h"

#include <algorithm>

ProcessorLibrary::ProcessorLibrary(const QString &storageDirectory)
    : m_store(storageDirectory)
{
    reload();
}

bool ProcessorLibrary::isReady() const
{
    return m_store.isReady() && m_lastError.isEmpty();
}

QString ProcessorLibrary::lastError() const
{
    return m_lastError;
}

QString ProcessorLibrary::storageDirectory() const
{
    return m_store.storageDirectory();
}

bool ProcessorLibrary::reload()
{
    if (!m_store.isReady()) {
        m_lastError = m_store.lastError();
        return false;
    }

    QHash<QString, ProcessorDefinition> processors;
    QHash<QString, QSharedPointer<const ProcessorRevisionSnapshot>> revisions;
    const QVector<ProcessorDefinition> definitions = m_store.processors();
    if (!m_store.lastError().isEmpty()) {
        m_lastError = m_store.lastError();
        return false;
    }
    for (const ProcessorDefinition &processor : definitions) {
        processors.insert(processor.id, processor);
        const auto processorRevisions = m_store.revisions(processor.id);
        if (!m_store.lastError().isEmpty()) {
            m_lastError = m_store.lastError();
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
    SaveProcessorRevisionResult result = m_store.saveRevision(command);
    if (!result.ok) {
        m_lastError = result.error;
        return result;
    }

    if (!reload()) {
        result.ok = false;
        result.error = m_lastError;
        return result;
    }

    const auto processor = processorById(result.processor.id);
    const auto revision = revisionById(result.revision->id);
    if (!processor || revision.isNull()) {
        result.ok = false;
        result.error = QStringLiteral("Cannot refresh the saved Processor snapshot.");
        m_lastError = result.error;
        return result;
    }
    result.processor = *processor;
    result.revision = revision;
    return result;
}

bool ProcessorLibrary::deleteProcessor(const QString &processorId)
{
    if (!m_store.deleteProcessor(processorId)) {
        m_lastError = m_store.lastError();
        return false;
    }
    return reload();
}
