#pragma once

#include "domain/messageprocessor.h"
#include "services/processors/processorpackagehash.h"

#include <QSharedPointer>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

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

struct ProcessorLibraryStoreResult
{
    bool ok = false;
    QString error;
};

class ProcessorLibraryStore
{
public:
    explicit ProcessorLibraryStore(
        const QString &storageDirectory = QString(),
        ProcessorPackageLimits packageLimits = {});
    ~ProcessorLibraryStore();

    Q_DISABLE_COPY_MOVE(ProcessorLibraryStore)

    bool isReady() const;
    QString lastError() const;
    QString storageDirectory() const;
    QString databasePath() const;

    SaveProcessorRevisionResult saveRevision(const SaveProcessorRevisionCommand &command);
    std::optional<ProcessorDefinition> processorById(const QString &processorId) const;
    QVector<ProcessorDefinition> processors(bool includeArchived = false) const;
    QVector<QSharedPointer<const ProcessorRevisionSnapshot>> revisions(
        const QString &processorId) const;
    QSharedPointer<const ProcessorRevisionSnapshot> revisionById(
        const QString &revisionId) const;
    QSharedPointer<const ProcessorRevisionSnapshot> resolve(
        const ProcessorReference &reference,
        QString *error = nullptr) const;
    ProcessorLibraryStoreResult archiveProcessor(const QString &processorId);
    ProcessorLibraryStoreResult restoreProcessor(const QString &processorId);

private:
    bool initialize(const QString &storageDirectory);
    bool createSchema();
    bool executeStatement(const QString &statement);
    QSharedPointer<const ProcessorRevisionSnapshot> loadRevision(
        const QString &revisionId,
        QString *error = nullptr) const;
    ProcessorLibraryStoreResult setArchived(const QString &processorId, bool archived);

    QSqlDatabase m_db;
    QString m_connectionName;
    QString m_storageDirectory;
    QString m_databasePath;
    mutable QString m_lastError;
    ProcessorPackageLimits m_packageLimits;
};
