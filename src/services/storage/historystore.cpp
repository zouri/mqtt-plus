#include "historystore.h"

#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariantMap>
#include <QUuid>

#include <algorithm>

HistoryStore::HistoryStore()
{
    m_connectionName = QStringLiteral("history-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    initialize();
}

HistoryStore::~HistoryStore()
{
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

qint64 HistoryStore::appendMessage(
    const QString &sessionId,
    const QString &timestamp,
    const QString &topic,
    const QByteArray &payloadBytes,
    const QString &parsedPayload,
    const QString &parsedFormat,
    const QString &parseError,
    const QString &scriptId,
    const QString &scriptName)
{
    if (!isReady()) {
        return 0;
    }

    QSqlQuery query(m_db);
    query.prepare(
        QStringLiteral(
            "INSERT INTO mqtt_messages("
            "session_id, timestamp, topic, payload, payload_b64, "
            "parsed_payload, parsed_format, parse_error, script_id, script_name) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(sessionId);
    query.addBindValue(timestamp);
    query.addBindValue(topic);
    query.addBindValue(QString::fromUtf8(payloadBytes));
    query.addBindValue(QString::fromLatin1(payloadBytes.toBase64()));
    query.addBindValue(parsedPayload);
    query.addBindValue(parsedFormat);
    query.addBindValue(parseError);
    query.addBindValue(scriptId);
    query.addBindValue(scriptName);

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return 0;
    }
    return query.lastInsertId().toLongLong();
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
            "parsed_payload, parsed_format, parse_error, script_id, script_name "
            "FROM ("
            "    SELECT id, timestamp, topic, payload, payload_b64, "
            "    parsed_payload, parsed_format, parse_error, script_id, script_name "
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
            "parsed_payload, parsed_format, parse_error, script_id, script_name "
            "FROM ("
            "    SELECT id, timestamp, topic, payload, payload_b64, "
            "    parsed_payload, parsed_format, parse_error, script_id, script_name "
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
    if (!resetLegacySchema()) {
        return false;
    }

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
                "script_name TEXT NOT NULL DEFAULT '')"))) {
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

bool HistoryStore::resetLegacySchema()
{
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("SELECT name FROM sqlite_master WHERE type = 'table' AND name = 'messages'"))) {
        m_lastError = query.lastError().text();
        return false;
    }

    if (!query.next()) {
        return true;
    }

    if (!query.exec(QStringLiteral("DROP TABLE IF EXISTS messages"))) {
        m_lastError = query.lastError().text();
        return false;
    }
    if (!query.exec(QStringLiteral("DROP INDEX IF EXISTS idx_messages_session_id_id"))) {
        m_lastError = query.lastError().text();
        return false;
    }
    return true;
}
