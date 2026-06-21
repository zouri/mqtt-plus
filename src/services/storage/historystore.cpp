#include "historystore.h"

#include <QDir>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariantMap>
#include <QUuid>

#include <algorithm>
#include <utility>

namespace {
QString nonNullString(const QString &value)
{
    return value.isNull() ? QStringLiteral("") : value;
}

bool ensureColumn(QSqlDatabase &db, const QString &table, const QString &column, const QString &definition, QString &error)
{
    QSqlQuery infoQuery(db);
    if (!infoQuery.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) {
        error = infoQuery.lastError().text();
        return false;
    }

    while (infoQuery.next()) {
        if (infoQuery.value(1).toString() == column) {
            return true;
        }
    }

    QSqlQuery alterQuery(db);
    if (!alterQuery.exec(QStringLiteral("ALTER TABLE %1 ADD COLUMN %2").arg(table, definition))) {
        error = alterQuery.lastError().text();
        return false;
    }
    return true;
}
} // namespace

HistoryStore::HistoryStore()
{
    m_connectionName = QStringLiteral("history-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    initialize();
}

HistoryStore::~HistoryStore()
{
    flushPendingMessages();
    if (m_db.isValid()) {
        m_db.close();
        m_db = QSqlDatabase();
    }
    if (!m_connectionName.isEmpty()) {
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool HistoryStore::isReady() const
{
    return m_db.isOpen();
}

QString HistoryStore::lastError() const
{
    return m_lastError;
}

qint64 HistoryStore::enqueueMessage(
    const QString &sessionId,
    const QString &timestamp,
    const QString &topic,
    const QByteArray &payloadBytes,
    const QString &parsedPayload,
    const QString &parsedFormat,
    const QString &parseError,
    const QString &scriptId,
    const QString &scriptName,
    const QString &payloadPreview,
    const QString &payloadState,
    qint64 payloadSize,
    const QString &payloadHash)
{
    if (!isReady()) {
        if (m_lastError.isEmpty()) {
            m_lastError = QStringLiteral("History database is not open.");
        }
        return 0;
    }

    if (m_nextMessageId <= 0) {
        QSqlQuery nextIdQuery(m_db);
        if (!nextIdQuery.exec(
                QStringLiteral(
                    "SELECT COALESCE("
                    "    (SELECT seq FROM sqlite_sequence WHERE name = 'mqtt_messages'), "
                    "    COALESCE(MAX(id), 0)"
                    ") + 1 "
                    "FROM mqtt_messages"))
                || !nextIdQuery.next()) {
            m_lastError = nextIdQuery.lastError().text();
            return 0;
        }
        m_nextMessageId = nextIdQuery.value(0).toLongLong();
    }

    const qint64 reservedId = m_nextMessageId++;
    m_pendingMessages.append(PendingMessage {
        sessionId,
        timestamp,
        topic,
        payloadBytes,
        parsedPayload,
        parsedFormat,
        parseError,
        scriptId,
        scriptName,
        payloadPreview,
        payloadState.isEmpty() ? QStringLiteral("full") : payloadState,
        payloadSize >= 0 ? payloadSize : payloadBytes.size(),
        payloadHash,
    });
    return reservedId;
}

QStringList HistoryStore::flushPendingMessages()
{
    QStringList flushedSessionIds;
    if (!isReady() || m_pendingMessages.isEmpty()) {
        return flushedSessionIds;
    }

    QSqlQuery query(m_db);
    if (!query.prepare(
        QStringLiteral(
            "INSERT INTO mqtt_messages("
            "session_id, timestamp, topic, payload, payload_b64, "
            "parsed_payload, parsed_format, parse_error, script_id, script_name, "
            "payload_bytes, payload_size, payload_state, payload_preview, payload_hash) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"))) {
        m_lastError = query.lastError().text();
        return flushedSessionIds;
    }

    if (!m_db.transaction()) {
        m_lastError = m_db.lastError().text();
        return flushedSessionIds;
    }

    QSet<QString> seenSessionIds;
    for (const PendingMessage &message : std::as_const(m_pendingMessages)) {
        query.bindValue(0, message.sessionId);
        query.bindValue(1, message.timestamp);
        query.bindValue(2, message.topic);
        query.bindValue(3, nonNullString(message.payloadPreview));
        query.bindValue(4, QString());
        query.bindValue(5, nonNullString(message.parsedPayload));
        query.bindValue(6, nonNullString(message.parsedFormat));
        query.bindValue(7, nonNullString(message.parseError));
        query.bindValue(8, nonNullString(message.scriptId));
        query.bindValue(9, nonNullString(message.scriptName));
        query.bindValue(10, message.payloadBytes);
        query.bindValue(11, message.payloadSize);
        query.bindValue(12, nonNullString(message.payloadState));
        query.bindValue(13, nonNullString(message.payloadPreview));
        query.bindValue(14, nonNullString(message.payloadHash));
        if (!query.exec()) {
            m_lastError = query.lastError().text();
            m_db.rollback();
            return {};
        }
        if (!seenSessionIds.contains(message.sessionId)) {
            seenSessionIds.insert(message.sessionId);
            flushedSessionIds.append(message.sessionId);
        }
    }

    if (!m_db.commit()) {
        m_lastError = m_db.lastError().text();
        m_db.rollback();
        return {};
    }

    m_pendingMessages.clear();
    m_lastError.clear();
    return flushedSessionIds;
}

int HistoryStore::pendingMessageCount() const
{
    return m_pendingMessages.size();
}

qint64 HistoryStore::appendEvent(
    const QString &sessionId,
    const QString &timestamp,
    const QString &channel,
    const QString &message)
{
    if (!isReady()) {
        return 0;
    }

    QSqlQuery query(m_db);
    query.prepare(
        QStringLiteral(
            "INSERT INTO event_logs(session_id, timestamp, channel, message) "
            "VALUES(?, ?, ?, ?)"));
    query.addBindValue(sessionId);
    query.addBindValue(timestamp);
    query.addBindValue(channel);
    query.addBindValue(message);

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return 0;
    }
    return query.lastInsertId().toLongLong();
}

QVariantList HistoryStore::loadMessages(const QString &sessionId, int limit) const
{
    QVariantList result;
    if (!isReady()) {
        return result;
    }

    QSqlQuery query(m_db);
    query.prepare(
        QStringLiteral(
            "SELECT id, timestamp, topic, payload, payload_b64, "
            "parsed_payload, parsed_format, parse_error, script_id, script_name, "
            "CASE WHEN payload_state = 'full' THEN payload_bytes ELSE NULL END, "
            "payload_size, payload_state, payload_preview, payload_hash "
            "FROM ("
            "    SELECT id, timestamp, topic, payload, payload_b64, "
            "    parsed_payload, parsed_format, parse_error, script_id, script_name, "
            "    payload_bytes, payload_size, payload_state, payload_preview, payload_hash "
            "    FROM mqtt_messages "
            "    WHERE session_id = ? "
            "    ORDER BY id DESC "
            "    LIMIT ?"
            ") recent_entries "
            "ORDER BY id ASC"));
    query.addBindValue(sessionId);
    query.addBindValue((std::max)(1, limit));

    if (!query.exec()) {
        return result;
    }

    while (query.next()) {
        QVariantMap row;
        row.insert(QStringLiteral("id"), query.value(0).toLongLong());
        row.insert(QStringLiteral("timestamp"), query.value(1).toString());
        row.insert(QStringLiteral("entry_type"), QStringLiteral("message"));
        row.insert(QStringLiteral("topic"), query.value(2).toString());
        row.insert(QStringLiteral("payload"), query.value(3).toString());
        row.insert(QStringLiteral("payload_b64"), query.value(4).toString());
        row.insert(QStringLiteral("parsed_payload"), query.value(5).toString());
        row.insert(QStringLiteral("parsed_format"), query.value(6).toString());
        row.insert(QStringLiteral("parse_error"), query.value(7).toString());
        row.insert(QStringLiteral("script_id"), query.value(8).toString());
        row.insert(QStringLiteral("script_name"), query.value(9).toString());
        row.insert(QStringLiteral("payload_bytes"), query.value(10).toByteArray());
        row.insert(QStringLiteral("payload_size"), query.value(11).toLongLong());
        row.insert(QStringLiteral("payload_state"), query.value(12).toString());
        row.insert(QStringLiteral("payload_preview"), query.value(13).toString());
        row.insert(QStringLiteral("payload_hash"), query.value(14).toString());
        result.append(row);
    }

    return result;
}

QVariantList HistoryStore::loadMessagesBefore(const QString &sessionId, qint64 beforeId, int limit) const
{
    QVariantList result;
    if (!isReady() || beforeId <= 0) {
        return result;
    }

    QSqlQuery query(m_db);
    query.prepare(
        QStringLiteral(
            "SELECT id, timestamp, topic, payload, payload_b64, "
            "parsed_payload, parsed_format, parse_error, script_id, script_name, "
            "CASE WHEN payload_state = 'full' THEN payload_bytes ELSE NULL END, "
            "payload_size, payload_state, payload_preview, payload_hash "
            "FROM ("
            "    SELECT id, timestamp, topic, payload, payload_b64, "
            "    parsed_payload, parsed_format, parse_error, script_id, script_name, "
            "    payload_bytes, payload_size, payload_state, payload_preview, payload_hash "
            "    FROM mqtt_messages "
            "    WHERE session_id = ? AND id < ? "
            "    ORDER BY id DESC "
            "    LIMIT ?"
            ") older_entries "
            "ORDER BY id ASC"));
    query.addBindValue(sessionId);
    query.addBindValue(beforeId);
    query.addBindValue((std::max)(1, limit));

    if (!query.exec()) {
        return result;
    }

    while (query.next()) {
        QVariantMap row;
        row.insert(QStringLiteral("id"), query.value(0).toLongLong());
        row.insert(QStringLiteral("timestamp"), query.value(1).toString());
        row.insert(QStringLiteral("entry_type"), QStringLiteral("message"));
        row.insert(QStringLiteral("topic"), query.value(2).toString());
        row.insert(QStringLiteral("payload"), query.value(3).toString());
        row.insert(QStringLiteral("payload_b64"), query.value(4).toString());
        row.insert(QStringLiteral("parsed_payload"), query.value(5).toString());
        row.insert(QStringLiteral("parsed_format"), query.value(6).toString());
        row.insert(QStringLiteral("parse_error"), query.value(7).toString());
        row.insert(QStringLiteral("script_id"), query.value(8).toString());
        row.insert(QStringLiteral("script_name"), query.value(9).toString());
        row.insert(QStringLiteral("payload_bytes"), query.value(10).toByteArray());
        row.insert(QStringLiteral("payload_size"), query.value(11).toLongLong());
        row.insert(QStringLiteral("payload_state"), query.value(12).toString());
        row.insert(QStringLiteral("payload_preview"), query.value(13).toString());
        row.insert(QStringLiteral("payload_hash"), query.value(14).toString());
        result.append(row);
    }

    return result;
}

QVariantList HistoryStore::loadLogs(const QString &sessionId, int limit) const
{
    QVariantList result;
    if (!isReady()) {
        return result;
    }

    QSqlQuery query(m_db);
    query.prepare(
        QStringLiteral(
            "SELECT id, timestamp, channel, message "
            "FROM ("
            "    SELECT id, timestamp, channel, message "
            "    FROM event_logs "
            "    WHERE session_id = ? "
            "    ORDER BY id DESC "
            "    LIMIT ?"
            ") recent_logs "
            "ORDER BY id ASC"));
    query.addBindValue(sessionId);
    query.addBindValue((std::max)(1, limit));

    if (!query.exec()) {
        return result;
    }

    while (query.next()) {
        QVariantMap row;
        row.insert(QStringLiteral("id"), query.value(0).toLongLong());
        row.insert(QStringLiteral("timestamp"), query.value(1).toString());
        row.insert(QStringLiteral("entry_type"), QStringLiteral("event"));
        row.insert(QStringLiteral("topic"), query.value(2).toString());
        row.insert(QStringLiteral("payload"), query.value(3).toString());
        result.append(row);
    }

    return result;
}

QVariantList HistoryStore::loadLogsBefore(const QString &sessionId, qint64 beforeId, int limit) const
{
    QVariantList result;
    if (!isReady() || beforeId <= 0) {
        return result;
    }

    QSqlQuery query(m_db);
    query.prepare(
        QStringLiteral(
            "SELECT id, timestamp, channel, message "
            "FROM ("
            "    SELECT id, timestamp, channel, message "
            "    FROM event_logs "
            "    WHERE session_id = ? AND id < ? "
            "    ORDER BY id DESC "
            "    LIMIT ?"
            ") older_logs "
            "ORDER BY id ASC"));
    query.addBindValue(sessionId);
    query.addBindValue(beforeId);
    query.addBindValue((std::max)(1, limit));

    if (!query.exec()) {
        return result;
    }

    while (query.next()) {
        QVariantMap row;
        row.insert(QStringLiteral("id"), query.value(0).toLongLong());
        row.insert(QStringLiteral("timestamp"), query.value(1).toString());
        row.insert(QStringLiteral("entry_type"), QStringLiteral("event"));
        row.insert(QStringLiteral("topic"), query.value(2).toString());
        row.insert(QStringLiteral("payload"), query.value(3).toString());
        result.append(row);
    }

    return result;
}

void HistoryStore::clearMessages(const QString &sessionId)
{
    if (!isReady()) {
        return;
    }
    flushPendingMessages();

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM mqtt_messages WHERE session_id = ?"));
    query.addBindValue(sessionId);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
    }
}

void HistoryStore::clearLogs(const QString &sessionId)
{
    if (!isReady()) {
        return;
    }
    flushPendingMessages();

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM event_logs WHERE session_id = ?"));
    query.addBindValue(sessionId);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
    }
}

void HistoryStore::clearAllMessages()
{
    if (!isReady()) {
        return;
    }
    flushPendingMessages();

    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("DELETE FROM mqtt_messages"))) {
        m_lastError = query.lastError().text();
    }
}

void HistoryStore::clearAllLogs()
{
    if (!isReady()) {
        return;
    }
    flushPendingMessages();

    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("DELETE FROM event_logs"))) {
        m_lastError = query.lastError().text();
    }
}

void HistoryStore::clearSessionHistory(const QString &sessionId)
{
    clearMessages(sessionId);
    clearLogs(sessionId);
}

void HistoryStore::pruneMessages(const QString &sessionId, int keepCount)
{
    if (!isReady() || keepCount <= 0) {
        return;
    }

    QSqlQuery query(m_db);
    query.prepare(
        QStringLiteral(
            "DELETE FROM mqtt_messages "
            "WHERE session_id = ? "
            "AND id NOT IN ("
            "    SELECT id FROM mqtt_messages "
            "    WHERE session_id = ? "
            "    ORDER BY id DESC "
            "    LIMIT ?"
            ")"));
    query.addBindValue(sessionId);
    query.addBindValue(sessionId);
    query.addBindValue(keepCount);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
    }
}

void HistoryStore::pruneLogs(const QString &sessionId, int keepCount)
{
    if (!isReady() || keepCount <= 0) {
        return;
    }

    QSqlQuery query(m_db);
    query.prepare(
        QStringLiteral(
            "DELETE FROM event_logs "
            "WHERE session_id = ? "
            "AND id NOT IN ("
            "    SELECT id FROM event_logs "
            "    WHERE session_id = ? "
            "    ORDER BY id DESC "
            "    LIMIT ?"
            ")"));
    query.addBindValue(sessionId);
    query.addBindValue(sessionId);
    query.addBindValue(keepCount);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
    }
}

bool HistoryStore::initialize()
{
    const QString dataPath =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataPath.isEmpty()) {
        m_lastError = QStringLiteral("Cannot resolve app data path.");
        return false;
    }

    QDir dir(dataPath);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        m_lastError = QStringLiteral("Cannot create app data directory.");
        return false;
    }

    const QString dbPath = dir.filePath(QStringLiteral("history.db"));

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(dbPath);
    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        return false;
    }

    QSqlQuery query(m_db);

    if (!query.exec(
            QStringLiteral(
                "CREATE TABLE IF NOT EXISTS mqtt_messages ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "session_id TEXT NOT NULL, "
                "timestamp TEXT NOT NULL, "
                "topic TEXT NOT NULL, "
                "payload TEXT NOT NULL DEFAULT '', "
                "payload_b64 TEXT NOT NULL DEFAULT '', "
                "parsed_payload TEXT NOT NULL DEFAULT '', "
                "parsed_format TEXT NOT NULL DEFAULT '', "
                "parse_error TEXT NOT NULL DEFAULT '', "
                "script_id TEXT NOT NULL DEFAULT '', "
                "script_name TEXT NOT NULL DEFAULT '', "
                "payload_bytes BLOB, "
                "payload_size INTEGER NOT NULL DEFAULT 0, "
                "payload_state TEXT NOT NULL DEFAULT 'full', "
                "payload_preview TEXT NOT NULL DEFAULT '', "
                "payload_hash TEXT NOT NULL DEFAULT '')"))) {
        m_lastError = query.lastError().text();
        return false;
    }

    const QVector<QPair<QString, QString>> messageColumns = {
        {QStringLiteral("payload_bytes"), QStringLiteral("payload_bytes BLOB")},
        {QStringLiteral("payload_size"), QStringLiteral("payload_size INTEGER NOT NULL DEFAULT 0")},
        {QStringLiteral("payload_state"), QStringLiteral("payload_state TEXT NOT NULL DEFAULT 'full'")},
        {QStringLiteral("payload_preview"), QStringLiteral("payload_preview TEXT NOT NULL DEFAULT ''")},
        {QStringLiteral("payload_hash"), QStringLiteral("payload_hash TEXT NOT NULL DEFAULT ''")},
    };
    for (const auto &column : messageColumns) {
        if (!ensureColumn(m_db, QStringLiteral("mqtt_messages"), column.first, column.second, m_lastError)) {
            return false;
        }
    }

    if (!query.exec(
            QStringLiteral(
                "CREATE INDEX IF NOT EXISTS idx_mqtt_messages_session_id_id "
                "ON mqtt_messages(session_id, id)"))) {
        m_lastError = query.lastError().text();
        return false;
    }

    if (!query.exec(
            QStringLiteral(
                "CREATE TABLE IF NOT EXISTS event_logs ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "session_id TEXT NOT NULL, "
                "timestamp TEXT NOT NULL, "
                "channel TEXT NOT NULL, "
                "message TEXT NOT NULL DEFAULT '')"))) {
        m_lastError = query.lastError().text();
        return false;
    }

    if (!query.exec(
            QStringLiteral(
                "CREATE INDEX IF NOT EXISTS idx_event_logs_session_id_id "
                "ON event_logs(session_id, id)"))) {
        m_lastError = query.lastError().text();
        return false;
    }

    return true;
}
