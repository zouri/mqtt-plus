#include "services/parsing/messageparseworker.h"
#include "services/payload/payloadcodec.h"

#include <QtTest/QtTest>

#include <QMetaObject>
#include <QSignalSpy>
#include <QThread>

namespace {
MessageParseTask makeTask(
    qint64 messageId,
    const QString &sessionId,
    qint64 sequence,
    const QByteArray &payload)
{
    MessageParseTask task;
    task.envelope.messageId = messageId;
    task.envelope.sessionId = sessionId;
    task.envelope.sequence = sequence;
    task.envelope.timestamp = QStringLiteral("2026-08-01T12:00:00.000Z");
    task.envelope.topic = QStringLiteral("devices/%1").arg(messageId);
    task.envelope.payloadBytes = payload;
    task.envelope.payloadFormat = static_cast<int>(PayloadFormat::Json);
    return task;
}

class ThreadedParser
{
public:
    explicit ThreadedParser(MessageParseLimits limits = {})
        : worker(new MessageParseWorker(limits))
    {
        worker->moveToThread(&thread);
        QObject::connect(&thread, &QThread::finished, worker, &QObject::deleteLater);
        thread.start();
        QMetaObject::invokeMethod(
            worker,
            &MessageParseWorker::start,
            Qt::BlockingQueuedConnection);
    }

    ~ThreadedParser()
    {
        worker->stopAccepting();
        worker->drain(2000);
        QMetaObject::invokeMethod(
            worker,
            &MessageParseWorker::shutdown,
            Qt::BlockingQueuedConnection);
        thread.quit();
        thread.wait();
    }

    QThread thread;
    MessageParseWorker *worker = nullptr;
};
} // namespace

class MessageParseWorkerTest : public QObject
{
    Q_OBJECT

private slots:
    void rejectsTasksAtCountAndByteLimits();
    void reportsQueuePressureAndRecovers();
    void preservesPerSessionSequenceWhileDecoding();
    void runsLuaWithWorkerOwnedRuntimeCache();
};

void MessageParseWorkerTest::rejectsTasksAtCountAndByteLimits()
{
    MessageParseLimits limits;
    limits.maxTasks = 2;
    limits.maxBytes = 4096;
    MessageParseWorker worker(limits);

    QVERIFY(worker.enqueueTask(makeTask(1, QStringLiteral("session-1"), 1, QByteArrayLiteral("{}"))));
    QVERIFY(worker.enqueueTask(makeTask(2, QStringLiteral("session-1"), 2, QByteArrayLiteral("{}"))));
    QVERIFY(!worker.enqueueTask(makeTask(3, QStringLiteral("session-1"), 3, QByteArrayLiteral("{}"))));
    QCOMPARE(worker.pendingTaskCount(), 2);
    QCOMPARE(worker.droppedTaskCount(), 1);

    MessageParseLimits byteLimits = limits;
    byteLimits.maxTasks = 10;
    byteLimits.maxBytes = 256;
    MessageParseWorker byteLimitedWorker(byteLimits);
    QVERIFY(!byteLimitedWorker.enqueueTask(
        makeTask(4, QStringLiteral("session-1"), 4, QByteArray(512, 'x'))));
    QCOMPARE(byteLimitedWorker.pendingTaskCount(), 0);
    QCOMPARE(byteLimitedWorker.droppedTaskCount(), 1);
}

void MessageParseWorkerTest::reportsQueuePressureAndRecovers()
{
    MessageParseLimits limits;
    limits.highWaterTasks = 1;
    limits.lowWaterTasks = 0;
    limits.maxTasks = 2;
    limits.highWaterBytes = 4096;
    limits.lowWaterBytes = 0;
    limits.maxBytes = 8192;
    limits.batchSize = 2;
    MessageParseWorker worker(limits);

    QVERIFY(worker.enqueueTask(
        makeTask(1, QStringLiteral("session-1"), 1, QByteArrayLiteral("{}"))));
    QCOMPARE(worker.pressureState(), MessageParseWorker::PressureState::Elevated);
    QVERIFY(worker.enqueueTask(
        makeTask(2, QStringLiteral("session-1"), 2, QByteArrayLiteral("{}"))));
    QVERIFY(!worker.enqueueTask(
        makeTask(3, QStringLiteral("session-1"), 3, QByteArrayLiteral("{}"))));
    QCOMPARE(worker.pressureState(), MessageParseWorker::PressureState::Dropping);

    worker.start();
    QVERIFY(worker.drain(2000));
    QCOMPARE(worker.pressureState(), MessageParseWorker::PressureState::Normal);
}

void MessageParseWorkerTest::preservesPerSessionSequenceWhileDecoding()
{
    MessageParseLimits limits;
    limits.batchSize = 3;
    ThreadedParser threaded(limits);
    QSignalSpy resultSpy(threaded.worker, &MessageParseWorker::parseCompleted);

    QVERIFY(threaded.worker->enqueueTask(
        makeTask(1, QStringLiteral("session-1"), 1, QByteArrayLiteral("{\"value\":1}"))));
    QVERIFY(threaded.worker->enqueueTask(
        makeTask(2, QStringLiteral("session-2"), 1, QByteArrayLiteral("{\"value\":2}"))));
    QVERIFY(threaded.worker->enqueueTask(
        makeTask(3, QStringLiteral("session-1"), 2, QByteArrayLiteral("{\"value\":3}"))));

    QTRY_COMPARE_WITH_TIMEOUT(resultSpy.count(), 3, 2000);
    QCOMPARE(resultSpy.at(0).at(0).value<MessageParseResult>().sequence, 1);
    QCOMPARE(resultSpy.at(1).at(0).value<MessageParseResult>().sessionId, QStringLiteral("session-2"));
    QCOMPARE(resultSpy.at(2).at(0).value<MessageParseResult>().sequence, 2);
    QCOMPARE(resultSpy.at(2).at(0).value<MessageParseResult>().state, MessageParseState::Succeeded);
    QVERIFY(resultSpy.at(2).at(0).value<MessageParseResult>().parsedPayload.contains(QStringLiteral("\"value\": 3")));
    QVERIFY(threaded.worker->drain(2000));
}

void MessageParseWorkerTest::runsLuaWithWorkerOwnedRuntimeCache()
{
    ThreadedParser threaded;
    QSignalSpy resultSpy(threaded.worker, &MessageParseWorker::parseCompleted);

    MessageParseTask task = makeTask(
        7,
        QStringLiteral("session-1"),
        1,
        QByteArrayLiteral("{\"value\":7}"));
    task.scriptId = QStringLiteral("script-1");
    task.scriptName = QStringLiteral("Value parser");
    task.scriptCode = QStringLiteral(
        "function parse(ctx) return ctx.topic .. ':' .. ctx.decoded end");

    QVERIFY(threaded.worker->enqueueTask(task));
    QTRY_COMPARE_WITH_TIMEOUT(resultSpy.count(), 1, 2000);
    const MessageParseResult result = resultSpy.first().at(0).value<MessageParseResult>();
    QCOMPARE(result.state, MessageParseState::Succeeded);
    QCOMPARE(result.parsedFormat, QStringLiteral("Lua: Value parser"));
    QVERIFY(result.parsedPayload.startsWith(QStringLiteral("devices/7:")));
}

QTEST_MAIN(MessageParseWorkerTest)

#include "test_messageparseworker.moc"
