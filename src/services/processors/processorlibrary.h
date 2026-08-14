#pragma once

#include "domain/messageprocessor.h"

#include <QHash>
#include <QSharedPointer>
#include <QString>
#include <QVector>

#include <memory>
#include <optional>

struct SaveProcessorRevisionCommand
{
    QString processorId;
    QString name;
    QString description;
    ProcessorRevisionContent content;
};

struct SaveProcessorRevisionResult
{
    bool ok = false;
    bool createdProcessor = false;
    bool createdRevision = false;
    ProcessorDefinition processor;
    QSharedPointer<const ProcessorRevisionSnapshot> revision;
    QString error;
};

struct ResolvedProcessor
{
    ProcessorDefinition processor;
    QSharedPointer<const ProcessorRevisionSnapshot> revision;
};

class ProcessorLibrary
{
public:
    explicit ProcessorLibrary(const QString &storageDirectory = QString());
    ~ProcessorLibrary();

    Q_DISABLE_COPY_MOVE(ProcessorLibrary)

    bool isReady() const;
    QString lastError() const;
    QString storageDirectory() const;
    bool reload();

    QVector<ProcessorDefinition> processors() const;
    std::optional<ProcessorDefinition> processorById(const QString &processorId) const;
    QVector<QSharedPointer<const ProcessorRevisionSnapshot>> revisions(
        const QString &processorId) const;
    QSharedPointer<const ProcessorRevisionSnapshot> revisionById(
        const QString &revisionId) const;
    std::optional<ResolvedProcessor> resolve(
        const ProcessorReference &reference,
        QString *error = nullptr) const;
    SaveProcessorRevisionResult saveRevision(const SaveProcessorRevisionCommand &command);
    bool deleteProcessor(const QString &processorId);

private:
    class Impl;

    std::unique_ptr<Impl> m_impl;
    QHash<QString, ProcessorDefinition> m_processors;
    QHash<QString, QSharedPointer<const ProcessorRevisionSnapshot>> m_revisions;
    QString m_lastError;
};
