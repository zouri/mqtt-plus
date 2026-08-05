#pragma once

#include "services/processors/processorruntimeadapter.h"

#include <QHash>
#include <QSharedPointer>
#include <QString>
#include <QVector>

#include <optional>

class ProcessorRuntimeRegistry
{
public:
    bool addAdapter(
        const QSharedPointer<ProcessorRuntimeAdapter> &adapter,
        QString *error = nullptr);

    QSharedPointer<ProcessorRuntimeAdapter> adapter(const QString &runtimeId) const;
    std::optional<RuntimeDescriptor> descriptor(const QString &runtimeId) const;
    QVector<RuntimeDescriptor> descriptors() const;

private:
    struct Entry
    {
        RuntimeDescriptor descriptor;
        QSharedPointer<ProcessorRuntimeAdapter> adapter;
    };

    QHash<QString, Entry> m_entries;
};
