#include "services/storage/historystore.h"
#include "domain/messagerecord.h"

#include <QtTest/QtTest>

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>

namespace {
qint64 appendMessage(HistoryStore &store, MessageRecord record)
{
    record.id = store.nextMessageId();
    if (record.id <= 0) {
        return 0;
    }
    const HistoryWriteResult result = store.appendMessages({record});
    return result.ok ? record.id : 0;
}

bool appendMessages(HistoryStore &store, QVector<MessageRecord> records)
{
    qint64 nextId = store.nextMessageId();
    if (nextId <= 0) {
        return false;
    }
    for (MessageRecord &record : records) {
        record.id = nextId++;
    }
    return store.appendMessages(records).ok;
}
} // namespace

class HistoryStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void writesRawPayloadWithProcessorSchema();
    void resetsOnlyMessageTableWhenSchemaIsStale();
    void roundTripsProcessorIdentityAndResult();
    void loadMessagesExcludePayloadBytes();
    void loadsMessageByExplicitId();
    void roundTripsCanonicalOutgoingMessage();
    void countsPersistedMessagesPerSession();
    void keepsTotalMessageCountAcrossPruneAndReopen();
    void backfillsTotalMessageCountForExistingHistory();
    void clearMessagesRollsBackWhenTotalDeleteFails();
    void clearAllHistoryRollsBackWhenLogDeleteFails();
};

void HistoryStoreTest::writesRawPayloadWithProcessorSchema()
{
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());

    HistoryStore store(dataDir.path());
    QVERIFY2(store.isReady(), qPrintable(store.lastError()));

    const QString sessionId = QStringLiteral("test-session-%1")
                                  .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QByteArray payload;
    payload.append('\0');
    payload.append(char(0xff));
    payload.append("raw");

    MessageRecord record;
    record.sessionId = sessionId;
    record.timestamp = QStringLiteral("2026-07-02 15:02:20.304");
    record.topic = QStringLiteral("111111/ros_to_android");
    record.payloadBytes = payload;
    record.processorResultCbor = QByteArray::fromHex("a16576616c756501");
    record.payloadPreview = QStringLiteral("raw preview");
    record.payloadSize = payload.size();
    record.payloadHash = QStringLiteral("hash");
    const qint64 reservedId = appendMessage(store, record);

    QVERIFY(reservedId > 0);
    QVERIFY2(store.lastError().isEmpty(), qPrintable(store.lastError()));

    const QVariantList rows = store.loadMessages(sessionId, 10);
    QCOMPARE(rows.size(), 1);

    const QVariantMap row = rows.first().toMap();
    QVERIFY(!row.contains(QStringLiteral("payload")));
    QVERIFY(row.value(QStringLiteral("payload_bytes")).toByteArray().isEmpty());
    QVERIFY(row.value(QStringLiteral("processor_result_cbor")).toByteArray().isEmpty());
    QCOMPARE(row.value(QStringLiteral("payload_size")).toLongLong(), qint64(payload.size()));
    QCOMPARE(store.loadMessagePayloadBytes(reservedId), payload);
    QCOMPARE(
        store.loadMessage(reservedId).value(QStringLiteral("processor_result_cbor")).toByteArray(),
        record.processorResultCbor);
}

void HistoryStoreTest::resetsOnlyMessageTableWhenSchemaIsStale()
{
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());

    const QString connectionName = QStringLiteral("stale-message-schema-%1")
                                       .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(dataDir.filePath(QStringLiteral("history.db")));
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));

        QSqlQuery query(db);
        QVERIFY2(query.exec(QStringLiteral(
                     "CREATE TABLE mqtt_messages ("
                     "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                     "session_id TEXT NOT NULL, "
                     "timestamp TEXT NOT NULL, "
                     "direction TEXT NOT NULL, topic TEXT NOT NULL, "
                     "qos INTEGER NOT NULL DEFAULT -1, retain INTEGER NOT NULL DEFAULT 0, "
                     "retain_known INTEGER NOT NULL DEFAULT 0, "
                     "parsed_payload TEXT NOT NULL DEFAULT '', parsed_format TEXT NOT NULL DEFAULT '', "
                     "parse_error TEXT NOT NULL DEFAULT '', script_id TEXT NOT NULL DEFAULT '', "
                     "script_name TEXT NOT NULL DEFAULT '', payload_bytes BLOB, "
                     "payload_size INTEGER NOT NULL DEFAULT 0, payload_state TEXT NOT NULL DEFAULT 'full', "
                     "payload_preview TEXT NOT NULL DEFAULT '', payload_hash TEXT NOT NULL DEFAULT '', "
                     "payload_format INTEGER NOT NULL DEFAULT -1, "
                     "parse_state TEXT NOT NULL DEFAULT 'not_required')")),
            qPrintable(query.lastError().text()));
        QVERIFY2(query.exec(QStringLiteral(
                     "INSERT INTO mqtt_messages(session_id, timestamp, direction, topic, script_id) "
                     "VALUES('stale-session', '2026-07-02T15:02:20.304Z', "
                     "'incoming', 'devices/stale', 'legacy-script')")),
            qPrintable(query.lastError().text()));

        QVERIFY2(query.exec(QStringLiteral(
                     "CREATE TABLE mqtt_message_totals ("
                     "session_id TEXT PRIMARY KEY, total_count INTEGER NOT NULL DEFAULT 0)")),
            qPrintable(query.lastError().text()));
        QVERIFY2(query.exec(QStringLiteral(
                     "INSERT INTO mqtt_message_totals(session_id, total_count) "
                     "VALUES('stale-session', 17)")),
            qPrintable(query.lastError().text()));

        QVERIFY2(query.exec(QStringLiteral(
                     "CREATE TABLE event_logs ("
                     "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                     "session_id TEXT NOT NULL, "
                     "timestamp TEXT NOT NULL, "
                     "channel TEXT NOT NULL, "
                     "message TEXT NOT NULL DEFAULT '')")),
            qPrintable(query.lastError().text()));
        QVERIFY2(query.exec(QStringLiteral(
                     "INSERT INTO event_logs(session_id, timestamp, channel, message) "
                     "VALUES('stale-session', '2026-07-02T15:02:21.000Z', 'Network', 'kept log')")),
            qPrintable(query.lastError().text()));
    }
    QSqlDatabase::removeDatabase(connectionName);

    HistoryStore store(dataDir.path());
    QVERIFY2(store.isReady(), qPrintable(store.lastError()));
    QVERIFY(store.loadMessages(QStringLiteral("stale-session"), 10).isEmpty());
    QCOMPARE(store.totalMessageCount(QStringLiteral("stale-session")), 0);

    const QVariantList logs = store.loadLogs(QStringLiteral("stale-session"), 10);
    QCOMPARE(logs.size(), 1);
    QCOMPARE(logs.first().toMap().value(QStringLiteral("payload")).toString(), QStringLiteral("kept log"));

    const QString newSessionId = QStringLiteral("new-session");
    MessageRecord record;
    record.sessionId = newSessionId;
    record.timestamp = QStringLiteral("2026-07-02T15:02:22.000Z");
    record.topic = QStringLiteral("devices/current");
    record.payloadBytes = QByteArrayLiteral("current");
    record.payloadPreview = QStringLiteral("current");
    record.payloadSize = 7;
    record.payloadHash = QStringLiteral("hash");
    const qint64 reservedId = appendMessage(store, record);

    QVERIFY(reservedId > 0);
    QCOMPARE(store.loadMessages(newSessionId, 10).size(), 1);
}

void HistoryStoreTest::roundTripsProcessorIdentityAndResult()
{
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());

    HistoryStore store(dataDir.path());
    QVERIFY2(store.isReady(), qPrintable(store.lastError()));

    MessageRecord record;
    record.id = store.nextMessageId();
    record.sessionId = QStringLiteral("session-1");
    record.timestamp = QStringLiteral("2026-08-05T18:00:00.000Z");
    record.topic = QStringLiteral("devices/processor");
    record.payloadBytes = QByteArrayLiteral("payload");
    record.payloadSize = record.payloadBytes.size();
    record.processorId = QStringLiteral("processor-1");
    record.processorRevisionId = QStringLiteral("revision-2");
    record.processorName = QStringLiteral("Temperature Decoder");
    record.processorLanguageId = QStringLiteral("javascript");
    record.processorRuntimeId = QStringLiteral("qt-qjsengine");
    record.processorContentHash = QStringLiteral("sha256:abc");
    record.processorExecutionState = QStringLiteral("pending");
    record.displayState = QStringLiteral("pending");
    QVERIFY(store.appendMessages({record}).ok);

    const QVariantMap captured = store.loadMessage(record.id);
    QCOMPARE(captured.value(QStringLiteral("processor_id")).toString(), record.processorId);
    QCOMPARE(captured.value(QStringLiteral("processor_revision_id")).toString(), record.processorRevisionId);
    QCOMPARE(captured.value(QStringLiteral("processor_execution_state")).toString(), QStringLiteral("pending"));

    MessageParseResult parseResult;
    parseResult.messageId = record.id;
    parseResult.sessionId = record.sessionId;
    parseResult.state = MessageParseState::Succeeded;
    parseResult.displayPayload = QStringLiteral("{\"value\":42}");
    parseResult.displayFormat = QStringLiteral("JavaScript: Temperature Decoder");
    parseResult.processorId = record.processorId;
    parseResult.processorRevisionId = record.processorRevisionId;
    parseResult.processorName = record.processorName;
    parseResult.processorLanguageId = record.processorLanguageId;
    parseResult.processorRuntimeId = record.processorRuntimeId;
    parseResult.processorContentHash = record.processorContentHash;
    parseResult.processorResultCbor = QByteArray::fromHex("a16576616c7565182a");
    parseResult.processorResultPreview = parseResult.displayPayload;
    parseResult.processorExecutionState = QStringLiteral("succeeded");
    parseResult.processorExecutionDurationUs = 375;
    QVERIFY(store.writeMessageBatch({}, {parseResult}).ok);

    const QVariantMap completed = store.loadMessage(record.id);
    QCOMPARE(completed.value(QStringLiteral("display_state")).toString(), QStringLiteral("succeeded"));
    QCOMPARE(completed.value(QStringLiteral("display_payload")).toString(), parseResult.displayPayload);
    QCOMPARE(completed.value(QStringLiteral("processor_result_cbor")).toByteArray(), parseResult.processorResultCbor);
    QCOMPARE(completed.value(QStringLiteral("processor_result_preview")).toString(), parseResult.processorResultPreview);
    QCOMPARE(completed.value(QStringLiteral("processor_execution_state")).toString(), QStringLiteral("succeeded"));
    QCOMPARE(completed.value(QStringLiteral("processor_execution_duration_us")).toLongLong(), qint64(375));
}

void HistoryStoreTest::loadMessagesExcludePayloadBytes()
{
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());

    HistoryStore store(dataDir.path());
    QVERIFY2(store.isReady(), qPrintable(store.lastError()));

    const QString sessionId = QStringLiteral("test-session-%1")
                                  .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QByteArray payload(1024 * 32, 'x');

    MessageRecord record;
    record.sessionId = sessionId;
    record.timestamp = QStringLiteral("2026-07-02T15:02:20.304Z");
    record.topic = QStringLiteral("devices/large");
    record.payloadBytes = payload;
    record.payloadPreview = QStringLiteral("preview only");
    record.payloadSize = payload.size();
    record.payloadHash = QStringLiteral("hash");
    const qint64 reservedId = appendMessage(store, record);

    QVERIFY(reservedId > 0);

    const QVariantList rows = store.loadMessages(sessionId, 10);
    QCOMPARE(rows.size(), 1);

    const QVariantMap row = rows.first().toMap();
    QCOMPARE(row.value(QStringLiteral("payload_preview")).toString(), QStringLiteral("preview only"));
    QCOMPARE(row.value(QStringLiteral("payload_state")).toString(), QStringLiteral("full"));
    QCOMPARE(row.value(QStringLiteral("payload_size")).toLongLong(), qint64(payload.size()));
    QVERIFY(row.value(QStringLiteral("payload_bytes")).toByteArray().isEmpty());
    QCOMPARE(store.loadMessagePayloadBytes(reservedId), payload);
}

void HistoryStoreTest::loadsMessageByExplicitId()
{
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());

    HistoryStore store(dataDir.path());
    QVERIFY2(store.isReady(), qPrintable(store.lastError()));

    MessageRecord record;
    record.sessionId = QStringLiteral("session-1");
    record.timestamp = QStringLiteral("2026-07-15T14:00:00.000");
    record.direction = MessageDirection::Incoming;
    record.topic = QStringLiteral("devices/pending");
    record.payloadBytes = QByteArrayLiteral("full pending payload");
    record.payloadSize = record.payloadBytes.size();
    record.payloadPreview = QStringLiteral("preview");
    record.payloadFormat = 0;

    const qint64 id = appendMessage(store, record);
    QVERIFY(id > 0);

    const QVariantMap loaded = store.loadMessage(id);
    QCOMPARE(loaded.value(QStringLiteral("id")).toLongLong(), id);
    QCOMPARE(loaded.value(QStringLiteral("topic")).toString(), record.topic);
    QCOMPARE(loaded.value(QStringLiteral("payload_bytes")).toByteArray(), record.payloadBytes);
    QCOMPARE(store.loadMessagePayloadBytes(id), record.payloadBytes);
}

void HistoryStoreTest::countsPersistedMessagesPerSession()
{
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());

    HistoryStore store(dataDir.path());
    QVERIFY2(store.isReady(), qPrintable(store.lastError()));

    MessageRecord first;
    first.sessionId = QStringLiteral("session-1");
    first.timestamp = QStringLiteral("2026-07-15T16:00:00.000");
    first.topic = QStringLiteral("devices/one");
    first.payloadPreview = QStringLiteral("one");
    first.payloadState = QStringLiteral("full");

    MessageRecord second = first;
    second.topic = QStringLiteral("devices/two");
    MessageRecord other = first;
    other.sessionId = QStringLiteral("session-2");

    QVERIFY(appendMessages(store, {first, second, other}));
    QCOMPARE(store.totalMessageCount(first.sessionId), 2);
    QCOMPARE(store.totalMessageCount(other.sessionId), 1);

    store.clearMessages(first.sessionId);
    QCOMPARE(store.totalMessageCount(first.sessionId), 0);
    QCOMPARE(store.totalMessageCount(other.sessionId), 1);
}

void HistoryStoreTest::keepsTotalMessageCountAcrossPruneAndReopen()
{
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());
    const QString sessionId = QStringLiteral("session-1");

    {
        HistoryStore store(dataDir.path());
        QVERIFY2(store.isReady(), qPrintable(store.lastError()));

        QVector<MessageRecord> records;
        for (int index = 0; index < 3; ++index) {
            MessageRecord record;
            record.sessionId = sessionId;
            record.timestamp = QStringLiteral("2026-07-15T18:00:0%1.000").arg(index);
            record.topic = QStringLiteral("devices/%1").arg(index);
            record.payloadPreview = QString::number(index);
            record.payloadState = QStringLiteral("full");
            records.append(record);
        }
        QVERIFY(appendMessages(store, records));
        QCOMPARE(store.totalMessageCount(sessionId), 3);
        store.pruneMessages(sessionId, 1);
        QCOMPARE(store.loadMessages(sessionId, 10).size(), 1);
        QCOMPARE(store.totalMessageCount(sessionId), 3);
    }

    HistoryStore reopened(dataDir.path());
    QVERIFY2(reopened.isReady(), qPrintable(reopened.lastError()));
    QCOMPARE(reopened.totalMessageCount(sessionId), 3);
    reopened.clearMessages(sessionId);
    QCOMPARE(reopened.totalMessageCount(sessionId), 0);
}

void HistoryStoreTest::backfillsTotalMessageCountForExistingHistory()
{
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());
    const QString sessionId = QStringLiteral("session-1");

    {
        HistoryStore store(dataDir.path());
        QVERIFY2(store.isReady(), qPrintable(store.lastError()));
        QVector<MessageRecord> records;
        for (int index = 0; index < 2; ++index) {
            MessageRecord record;
            record.sessionId = sessionId;
            record.timestamp = QStringLiteral("2026-07-15T18:30:0%1.000").arg(index);
            record.topic = QStringLiteral("devices/%1").arg(index);
            record.payloadPreview = QString::number(index);
            record.payloadState = QStringLiteral("full");
            records.append(record);
        }
        QVERIFY(appendMessages(store, records));
    }

    const QString connectionName = QStringLiteral("drop-message-total-%1")
                                       .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(dataDir.filePath(QStringLiteral("history.db")));
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QSqlQuery query(db);
        QVERIFY2(query.exec(QStringLiteral("DROP TABLE mqtt_message_totals")), qPrintable(query.lastError().text()));
    }
    QSqlDatabase::removeDatabase(connectionName);

    HistoryStore migrated(dataDir.path());
    QVERIFY2(migrated.isReady(), qPrintable(migrated.lastError()));
    QCOMPARE(migrated.totalMessageCount(sessionId), 2);
}

void HistoryStoreTest::clearMessagesRollsBackWhenTotalDeleteFails()
{
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());

    HistoryStore store(dataDir.path());
    QVERIFY2(store.isReady(), qPrintable(store.lastError()));

    MessageRecord record;
    record.sessionId = QStringLiteral("session-1");
    record.timestamp = QStringLiteral("2026-07-25T10:00:00.000");
    record.topic = QStringLiteral("devices/one");
    record.payloadPreview = QStringLiteral("one");
    record.payloadState = QStringLiteral("full");
    QVERIFY(appendMessage(store, record) > 0);

    const QString connectionName = QStringLiteral("fail-total-delete-%1")
                                       .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(dataDir.filePath(QStringLiteral("history.db")));
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QSqlQuery query(db);
        QVERIFY2(query.exec(QStringLiteral(
                     "CREATE TRIGGER fail_message_total_delete "
                     "BEFORE DELETE ON mqtt_message_totals "
                     "BEGIN SELECT RAISE(ABORT, 'forced delete failure'); END")),
            qPrintable(query.lastError().text()));
    }
    QSqlDatabase::removeDatabase(connectionName);

    QVERIFY(!store.clearMessages(record.sessionId));
    QVERIFY(store.lastError().contains(QStringLiteral("forced delete failure")));
    QCOMPARE(store.loadMessages(record.sessionId, 10).size(), 1);
    QCOMPARE(store.totalMessageCount(record.sessionId), 1);
}

void HistoryStoreTest::clearAllHistoryRollsBackWhenLogDeleteFails()
{
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());

    HistoryStore store(dataDir.path());
    QVERIFY2(store.isReady(), qPrintable(store.lastError()));

    MessageRecord record;
    record.sessionId = QStringLiteral("session-1");
    record.timestamp = QStringLiteral("2026-07-25T10:00:00.000");
    record.topic = QStringLiteral("devices/one");
    record.payloadPreview = QStringLiteral("one");
    record.payloadState = QStringLiteral("full");
    QVERIFY(appendMessage(store, record) > 0);
    QVERIFY(store.appendEvent(
                record.sessionId,
                QStringLiteral("2026-07-25T10:00:01.000"),
                QStringLiteral("MQTT"),
                QStringLiteral("kept log"))
        > 0);

    const QString connectionName = QStringLiteral("fail-log-delete-%1")
                                       .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(dataDir.filePath(QStringLiteral("history.db")));
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QSqlQuery query(db);
        QVERIFY2(query.exec(QStringLiteral(
                     "CREATE TRIGGER fail_log_delete "
                     "BEFORE DELETE ON event_logs "
                     "BEGIN SELECT RAISE(ABORT, 'forced delete failure'); END")),
            qPrintable(query.lastError().text()));
    }
    QSqlDatabase::removeDatabase(connectionName);

    QVERIFY(!store.clearAllHistory());
    QVERIFY(store.lastError().contains(QStringLiteral("forced delete failure")));
    QCOMPARE(store.loadMessages(record.sessionId, 10).size(), 1);
    QCOMPARE(store.totalMessageCount(record.sessionId), 1);
    QCOMPARE(store.loadLogs(record.sessionId, 10).size(), 1);
}

void HistoryStoreTest::roundTripsCanonicalOutgoingMessage()
{
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());

    HistoryStore store(dataDir.path());
    QVERIFY2(store.isReady(), qPrintable(store.lastError()));

    MessageRecord record;
    record.sessionId = QStringLiteral("session-1");
    record.timestamp = QStringLiteral("2026-07-14T10:20:30.123");
    record.direction = MessageDirection::Outgoing;
    record.topic = QStringLiteral("home/light/set");
    record.qos = 1;
    record.retain = true;
    record.retainKnown = true;
    record.payloadBytes = QByteArrayLiteral("{\"value\":23.7}");
    record.payloadSize = record.payloadBytes.size();
    record.payloadState = QStringLiteral("full");
    record.payloadPreview = QString::fromUtf8(record.payloadBytes);
    record.payloadFormat = 1;

    const qint64 id = appendMessage(store, record);
    QVERIFY(id > 0);

    const QVariantMap loaded = store.loadMessage(id);
    QCOMPARE(loaded.value(QStringLiteral("direction")).toString(), QStringLiteral("outgoing"));
    QCOMPARE(loaded.value(QStringLiteral("qos")).toInt(), 1);
    QCOMPARE(loaded.value(QStringLiteral("retain")).toBool(), true);
    QCOMPARE(loaded.value(QStringLiteral("retain_known")).toBool(), true);
    QCOMPARE(loaded.value(QStringLiteral("payload_bytes")).toByteArray(), record.payloadBytes);
}

QTEST_MAIN(HistoryStoreTest)

#include "test_historystore.moc"
