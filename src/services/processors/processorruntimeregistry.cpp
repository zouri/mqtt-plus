#include "processorruntimeregistry.h"

#include <algorithm>

namespace {

bool isStableIdentifier(const QString &value)
{
    return !value.isEmpty() && value == value.trimmed() && !value.contains(QChar::Null);
}

} // namespace

bool ProcessorRuntimeRegistry::addAdapter(
    const QSharedPointer<ProcessorRuntimeAdapter> &adapter,
    QString *error)
{
    if (adapter.isNull()) {
        if (error) {
            *error = QStringLiteral("Processor runtime adapter is required.");
        }
        return false;
    }

    const RuntimeDescriptor descriptor = adapter->descriptor();
    if (!isStableIdentifier(descriptor.runtimeId)) {
        if (error) {
            *error = QStringLiteral("Processor runtime ID is invalid.");
        }
        return false;
    }
    if (!isStableIdentifier(descriptor.languageId)) {
        if (error) {
            *error = QStringLiteral("Processor runtime language ID is invalid.");
        }
        return false;
    }
    if (descriptor.displayName.trimmed().isEmpty()) {
        if (error) {
            *error = QStringLiteral("Processor runtime display name is required.");
        }
        return false;
    }
    if (!isStableIdentifier(descriptor.runtimeVersion)) {
        if (error) {
            *error = QStringLiteral("Processor runtime version is invalid.");
        }
        return false;
    }
    if (descriptor.supportedContractIds.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Processor runtime must support at least one contract.");
        }
        return false;
    }
    for (const QString &contractId : descriptor.supportedContractIds) {
        if (!isStableIdentifier(contractId)) {
            if (error) {
                *error = QStringLiteral("Processor runtime contract ID is invalid.");
            }
            return false;
        }
    }
    if (m_entries.contains(descriptor.runtimeId)) {
        if (error) {
            *error = QStringLiteral("Processor runtime is already registered: %1")
                         .arg(descriptor.runtimeId);
        }
        return false;
    }

    m_entries.insert(descriptor.runtimeId, {descriptor, adapter});
    if (error) {
        error->clear();
    }
    return true;
}

QSharedPointer<ProcessorRuntimeAdapter> ProcessorRuntimeRegistry::adapter(
    const QString &runtimeId) const
{
    const auto it = m_entries.constFind(runtimeId);
    return it == m_entries.constEnd()
        ? QSharedPointer<ProcessorRuntimeAdapter>()
        : it->adapter;
}

std::optional<RuntimeDescriptor> ProcessorRuntimeRegistry::descriptor(
    const QString &runtimeId) const
{
    const auto it = m_entries.constFind(runtimeId);
    return it == m_entries.constEnd()
        ? std::nullopt
        : std::optional<RuntimeDescriptor>(it->descriptor);
}

QVector<RuntimeDescriptor> ProcessorRuntimeRegistry::descriptors() const
{
    QVector<RuntimeDescriptor> result;
    result.reserve(m_entries.size());
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        result.append(it->descriptor);
    }
    std::sort(
        result.begin(),
        result.end(),
        [](const RuntimeDescriptor &left, const RuntimeDescriptor &right) {
            return left.runtimeId < right.runtimeId;
        });
    return result;
}
