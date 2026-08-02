#include "services/storage/historystore.h"
#include "services/storage/historywriterworker.h"

#include <QtTest/QtTest>

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QMetaObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QThread>
#include <QUuid>

namespace {
MessageRecord makeRecord(const QString &sessionId, const QString &topic, const QByteArray &payload)
{
    MessageRecord record;
    record.sessionId = sessionId;
    record.timestamp = QStringLiteral("2026-08-01T10:00:00.000Z");
    record.topic = topic;
    record.payloadBytes = payload;
    record.payloadSize = payload.size();
    record.payloadPreview = QString::fromUtf8(payload.left(64));
    record.payloadState = QStringLiteral("full");
    return record;
}

class ThreadedWriter
{
public:
    ThreadedWriter(const QString &dataPath, qint64 nextMessageId, HistoryWriterLimits limits)
        : worker(new HistoryWriterWorker(dataPath, nextMessageId, limits))
    {
        worker->moveToThread(&thread);
        QObject::connect(&thread, &QThread::finished, worker, &QObject::deleteLater);
        thread.start();
        QMetaObject::invokeMethod(
            worker,
            &HistoryWriterWorker::start,
            Qt::BlockingQueuedConnection);
    }

    ~ThreadedWriter()
    {
        worker->stopAccepting();
        worker->drain(2000);
        QMetaObject::invokeMethod(
            worker,
            &HistoryWriterWorker::shutdown,
            Qt::BlockingQueuedConnection);
        thread.quit();
        thread.wait();
    }

    QThread thread;
    HistoryWriterWorker *worker = nullptr;
};
} // namespace

class HistoryWriterWorkerTest : public QObject
{
    Q_OBJECT

private slots:
    void rejectsMessagesAtCountAndByteLimits();
    void recoversInvalidInitialMessageId();
    void writesQueuedMessagesInReservedIdOrder();
    void retriesLockedDatabaseWithoutAnotherEnqueue();
    void writesCaptureBeforeParseUpdateInOneQueue();
    void historyStoreEnablesWalAndBusyTimeout();
};

void HistoryWriterWorkerTest::rejectsMessagesAtCountAndByteLimits()
{
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());

    HistoryWriterLimits limits;
    limits.highWaterMessages = 1;
    limits.lowWaterMessages = 0;
    limits.maxMessages = 2;
    limits.highWaterBytes = 1024;
    limits.lowWaterBytes = 512;
    limits.maxBytes = 2048;

    HistoryWriterWorker writer(dataDir.path(), 41, limits);
    QCOMPARE(writer.enqueueMessage(makeRecord(QStringLiteral("session-1"), QStringLiteral("one"), QByteArray(100, 'a'))), 41);
    QCOMPARE(writer.enqueueMessage(makeRecord(QStringLiteral("session-1"), QStringLiteral("two"), QByteArray(100, 'b'))), 42);
    QCOMPARE(writer.enqueueMessage(makeRecord(QStringLiteral("session-1"), QStringLiteral("three"), QByteArray(100, 'c'))), 0);
    QCOMPARE(writer.pendingMessageCount(), 2);
    QCOMPARE(writer.droppedMessageCount(), 1);
    QCOMPARE(writer.pressureState(), HistoryWriterWorker::PressureState::Dropping);
    writer.start();
    QVERIFY(writer.drain(2000));
    QCOMPARE(writer.pressureState(), HistoryWriterWorker::PressureState::Normal);

    HistoryWriterLimits byteLimits = limits;
    byteLimits.maxMessages = 10;
    byteLimits.maxBytes = 256;
    HistoryWriterWorker byteLimitedWriter(dataDir.path(), 1, byteLimits);
    QCOMPARE(
        byteLimitedWriter.enqueueMessage(
            makeRecord(QStringLiteral("session-1"), QStringLiteral("oversized"), QByteArray(512, 'x'))),
        0);
    QCOMPARE(byteLimitedWriter.pendingMessageCount(), 0);
    QCOMPARE(byteLimitedWriter.droppedMessageCount(), 1);
    QCOMPARE(byteLimitedWriter.droppedParseResultCount(), 0);
    QCOMPARE(byteLimitedWriter.pressureState(), HistoryWriterWorker::PressureState::Normal);

    HistoryWriterLimits parseResultLimits = limits;
    parseResultLimits.maxMessages = 1;
    parseResultLimits.maxBytes = 4096;
    HistoryWriterWorker parseResultWriter(dataDir.path(), 100, parseResultLimits);
    const qint64 parseMessageId = parseResultWriter.enqueueMessage(
        makeRecord(QStringLiteral("session-1"), QStringLiteral("parsed"), QByteArrayLiteral("{}")));
    QVERIFY(parseMessageId > 0);
    MessageParseResult parseResult;
    parseResult.messageId = parseMessageId;
    parseResult.sessionId = QStringLiteral("session-1");
    parseResult.state = MessageParseState::Succeeded;
    QVERIFY(parseResultWriter.enqueueParseResult(parseResult));
    QVERIFY(!parseResultWriter.enqueueParseResult(parseResult));
    QCOMPARE(parseResultWriter.droppedMessageCount(), 0);
    QCOMPARE(parseResultWriter.droppedParseResultCount(), 1);
}

void HistoryWriterWorkerTest::recoversInvalidInitialMessageId()
{
    QTemporaryDir rootDir;
    QVERIFY(rootDir.isValid());
    const QString dataPath = rootDir.filePath(QStringLiteral("history-data"));
    const QString backupPath = rootDir.filePath(QStringLiteral("history-ready"));
    QVERIFY(QDir().mkpath(dataPath));

    qint64 expectedNextId = 0;
    {
        HistoryStore seed(dataPath);
        QVERIFY2(seed.isReady(), qPrintable(seed.lastError()));
        MessageRecord existing = makeRecord(
            QStringLiteral("session-1"),
            QStringLiteral("existing"),
            QByteArrayLiteral("existing"));
        existing.id = seed.nextMessageId();
        const HistoryWriteResult seedResult = seed.appendMessages({existing});
        QVERIFY2(seedResult.ok, qPrintable(seedResult.error));
        expectedNextId = seed.nextMessageId();
    }

    QVERIFY(QDir().rename(dataPath, backupPath));
    QFile blocker(dataPath);
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    blocker.close();

    HistoryWriterLimits limits;
    limits.initialRetryMs = 10;
    limits.maxRetryMs = 20;
    HistoryWriterWorker writer(dataPath, 0, limits);
    writer.start();
    QCOMPARE(
        writer.enqueueMessage(
            makeRecord(
                QStringLiteral("session-1"),
                QStringLiteral("unavailable"),
                QByteArrayLiteral("unavailable"))),
        qint64(0));

    QVERIFY(QFile::remove(dataPath));
    QVERIFY(QDir().rename(backupPath, dataPath));
    QTest::qWait(100);

    const qint64 messageId = writer.enqueueMessage(
        makeRecord(
            QStringLiteral("session-1"),
            QStringLiteral("recovered"),
            QByteArrayLiteral("recovered")));
    QCOMPARE(messageId, expectedNextId);
    QVERIFY(writer.drain(2000));

    HistoryStore reader(dataPath);
    QCOMPARE(
        reader.loadMessage(messageId).value(QStringLiteral("topic")).toString(),
        QStringLiteral("recovered"));
}

void HistoryWriterWorkerTest::writesQueuedMessagesInReservedIdOrder()
{
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());

    HistoryStore seed(dataDir.path());
    QVERIFY2(seed.isReady(), qPrintable(seed.lastError()));
    const qint64 nextId = seed.nextMessageId();

    HistoryWriterLimits limits;
    limits.batchSize = 2;
    limits.flushIntervalMs = 20;
    limits.busyTimeoutMs = 50;
    ThreadedWriter threaded(dataDir.path(), nextId, limits);

    QCOMPARE(threaded.worker->enqueueMessage(makeRecord(QStringLiteral("session-1"), QStringLiteral("one"), QByteArrayLiteral("1"))), nextId);
    QCOMPARE(threaded.worker->enqueueMessage(makeRecord(QStringLiteral("session-1"), QStringLiteral("two"), QByteArrayLiteral("2"))), nextId + 1);
    QCOMPARE(threaded.worker->enqueueMessage(makeRecord(QStringLiteral("session-1"), QStringLiteral("three"), QByteArrayLiteral("3"))), nextId + 2);
    QVERIFY(threaded.worker->drain(3000));
    QCOMPARE(threaded.worker->pendingMessageCount(), 0);

    HistoryStore reader(dataDir.path());
    const QVariantList rows = reader.loadMessages(QStringLiteral("session-1"), 10);
    QCOMPARE(rows.size(), 3);
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("id")).toLongLong(), nextId);
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("id")).toLongLong(), nextId + 1);
    QCOMPARE(rows.at(2).toMap().value(QStringLiteral("id")).toLongLong(), nextId + 2);
}

void HistoryWriterWorkerTest::retriesLockedDatabaseWithoutAnotherEnqueue()
{
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());

    HistoryStore seed(dataDir.path());
    QVERIFY2(seed.isReady(), qPrintable(seed.lastError()));

    HistoryWriterLimits limits;
    limits.batchSize = 1;
    limits.flushIntervalMs = 5;
    limits.initialRetryMs = 30;
    limits.maxRetryMs = 120;
    limits.busyTimeoutMs = 20;
    ThreadedWriter threaded(dataDir.path(), seed.nextMessageId(), limits);
    QSignalSpy errorSpy(threaded.worker, &HistoryWriterWorker::storageErrorChanged);
    QSignalSpy persistedSpy(threaded.worker, &HistoryWriterWorker::messagesPersisted);

    const QString connectionName = QStringLiteral("writer-lock-%1")
                                       .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QSqlDatabase lockDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    lockDb.setDatabaseName(dataDir.filePath(QStringLiteral("history.db")));
    QVERIFY2(lockDb.open(), qPrintable(lockDb.lastError().text()));
    QSqlQuery lockQuery(lockDb);
    QVERIFY2(lockQuery.exec(QStringLiteral("PRAGMA busy_timeout = 20")), qPrintable(lockQuery.lastError().text()));
    QVERIFY2(lockQuery.exec(QStringLiteral("BEGIN IMMEDIATE")), qPrintable(lockQuery.lastError().text()));

    QElapsedTimer enqueueTimer;
    enqueueTimer.start();
    const qint64 messageId = threaded.worker->enqueueMessage(
        makeRecord(QStringLiteral("session-1"), QStringLiteral("locked"), QByteArrayLiteral("payload")));
    QVERIFY(messageId > 0);
    QVERIFY2(enqueueTimer.elapsed() < 100, "GUI-side admission waited for the SQLite transaction");
    QTRY_VERIFY_WITH_TIMEOUT(errorSpy.count() >= 1, 1500);
    QCOMPARE(threaded.worker->pendingMessageCount(), 1);
    QCOMPARE(threaded.worker->pressureState(), HistoryWriterWorker::PressureState::Degraded);

    QVERIFY2(lockQuery.exec(QStringLiteral("COMMIT")), qPrintable(lockQuery.lastError().text()));
    QTRY_VERIFY_WITH_TIMEOUT(persistedSpy.count() >= 1, 2500);
    QTRY_COMPARE_WITH_TIMEOUT(threaded.worker->pendingMessageCount(), 0, 2500);
    QTRY_COMPARE_WITH_TIMEOUT(threaded.worker->pressureState(), HistoryWriterWorker::PressureState::Normal, 2500);
    QVERIFY(errorSpy.count() >= 2);
    QCOMPARE(errorSpy.last().at(0).toString(), QString());

    lockDb.close();
    lockDb = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);

    HistoryStore reader(dataDir.path());
    QCOMPARE(reader.loadMessage(messageId).value(QStringLiteral("topic")).toString(), QStringLiteral("locked"));
}

void HistoryWriterWorkerTest::writesCaptureBeforeParseUpdateInOneQueue()
{
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());

    HistoryStore seed(dataDir.path());
    QVERIFY2(seed.isReady(), qPrintable(seed.lastError()));

    HistoryWriterLimits limits;
    limits.batchSize = 2;
    HistoryWriterWorker writer(dataDir.path(), seed.nextMessageId(), limits);
    writer.start();

    const qint64 messageId = writer.enqueueMessage(
        makeRecord(
            QStringLiteral("session-1"),
            QStringLiteral("devices/parsed"),
            QByteArrayLiteral("{\"value\":1}")));
    QVERIFY(messageId > 0);

    MessageParseResult parseResult;
    parseResult.messageId = messageId;
    parseResult.sessionId = QStringLiteral("session-1");
    parseResult.parsedPayload = QStringLiteral("{\n    \"value\": 1\n}");
    parseResult.parsedFormat = QStringLiteral("JSON");
    parseResult.state = MessageParseState::Succeeded;
    QVERIFY(writer.enqueueParseResult(parseResult));
    QVERIFY(writer.drain(2000));

    HistoryStore reader(dataDir.path());
    const QVariantMap stored = reader.loadMessage(messageId);
    QCOMPARE(stored.value(QStringLiteral("parse_state")).toString(), QStringLiteral("succeeded"));
    QCOMPARE(stored.value(QStringLiteral("parsed_payload")).toString(), parseResult.parsedPayload);
    QCOMPARE(stored.value(QStringLiteral("parsed_format")).toString(), QStringLiteral("JSON"));
}

void HistoryWriterWorkerTest::historyStoreEnablesWalAndBusyTimeout()
{
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());

    HistoryStore store(dataDir.path(), 321);
    QVERIFY2(store.isReady(), qPrintable(store.lastError()));
    QCOMPARE(store.journalMode().toLower(), QStringLiteral("wal"));
    QCOMPARE(store.busyTimeoutMs(), 321);
}

QTEST_MAIN(HistoryWriterWorkerTest)

#include "test_historywriterworker.moc"
