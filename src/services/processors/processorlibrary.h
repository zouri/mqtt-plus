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
    QString expectedVersion;
    QString requestId;
};

struct SaveProcessorRevisionResult
{
    bool ok = false;
    bool createdProcessor = false;
    bool createdRevision = false;
    ProcessorDefinition processor;
    QSharedPointer<const ProcessorRevisionSnapshot> revision;
    QString error;
    int status = 200;
    bool replayed = false;
};

struct ResolvedProcessor
{
    ProcessorDefinition processor;
    QSharedPointer<const ProcessorRevisionSnapshot> revision;
};

class ProcessorLibrary
{
public:
    explicit ProcessorLibrary(const QString &storageDirectory = QString(), bool snapshotOnly = false);
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
    void replaceSnapshot(const QVector<ProcessorDefinition> &processors,
        const QVector<QSharedPointer<const ProcessorRevisionSnapshot>> &revisions);
    void applySavedSnapshot(const SaveProcessorRevisionResult &result);
    void applyDeletedSnapshot(const QString &processorId);
    SaveProcessorRevisionResult savedRequest(const QString &requestId) const;

private:
    class Impl;

    std::unique_ptr<Impl> m_impl;
    QHash<QString, ProcessorDefinition> m_processors;
    QHash<QString, QSharedPointer<const ProcessorRevisionSnapshot>> m_revisions;
    QString m_lastError;
    QString m_storageDirectory;
};
