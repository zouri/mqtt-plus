#include "historystore.h"

#include <QDir>
#include <QHash>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariantMap>
#include <QUuid>

#include <algorithm>

namespace {
QString nonNullString(const QString &value)
{
    return value.isNull() ? QStringLiteral("") : value;
}

QStringList requiredMessageColumns()
{
    return {
        QStringLiteral("id"),
        QStringLiteral("session_id"),
        QStringLiteral("timestamp"),
        QStringLiteral("direction"),
        QStringLiteral("topic"),
        QStringLiteral("qos"),
        QStringLiteral("retain"),
        QStringLiteral("retain_known"),
        QStringLiteral("parsed_payload"),
        QStringLiteral("parsed_format"),
        QStringLiteral("parse_error"),
        QStringLiteral("script_id"),
        QStringLiteral("script_name"),
        QStringLiteral("payload_bytes"),
        QStringLiteral("payload_size"),
        QStringLiteral("payload_state"),
        QStringLiteral("payload_preview"),
        QStringLiteral("payload_hash"),
        QStringLiteral("payload_format"),
    };
}

QVariantMap messageRowFromQuery(const QSqlQuery &query)
{
    QVariantMap row;
    row.insert(QStringLiteral("id"), query.value(0).toLongLong());
    row.insert(QStringLiteral("timestamp"), query.value(1).toString());
    row.insert(QStringLiteral("entry_type"), QStringLiteral("message"));
    row.insert(QStringLiteral("direction"), query.value(2).toString());
    row.insert(QStringLiteral("topic"), query.value(3).toString());
    row.insert(QStringLiteral("qos"), query.value(4).toInt());
    row.insert(QStringLiteral("retain"), query.value(5).toBool());
    row.insert(QStringLiteral("retain_known"), query.value(6).toBool());
    row.insert(QStringLiteral("parsed_payload"), query.value(7).toString());
    row.insert(QStringLiteral("parsed_format"), query.value(8).toString());
    row.insert(QStringLiteral("parse_error"), query.value(9).toString());
    row.insert(QStringLiteral("script_id"), query.value(10).toString());
    row.insert(QStringLiteral("script_name"), query.value(11).toString());
    row.insert(QStringLiteral("payload_bytes"), query.value(12).toByteArray());
    row.insert(QStringLiteral("payload_size"), query.value(13).toLongLong());
    row.insert(QStringLiteral("payload_state"), query.value(14).toString());
    row.insert(QStringLiteral("payload_preview"), query.value(15).toString());
    row.insert(QStringLiteral("payload_hash"), query.value(16).toString());
    row.insert(QStringLiteral("payload_format"), query.value(17).toInt());
    return row;
}

QVariantMap messageRowFromRecord(const MessageRecord &record)
{
    QVariantMap row;
    row.insert(QStringLiteral("id"), record.id);
    row.insert(QStringLiteral("timestamp"), record.timestamp);
    row.insert(QStringLiteral("entry_type"), QStringLiteral("message"));
    row.insert(QStringLiteral("direction"), messageDirectionName(record.direction));
    row.insert(QStringLiteral("topic"), record.topic);
    row.insert(QStringLiteral("qos"), record.qos);
    row.insert(QStringLiteral("retain"), record.retain);
    row.insert(QStringLiteral("retain_known"), record.retainKnown);
    row.insert(QStringLiteral("parsed_payload"), record.parsedPayload);
    row.insert(QStringLiteral("parsed_format"), record.parsedFormat);
    row.insert(QStringLiteral("parse_error"), record.parseError);
    row.insert(QStringLiteral("script_id"), record.scriptId);
    row.insert(QStringLiteral("script_name"), record.scriptName);
    row.insert(QStringLiteral("payload_bytes"), record.payloadBytes);
    row.insert(QStringLiteral("payload_size"), record.payloadSize);
    row.insert(QStringLiteral("payload_state"), record.payloadState);
    row.insert(QStringLiteral("payload_preview"), record.payloadPreview);
    row.insert(QStringLiteral("payload_hash"), record.payloadHash);
    row.insert(QStringLiteral("payload_format"), record.payloadFormat);
    return row;
}

bool resetStaleMessageTable(QSqlDatabase &db, QString &error)
{
    QSqlQuery infoQuery(db);
    if (!infoQuery.exec(QStringLiteral("PRAGMA table_info(mqtt_messages)"))) {
        error = infoQuery.lastError().text();
        return false;
    }

    QSet<QString> columns;
    while (infoQuery.next()) {
        columns.insert(infoQuery.value(1).toString());
    }
    if (columns.isEmpty()) {
        return true;
    }

    for (const QString &column : requiredMessageColumns()) {
        if (!columns.contains(column)) {
            QSqlQuery dropQuery(db);
            if (!dropQuery.exec(QStringLiteral("DROP TABLE mqtt_messages"))) {
                error = dropQuery.lastError().text();
                return false;
            }
            return true;
        }
    }
    return true;
}

} // namespace

HistoryStore::HistoryStore()
{
    m_connectionName = QStringLiteral("history-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    initialize(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
}

HistoryStore::HistoryStore(const QString &dataPath)
{
    m_connectionName = QStringLiteral("history-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    initialize(dataPath);
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

qint64 HistoryStore::enqueueMessage(const MessageRecord &message)
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
    MessageRecord pending = message;
    pending.id = reservedId;
    pending.payloadState = pending.payloadState.isEmpty() ? QStringLiteral("full") : pending.payloadState;
    if (pending.payloadSize < 0) {
        pending.payloadSize = pending.payloadBytes.size();
    }
    m_pendingMessages.append(std::move(pending));
    return reservedId;
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
    const QString &payloadHash,
    int payloadFormat)
{
    MessageRecord message;
    message.sessionId = sessionId;
    message.timestamp = timestamp;
    message.topic = topic;
    message.payloadBytes = payloadBytes;
    message.parsedPayload = parsedPayload;
    message.parsedFormat = parsedFormat;
    message.parseError = parseError;
    message.scriptId = scriptId;
    message.scriptName = scriptName;
    message.payloadPreview = payloadPreview;
    message.payloadState = payloadState;
    message.payloadSize = payloadSize >= 0 ? payloadSize : payloadBytes.size();
    message.payloadHash = payloadHash;
    message.payloadFormat = payloadFormat;
    return enqueueMessage(message);
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
            "session_id, timestamp, direction, topic, qos, retain, retain_known, "
            "parsed_payload, parsed_format, parse_error, script_id, script_name, "
            "payload_bytes, payload_size, payload_state, payload_preview, payload_hash, payload_format) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"))) {
        m_lastError = query.lastError().text();
        return flushedSessionIds;
    }

    if (!m_db.transaction()) {
        m_lastError = m_db.lastError().text();
        return flushedSessionIds;
    }

    QHash<QString, qint64> flushedCounts;
    for (const MessageRecord &message : std::as_const(m_pendingMessages)) {
        query.bindValue(0, message.sessionId);
        query.bindValue(1, message.timestamp);
        query.bindValue(2, messageDirectionName(message.direction));
        query.bindValue(3, message.topic);
        query.bindValue(4, message.qos);
        query.bindValue(5, message.retain);
        query.bindValue(6, message.retainKnown);
        query.bindValue(7, nonNullString(message.parsedPayload));
        query.bindValue(8, nonNullString(message.parsedFormat));
        query.bindValue(9, nonNullString(message.parseError));
        query.bindValue(10, nonNullString(message.scriptId));
        query.bindValue(11, nonNullString(message.scriptName));
        query.bindValue(12, message.payloadBytes);
        query.bindValue(13, message.payloadSize);
        query.bindValue(14, nonNullString(message.payloadState));
        query.bindValue(15, nonNullString(message.payloadPreview));
        query.bindValue(16, nonNullString(message.payloadHash));
        query.bindValue(17, message.payloadFormat);
        if (!query.exec()) {
            m_lastError = query.lastError().text();
            m_db.rollback();
            return {};
        }
        ++flushedCounts[message.sessionId];
        if (!flushedSessionIds.contains(message.sessionId)) {
            flushedSessionIds.append(message.sessionId);
        }
    }

    QSqlQuery countQuery(m_db);
    if (!countQuery.prepare(QStringLiteral(
            "INSERT INTO mqtt_message_totals(session_id, total_count) VALUES(?, ?) "
            "ON CONFLICT(session_id) DO UPDATE SET total_count = total_count + excluded.total_count"))) {
        m_lastError = countQuery.lastError().text();
        m_db.rollback();
        return {};
    }
    for (auto it = flushedCounts.cbegin(); it != flushedCounts.cend(); ++it) {
        countQuery.bindValue(0, it.key());
        countQuery.bindValue(1, it.value());
        if (!countQuery.exec()) {
            m_lastError = countQuery.lastError().text();
            m_db.rollback();
            return {};
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

qint64 HistoryStore::totalMessageCount(const QString &sessionId) const
{
    qint64 count = std::count_if(
        m_pendingMessages.cbegin(),
        m_pendingMessages.cend(),
        [&sessionId](const MessageRecord &message) { return message.sessionId == sessionId; });
    if (!isReady()) {
        return count;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT total_count FROM mqtt_message_totals WHERE session_id = ?"));
    query.addBindValue(sessionId);
    if (query.exec() && query.next()) {
        count += query.value(0).toLongLong();
    }
    return count;
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
            "SELECT id, timestamp, direction, topic, qos, retain, retain_known, "
            "parsed_payload, parsed_format, parse_error, script_id, script_name, "
            "payload_bytes, "
            "payload_size, payload_state, payload_preview, payload_hash, payload_format "
            "FROM ("
            "    SELECT id, timestamp, direction, topic, qos, retain, retain_known, "
            "    parsed_payload, parsed_format, parse_error, script_id, script_name, "
            "    payload_bytes, payload_size, payload_state, payload_preview, payload_hash, payload_format "
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
        result.append(messageRowFromQuery(query));
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
            "SELECT id, timestamp, direction, topic, qos, retain, retain_known, "
            "parsed_payload, parsed_format, parse_error, script_id, script_name, "
            "payload_bytes, "
            "payload_size, payload_state, payload_preview, payload_hash, payload_format "
            "FROM ("
            "    SELECT id, timestamp, direction, topic, qos, retain, retain_known, "
            "    parsed_payload, parsed_format, parse_error, script_id, script_name, "
            "    payload_bytes, payload_size, payload_state, payload_preview, payload_hash, payload_format "
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
        result.append(messageRowFromQuery(query));
    }

    return result;
}

QVariantMap HistoryStore::loadMessage(qint64 messageId) const
{
    if (!isReady() || messageId <= 0) {
        return {};
    }

    const auto pending = std::find_if(
        m_pendingMessages.cbegin(),
        m_pendingMessages.cend(),
        [messageId](const MessageRecord &message) { return message.id == messageId; });
    if (pending != m_pendingMessages.cend()) {
        return messageRowFromRecord(*pending);
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT id, timestamp, direction, topic, qos, retain, retain_known, "
        "parsed_payload, parsed_format, parse_error, script_id, script_name, "
        "payload_bytes, payload_size, payload_state, payload_preview, payload_hash, payload_format "
        "FROM mqtt_messages WHERE id = ?"));
    query.addBindValue(messageId);
    if (!query.exec() || !query.next()) {
        return {};
    }
    return messageRowFromQuery(query);
}

QByteArray HistoryStore::loadMessagePayloadBytes(qint64 messageId) const
{
    if (!isReady() || messageId <= 0) {
        return {};
    }

    const auto pending = std::find_if(
        m_pendingMessages.cbegin(),
        m_pendingMessages.cend(),
        [messageId](const MessageRecord &message) { return message.id == messageId; });
    if (pending != m_pendingMessages.cend()) {
        return pending->payloadBytes;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT payload_bytes FROM mqtt_messages WHERE id = ?"));
    query.addBindValue(messageId);
    if (!query.exec() || !query.next()) {
        return {};
    }
    return query.value(0).toByteArray();
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
        return;
    }

    query.prepare(QStringLiteral("DELETE FROM mqtt_message_totals WHERE session_id = ?"));
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
        return;
    }
    if (!query.exec(QStringLiteral("DELETE FROM mqtt_message_totals"))) {
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

bool HistoryStore::initialize(const QString &dataPath)
{
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

    if (!resetStaleMessageTable(m_db, m_lastError)) {
        return false;
    }

    if (!query.exec(
            QStringLiteral(
                "CREATE TABLE IF NOT EXISTS mqtt_messages ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "session_id TEXT NOT NULL, "
                "timestamp TEXT NOT NULL, "
                "direction TEXT NOT NULL CHECK(direction IN ('incoming', 'outgoing')), "
                "topic TEXT NOT NULL, "
                "qos INTEGER NOT NULL DEFAULT -1, "
                "retain INTEGER NOT NULL DEFAULT 0, "
                "retain_known INTEGER NOT NULL DEFAULT 0, "
                "parsed_payload TEXT NOT NULL DEFAULT '', "
                "parsed_format TEXT NOT NULL DEFAULT '', "
                "parse_error TEXT NOT NULL DEFAULT '', "
                "script_id TEXT NOT NULL DEFAULT '', "
                "script_name TEXT NOT NULL DEFAULT '', "
                "payload_bytes BLOB, "
                "payload_size INTEGER NOT NULL DEFAULT 0, "
                "payload_state TEXT NOT NULL DEFAULT 'full', "
                "payload_preview TEXT NOT NULL DEFAULT '', "
                "payload_hash TEXT NOT NULL DEFAULT '', "
                "payload_format INTEGER NOT NULL DEFAULT -1)"))) {
        m_lastError = query.lastError().text();
        return false;
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
                "CREATE TABLE IF NOT EXISTS mqtt_message_totals ("
                "session_id TEXT PRIMARY KEY, "
                "total_count INTEGER NOT NULL DEFAULT 0)"))) {
        m_lastError = query.lastError().text();
        return false;
    }

    if (!query.exec(QStringLiteral(
            "DELETE FROM mqtt_message_totals "
            "WHERE session_id NOT IN (SELECT DISTINCT session_id FROM mqtt_messages)"))) {
        m_lastError = query.lastError().text();
        return false;
    }

    if (!query.exec(QStringLiteral(
            "INSERT OR IGNORE INTO mqtt_message_totals(session_id, total_count) "
            "SELECT session_id, COUNT(*) FROM mqtt_messages GROUP BY session_id"))) {
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
