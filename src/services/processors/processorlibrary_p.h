#pragma once

#include "services/processors/processorlibrary.h"
#include "services/processors/processorpackagehash.h"

#include <QSharedPointer>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

#include <optional>

class ProcessorLibrary::Impl
{
public:
    explicit Impl(
        const QString &storageDirectory = QString(),
        ProcessorPackageLimits packageLimits = {});
    ~Impl();

    Q_DISABLE_COPY_MOVE(Impl)

    bool isReady() const;
    QString lastError() const;
    QString storageDirectory() const;

    SaveProcessorRevisionResult saveRevision(const SaveProcessorRevisionCommand &command);
    bool deleteProcessor(const QString &processorId);
    std::optional<ProcessorDefinition> processorById(const QString &processorId) const;
    QVector<ProcessorDefinition> processors() const;
    QVector<QSharedPointer<const ProcessorRevisionSnapshot>> revisions(
        const QString &processorId) const;
    QSharedPointer<const ProcessorRevisionSnapshot> revisionById(
        const QString &revisionId) const;

private:
    bool initialize(const QString &storageDirectory);
    bool createSchema();
    bool executeStatement(const QString &statement);
    QSharedPointer<const ProcessorRevisionSnapshot> loadRevision(
        const QString &revisionId,
        QString *error = nullptr) const;

    QSqlDatabase m_db;
    QString m_connectionName;
    QString m_storageDirectory;
    QString m_databasePath;
    mutable QString m_lastError;
    ProcessorPackageLimits m_packageLimits;
};
