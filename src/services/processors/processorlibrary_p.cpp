#include "processorlibrary_p.h"

#include <QDateTime>
#include <QDir>
#include <QCborValue>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStringList>
#include <QUuid>

#include <utility>

namespace {
constexpr int kProcessorLibrarySchemaVersion = 1;

QString timestampNow()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

QString defaultStorageDirectory()
{
    const QString configRoot = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QString basePath = configRoot.isEmpty()
        ? QDir::home().filePath(QStringLiteral(".config"))
        : configRoot;
    return QDir(basePath).filePath(QStringLiteral("mqtt_plus/processors"));
}

ProcessorDefinition processorFromQuery(const QSqlQuery &query)
{
    ProcessorDefinition processor;
    processor.id = query.value(0).toString();
    processor.name = query.value(1).toString();
    processor.description = query.value(2).toString();
    processor.currentRevisionId = query.value(3).toString();
    processor.createdAt = query.value(4).toString();
    processor.updatedAt = query.value(5).toString();
    return processor;
}

QString normalizedId(const QString &id)
{
    return id.trimmed();
}

} // namespace

ProcessorLibrary::Impl::Impl(
    const QString &storageDirectory,
    ProcessorPackageLimits packageLimits)
    : m_packageLimits(std::move(packageLimits))
{
    initialize(storageDirectory);
}

ProcessorLibrary::Impl::~Impl()
{
    if (m_db.isValid()) {
        m_db.close();
    }
    m_db = QSqlDatabase();
    if (!m_connectionName.isEmpty()) {
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool ProcessorLibrary::Impl::isReady() const
{
    return m_db.isValid() && m_db.isOpen() && m_lastError.isEmpty();
}

QString ProcessorLibrary::Impl::lastError() const
{
    return m_lastError;
}

QString ProcessorLibrary::Impl::storageDirectory() const
{
    return m_storageDirectory;
}

bool ProcessorLibrary::Impl::initialize(const QString &storageDirectory)
{
    m_storageDirectory = storageDirectory.trimmed().isEmpty()
        ? defaultStorageDirectory()
        : QDir(storageDirectory).absolutePath();
    if (!QDir().mkpath(m_storageDirectory)) {
        m_lastError = QStringLiteral("Cannot create Processor Library directory: %1")
                          .arg(m_storageDirectory);
        return false;
    }

    m_databasePath = QDir(m_storageDirectory).filePath(QStringLiteral("library.db"));
    m_connectionName = QStringLiteral("processor-library-%1")
                           .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(m_databasePath);
    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        return false;
    }

    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("PRAGMA foreign_keys = ON"))
        || !query.exec(QStringLiteral("PRAGMA busy_timeout = 5000"))) {
        m_lastError = query.lastError().text();
        return false;
    }
    if (!query.exec(QStringLiteral("PRAGMA journal_mode = WAL"))
        || !query.next()
        || query.value(0).toString().compare(QStringLiteral("wal"), Qt::CaseInsensitive) != 0) {
        m_lastError = query.lastError().text();
        if (m_lastError.isEmpty()) {
            m_lastError = QStringLiteral("Cannot enable WAL mode for the Processor Library.");
        }
        return false;
    }
    if (!query.exec(QStringLiteral("PRAGMA synchronous = NORMAL"))) {
        m_lastError = query.lastError().text();
        return false;
    }
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next()) {
        m_lastError = query.lastError().text();
        return false;
    }

    const int schemaVersion = query.value(0).toInt();
    if (schemaVersion > kProcessorLibrarySchemaVersion) {
        m_lastError = QStringLiteral("Processor Library schema version %1 is newer than supported version %2.")
                          .arg(schemaVersion)
                          .arg(kProcessorLibrarySchemaVersion);
        return false;
    }
    if (schemaVersion == 0 && !createSchema()) {
        return false;
    }
    // Archived processors from older builds become ordinary editable processors.
    return executeStatement(QStringLiteral(
        "UPDATE processors SET archived_at = NULL WHERE archived_at IS NOT NULL"));
}

bool ProcessorLibrary::Impl::createSchema()
{
    if (!m_db.transaction()) {
        m_lastError = m_db.lastError().text();
        return false;
    }

    const QStringList statements {
        QStringLiteral(
            "CREATE TABLE processors ("
            "id TEXT PRIMARY KEY, "
            "name TEXT NOT NULL, "
            "description TEXT NOT NULL DEFAULT '', "
            "current_revision_id TEXT, "
            "created_at TEXT NOT NULL, "
            "updated_at TEXT NOT NULL, "
            "archived_at TEXT, "
            "FOREIGN KEY(current_revision_id) REFERENCES processor_revisions(id) "
            "ON DELETE SET NULL DEFERRABLE INITIALLY DEFERRED)"),
        QStringLiteral(
            "CREATE TABLE processor_revisions ("
            "id TEXT PRIMARY KEY, "
            "processor_id TEXT NOT NULL, "
            "revision_number INTEGER NOT NULL CHECK(revision_number > 0), "
            "contract_id TEXT NOT NULL, "
            "language_id TEXT NOT NULL, "
            "runtime_id TEXT NOT NULL, "
            "entry_file TEXT NOT NULL, "
            "entry_symbol TEXT NOT NULL, "
            "manifest_json TEXT NOT NULL DEFAULT '{}', "
            "content_hash TEXT NOT NULL, "
            "created_at TEXT NOT NULL, "
            "FOREIGN KEY(processor_id) REFERENCES processors(id) ON DELETE CASCADE, "
            "UNIQUE(processor_id, revision_number), "
            "UNIQUE(processor_id, content_hash))"),
        QStringLiteral(
            "CREATE TABLE processor_files ("
            "revision_id TEXT NOT NULL, "
            "path TEXT NOT NULL, "
            "media_type TEXT NOT NULL, "
            "content BLOB NOT NULL, "
            "content_hash TEXT NOT NULL, "
            "PRIMARY KEY(revision_id, path), "
            "FOREIGN KEY(revision_id) REFERENCES processor_revisions(id) ON DELETE CASCADE)"),
        QStringLiteral(
            "CREATE INDEX idx_processor_revisions_processor_number "
            "ON processor_revisions(processor_id, revision_number DESC)"),
        QStringLiteral(
            "CREATE INDEX idx_processors_active_name "
            "ON processors(archived_at, name COLLATE NOCASE)"),
        QStringLiteral(
            "CREATE TRIGGER validate_processor_current_revision "
            "BEFORE UPDATE OF current_revision_id ON processors "
            "WHEN NEW.current_revision_id IS NOT NULL "
            "AND NOT EXISTS ("
            "SELECT 1 FROM processor_revisions "
            "WHERE id = NEW.current_revision_id AND processor_id = NEW.id) "
            "BEGIN "
            "SELECT RAISE(ABORT, 'current revision must belong to processor'); "
            "END"),
        QStringLiteral("PRAGMA user_version = 1"),
    };

    for (const QString &statement : statements) {
        if (!executeStatement(statement)) {
            m_db.rollback();
            return false;
        }
    }
    if (!m_db.commit()) {
        m_lastError = m_db.lastError().text();
        return false;
    }
    return true;
}

bool ProcessorLibrary::Impl::executeStatement(const QString &statement)
{
    QSqlQuery query(m_db);
    if (query.exec(statement)) {
        return true;
    }
    m_lastError = query.lastError().text();
    return false;
}

SaveProcessorRevisionResult ProcessorLibrary::Impl::saveRevision(
    const SaveProcessorRevisionCommand &sourceCommand)
{
    SaveProcessorRevisionResult result;
    if (!isReady()) {
        result.error = m_lastError.isEmpty()
            ? QStringLiteral("Processor Library is not ready.")
            : m_lastError;
        return result;
    }

    SaveProcessorRevisionCommand command = sourceCommand;
    command.name = command.name.trimmed();
    command.description = command.description.trimmed();
    if (command.description.isNull()) {
        command.description = QStringLiteral("");
    }
    command.processorId = normalizedId(command.processorId);
    if (command.name.isEmpty()) {
        result.error = QStringLiteral("Processor name is required.");
        return result;
    }

    const PreparedProcessorPackage package = ProcessorPackageHash::prepare(
        command.content,
        m_packageLimits);
    if (!package.ok) {
        result.error = package.error;
        return result;
    }

    const QString processorId = command.processorId.isEmpty()
        ? QUuid::createUuid().toString(QUuid::WithoutBraces)
        : command.processorId;
    const QString now = timestampNow();

    if (!m_db.transaction()) {
        result.error = m_db.lastError().text();
        return result;
    }

    QSqlQuery processorQuery(m_db);
    processorQuery.prepare(QStringLiteral(
        "SELECT id FROM processors WHERE id = ?"));
    processorQuery.addBindValue(processorId);
    if (!processorQuery.exec()) {
        result.error = processorQuery.lastError().text();
        m_db.rollback();
        return result;
    }

    result.createdProcessor = !processorQuery.next();
    if (result.createdProcessor) {
        QSqlQuery insertProcessor(m_db);
        insertProcessor.prepare(QStringLiteral(
            "INSERT INTO processors("
            "id, name, description, current_revision_id, created_at, updated_at) "
            "VALUES(?, ?, ?, NULL, ?, ?)"));
        insertProcessor.addBindValue(processorId);
        insertProcessor.addBindValue(command.name);
        insertProcessor.addBindValue(command.description);
        insertProcessor.addBindValue(now);
        insertProcessor.addBindValue(now);
        if (!insertProcessor.exec()) {
            result.error = insertProcessor.lastError().text();
            m_db.rollback();
            return result;
        }
    }

    QString revisionId;
    QSqlQuery existingRevision(m_db);
    existingRevision.prepare(QStringLiteral(
        "SELECT id FROM processor_revisions "
        "WHERE processor_id = ? AND content_hash = ?"));
    existingRevision.addBindValue(processorId);
    existingRevision.addBindValue(package.contentHash);
    if (!existingRevision.exec()) {
        result.error = existingRevision.lastError().text();
        m_db.rollback();
        return result;
    }

    if (existingRevision.next()) {
        revisionId = existingRevision.value(0).toString();
    } else {
        QSqlQuery nextRevisionQuery(m_db);
        nextRevisionQuery.prepare(QStringLiteral(
            "SELECT COALESCE(MAX(revision_number), 0) + 1 "
            "FROM processor_revisions WHERE processor_id = ?"));
        nextRevisionQuery.addBindValue(processorId);
        if (!nextRevisionQuery.exec() || !nextRevisionQuery.next()) {
            result.error = nextRevisionQuery.lastError().text();
            m_db.rollback();
            return result;
        }
        const qint64 revisionNumber = nextRevisionQuery.value(0).toLongLong();
        revisionId = QUuid::createUuid().toString(QUuid::WithoutBraces);

        QSqlQuery insertRevision(m_db);
        insertRevision.prepare(QStringLiteral(
            "INSERT INTO processor_revisions("
            "id, processor_id, revision_number, contract_id, language_id, runtime_id, "
            "entry_file, entry_symbol, manifest_json, content_hash, created_at) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
        insertRevision.addBindValue(revisionId);
        insertRevision.addBindValue(processorId);
        insertRevision.addBindValue(revisionNumber);
        insertRevision.addBindValue(package.content.contractId);
        insertRevision.addBindValue(package.content.languageId);
        insertRevision.addBindValue(package.content.runtimeId);
        insertRevision.addBindValue(package.content.entryFile);
        insertRevision.addBindValue(package.content.entrySymbol);
        insertRevision.addBindValue(QString::fromUtf8(package.manifestJson));
        insertRevision.addBindValue(package.contentHash);
        insertRevision.addBindValue(now);
        if (!insertRevision.exec()) {
            result.error = insertRevision.lastError().text();
            m_db.rollback();
            return result;
        }

        QSqlQuery insertFile(m_db);
        insertFile.prepare(QStringLiteral(
            "INSERT INTO processor_files("
            "revision_id, path, media_type, content, content_hash) "
            "VALUES(?, ?, ?, ?, ?)"));
        for (const ProcessorSourceFile &file : package.content.files) {
            insertFile.bindValue(0, revisionId);
            insertFile.bindValue(1, file.path);
            insertFile.bindValue(2, file.mediaType);
            insertFile.bindValue(3, file.content);
            insertFile.bindValue(4, file.contentHash);
            if (!insertFile.exec()) {
                result.error = insertFile.lastError().text();
                m_db.rollback();
                return result;
            }
        }
        result.createdRevision = true;
    }

    QSqlQuery updateProcessor(m_db);
    updateProcessor.prepare(QStringLiteral(
        "UPDATE processors SET name = ?, description = ?, current_revision_id = ?, "
        "updated_at = ? WHERE id = ?"));
    updateProcessor.addBindValue(command.name);
    updateProcessor.addBindValue(command.description);
    updateProcessor.addBindValue(revisionId);
    updateProcessor.addBindValue(now);
    updateProcessor.addBindValue(processorId);
    if (!updateProcessor.exec() || updateProcessor.numRowsAffected() != 1) {
        result.error = updateProcessor.lastError().text();
        if (result.error.isEmpty()) {
            result.error = QStringLiteral("Cannot update Processor Library metadata.");
        }
        m_db.rollback();
        return result;
    }

    if (!m_db.commit()) {
        result.error = m_db.lastError().text();
        return result;
    }

    const std::optional<ProcessorDefinition> processor = processorById(processorId);
    if (!processor) {
        result.error = m_lastError.isEmpty()
            ? QStringLiteral("Cannot reload saved processor.")
            : m_lastError;
        return result;
    }
    result.revision = revisionById(revisionId);
    if (!result.revision) {
        result.error = m_lastError.isEmpty()
            ? QStringLiteral("Cannot reload saved processor revision.")
            : m_lastError;
        return result;
    }

    result.processor = *processor;
    result.ok = true;
    m_lastError.clear();
    return result;
}

bool ProcessorLibrary::Impl::deleteProcessor(const QString &processorId)
{
    if (!isReady()) {
        return false;
    }
    const QString id = normalizedId(processorId);
    if (id.isEmpty()) {
        m_lastError = QStringLiteral("Processor ID is required.");
        return false;
    }
    if (!m_db.transaction()) {
        m_lastError = m_db.lastError().text();
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM processors WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec() || query.numRowsAffected() != 1) {
        m_lastError = query.lastError().text();
        if (m_lastError.isEmpty()) {
            m_lastError = QStringLiteral("Processor was not found.");
        }
        m_db.rollback();
        return false;
    }
    if (!m_db.commit()) {
        m_lastError = m_db.lastError().text();
        return false;
    }
    m_lastError.clear();
    return true;
}

std::optional<ProcessorDefinition> ProcessorLibrary::Impl::processorById(
    const QString &processorId) const
{
    if (!isReady()) {
        return std::nullopt;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT id, name, description, current_revision_id, created_at, updated_at "
        "FROM processors WHERE id = ?"));
    query.addBindValue(normalizedId(processorId));
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return std::nullopt;
    }
    if (!query.next()) {
        m_lastError.clear();
        return std::nullopt;
    }
    m_lastError.clear();
    return processorFromQuery(query);
}

QVector<ProcessorDefinition> ProcessorLibrary::Impl::processors() const
{
    QVector<ProcessorDefinition> result;
    if (!isReady()) {
        return result;
    }

    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral(
            "SELECT id, name, description, current_revision_id, created_at, updated_at "
            "FROM processors ORDER BY name COLLATE NOCASE, id"))) {
        m_lastError = query.lastError().text();
        return result;
    }
    while (query.next()) {
        result.append(processorFromQuery(query));
    }
    m_lastError.clear();
    return result;
}

QVector<QSharedPointer<const ProcessorRevisionSnapshot>> ProcessorLibrary::Impl::revisions(
    const QString &processorId) const
{
    QVector<QSharedPointer<const ProcessorRevisionSnapshot>> result;
    if (!isReady()) {
        return result;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT id FROM processor_revisions "
        "WHERE processor_id = ? ORDER BY revision_number"));
    query.addBindValue(normalizedId(processorId));
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return result;
    }
    QStringList revisionIds;
    while (query.next()) {
        revisionIds.append(query.value(0).toString());
    }
    for (const QString &revisionId : revisionIds) {
        const auto revision = loadRevision(revisionId, nullptr);
        if (!revision) {
            result.clear();
            return result;
        }
        result.append(revision);
    }
    m_lastError.clear();
    return result;
}

QSharedPointer<const ProcessorRevisionSnapshot> ProcessorLibrary::Impl::revisionById(
    const QString &revisionId) const
{
    return loadRevision(normalizedId(revisionId), nullptr);
}

QSharedPointer<const ProcessorRevisionSnapshot> ProcessorLibrary::Impl::loadRevision(
    const QString &revisionId,
    QString *error) const
{
    if (!isReady()) {
        const QString message = m_lastError.isEmpty()
            ? QStringLiteral("Processor Library is not ready.")
            : m_lastError;
        if (error) {
            *error = message;
        }
        return {};
    }

    QSqlQuery revisionQuery(m_db);
    revisionQuery.prepare(QStringLiteral(
        "SELECT id, processor_id, revision_number, contract_id, language_id, runtime_id, "
        "entry_file, entry_symbol, manifest_json, content_hash, created_at "
        "FROM processor_revisions WHERE id = ?"));
    revisionQuery.addBindValue(revisionId);
    if (!revisionQuery.exec()) {
        m_lastError = revisionQuery.lastError().text();
        if (error) {
            *error = m_lastError;
        }
        return {};
    }
    if (!revisionQuery.next()) {
        m_lastError.clear();
        if (error) {
            *error = QStringLiteral("Processor revision was not found.");
        }
        return {};
    }

    auto revision = QSharedPointer<ProcessorRevisionSnapshot>::create();
    revision->id = revisionQuery.value(0).toString();
    revision->processorId = revisionQuery.value(1).toString();
    revision->revisionNumber = revisionQuery.value(2).toLongLong();
    revision->contractId = revisionQuery.value(3).toString();
    revision->languageId = revisionQuery.value(4).toString();
    revision->runtimeId = revisionQuery.value(5).toString();
    revision->entryFile = revisionQuery.value(6).toString();
    revision->entrySymbol = revisionQuery.value(7).toString();
    revision->contentHash = revisionQuery.value(9).toString();
    revision->createdAt = revisionQuery.value(10).toString();

    QJsonParseError parseError;
    const QJsonDocument manifestDocument = QJsonDocument::fromJson(
        revisionQuery.value(8).toString().toUtf8(),
        &parseError);
    if (parseError.error != QJsonParseError::NoError || !manifestDocument.isObject()) {
        m_lastError = QStringLiteral("Stored processor manifest is invalid.");
        if (error) {
            *error = m_lastError;
        }
        return {};
    }
    revision->manifest = QCborValue::fromJsonValue(manifestDocument.object()).toMap();

    QSqlQuery fileQuery(m_db);
    fileQuery.prepare(QStringLiteral(
        "SELECT path, media_type, content, content_hash "
        "FROM processor_files WHERE revision_id = ? ORDER BY path"));
    fileQuery.addBindValue(revisionId);
    if (!fileQuery.exec()) {
        m_lastError = fileQuery.lastError().text();
        if (error) {
            *error = m_lastError;
        }
        return {};
    }
    while (fileQuery.next()) {
        ProcessorSourceFile file;
        file.path = fileQuery.value(0).toString();
        file.mediaType = fileQuery.value(1).toString();
        file.content = fileQuery.value(2).toByteArray();
        file.contentHash = fileQuery.value(3).toString();
        revision->files.append(file);
    }
    if (revision->files.isEmpty()) {
        m_lastError = QStringLiteral("Stored processor revision contains no source files.");
        if (error) {
            *error = m_lastError;
        }
        return {};
    }

    m_lastError.clear();
    if (error) {
        error->clear();
    }
    return revision;
}
