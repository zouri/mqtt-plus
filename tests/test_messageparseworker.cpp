#include "services/parsing/messageparseworker.h"
#include "services/payload/payloadcodec.h"
#include "services/processors/processorvaluecodec.h"

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
    task.messageId = messageId;
    task.sessionId = sessionId;
    task.sequence = sequence;
    task.timestamp = QStringLiteral("2026-08-01T12:00:00.000Z");
    task.topic = QStringLiteral("devices/%1").arg(messageId);
    task.payloadBytes = payload;
    task.payloadFormat = static_cast<int>(PayloadFormat::Json);
    return task;
}

QSharedPointer<const ProcessorRevisionSnapshot> javascriptRevision(
    const QString &revisionId,
    const QByteArray &source)
{
    auto revision = QSharedPointer<ProcessorRevisionSnapshot>::create();
    revision->id = revisionId;
    revision->processorId = QStringLiteral("processor-1");
    revision->revisionNumber = revisionId.endsWith(QLatin1Char('2')) ? 2 : 1;
    revision->contractId = QStringLiteral("mqtt-plus.message-processor/v1");
    revision->languageId = QStringLiteral("javascript");
    revision->runtimeId = QStringLiteral("qt-qjs");
    revision->entryFile = QStringLiteral("main.js");
    revision->entrySymbol = QStringLiteral("process");
    revision->contentHash = revisionId + QStringLiteral("-hash");
    revision->files = {
        {
            QStringLiteral("main.js"),
            QStringLiteral("text/javascript"),
            source,
            {},
        },
    };
    revision->createdAt = QStringLiteral("2026-08-05T08:00:00.000Z");
    return revision;
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
    void runsProcessorEngineWithImmutableRevision();
    void queuedTasksKeepTheirResolvedRevision();
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
    QCOMPARE(resultSpy.at(0).at(0).value<ParseOutcome>().sequence, 1);
    QCOMPARE(resultSpy.at(1).at(0).value<ParseOutcome>().sessionId, QStringLiteral("session-2"));
    QCOMPARE(resultSpy.at(2).at(0).value<ParseOutcome>().sequence, 2);
    QCOMPARE(resultSpy.at(2).at(0).value<ParseOutcome>().state, MessageParseState::Succeeded);
    QVERIFY(resultSpy.at(2).at(0).value<ParseOutcome>().displayPayload.contains(QStringLiteral("\"value\": 3")));
    QVERIFY(threaded.worker->drain(2000));
}

void MessageParseWorkerTest::runsProcessorEngineWithImmutableRevision()
{
    ThreadedParser threaded;
    QSignalSpy resultSpy(threaded.worker, &MessageParseWorker::parseCompleted);

    MessageParseTask task = makeTask(
        7,
        QStringLiteral("session-1"),
        1,
        QByteArrayLiteral("{\"value\":7}"));
    task.processorRevision = javascriptRevision(
        QStringLiteral("revision-1"),
        QByteArrayLiteral(
            "function process(context) {\n"
            "    return { topic: context.topic, value: context.parameters.value }\n"
            "}\n"));
    task.processorName = QStringLiteral("Value processor");
    task.processorParameters.insert(QStringLiteral("value"), 7);

    QVERIFY(threaded.worker->enqueueTask(task));
    QTRY_COMPARE_WITH_TIMEOUT(resultSpy.count(), 1, 2000);
    const ParseOutcome result = resultSpy.first().at(0).value<ParseOutcome>();
    QCOMPARE(result.state, MessageParseState::Succeeded);
    QCOMPARE(result.processorId, QStringLiteral("processor-1"));
    QCOMPARE(result.processorRevisionId, QStringLiteral("revision-1"));
    QCOMPARE(result.processorExecutionState, QStringLiteral("succeeded"));
    QCOMPARE(result.displayFormat, QStringLiteral("JavaScript: Value processor"));
    QCborParserError error = {};
    const QCborValue value = QCborValue::fromCbor(result.processorResultCbor, &error);
    QCOMPARE(error.error, QCborError::NoError);
    QCOMPARE(value.toMap().value(QStringLiteral("topic")).toString(), QStringLiteral("devices/7"));
    QCOMPARE(value.toMap().value(QStringLiteral("value")).toInteger(), qint64(7));
}

void MessageParseWorkerTest::queuedTasksKeepTheirResolvedRevision()
{
    MessageParseWorker worker;
    QSignalSpy resultSpy(&worker, &MessageParseWorker::parseCompleted);

    MessageParseTask first = makeTask(
        8,
        QStringLiteral("session-1"),
        1,
        QByteArrayLiteral("{}"));
    first.processorRevision = javascriptRevision(
        QStringLiteral("revision-1"),
        QByteArrayLiteral("function process(context) { return 1 }\n"));
    first.processorName = QStringLiteral("Versioned processor");
    MessageParseTask second = makeTask(
        9,
        QStringLiteral("session-1"),
        2,
        QByteArrayLiteral("{}"));
    second.processorRevision = javascriptRevision(
        QStringLiteral("revision-2"),
        QByteArrayLiteral("function process(context) { return 2 }\n"));
    second.processorName = first.processorName;

    QVERIFY(worker.enqueueTask(first));
    QVERIFY(worker.enqueueTask(second));
    worker.start();
    QVERIFY(worker.drain(2000));
    QCOMPARE(resultSpy.count(), 2);

    const ParseOutcome firstResult = resultSpy.at(0).at(0).value<ParseOutcome>();
    const ParseOutcome secondResult = resultSpy.at(1).at(0).value<ParseOutcome>();
    QCOMPARE(firstResult.processorRevisionId, QStringLiteral("revision-1"));
    QCOMPARE(secondResult.processorRevisionId, QStringLiteral("revision-2"));
    QCOMPARE(QCborValue::fromCbor(firstResult.processorResultCbor).toInteger(), qint64(1));
    QCOMPARE(QCborValue::fromCbor(secondResult.processorResultCbor).toInteger(), qint64(2));
}

QTEST_MAIN(MessageParseWorkerTest)

#include "test_messageparseworker.moc"
