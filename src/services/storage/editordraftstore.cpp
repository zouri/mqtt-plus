#include "editordraftstore.h"
#include "services/processors/editorcodec.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QScopeGuard>

namespace {
EditorStoreReply failure(int status, const QString &error)
{
    return {status, {{"error", error}}, {}, {}};
}
QString now() { return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs); }
}

EditorDraftStore::EditorDraftStore(const QString &directory)
    : m_library(directory)
    , m_connection(QStringLiteral("editor-drafts-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connection);
    const auto path = QDir(m_library.storageDirectory()).filePath(QStringLiteral("editor-drafts.db"));
    m_db.setDatabaseName(path);
    if (!m_db.open()) { m_error = QStringLiteral("Cannot open editor draft storage."); return; }
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next()
        || query.value(0).toInt() > 1) {
        m_error = QStringLiteral("Editor draft schema is unavailable or newer than supported.");
        return;
    }
    if (!query.exec(QStringLiteral("PRAGMA journal_mode = WAL"))
        || !query.exec(QStringLiteral("PRAGMA busy_timeout = 5000"))
        || !query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS drafts (id TEXT PRIMARY KEY, processor_id TEXT NOT NULL, version INTEGER NOT NULL, bytes INTEGER NOT NULL, data BLOB NOT NULL)"))
        || !query.exec(QStringLiteral("PRAGMA user_version = 1")))
        m_error = QStringLiteral("Cannot initialize editor draft storage.");
}

EditorDraftStore::~EditorDraftStore()
{
    m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connection);
}

EditorStoreReply EditorDraftStore::readDraft(const QString &id)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT data FROM drafts WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec()) return failure(500, QStringLiteral("Cannot read draft."));
    if (!query.next()) return failure(404, QStringLiteral("Draft is unavailable."));
    return {200, QJsonDocument::fromJson(query.value(0).toByteArray()).object(), {}, {}};
}

EditorStoreReply EditorDraftStore::writeDraft(QJsonObject draft, bool create, const QString &version)
{
    QSqlQuery begin(m_db);
    if (!begin.exec(QStringLiteral("BEGIN IMMEDIATE"))) return failure(503, QStringLiteral("Draft storage is busy."));
    auto rollback = qScopeGuard([this]() { m_db.rollback(); });
    SaveProcessorRevisionCommand command;
    QString error;
    if (!EditorCodec::command(draft, command, error, false)) return failure(422, error);
    const QString id = draft.value("draftId").toString();
    if (!create) {
        const auto previous = readDraft(id);
        if (previous.status != 200) return previous;
        if (version.isEmpty() || previous.data.value("draftVersion").toString() != version)
            return failure(409, QStringLiteral("Draft changed in another tab."));
        if (draft.value("processorId") != previous.data.value("processorId")
            || draft.value("baseVersion") != previous.data.value("baseVersion"))
            return failure(409, QStringLiteral("Draft base cannot be replaced."));
    }
    if (!command.processorId.isEmpty() && !m_library.processorById(command.processorId))
        return failure(410, QStringLiteral("Processor was deleted."));
    qint64 bytes = 0;
    for (const auto &file : command.content.files) bytes += file.content.size();
    QSqlQuery limits(m_db);
    limits.prepare(QStringLiteral("SELECT COUNT(*), COALESCE(SUM(bytes), 0) FROM drafts WHERE id != ?"));
    limits.addBindValue(id);
    if (!limits.exec() || !limits.next()) return failure(500, QStringLiteral("Cannot check draft quota."));
    if (limits.value(0).toInt() >= 100 || limits.value(1).toLongLong() + bytes > 64 * 1024 * 1024)
        return failure(413, QStringLiteral("Draft storage is full. Discard unused drafts first."));
    limits.finish();
    draft = EditorCodec::package(command);
    draft.insert("draftId", id);
    draft.insert("processorId", command.processorId);
    draft.insert("baseVersion", command.expectedVersion);
    draft.insert("draftVersion", QString::number(create ? 1 : version.toLongLong() + 1));
    draft.insert("updatedAt", now());
    QSqlQuery query(m_db);
    query.prepare(create ? QStringLiteral("INSERT INTO drafts VALUES (?, ?, ?, ?, ?)")
        : QStringLiteral("UPDATE drafts SET processor_id = ?, version = ?, bytes = ?, data = ? WHERE id = ? AND version = ?"));
    if (create) query.addBindValue(id);
    query.addBindValue(command.processorId.isNull() ? QStringLiteral("") : command.processorId);
    query.addBindValue(draft.value("draftVersion").toString().toLongLong());
    query.addBindValue(bytes);
    query.addBindValue(QJsonDocument(draft).toJson(QJsonDocument::Compact));
    if (!create) { query.addBindValue(id); query.addBindValue(version.toLongLong()); }
    if (!query.exec()) return failure(500, QStringLiteral("Cannot persist draft."));
    if (query.numRowsAffected() != 1) return failure(409, QStringLiteral("Draft changed in another tab."));
    if (!m_db.commit()) return failure(500, QStringLiteral("Cannot commit draft."));
    rollback.dismiss();
    return {create ? 201 : 200, draft, {}, {}};
}

bool EditorDraftStore::removeDraft(const QString &id, const QString &version)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM drafts WHERE id = ? AND version = ?"));
    query.addBindValue(id);
    query.addBindValue(version.toLongLong());
    return query.exec() && query.numRowsAffected() == 1;
}

EditorStoreReply EditorDraftStore::request(const QString &method, const QString &path,
    const QJsonObject &body, const QString &version, const QString &processorId)
{
    if (!m_error.isEmpty() || !m_library.isReady()) return failure(503, QStringLiteral("Editor storage is unavailable."));
    if (path == "/processors" && method == "GET") {
        QJsonArray rows;
        for (const auto &processor : m_library.processors()) {
            auto metadata = EditorCodec::processor(processor);
            const auto revision = m_library.revisionById(processor.currentRevisionId);
            if (revision) metadata.insert("languageId", revision->languageId);
            rows.append(metadata);
        }
        return {200, {{"processors", rows}}, {}, {}};
    }
    if (path.startsWith("/processors/") && method == "GET") {
        const auto processor = m_library.processorById(path.sliced(12));
        return processor ? EditorStoreReply{200, EditorCodec::processor(*processor,
            m_library.revisionById(processor->currentRevisionId)), {}, {}}
            : failure(410, QStringLiteral("Processor was deleted."));
    }
    if (path == "/drafts" && method == "GET") {
        QSqlQuery query(m_db);
        query.prepare(QStringLiteral("SELECT data FROM drafts WHERE processor_id = ? ORDER BY id"));
        query.addBindValue(processorId.isNull() ? QStringLiteral("") : processorId);
        if (!query.exec()) return failure(500, QStringLiteral("Cannot list drafts."));
        QJsonArray drafts;
        while (query.next()) {
            auto summary = QJsonDocument::fromJson(query.value(0).toByteArray()).object();
            summary.insert("fileCount", summary.value("files").toArray().size());
            summary.remove("files");
            summary.remove("manifest");
            drafts.append(summary);
        }
        return {200, {{"drafts", drafts}}, {}, {}};
    }
    if (path == "/drafts" && method == "POST") {
        QJsonObject draft = body;
        if (!draft.contains("files")) {
            const auto processor = m_library.processorById(body.value("processorId").toString());
            if (!body.value("processorId").toString().isEmpty() && !processor)
                return failure(410, QStringLiteral("Processor was deleted."));
            if (processor) {
                const auto snapshot = EditorCodec::processor(*processor, m_library.revisionById(processor->currentRevisionId));
                draft = snapshot.value("package").toObject();
                draft.insert("processorId", processor->id);
                draft.insert("baseVersion", snapshot.value("version"));
            } else draft = EditorCodec::templateDraft(body.value("languageId").toString(QStringLiteral("lua")));
        }
        draft.insert("draftId", QUuid::createUuid().toString(QUuid::WithoutBraces));
        return writeDraft(draft, true, {});
    }
    if (path.startsWith("/drafts/")) {
        const auto id = path.sliced(8);
        if (id.contains('/')) return failure(404, QStringLiteral("Unknown editor operation."));
        if (method == "GET") return readDraft(id);
        if (method == "PUT") { auto draft = body; draft.insert("draftId", id); return writeDraft(draft, false, version); }
        if (method == "DELETE") return removeDraft(id, version) ? EditorStoreReply{} : failure(409, QStringLiteral("Draft changed or disappeared."));
    }
    if (path.startsWith("/requests/") && method == "GET") {
        const auto saved = m_library.savedRequest(path.sliced(10));
        if (!saved.ok) return failure(saved.status == 410 ? 410 : 404, QStringLiteral("Save result is unavailable."));
        return {200, {{"processor", EditorCodec::processor(saved.processor, saved.revision)}}, {}, {}};
    }
    return failure(404, QStringLiteral("Unknown editor operation."));
}

EditorStoreReply EditorDraftStore::save(const QJsonObject &draft, const QString &requestId)
{
    SaveProcessorRevisionCommand command;
    QString error;
    if (requestId.isEmpty() || requestId.size() > 128 || !EditorCodec::command(draft, command, error))
        return failure(422, error.isEmpty() ? QStringLiteral("Save request identity is required.") : error);
    command.requestId = requestId;
    if (!command.processorId.isEmpty() && command.expectedVersion.isEmpty())
        return failure(422, QStringLiteral("Base version is required."));
    const auto result = m_library.saveRevision(command);
    if (!result.ok) return failure(result.status == 200 ? 500 : result.status,
        result.status == 500 || result.status == 200 ? QStringLiteral("Cannot commit processor.") : result.error);
    QJsonArray warnings;
    const auto newer = readDraft(draft.value("draftId").toString());
    // Reconcile a first commit, never rebase a later draft when replaying a receipt.
    if (!result.replayed && newer.status == 200
        && newer.data.value("processorId") == draft.value("processorId")
        && newer.data.value("baseVersion") == draft.value("baseVersion")) {
        auto data = newer.data;
        if (data.value("draftVersion") == draft.value("draftVersion")) {
            if (!removeDraft(data.value("draftId").toString(), data.value("draftVersion").toString()))
                warnings.append(QStringLiteral("Version saved, but the draft could not be cleared."));
        } else {
            const auto oldVersion = data.value("draftVersion").toString();
            data.insert("processorId", result.processor.id);
            data.insert("baseVersion", QString::number(result.processor.changeSequence));
            data.insert("draftVersion", QString::number(oldVersion.toLongLong() + 1));
            QSqlQuery query(m_db);
            query.prepare(QStringLiteral("UPDATE drafts SET processor_id = ?, version = ?, data = ? WHERE id = ? AND version = ?"));
            query.addBindValue(result.processor.id);
            query.addBindValue(data.value("draftVersion").toString().toLongLong());
            query.addBindValue(QJsonDocument(data).toJson(QJsonDocument::Compact));
            query.addBindValue(data.value("draftId").toString());
            query.addBindValue(oldVersion.toLongLong());
            if (!query.exec() || query.numRowsAffected() != 1)
                warnings.append(QStringLiteral("Version saved, but the newer draft base could not be updated."));
        }
    }
    auto current = result;
    const auto currentProcessor = m_library.processorById(result.processor.id);
    if (!currentProcessor) return failure(410, QStringLiteral("Processor was deleted."));
    current.processor = *currentProcessor;
    current.revision = m_library.revisionById(current.processor.currentRevisionId);
    const auto surviving = readDraft(draft.value("draftId").toString());
    return {200, {{"processor", EditorCodec::processor(result.processor, result.revision)},
        {"draft", surviving.status == 200 ? QJsonValue(surviving.data) : QJsonValue(QJsonValue::Null)},
        {"warnings", warnings}}, current, {}};
}

EditorStoreReply EditorDraftStore::deleteProcessor(const QString &processorId)
{
    if (!m_library.deleteProcessor(processorId)) return failure(500, QStringLiteral("Cannot delete processor."));
    const auto drafts = request("GET", "/drafts", {}, {}, processorId).data.value("drafts").toArray();
    for (const auto &item : drafts) {
        const auto loaded = readDraft(item.toObject().value("draftId").toString());
        if (loaded.status != 200) return {200, {{"warning", "Processor deleted, but some drafts could not be detached."}}, {}, processorId};
        auto data = loaded.data;
        data.insert("processorId", "");
        data.insert("baseVersion", "");
        data.insert("draftVersion", QString::number(data.value("draftVersion").toString().toLongLong() + 1));
        QSqlQuery query(m_db);
        query.prepare(QStringLiteral("UPDATE drafts SET processor_id = '', version = ?, data = ? WHERE id = ?"));
        query.addBindValue(data.value("draftVersion").toString().toLongLong());
        query.addBindValue(QJsonDocument(data).toJson(QJsonDocument::Compact));
        query.addBindValue(data.value("draftId").toString());
        if (!query.exec()) return {200, {{"warning", "Processor deleted, but some drafts could not be detached."}}, {}, processorId};
    }
    return {200, {}, {}, processorId};
}

QJsonObject EditorDraftStore::draftCounts() const
{
    QJsonObject counts;
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("SELECT processor_id, COUNT(*) FROM drafts GROUP BY processor_id"))) return counts;
    while (query.next()) counts.insert(query.value(0).toString(), query.value(1).toInt());
    return counts;
}
