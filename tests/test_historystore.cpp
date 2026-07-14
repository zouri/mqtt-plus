#include "services/storage/historystore.h"
#include "domain/messagerecord.h"

#include <QtTest/QtTest>

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>

class HistoryStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void flushesRawPayloadWithoutLegacyColumns();
    void resetsOnlyMessageTableWhenSchemaIsStale();
    void loadMessagesUsesPreviewWithoutPayloadBytes();
    void loadsPayloadBytesByMessageId();
    void roundTripsCanonicalOutgoingMessage();
};

void HistoryStoreTest::flushesRawPayloadWithoutLegacyColumns()
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

    const qint64 reservedId = store.enqueueMessage(
        sessionId,
        QStringLiteral("2026-07-02 15:02:20.304"),
        QStringLiteral("111111/ros_to_android"),
        payload,
        QString(),
        QString(),
        QString(),
        QString(),
        QString(),
        QStringLiteral("raw preview"),
        QStringLiteral("full"),
        payload.size(),
        QStringLiteral("hash"));

    QVERIFY(reservedId > 0);
    QCOMPARE(store.pendingMessageCount(), 1);

    QCOMPARE(store.flushPendingMessages(), QStringList({sessionId}));
    QVERIFY2(store.lastError().isEmpty(), qPrintable(store.lastError()));
    QCOMPARE(store.pendingMessageCount(), 0);

    const QVariantList rows = store.loadMessages(sessionId, 10);
    QCOMPARE(rows.size(), 1);

    const QVariantMap row = rows.first().toMap();
    QVERIFY(!row.contains(QStringLiteral("payload")));
    QCOMPARE(row.value(QStringLiteral("payload_bytes")).toByteArray(), QByteArray());
    QCOMPARE(row.value(QStringLiteral("payload_size")).toLongLong(), qint64(payload.size()));
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
                     "topic TEXT NOT NULL, "
                     "payload TEXT NOT NULL DEFAULT '', "
                     "payload_b64 TEXT NOT NULL DEFAULT '')")),
            qPrintable(query.lastError().text()));
        QVERIFY2(query.exec(QStringLiteral(
                     "INSERT INTO mqtt_messages(session_id, timestamp, topic, payload, payload_b64) "
                     "VALUES('stale-session', '2026-07-02T15:02:20.304Z', 'devices/stale', 'old', 'b2xk')")),
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

    const QVariantList logs = store.loadLogs(QStringLiteral("stale-session"), 10);
    QCOMPARE(logs.size(), 1);
    QCOMPARE(logs.first().toMap().value(QStringLiteral("payload")).toString(), QStringLiteral("kept log"));

    const QString newSessionId = QStringLiteral("new-session");
    const qint64 reservedId = store.enqueueMessage(
        newSessionId,
        QStringLiteral("2026-07-02T15:02:22.000Z"),
        QStringLiteral("devices/current"),
        QByteArrayLiteral("current"),
        QString(),
        QString(),
        QString(),
        QString(),
        QString(),
        QStringLiteral("current"),
        QStringLiteral("full"),
        7,
        QStringLiteral("hash"));

    QVERIFY(reservedId > 0);
    QCOMPARE(store.flushPendingMessages(), QStringList({newSessionId}));
    QCOMPARE(store.loadMessages(newSessionId, 10).size(), 1);
}

void HistoryStoreTest::loadMessagesUsesPreviewWithoutPayloadBytes()
{
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());

    HistoryStore store(dataDir.path());
    QVERIFY2(store.isReady(), qPrintable(store.lastError()));

    const QString sessionId = QStringLiteral("test-session-%1")
                                  .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QByteArray payload(1024 * 32, 'x');

    const qint64 reservedId = store.enqueueMessage(
        sessionId,
        QStringLiteral("2026-07-02T15:02:20.304Z"),
        QStringLiteral("devices/large"),
        payload,
        QString(),
        QString(),
        QString(),
        QString(),
        QString(),
        QStringLiteral("preview only"),
        QStringLiteral("full"),
        payload.size(),
        QStringLiteral("hash"));

    QVERIFY(reservedId > 0);
    QCOMPARE(store.flushPendingMessages(), QStringList({sessionId}));

    const QVariantList rows = store.loadMessages(sessionId, 10);
    QCOMPARE(rows.size(), 1);

    const QVariantMap row = rows.first().toMap();
    QCOMPARE(row.value(QStringLiteral("payload_preview")).toString(), QStringLiteral("preview only"));
    QCOMPARE(row.value(QStringLiteral("payload_state")).toString(), QStringLiteral("full"));
    QCOMPARE(row.value(QStringLiteral("payload_size")).toLongLong(), qint64(payload.size()));
    QCOMPARE(row.value(QStringLiteral("payload_bytes")).toByteArray(), QByteArray());
}

void HistoryStoreTest::loadsPayloadBytesByMessageId()
{
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());

    HistoryStore store(dataDir.path());
    QVERIFY2(store.isReady(), qPrintable(store.lastError()));

    const QString sessionId = QStringLiteral("test-session-%1")
                                  .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QByteArray payload("full payload");

    const qint64 reservedId = store.enqueueMessage(
        sessionId,
        QStringLiteral("2026-07-02T15:02:20.304Z"),
        QStringLiteral("devices/payload"),
        payload,
        QString(),
        QString(),
        QString(),
        QString(),
        QString(),
        QStringLiteral("preview"),
        QStringLiteral("full"),
        payload.size(),
        QStringLiteral("hash"));

    QVERIFY(reservedId > 0);
    QCOMPARE(store.flushPendingMessages(), QStringList({sessionId}));
    QCOMPARE(store.loadMessagePayloadBytes(reservedId), payload);
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

    const qint64 id = store.enqueueMessage(record);
    QVERIFY(id > 0);
    QCOMPARE(store.flushPendingMessages(), QStringList {QStringLiteral("session-1")});

    const QVariantMap loaded = store.loadMessage(id);
    QCOMPARE(loaded.value(QStringLiteral("direction")).toString(), QStringLiteral("outgoing"));
    QCOMPARE(loaded.value(QStringLiteral("qos")).toInt(), 1);
    QCOMPARE(loaded.value(QStringLiteral("retain")).toBool(), true);
    QCOMPARE(loaded.value(QStringLiteral("retain_known")).toBool(), true);
    QCOMPARE(loaded.value(QStringLiteral("payload_bytes")).toByteArray(), record.payloadBytes);
}

QTEST_MAIN(HistoryStoreTest)

#include "test_historystore.moc"
