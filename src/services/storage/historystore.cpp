#include "historystore.h"

#include <QDir>
#include <QCborParserError>
#include <QCborValue>
#include <QHash>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

#include <algorithm>

namespace {
constexpr int kHistorySchemaVersion = 3;

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
        QStringLiteral("display_payload"),
        QStringLiteral("display_format"),
        QStringLiteral("display_error"),
        QStringLiteral("display_state"),
        QStringLiteral("processor_id"),
        QStringLiteral("processor_revision_id"),
        QStringLiteral("processor_name"),
        QStringLiteral("processor_language_id"),
        QStringLiteral("processor_runtime_id"),
        QStringLiteral("processor_content_hash"),
        QStringLiteral("processor_result_cbor"),
        QStringLiteral("processor_result_preview"),
        QStringLiteral("processor_execution_state"),
        QStringLiteral("processor_execution_error_code"),
        QStringLiteral("processor_execution_error"),
        QStringLiteral("processor_execution_duration_us"),
        QStringLiteral("payload_bytes"),
        QStringLiteral("payload_size"),
        QStringLiteral("payload_state"),
        QStringLiteral("payload_preview"),
        QStringLiteral("payload_hash"),
        QStringLiteral("payload_format"),
        QStringLiteral("publish_properties_cbor"),
    };
}

MessageRecord messageRecordFromQuery(const QSqlQuery &query, const QString &sessionId)
{
    MessageRecord row;
    row.id = query.value(0).toLongLong();
    row.sessionId = sessionId;
    row.timestamp = query.value(1).toString();
    row.direction = query.value(2).toString() == QStringLiteral("outgoing")
        ? MessageDirection::Outgoing
        : MessageDirection::Incoming;
    row.topic = query.value(3).toString();
    row.qos = query.value(4).toInt();
    row.retain = query.value(5).toBool();
    row.retainKnown = query.value(6).toBool();
    row.displayPayload = query.value(7).toString();
    row.displayFormat = query.value(8).toString();
    row.displayError = query.value(9).toString();
    row.displayState = query.value(10).toString();
    row.processorId = query.value(11).toString();
    row.processorRevisionId = query.value(12).toString();
    row.processorName = query.value(13).toString();
    row.processorLanguageId = query.value(14).toString();
    row.processorRuntimeId = query.value(15).toString();
    row.processorContentHash = query.value(16).toString();
    row.processorResultCbor = query.value(17).toByteArray();
    row.processorResultPreview = query.value(18).toString();
    row.processorExecutionState = query.value(19).toString();
    row.processorExecutionErrorCode = query.value(20).toString();
    row.processorExecutionError = query.value(21).toString();
    row.processorExecutionDurationUs = query.value(22).toLongLong();
    row.payloadBytes = query.value(23).toByteArray();
    row.payloadSize = query.value(24).toLongLong();
    row.payloadState = query.value(25).toString();
    row.payloadPreview = query.value(26).toString();
    row.payloadHash = query.value(27).toString();
    row.payloadFormat = query.value(28).toInt();
    QCborParserError propertiesError;
    const QCborValue properties = QCborValue::fromCbor(query.value(29).toByteArray(), &propertiesError);
    if (propertiesError.error == QCborError::NoError && properties.isMap()) {
        row.publishProperties = mqttPublishPropertiesFromCbor(properties.toMap());
    }
    return row;
}

bool resetIncompatibleMessageTable(QSqlDatabase &db, QString &error)
{
    QSqlQuery tableQuery(db);
    if (!tableQuery.exec(QStringLiteral(
            "SELECT 1 FROM sqlite_master "
            "WHERE type = 'table' AND name = 'mqtt_messages'"))) {
        error = tableQuery.lastError().text();
        return false;
    }
    if (!tableQuery.next()) {
        return true;
    }

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
        error = QStringLiteral("Cannot inspect the existing mqtt_messages schema.");
        return false;
    }

    QSqlQuery versionQuery(db);
    if (!versionQuery.exec(QStringLiteral("PRAGMA user_version"))
        || !versionQuery.next()) {
        error = versionQuery.lastError().text();
        return false;
    }

    const QStringList requiredColumns = requiredMessageColumns();
    if (!columns.contains(QStringLiteral("publish_properties_cbor"))) {
        bool legacySchemaComplete = true;
        for (const QString &column : requiredColumns) {
            if (column != QStringLiteral("publish_properties_cbor")
                && !columns.contains(column)) {
                legacySchemaComplete = false;
                break;
            }
        }
        if (legacySchemaComplete) {
            QSqlQuery migrationQuery(db);
            if (!migrationQuery.exec(QStringLiteral(
                    "ALTER TABLE mqtt_messages ADD COLUMN publish_properties_cbor BLOB"))
                || !migrationQuery.exec(QStringLiteral("PRAGMA user_version = 3"))) {
                error = migrationQuery.lastError().text();
                return false;
            }
            return true;
        }
    }
    bool isCompatible = true;
    for (const QString &column : requiredColumns) {
        if (!columns.contains(column)) {
            isCompatible = false;
            break;
        }
    }
    if (isCompatible) {
        if (versionQuery.value(0).toInt() < kHistorySchemaVersion) {
            QSqlQuery migrationQuery(db);
            if (!migrationQuery.exec(QStringLiteral("PRAGMA user_version = 3"))) {
                error = migrationQuery.lastError().text();
                return false;
            }
        }
        return true;
    }

    QSqlQuery totalsQuery(db);
    if (!totalsQuery.exec(QStringLiteral(
            "SELECT 1 FROM sqlite_master "
            "WHERE type = 'table' AND name = 'mqtt_message_totals'"))) {
        error = totalsQuery.lastError().text();
        return false;
    }
    const bool totalsTableExists = totalsQuery.next();
    tableQuery.finish();
    infoQuery.finish();
    versionQuery.finish();
    totalsQuery.finish();

    if (!db.transaction()) {
        error = db.lastError().text();
        return false;
    }

    QSqlQuery resetQuery(db);
    if (!resetQuery.exec(QStringLiteral("DROP TABLE mqtt_messages"))) {
        error = resetQuery.lastError().text();
        db.rollback();
        return false;
    }
    if (totalsTableExists
        && !resetQuery.exec(QStringLiteral("DELETE FROM mqtt_message_totals"))) {
        error = resetQuery.lastError().text();
        db.rollback();
        return false;
    }

    if (!db.commit()) {
        error = db.lastError().text();
        db.rollback();
        return false;
    }
    return true;
}

} // namespace

HistoryStore::HistoryStore()
{
    m_connectionName = QStringLiteral("history-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    initialize(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation), 5000);
}

HistoryStore::HistoryStore(const QString &dataPath)
    : HistoryStore(dataPath, 5000)
{
}

HistoryStore::HistoryStore(const QString &dataPath, int busyTimeoutMs)
{
    m_connectionName = QStringLiteral("history-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    initialize(dataPath, busyTimeoutMs);
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

QString HistoryStore::dataPath() const
{
    return m_dataPath;
}

QString HistoryStore::journalMode() const
{
    if (!isReady()) {
        return {};
    }
    QSqlQuery query(m_db);
    return query.exec(QStringLiteral("PRAGMA journal_mode")) && query.next()
        ? query.value(0).toString()
        : QString();
}

int HistoryStore::busyTimeoutMs() const
{
    if (!isReady()) {
        return 0;
    }
    QSqlQuery query(m_db);
    return query.exec(QStringLiteral("PRAGMA busy_timeout")) && query.next()
        ? query.value(0).toInt()
        : 0;
}

qint64 HistoryStore::nextMessageId()
{
    if (!isReady()) {
        if (m_lastError.isEmpty()) {
            m_lastError = QStringLiteral("History database is not open.");
        }
        return 0;
    }

    QSqlQuery query(m_db);
    if (!query.exec(
            QStringLiteral(
                "SELECT COALESCE("
                "    (SELECT seq FROM sqlite_sequence WHERE name = 'mqtt_messages'), "
                "    COALESCE(MAX(id), 0)"
                ") + 1 "
                "FROM mqtt_messages"))
        || !query.next()) {
        m_lastError = query.lastError().text();
        return 0;
    }

    m_lastError.clear();
    return query.value(0).toLongLong();
}

HistoryWriteResult HistoryStore::appendMessages(const QVector<MessageRecord> &messages)
{
    return writeMessageBatch(messages, {});
}

HistoryWriteResult HistoryStore::writeMessageBatch(
    const QVector<MessageRecord> &messages,
    const QVector<ParseOutcome> &parseResults)
{
    HistoryWriteResult result;
    if (messages.isEmpty() && parseResults.isEmpty()) {
        result.ok = true;
        return result;
    }
    if (!isReady()) {
        if (m_lastError.isEmpty()) {
            m_lastError = QStringLiteral("History database is not open.");
        }
        result.error = m_lastError;
        return result;
    }

    QSqlQuery insertQuery(m_db);
    if (!messages.isEmpty() && !insertQuery.prepare(
            QStringLiteral(
                "INSERT INTO mqtt_messages("
                "id, session_id, timestamp, direction, topic, qos, retain, retain_known, "
                "display_payload, display_format, display_error, display_state, "
                "processor_id, processor_revision_id, processor_name, processor_language_id, "
                "processor_runtime_id, processor_content_hash, processor_result_cbor, "
                "processor_result_preview, processor_execution_state, "
                "processor_execution_error_code, processor_execution_error, "
                "processor_execution_duration_us, payload_bytes, payload_size, payload_state, "
                "payload_preview, payload_hash, payload_format, publish_properties_cbor) "
                "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"))) {
        m_lastError = insertQuery.lastError().text();
        result.error = m_lastError;
        return result;
    }

    QSqlQuery updateQuery(m_db);
    if (!parseResults.isEmpty() && !updateQuery.prepare(QStringLiteral(
            "UPDATE mqtt_messages SET "
            "display_payload = ?, display_format = ?, display_error = ?, display_state = ?, "
            "processor_id = ?, processor_revision_id = ?, processor_name = ?, "
            "processor_language_id = ?, processor_runtime_id = ?, processor_content_hash = ?, "
            "processor_result_cbor = ?, processor_result_preview = ?, processor_execution_state = ?, "
            "processor_execution_error_code = ?, processor_execution_error = ?, "
            "processor_execution_duration_us = ? "
            "WHERE id = ?"))) {
        m_lastError = updateQuery.lastError().text();
        result.error = m_lastError;
        return result;
    }

    if (!m_db.transaction()) {
        m_lastError = m_db.lastError().text();
        result.error = m_lastError;
        return result;
    }

    QHash<QString, qint64> appendedCounts;
    for (MessageRecord message : messages) {
        if (message.id <= 0) {
            m_lastError = QStringLiteral("Message history ID must be positive.");
            m_db.rollback();
            result.error = m_lastError;
            return result;
        }
        if (message.payloadState.isEmpty()) {
            message.payloadState = QStringLiteral("full");
        }
        if (message.payloadSize < 0) {
            message.payloadSize = message.payloadBytes.size();
        }

        if (message.displayState.isEmpty()) {
            message.displayState = QStringLiteral("not_required");
        }
        if (message.processorExecutionState.isEmpty()) {
            message.processorExecutionState = QStringLiteral("not_required");
        }

        insertQuery.bindValue(0, message.id);
        insertQuery.bindValue(1, message.sessionId);
        insertQuery.bindValue(2, message.timestamp);
        insertQuery.bindValue(3, messageDirectionName(message.direction));
        insertQuery.bindValue(4, message.topic);
        insertQuery.bindValue(5, message.qos);
        insertQuery.bindValue(6, message.retain);
        insertQuery.bindValue(7, message.retainKnown);
        insertQuery.bindValue(8, nonNullString(message.displayPayload));
        insertQuery.bindValue(9, nonNullString(message.displayFormat));
        insertQuery.bindValue(10, nonNullString(message.displayError));
        insertQuery.bindValue(11, nonNullString(message.displayState));
        insertQuery.bindValue(12, nonNullString(message.processorId));
        insertQuery.bindValue(13, nonNullString(message.processorRevisionId));
        insertQuery.bindValue(14, nonNullString(message.processorName));
        insertQuery.bindValue(15, nonNullString(message.processorLanguageId));
        insertQuery.bindValue(16, nonNullString(message.processorRuntimeId));
        insertQuery.bindValue(17, nonNullString(message.processorContentHash));
        insertQuery.bindValue(18, message.processorResultCbor);
        insertQuery.bindValue(19, nonNullString(message.processorResultPreview));
        insertQuery.bindValue(20, nonNullString(message.processorExecutionState));
        insertQuery.bindValue(21, nonNullString(message.processorExecutionErrorCode));
        insertQuery.bindValue(22, nonNullString(message.processorExecutionError));
        insertQuery.bindValue(23, message.processorExecutionDurationUs);
        insertQuery.bindValue(24, message.payloadBytes);
        insertQuery.bindValue(25, message.payloadSize);
        insertQuery.bindValue(26, nonNullString(message.payloadState));
        insertQuery.bindValue(27, nonNullString(message.payloadPreview));
        insertQuery.bindValue(28, nonNullString(message.payloadHash));
        insertQuery.bindValue(29, message.payloadFormat);
        insertQuery.bindValue(
            30,
            QCborValue(mqttPublishPropertiesToCbor(message.publishProperties)).toCbor());
        if (!insertQuery.exec()) {
            m_lastError = insertQuery.lastError().text();
            m_db.rollback();
            result.error = m_lastError;
            return result;
        }

        ++appendedCounts[message.sessionId];
        if (!result.sessionIds.contains(message.sessionId)) {
            result.sessionIds.append(message.sessionId);
        }
    }

    for (const ParseOutcome &parseResult : parseResults) {
        if (parseResult.messageId <= 0) {
            m_lastError = QStringLiteral("Message parse update ID must be positive.");
            m_db.rollback();
            result.error = m_lastError;
            return result;
        }
        updateQuery.bindValue(0, nonNullString(parseResult.displayPayload));
        updateQuery.bindValue(1, nonNullString(parseResult.displayFormat));
        updateQuery.bindValue(2, nonNullString(parseResult.displayError));
        updateQuery.bindValue(3, messageParseStateName(parseResult.state));
        updateQuery.bindValue(4, nonNullString(parseResult.processorId));
        updateQuery.bindValue(5, nonNullString(parseResult.processorRevisionId));
        updateQuery.bindValue(6, nonNullString(parseResult.processorName));
        updateQuery.bindValue(7, nonNullString(parseResult.processorLanguageId));
        updateQuery.bindValue(8, nonNullString(parseResult.processorRuntimeId));
        updateQuery.bindValue(9, nonNullString(parseResult.processorContentHash));
        updateQuery.bindValue(10, parseResult.processorResultCbor);
        updateQuery.bindValue(11, nonNullString(parseResult.processorResultPreview));
        updateQuery.bindValue(12, nonNullString(parseResult.processorExecutionState));
        updateQuery.bindValue(13, nonNullString(parseResult.processorExecutionErrorCode));
        updateQuery.bindValue(14, nonNullString(parseResult.processorExecutionError));
        updateQuery.bindValue(15, parseResult.processorExecutionDurationUs);
        updateQuery.bindValue(16, parseResult.messageId);
        if (!updateQuery.exec() || updateQuery.numRowsAffected() != 1) {
            m_lastError = updateQuery.lastError().text();
            if (m_lastError.isEmpty()) {
                m_lastError = QStringLiteral("Message parse update target was not found: %1")
                                  .arg(parseResult.messageId);
            }
            m_db.rollback();
            result.error = m_lastError;
            return result;
        }
        if (!result.sessionIds.contains(parseResult.sessionId)) {
            result.sessionIds.append(parseResult.sessionId);
        }
    }

    QSqlQuery countQuery(m_db);
    if (!countQuery.prepare(QStringLiteral(
            "INSERT INTO mqtt_message_totals(session_id, total_count) VALUES(?, ?) "
            "ON CONFLICT(session_id) DO UPDATE SET total_count = total_count + excluded.total_count"))) {
        m_lastError = countQuery.lastError().text();
        m_db.rollback();
        result.error = m_lastError;
        return result;
    }
    for (auto it = appendedCounts.cbegin(); it != appendedCounts.cend(); ++it) {
        countQuery.bindValue(0, it.key());
        countQuery.bindValue(1, it.value());
        if (!countQuery.exec()) {
            m_lastError = countQuery.lastError().text();
            m_db.rollback();
            result.error = m_lastError;
            return result;
        }
    }

    if (!m_db.commit()) {
        m_lastError = m_db.lastError().text();
        m_db.rollback();
        result.error = m_lastError;
        return result;
    }

    m_lastError.clear();
    result.ok = true;
    result.messageCount = messages.size();
    return result;
}

qint64 HistoryStore::totalMessageCount(const QString &sessionId) const
{
    if (!isReady()) {
        return 0;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT total_count FROM mqtt_message_totals WHERE session_id = ?"));
    query.addBindValue(sessionId);
    if (query.exec() && query.next()) {
        return query.value(0).toLongLong();
    }
    return 0;
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

QVector<MessageRecord> HistoryStore::loadMessages(
    const QString &sessionId,
    int limit) const
{
    QVector<MessageRecord> result;
    if (!isReady()) {
        return result;
    }

    QSqlQuery query(m_db);
    query.prepare(
        QStringLiteral(
            "SELECT id, timestamp, direction, topic, qos, retain, retain_known, "
            "display_payload, display_format, display_error, display_state, "
            "processor_id, processor_revision_id, processor_name, processor_language_id, "
            "processor_runtime_id, processor_content_hash, processor_result_cbor, "
            "processor_result_preview, processor_execution_state, processor_execution_error_code, "
            "processor_execution_error, processor_execution_duration_us, payload_bytes, "
            "payload_size, payload_state, payload_preview, payload_hash, payload_format, publish_properties_cbor "
            "FROM ("
            "    SELECT id, timestamp, direction, topic, qos, retain, retain_known, "
            "    display_payload, display_format, display_error, display_state, "
            "    processor_id, processor_revision_id, processor_name, processor_language_id, "
            "    processor_runtime_id, processor_content_hash, NULL AS processor_result_cbor, "
            "    processor_result_preview, processor_execution_state, processor_execution_error_code, "
            "    processor_execution_error, processor_execution_duration_us, NULL AS payload_bytes, "
            "    payload_size, payload_state, payload_preview, payload_hash, payload_format, publish_properties_cbor "
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
        result.append(messageRecordFromQuery(query, sessionId));
    }

    return result;
}

QVector<MessageRecord> HistoryStore::loadMessagesBefore(
    const QString &sessionId,
    qint64 beforeId,
    int limit) const
{
    QVector<MessageRecord> result;
    if (!isReady() || beforeId <= 0) {
        return result;
    }

    QSqlQuery query(m_db);
    query.prepare(
        QStringLiteral(
            "SELECT id, timestamp, direction, topic, qos, retain, retain_known, "
            "display_payload, display_format, display_error, display_state, "
            "processor_id, processor_revision_id, processor_name, processor_language_id, "
            "processor_runtime_id, processor_content_hash, processor_result_cbor, "
            "processor_result_preview, processor_execution_state, processor_execution_error_code, "
            "processor_execution_error, processor_execution_duration_us, payload_bytes, "
            "payload_size, payload_state, payload_preview, payload_hash, payload_format, publish_properties_cbor "
            "FROM ("
            "    SELECT id, timestamp, direction, topic, qos, retain, retain_known, "
            "    display_payload, display_format, display_error, display_state, "
            "    processor_id, processor_revision_id, processor_name, processor_language_id, "
            "    processor_runtime_id, processor_content_hash, NULL AS processor_result_cbor, "
            "    processor_result_preview, processor_execution_state, processor_execution_error_code, "
            "    processor_execution_error, processor_execution_duration_us, NULL AS payload_bytes, "
            "    payload_size, payload_state, payload_preview, payload_hash, payload_format, publish_properties_cbor "
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
        result.append(messageRecordFromQuery(query, sessionId));
    }

    return result;
}

std::optional<MessageRecord> HistoryStore::loadMessage(qint64 messageId) const
{
    if (!isReady() || messageId <= 0) {
        return {};
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT id, timestamp, direction, topic, qos, retain, retain_known, "
        "display_payload, display_format, display_error, display_state, "
        "processor_id, processor_revision_id, processor_name, processor_language_id, "
        "processor_runtime_id, processor_content_hash, processor_result_cbor, "
        "processor_result_preview, processor_execution_state, processor_execution_error_code, "
        "processor_execution_error, processor_execution_duration_us, payload_bytes, "
        "payload_size, payload_state, payload_preview, payload_hash, payload_format, publish_properties_cbor, session_id "
        "FROM mqtt_messages WHERE id = ?"));
    query.addBindValue(messageId);
    if (!query.exec() || !query.next()) {
        return {};
    }
    return messageRecordFromQuery(query, query.value(30).toString());
}

QByteArray HistoryStore::loadMessagePayloadBytes(qint64 messageId) const
{
    if (!isReady() || messageId <= 0) {
        return {};
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

bool HistoryStore::clearMessages(const QString &sessionId)
{
    return executeDeletes(
        {
            QStringLiteral("DELETE FROM mqtt_messages WHERE session_id = ?"),
            QStringLiteral("DELETE FROM mqtt_message_totals WHERE session_id = ?"),
        },
        sessionId);
}

bool HistoryStore::clearLogs(const QString &sessionId)
{
    return executeDeletes(
        {QStringLiteral("DELETE FROM event_logs WHERE session_id = ?")},
        sessionId);
}

bool HistoryStore::clearAllMessages()
{
    return executeDeletes(
        {
            QStringLiteral("DELETE FROM mqtt_messages"),
            QStringLiteral("DELETE FROM mqtt_message_totals"),
        });
}

bool HistoryStore::clearAllLogs()
{
    return executeDeletes({QStringLiteral("DELETE FROM event_logs")});
}

bool HistoryStore::clearSessionHistory(const QString &sessionId)
{
    return executeDeletes(
        {
            QStringLiteral("DELETE FROM mqtt_messages WHERE session_id = ?"),
            QStringLiteral("DELETE FROM mqtt_message_totals WHERE session_id = ?"),
            QStringLiteral("DELETE FROM event_logs WHERE session_id = ?"),
        },
        sessionId);
}

bool HistoryStore::clearAllHistory()
{
    return executeDeletes(
        {
            QStringLiteral("DELETE FROM mqtt_messages"),
            QStringLiteral("DELETE FROM mqtt_message_totals"),
            QStringLiteral("DELETE FROM event_logs"),
        });
}

bool HistoryStore::executeDeletes(const QStringList &statements, const QString &sessionId)
{
    if (!isReady()) {
        if (m_lastError.isEmpty()) {
            m_lastError = QStringLiteral("History database is not open.");
        }
        return false;
    }

    if (!m_db.transaction()) {
        m_lastError = m_db.lastError().text();
        return false;
    }

    QSqlQuery query(m_db);
    for (const QString &statement : statements) {
        if (!query.prepare(statement)) {
            m_lastError = query.lastError().text();
            m_db.rollback();
            return false;
        }
        if (!sessionId.isNull()) {
            query.addBindValue(sessionId);
        }
        if (!query.exec()) {
            m_lastError = query.lastError().text();
            m_db.rollback();
            return false;
        }
    }

    if (!m_db.commit()) {
        m_lastError = m_db.lastError().text();
        m_db.rollback();
        return false;
    }

    m_lastError.clear();
    return true;
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

bool HistoryStore::initialize(const QString &dataPath, int busyTimeoutMs)
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
    m_dataPath = dir.absolutePath();

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(dbPath);
    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        return false;
    }

    QSqlQuery query(m_db);

    if (!query.exec(QStringLiteral("PRAGMA busy_timeout = %1").arg((std::max)(0, busyTimeoutMs)))) {
        m_lastError = query.lastError().text();
        return false;
    }

    if (!query.exec(QStringLiteral("PRAGMA journal_mode = WAL"))
        || !query.next()
        || query.value(0).toString().compare(QStringLiteral("wal"), Qt::CaseInsensitive) != 0) {
        m_lastError = query.lastError().text();
        if (m_lastError.isEmpty()) {
            m_lastError = QStringLiteral("Cannot enable SQLite WAL mode.");
        }
        return false;
    }

    if (!query.exec(QStringLiteral("PRAGMA synchronous = NORMAL"))) {
        m_lastError = query.lastError().text();
        return false;
    }

    if (!resetIncompatibleMessageTable(m_db, m_lastError)) {
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
                "display_payload TEXT NOT NULL DEFAULT '', "
                "display_format TEXT NOT NULL DEFAULT '', "
                "display_error TEXT NOT NULL DEFAULT '', "
                "display_state TEXT NOT NULL DEFAULT 'not_required', "
                "processor_id TEXT NOT NULL DEFAULT '', "
                "processor_revision_id TEXT NOT NULL DEFAULT '', "
                "processor_name TEXT NOT NULL DEFAULT '', "
                "processor_language_id TEXT NOT NULL DEFAULT '', "
                "processor_runtime_id TEXT NOT NULL DEFAULT '', "
                "processor_content_hash TEXT NOT NULL DEFAULT '', "
                "processor_result_cbor BLOB, "
                "processor_result_preview TEXT NOT NULL DEFAULT '', "
                "processor_execution_state TEXT NOT NULL DEFAULT 'not_required', "
                "processor_execution_error_code TEXT NOT NULL DEFAULT '', "
                "processor_execution_error TEXT NOT NULL DEFAULT '', "
                "processor_execution_duration_us INTEGER NOT NULL DEFAULT 0, "
                "payload_bytes BLOB, "
                "payload_size INTEGER NOT NULL DEFAULT 0, "
                "payload_state TEXT NOT NULL DEFAULT 'full', "
                "payload_preview TEXT NOT NULL DEFAULT '', "
                "payload_hash TEXT NOT NULL DEFAULT '', "
                "payload_format INTEGER NOT NULL DEFAULT -1, "
                "publish_properties_cbor BLOB)"))) {
        m_lastError = query.lastError().text();
        return false;
    }

    if (!query.exec(QStringLiteral("PRAGMA user_version"))
        || !query.next()) {
        m_lastError = query.lastError().text();
        return false;
    }
    if (query.value(0).toInt() < kHistorySchemaVersion
        && !query.exec(QStringLiteral("PRAGMA user_version = %1")
                .arg(kHistorySchemaVersion))) {
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
