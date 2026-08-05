#include "usecases/eventhistoryservice.h"
#include "usecases/preferencescontroller.h"
#include "usecases/scriptservice.h"
#include "usecases/sessionservice.h"
#include "models/eventstreammodel.h"
#include "presentation/eventrenderer.h"
#include "services/payload/payloadcodec.h"
#include "services/parsing/messageparseworker.h"
#include "services/storage/historystore.h"
#include "services/storage/historywriterworker.h"

#include <QtTest/QtTest>

#include <QSettings>
#include <QSemaphore>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QThread>
#include <QUuid>

#include <algorithm>

class EventHistoryServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void liveRowsDecodeConfiguredPayloadFormatWithoutScript();
    void structuredParseResultPersistsAfterRawCapture();
    void parserOverloadKeepsRawCapture();
    void rejectedParseResultDoesNotPublishTransientState();
    void parserDrainTimeoutStillFlushesWriter();
    void capturePolicyFiltersBeforePayloadPlanning();
    void parserPressureMapsToRawOnlyAndRecovers();
    void elevatedWriterSkipsParsingAndRecovers();
    void writerHardLimitReportsDroppingAndRecovers();
    void rawOnlyRowsKeepPayloadBodyFreeOfStatusText();
    void publishedRowsAppearInMessageStream();
    void publishedRowsKeepFormatAfterHistoryReload();
    void largePublishedRowsUsePreviewUntilInspectedAfterHistoryReload();
    void messageDetailsExposeFullPendingPayload();
    void messagePayloadDisplaySupportsSelectedFormats();
    void previewOnlyRowsKeepPayloadBodyFreeOfStatusText();
    void skippedRowsKeepPayloadBodyEmpty();
    void publishedRowsAppearWhenOutputPaused();
    void pausedIncomingRowsAreStoredWithoutScriptParsing();
    void pausedSubscriptionsDoNotAccumulateReceiveRate();
    void aggregateReceiveRateCountsOverlappingSubscriptionsOnce();
    void publishedAndIncomingRowsBothRemainInMessageStream();
    void pendingVisibleRowsDoNotDuplicateAfterModelRefresh();
    void batchedVisibleRowsEmitOneAppendSignalWithCount();
    void saturatedVisibleWindowUsesIncrementalRows();
    void frozenVisibleRowsWaitForResume();
    void frozenHistoryLoadingUsesSnapshotBoundary();
    void totalMessageCountExceedsVisibleWindowAndResets();
    void failedMessageClearKeepsRuntimeAndModel();
    void clearAllMessagesDiscardsPendingVisibleRows();
    void runtimeFlushDoesNotApplyMessageRetentionLimit();
    void reloadRestoresTotalMessageCount();
    void reusablePayloadLoadsStoredBytesAfterHistoryRowsDropBlobs();
};

namespace {

SessionState &initializeSession(SessionService &sessions)
{
    SessionState session;
    session.id = QStringLiteral("session-1");
    session.name = QStringLiteral("Session 1");
    sessions.sessions().append(session);
    sessions.setCurrentSessionIndex(0);
    return *sessions.currentSession();
}

struct Fixture {
    QTemporaryDir dataDir;
    QSettings settings;
    HistoryStore historyStore;
    HistoryWriterWorker historyWriter;
    MessageParseWorker messageParser;
    PreferencesController preferences;
    EventStreamModel messages;
    EventStreamModel logs;
    ScriptService scripts;
    QString launchTimestamp = QStringLiteral("2026-07-04T00:00:00.000Z");
    SessionService sessions;
    SessionState &session;
    EventHistoryService service;

    explicit Fixture(
        MessageParseLimits parserLimits = {},
        HistoryWriterLimits writerLimits = {},
        bool startWorkers = true)
        : settings(dataDir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat)
        , historyStore(dataDir.path())
        , historyWriter(dataDir.path(), historyStore.nextMessageId(), writerLimits)
        , messageParser(parserLimits)
        , preferences(&settings)
        , sessions(settings, scripts, historyStore, preferences)
        , session(initializeSession(sessions))
        , service(
              sessions,
              historyStore,
              historyWriter,
              messageParser,
              messages,
              logs,
              scripts,
              launchTimestamp,
              preferences)
    {
        if (startWorkers) {
            historyWriter.start();
            messageParser.start();
        }
        sessions.setHistoryWriter(&historyWriter);
        sessions.setMessageParser(&messageParser);
    }

    void addSubscription(const QString &filter, int format)
    {
        SubscriptionEntry entry;
        entry.topic = filter;
        entry.format = format;
        session.subscriptions.append(entry);
        session.runtime.subscriptionFormats.insert(filter, format);
    }
};

} // namespace

void EventHistoryServiceTest::liveRowsDecodeConfiguredPayloadFormatWithoutScript()
{
    Fixture fixture;
    QVERIFY2(fixture.historyStore.isReady(), qPrintable(fixture.historyStore.lastError()));
    fixture.addSubscription(QStringLiteral("devices/temp"), 1);
    const QByteArray payload = R"({"value":23})";
    QString decodeError;
    const QString expectedPayload = PayloadCodec::decodeForDisplay(PayloadFormat::Json, payload, decodeError);
    QVERIFY2(decodeError.isEmpty(), qPrintable(decodeError));
    QSignalSpy parseSpy(&fixture.service, &EventHistoryService::messageParseResultChanged);

    fixture.service.appendIncomingMessage(fixture.session.id, QStringLiteral("devices/temp"), payload);

    QTRY_COMPARE(fixture.messages.count(), 1);
    QTRY_COMPARE(
        fixture.messages.rowAt(0).value(QStringLiteral("payload")).toString(),
        expectedPayload);
    QTRY_COMPARE(parseSpy.count(), 1);
    QCOMPARE(
        parseSpy.first().at(0).toLongLong(),
        fixture.messages.rowAt(0).value(QStringLiteral("historyId")).toLongLong());
    QCOMPARE(fixture.messages.rowAt(0).value(QStringLiteral("payloadFormat")).toString(), QStringLiteral("JSON"));
}

void EventHistoryServiceTest::structuredParseResultPersistsAfterRawCapture()
{
    Fixture fixture;
    fixture.addSubscription(QStringLiteral("devices/temp"), static_cast<int>(PayloadFormat::Json));
    const QByteArray payload = QByteArrayLiteral("{\"value\":23}");

    fixture.service.appendIncomingMessage(
        fixture.session.id,
        QStringLiteral("devices/temp"),
        payload);
    QVERIFY(fixture.service.flushPendingMessageHistory());

    const QVariantList rows = fixture.historyStore.loadMessages(fixture.session.id, 10);
    QCOMPARE(rows.size(), 1);
    const QVariantMap row = rows.first().toMap();
    QCOMPARE(row.value(QStringLiteral("parse_state")).toString(), QStringLiteral("succeeded"));
    QCOMPARE(row.value(QStringLiteral("parsed_format")).toString(), QStringLiteral("JSON"));
    QVERIFY(row.value(QStringLiteral("parsed_payload")).toString().contains(QStringLiteral("\"value\": 23")));
    QCOMPARE(fixture.historyStore.loadMessagePayloadBytes(row.value(QStringLiteral("id")).toLongLong()), payload);
}

void EventHistoryServiceTest::parserOverloadKeepsRawCapture()
{
    MessageParseLimits limits;
    limits.maxTasks = 0;
    Fixture fixture(limits);
    fixture.addSubscription(QStringLiteral("devices/temp"), static_cast<int>(PayloadFormat::Json));
    const QByteArray payload = QByteArrayLiteral("{\"value\":23}");

    fixture.service.appendIncomingMessage(
        fixture.session.id,
        QStringLiteral("devices/temp"),
        payload);
    QVERIFY(fixture.service.flushPendingMessageHistory());

    QTRY_COMPARE(fixture.messages.count(), 1);
    QCOMPARE(
        fixture.messages.rowAt(0).value(QStringLiteral("payloadFormat")).toString(),
        QStringLiteral("Parse skipped"));
    const QVariantList rows = fixture.historyStore.loadMessages(fixture.session.id, 10);
    QCOMPARE(rows.size(), 1);
    const QVariantMap row = rows.first().toMap();
    QCOMPARE(row.value(QStringLiteral("parse_state")).toString(), QStringLiteral("skipped_overload"));
    QCOMPARE(fixture.historyStore.loadMessagePayloadBytes(row.value(QStringLiteral("id")).toLongLong()), payload);
}

void EventHistoryServiceTest::rejectedParseResultDoesNotPublishTransientState()
{
    MessageParseLimits parserLimits;
    parserLimits.maxTasks = 0;
    HistoryWriterLimits writerLimits;
    writerLimits.maxMessages = 2;
    writerLimits.maxBytes = 1024 * 1024;
    writerLimits.flushIntervalMs = 10'000;
    Fixture fixture(parserLimits, writerLimits);
    fixture.addSubscription(QStringLiteral("devices/temp"), static_cast<int>(PayloadFormat::Json));

    MessageRecord prefill;
    prefill.sessionId = fixture.session.id;
    prefill.timestamp = QStringLiteral("2026-08-01T10:00:00.000Z");
    prefill.topic = QStringLiteral("prefill");
    prefill.payloadBytes = QByteArrayLiteral("{}");
    prefill.payloadSize = prefill.payloadBytes.size();
    prefill.payloadPreview = QStringLiteral("{}");
    prefill.payloadState = QStringLiteral("full");
    prefill.parseState = QStringLiteral("pending");
    const qint64 prefillId = fixture.historyWriter.enqueueMessage(prefill);
    QVERIFY(prefillId > 0);

    MessageParseResult filler;
    filler.messageId = prefillId;
    filler.sessionId = fixture.session.id;
    filler.state = MessageParseState::Failed;
    filler.parseError = QStringLiteral("filler");
    QVERIFY(fixture.historyWriter.enqueueParseResult(filler));
    QVERIFY(fixture.historyWriter.enqueueParseResult(filler));

    QSignalSpy parseSpy(&fixture.service, &EventHistoryService::messageParseResultChanged);
    fixture.service.appendIncomingMessage(
        fixture.session.id,
        QStringLiteral("devices/temp"),
        QByteArrayLiteral("{\"value\":23}"));

    QTRY_COMPARE(fixture.messages.count(), 1);
    QCOMPARE(
        fixture.messages.rowAt(0).value(QStringLiteral("parseState")).toString(),
        QStringLiteral("pending"));
    QCOMPARE(parseSpy.count(), 0);
    QCOMPARE(fixture.service.droppedParseResultCount(), qint64(1));
    QVERIFY(fixture.service.flushPendingMessageHistory());

    const QVariantList rows = fixture.historyStore.loadMessages(fixture.session.id, 10);
    QCOMPARE(rows.size(), 2);
    const auto storedMessage = std::find_if(
        rows.cbegin(),
        rows.cend(),
        [](const QVariant &value) {
            return value.toMap().value(QStringLiteral("topic")).toString()
                == QStringLiteral("devices/temp");
        });
    QVERIFY(storedMessage != rows.cend());
    QCOMPARE(
        storedMessage->toMap().value(QStringLiteral("parse_state")).toString(),
        QStringLiteral("pending"));
}

void EventHistoryServiceTest::parserDrainTimeoutStillFlushesWriter()
{
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());

    QSettings settings(dataDir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
    HistoryStore historyStore(dataDir.path());
    QVERIFY2(historyStore.isReady(), qPrintable(historyStore.lastError()));
    HistoryWriterWorker historyWriter(dataDir.path(), historyStore.nextMessageId());
    historyWriter.start();

    QThread parserThread;
    auto *messageParser = new MessageParseWorker();
    messageParser->moveToThread(&parserThread);
    connect(&parserThread, &QThread::finished, messageParser, &QObject::deleteLater);
    parserThread.start();
    QVERIFY(QMetaObject::invokeMethod(
        messageParser,
        &MessageParseWorker::start,
        Qt::BlockingQueuedConnection));

    QSemaphore blockerEntered;
    QSemaphore releaseBlocker;
    QVERIFY(QMetaObject::invokeMethod(
        messageParser,
        [&blockerEntered, &releaseBlocker]() {
            blockerEntered.release();
            releaseBlocker.acquire();
        },
        Qt::QueuedConnection));
    blockerEntered.acquire();

    PreferencesController preferences(&settings);
    EventStreamModel messages;
    EventStreamModel logs;
    ScriptService scripts;
    SessionService sessions(settings, scripts, historyStore, preferences);
    SessionState &session = initializeSession(sessions);
    sessions.setHistoryWriter(&historyWriter);
    sessions.setMessageParser(messageParser);

    qint64 messageId = 0;
    {
        EventHistoryService service(
            sessions,
            historyStore,
            historyWriter,
            *messageParser,
            messages,
            logs,
            scripts,
            QStringLiteral("2026-08-01T00:00:00.000Z"),
            preferences);

        MessageRecord record;
        record.sessionId = session.id;
        record.timestamp = QStringLiteral("2026-08-01T10:00:00.000Z");
        record.topic = QStringLiteral("devices/raw");
        record.payloadBytes = QByteArrayLiteral("raw");
        record.payloadSize = record.payloadBytes.size();
        record.payloadPreview = QStringLiteral("raw");
        record.payloadState = QStringLiteral("full");
        record.parseState = QStringLiteral("pending");
        messageId = historyWriter.enqueueMessage(record);
        QVERIFY(messageId > 0);

        MessageParseTask task;
        task.envelope.messageId = messageId;
        task.envelope.sequence = 1;
        task.envelope.sessionId = session.id;
        task.envelope.topic = record.topic;
        task.envelope.payloadBytes = QByteArrayLiteral("{}");
        task.envelope.payloadFormat = static_cast<int>(PayloadFormat::Json);
        QVERIFY(messageParser->enqueueTask(task));

        QVERIFY(!service.flushPendingMessageHistory(0));
        QCOMPARE(historyWriter.pendingMessageCount(), 0);
        QCOMPARE(
            historyStore.loadMessage(messageId).value(QStringLiteral("topic")).toString(),
            QStringLiteral("devices/raw"));
    }

    releaseBlocker.release();
    QVERIFY(messageParser->drain(2000));
    messageParser->stopAccepting();
    QVERIFY(QMetaObject::invokeMethod(
        messageParser,
        &MessageParseWorker::shutdown,
        Qt::BlockingQueuedConnection));
    parserThread.quit();
    parserThread.wait();

    historyWriter.stopAccepting();
    QVERIFY(historyWriter.drain(2000));
    historyWriter.shutdown();
}

void EventHistoryServiceTest::capturePolicyFiltersBeforePayloadPlanning()
{
    Fixture fixture;
    fixture.addSubscription(QStringLiteral("devices/#"), static_cast<int>(PayloadFormat::Json));
    MessageCapturePolicy policy;
    policy.includeTopicFilters = {QStringLiteral("alerts/#")};
    fixture.service.setMessageCapturePolicy(fixture.session.id, policy);

    fixture.service.appendIncomingMessage(
        fixture.session.id,
        QStringLiteral("devices/private/temp"),
        QByteArray(2 * 1024 * 1024, 'x'));

    QCOMPARE(fixture.service.captureFilteredMessageCount(), qint64(1));
    QCOMPARE(fixture.service.messageWriterBacklog(), 0);
    QCOMPARE(fixture.service.messageParserBacklog(), 0);
    QCOMPARE(fixture.session.runtime.totalMessageCount, qint64(0));
    QCOMPARE(fixture.messages.count(), 0);
    QVERIFY(fixture.session.subscriptions.constFirst().recentMessages.isEmpty());
    QCOMPARE(
        fixture.session.runtime.recentReceivedTraffic.eventCount(
            QDateTime::currentMSecsSinceEpoch()),
        1);

    policy.captureOutgoing = false;
    fixture.service.setMessageCapturePolicy(fixture.session.id, policy);
    fixture.service.appendPublishedMessage(
        fixture.session.id,
        QStringLiteral("alerts/command"),
        QByteArrayLiteral("{}"),
        static_cast<int>(PayloadFormat::Json));
    QCOMPARE(fixture.service.captureFilteredMessageCount(), qint64(2));
    QCOMPARE(fixture.service.messageWriterBacklog(), 0);
}

void EventHistoryServiceTest::parserPressureMapsToRawOnlyAndRecovers()
{
    MessageParseLimits parserLimits;
    parserLimits.highWaterTasks = 1;
    parserLimits.lowWaterTasks = 0;
    parserLimits.maxTasks = 1;
    parserLimits.highWaterBytes = 4096;
    parserLimits.lowWaterBytes = 0;
    parserLimits.maxBytes = 8192;
    Fixture fixture(parserLimits, {}, false);

    MessageParseTask first;
    first.envelope.messageId = 1;
    first.envelope.sessionId = fixture.session.id;
    first.envelope.sequence = 1;
    first.envelope.timestamp = QStringLiteral("2026-08-02T12:00:00.000Z");
    first.envelope.topic = QStringLiteral("devices/first");
    first.envelope.payloadBytes = QByteArrayLiteral("{\"value\":1}");
    first.envelope.payloadFormat = static_cast<int>(PayloadFormat::Json);
    QVERIFY(fixture.messageParser.enqueueTask(first));
    QCOMPARE(fixture.service.messagePressureState(), QStringLiteral("elevated"));
    QCOMPARE(fixture.service.messageCaptureMode(), QStringLiteral("raw_only"));

    MessageParseTask second = first;
    second.envelope.messageId = 2;
    second.envelope.sequence = 2;
    second.envelope.topic = QStringLiteral("devices/second");
    QVERIFY(!fixture.messageParser.enqueueTask(second));
    QCOMPARE(fixture.service.messagePressureState(), QStringLiteral("degraded"));
    QCOMPARE(fixture.service.messageCaptureMode(), QStringLiteral("raw_only"));
    QCOMPARE(fixture.service.droppedParseTaskCount(), qint64(1));

    fixture.messageParser.start();
    QVERIFY(fixture.messageParser.drain(2000));
    QCOMPARE(fixture.service.messagePressureState(), QStringLiteral("normal"));
    QCOMPARE(fixture.service.messageCaptureMode(), QStringLiteral("full"));
}

void EventHistoryServiceTest::elevatedWriterSkipsParsingAndRecovers()
{
    HistoryWriterLimits writerLimits;
    writerLimits.highWaterMessages = 1;
    writerLimits.lowWaterMessages = 0;
    writerLimits.maxMessages = 10;
    writerLimits.flushIntervalMs = 10'000;
    Fixture fixture({}, writerLimits);
    fixture.addSubscription(QStringLiteral("devices/#"), static_cast<int>(PayloadFormat::Json));

    MessageRecord prefill;
    prefill.sessionId = fixture.session.id;
    prefill.timestamp = QStringLiteral("2026-08-01T10:00:00.000Z");
    prefill.topic = QStringLiteral("prefill");
    prefill.payloadBytes = QByteArrayLiteral("raw");
    prefill.payloadSize = prefill.payloadBytes.size();
    prefill.payloadPreview = QStringLiteral("raw");
    QVERIFY(fixture.historyWriter.enqueueMessage(prefill) > 0);
    QCOMPARE(fixture.service.messagePressureState(), QStringLiteral("elevated"));

    const QByteArray firstPayload = QByteArrayLiteral("{\"value\":\"")
        + QByteArray(8 * 1024, 'x')
        + QByteArrayLiteral("\"}");
    fixture.service.appendIncomingMessage(
        fixture.session.id,
        QStringLiteral("devices/first"),
        firstPayload);

    QTRY_COMPARE(fixture.messages.count(), 1);
    QCOMPARE(
        fixture.messages.rowAt(0).value(QStringLiteral("parseState")).toString(),
        QStringLiteral("skipped_overload"));
    QCOMPARE(fixture.service.pressureSkippedParseCount(), qint64(1));
    QCOMPARE(fixture.service.messageParserBacklog(), 0);
    const QVector<MessageRecord> pending = fixture.historyWriter.pendingMessages(fixture.session.id);
    const auto first = std::find_if(
        pending.cbegin(),
        pending.cend(),
        [](const MessageRecord &record) { return record.topic == QStringLiteral("devices/first"); });
    QVERIFY(first != pending.cend());
    QCOMPARE(first->payloadBytes, firstPayload);
    QVERIFY(first->payloadPreview.toUtf8().size() <= 4 * 1024);
    QCOMPARE(first->parseState, QStringLiteral("skipped_overload"));

    QVERIFY(fixture.service.flushPendingMessageHistory());
    QCOMPARE(fixture.service.messagePressureState(), QStringLiteral("normal"));

    fixture.service.appendIncomingMessage(
        fixture.session.id,
        QStringLiteral("devices/second"),
        QByteArrayLiteral("{\"value\":2}"));
    QVERIFY(fixture.service.flushPendingMessageHistory());

    const QVariantList rows = fixture.historyStore.loadMessages(fixture.session.id, 10);
    const auto second = std::find_if(
        rows.cbegin(),
        rows.cend(),
        [](const QVariant &value) {
            return value.toMap().value(QStringLiteral("topic")).toString()
                == QStringLiteral("devices/second");
        });
    QVERIFY(second != rows.cend());
    QCOMPARE(
        second->toMap().value(QStringLiteral("parse_state")).toString(),
        QStringLiteral("succeeded"));
}

void EventHistoryServiceTest::writerHardLimitReportsDroppingAndRecovers()
{
    HistoryWriterLimits writerLimits;
    writerLimits.highWaterMessages = 1;
    writerLimits.lowWaterMessages = 0;
    writerLimits.maxMessages = 1;
    writerLimits.flushIntervalMs = 10'000;
    Fixture fixture({}, writerLimits);

    MessageRecord prefill;
    prefill.sessionId = fixture.session.id;
    prefill.timestamp = QStringLiteral("2026-08-01T10:00:00.000Z");
    prefill.topic = QStringLiteral("prefill");
    prefill.payloadBytes = QByteArrayLiteral("raw");
    prefill.payloadSize = prefill.payloadBytes.size();
    prefill.payloadPreview = QStringLiteral("raw");
    QVERIFY(fixture.historyWriter.enqueueMessage(prefill) > 0);

    fixture.service.appendIncomingMessage(
        fixture.session.id,
        QStringLiteral("devices/dropped"),
        QByteArrayLiteral("payload"));

    QCOMPARE(fixture.service.droppedMessageCount(), qint64(1));
    QCOMPARE(fixture.service.messagePressureState(), QStringLiteral("dropping"));
    QCOMPARE(fixture.service.messageCaptureMode(), QStringLiteral("dropping"));
    QCOMPARE(fixture.session.runtime.totalMessageCount, qint64(0));

    QVERIFY(fixture.historyWriter.drain(2000));
    QCOMPARE(fixture.service.messagePressureState(), QStringLiteral("normal"));

    fixture.service.appendIncomingMessage(
        fixture.session.id,
        QStringLiteral("devices/recovered"),
        QByteArrayLiteral("payload"));
    QCOMPARE(fixture.session.runtime.totalMessageCount, qint64(1));
    QVERIFY(fixture.service.flushPendingMessageHistory());
}

void EventHistoryServiceTest::rawOnlyRowsKeepPayloadBodyFreeOfStatusText()
{
    Fixture fixture;
    QVERIFY2(fixture.historyStore.isReady(), qPrintable(fixture.historyStore.lastError()));
    fixture.addSubscription(QStringLiteral("devices/binary"), static_cast<int>(PayloadFormat::Hex));
    const QByteArray payload(80, '\0');
    const QString expectedPayload = QStringLiteral("%1 ...")
        .arg(QString::fromLatin1(payload.left(64).toHex(' ').toUpper()));

    fixture.service.appendIncomingMessage(
        fixture.session.id,
        QStringLiteral("devices/binary"),
        payload);

    QTRY_COMPARE(fixture.messages.count(), 1);
    const QVariantMap row = fixture.messages.rowAt(0);
    QCOMPARE(row.value(QStringLiteral("payload")).toString(), expectedPayload);
    QCOMPARE(row.value(QStringLiteral("testPayload")).toString(), expectedPayload);
    QCOMPARE(row.value(QStringLiteral("payloadFormat")).toString(), QStringLiteral("Hex · raw"));
}

void EventHistoryServiceTest::publishedRowsAppearInMessageStream()
{
    Fixture fixture;
    QVERIFY2(fixture.historyStore.isReady(), qPrintable(fixture.historyStore.lastError()));
    const QByteArray payload = R"({"sent":true})";
    QString decodeError;
    const QString expectedPayload = PayloadCodec::decodeForDisplay(PayloadFormat::Json, payload, decodeError);
    QVERIFY2(decodeError.isEmpty(), qPrintable(decodeError));

    fixture.service.appendPublishedMessage(
        fixture.session.id,
        QStringLiteral("devices/command"),
        payload,
        static_cast<int>(PayloadFormat::Json),
        1,
        true);

    QTRY_COMPARE(fixture.messages.count(), 1);
    QCOMPARE(fixture.messages.rowAt(0).value(QStringLiteral("topic")).toString(), QStringLiteral("devices/command"));
    QCOMPARE(fixture.messages.rowAt(0).value(QStringLiteral("payload")).toString(), expectedPayload);
    QCOMPARE(fixture.messages.rowAt(0).value(QStringLiteral("payloadFormat")).toString(), QStringLiteral("JSON"));
    QCOMPARE(fixture.messages.rowAt(0).value(QStringLiteral("direction")).toString(), QStringLiteral("outgoing"));
    QCOMPARE(fixture.messages.rowAt(0).value(QStringLiteral("qos")).toInt(), 1);
    QCOMPARE(fixture.messages.rowAt(0).value(QStringLiteral("retain")).toBool(), true);
    QCOMPARE(fixture.messages.rowAt(0).value(QStringLiteral("retainKnown")).toBool(), true);
}

void EventHistoryServiceTest::publishedRowsKeepFormatAfterHistoryReload()
{
    Fixture fixture;
    QVERIFY2(fixture.historyStore.isReady(), qPrintable(fixture.historyStore.lastError()));
    const QByteArray payload = R"({"sent":true})";
    QString decodeError;
    const QString expectedPayload = PayloadCodec::decodeForDisplay(PayloadFormat::Json, payload, decodeError);
    QVERIFY2(decodeError.isEmpty(), qPrintable(decodeError));

    fixture.service.appendPublishedMessage(
        fixture.session.id,
        QStringLiteral("devices/command"),
        payload,
        static_cast<int>(PayloadFormat::Json));
    fixture.service.flushPendingMessageHistory();
    fixture.session.runtime.messageRows.clear();
    fixture.messages.clear();

    fixture.service.reloadCurrentSessionHistory();

    QCOMPARE(fixture.messages.count(), 1);
    QCOMPARE(fixture.messages.rowAt(0).value(QStringLiteral("payload")).toString(), expectedPayload);
    QCOMPARE(fixture.messages.rowAt(0).value(QStringLiteral("payloadFormat")).toString(), QStringLiteral("JSON"));
}

void EventHistoryServiceTest::largePublishedRowsUsePreviewUntilInspectedAfterHistoryReload()
{
    Fixture fixture;
    QVERIFY2(fixture.historyStore.isReady(), qPrintable(fixture.historyStore.lastError()));
    fixture.preferences.setMaxIncomingPayloadBytes(1024 * 1024);
    const QByteArray payload(70 * 1024, 'x');

    fixture.service.appendPublishedMessage(
        fixture.session.id,
        QStringLiteral("devices/large"),
        payload,
        static_cast<int>(PayloadFormat::Plaintext));

    QTRY_COMPARE(fixture.messages.count(), 1);
    QVariantMap row = fixture.messages.rowAt(0);
    QCOMPARE(row.value(QStringLiteral("payload")).toString(), QString::fromUtf8(payload.left(64 * 1024)));
    QCOMPARE(row.value(QStringLiteral("testPayload")).toString(), QString::fromUtf8(payload.left(64 * 1024)));
    QCOMPARE(row.value(QStringLiteral("payloadFormat")).toString(), QStringLiteral("Truncated"));

    fixture.service.flushPendingMessageHistory();
    fixture.session.runtime.messageRows.clear();
    fixture.messages.clear();

    fixture.service.reloadCurrentSessionHistory();

    QCOMPARE(fixture.messages.count(), 1);
    row = fixture.messages.rowAt(0);
    QCOMPARE(row.value(QStringLiteral("payload")).toString(), QString::fromUtf8(payload.left(64 * 1024)));
    QCOMPARE(row.value(QStringLiteral("payloadFormat")).toString(), QStringLiteral("Truncated"));

    const QVariantMap details = fixture.service.messageDetails(row.value(QStringLiteral("historyId")).toLongLong());
    QCOMPARE(details.value(QStringLiteral("fullPayload")).toString(), QString::fromUtf8(payload));
    QVERIFY(details.value(QStringLiteral("fullPayloadAvailable")).toBool());
    QCOMPARE(details.value(QStringLiteral("payloadFormat")).toString(), QStringLiteral("Plaintext"));
}

void EventHistoryServiceTest::messageDetailsExposeFullPendingPayload()
{
    Fixture fixture;
    QVERIFY2(fixture.historyStore.isReady(), qPrintable(fixture.historyStore.lastError()));
    fixture.preferences.setMaxIncomingPayloadBytes(1024 * 1024);
    const QByteArray payload(70 * 1024, 'x');

    fixture.service.appendPublishedMessage(
        fixture.session.id,
        QStringLiteral("devices/pending-large"),
        payload,
        static_cast<int>(PayloadFormat::Plaintext));

    QCOMPARE(fixture.historyWriter.pendingMessageCount(), 1);
    QCOMPARE(fixture.session.runtime.messageRows.size(), 1);
    const qint64 historyId = fixture.session.runtime.messageRows.constLast()
                                 .toMap()
                                 .value(QStringLiteral("historyId"))
                                 .toLongLong();
    QVERIFY(historyId > 0);

    const QVariantMap details = fixture.service.messageDetails(historyId);
    QCOMPARE(details.value(QStringLiteral("fullPayloadAvailable")).toBool(), true);
    QCOMPARE(details.value(QStringLiteral("fullPayload")).toString(), QString::fromUtf8(payload));
}

void EventHistoryServiceTest::messagePayloadDisplaySupportsSelectedFormats()
{
    Fixture fixture;
    QVERIFY2(fixture.historyStore.isReady(), qPrintable(fixture.historyStore.lastError()));
    const QByteArray payload("Hi");

    fixture.service.appendPublishedMessage(
        fixture.session.id,
        QStringLiteral("devices/display-format"),
        payload,
        static_cast<int>(PayloadFormat::Plaintext));

    QCOMPARE(fixture.session.runtime.messageRows.size(), 1);
    const qint64 historyId = fixture.session.runtime.messageRows.constLast()
                                 .toMap()
                                 .value(QStringLiteral("historyId"))
                                 .toLongLong();
    QVERIFY(historyId > 0);
    QCOMPARE(
        fixture.service.messagePayloadForDisplay(
            historyId,
            QStringLiteral("fallback"),
            static_cast<int>(PayloadFormat::Plaintext)),
        QStringLiteral("Hi"));
    QCOMPARE(
        fixture.service.messagePayloadForDisplay(
            historyId,
            QStringLiteral("fallback"),
            static_cast<int>(PayloadFormat::Hex)),
        QStringLiteral("48 69"));
}

void EventHistoryServiceTest::previewOnlyRowsKeepPayloadBodyFreeOfStatusText()
{
    QVariantMap historyRow;
    historyRow.insert(QStringLiteral("id"), 1);
    historyRow.insert(QStringLiteral("timestamp"), QStringLiteral("2026-07-04T00:00:00.000Z"));
    historyRow.insert(QStringLiteral("entry_type"), QStringLiteral("message"));
    historyRow.insert(QStringLiteral("topic"), QStringLiteral("devices/legacy"));
    historyRow.insert(QStringLiteral("payload_bytes"), QByteArray());
    historyRow.insert(QStringLiteral("payload_size"), 10);
    historyRow.insert(QStringLiteral("payload_state"), QStringLiteral("full"));
    historyRow.insert(QStringLiteral("payload_preview"), QStringLiteral("abc"));
    historyRow.insert(QStringLiteral("payload_hash"), QString());
    historyRow.insert(QStringLiteral("payload_format"), static_cast<int>(PayloadFormat::Plaintext));
    historyRow.insert(QStringLiteral("parsed_payload"), QString());
    historyRow.insert(QStringLiteral("parsed_format"), QString());
    historyRow.insert(QStringLiteral("parse_error"), QString());

    const QVariantMap row = EventRenderer::renderHistoryRow(historyRow, {}, {});

    QCOMPARE(row.value(QStringLiteral("payload")).toString(), QStringLiteral("abc"));
    QCOMPARE(row.value(QStringLiteral("payloadFormat")).toString(), QStringLiteral("Plaintext preview"));
}

void EventHistoryServiceTest::skippedRowsKeepPayloadBodyEmpty()
{
    Fixture fixture;
    QVERIFY2(fixture.historyStore.isReady(), qPrintable(fixture.historyStore.lastError()));
    fixture.preferences.setMaxIncomingPayloadBytes(256 * 1024);

    fixture.service.appendPublishedMessage(
        fixture.session.id,
        QStringLiteral("devices/skipped"),
        QByteArray(300 * 1024, 'x'),
        static_cast<int>(PayloadFormat::Plaintext));

    QTRY_COMPARE(fixture.messages.count(), 1);
    const QVariantMap row = fixture.messages.rowAt(0);
    QCOMPARE(row.value(QStringLiteral("payload")).toString(), QString());
    QCOMPARE(row.value(QStringLiteral("payloadFormat")).toString(), QStringLiteral("Skipped"));
}

void EventHistoryServiceTest::publishedRowsAppearWhenOutputPaused()
{
    Fixture fixture;
    QVERIFY2(fixture.historyStore.isReady(), qPrintable(fixture.historyStore.lastError()));
    fixture.session.outputPaused = true;

    fixture.service.appendPublishedMessage(
        fixture.session.id,
        QStringLiteral("devices/command"),
        QByteArrayLiteral("on"),
        static_cast<int>(PayloadFormat::Plaintext));

    QTRY_COMPARE(fixture.messages.count(), 1);
    QCOMPARE(fixture.messages.rowAt(0).value(QStringLiteral("topic")).toString(), QStringLiteral("devices/command"));
    QCOMPARE(fixture.messages.rowAt(0).value(QStringLiteral("payload")).toString(), QStringLiteral("on"));
}

void EventHistoryServiceTest::pausedIncomingRowsAreStoredWithoutScriptParsing()
{
    Fixture fixture;
    QVERIFY2(fixture.historyStore.isReady(), qPrintable(fixture.historyStore.lastError()));
    fixture.session.outputPaused = true;

    SubscriptionEntry entry;
    entry.topic = QStringLiteral("devices/paused");
    entry.format = static_cast<int>(PayloadFormat::Json);
    entry.scriptId = QStringLiteral("missing-script");
    fixture.session.subscriptions.append(entry);
    fixture.session.runtime.subscriptionFormats.insert(entry.topic, entry.format);

    fixture.service.appendIncomingMessage(
        fixture.session.id,
        QStringLiteral("devices/paused"),
        QByteArrayLiteral("{\"paused\":true}"));
    fixture.service.flushPendingMessageHistory();

    QCOMPARE(fixture.messages.count(), 0);
    QCOMPARE(fixture.session.runtime.totalMessageCount, 1);

    const QVariantList rows = fixture.historyStore.loadMessages(fixture.session.id, 10);
    QCOMPARE(rows.size(), 1);
    const QVariantMap row = rows.first().toMap();
    QCOMPARE(row.value(QStringLiteral("topic")).toString(), QStringLiteral("devices/paused"));
    QCOMPARE(row.value(QStringLiteral("parse_error")).toString(), QString());
    QCOMPARE(row.value(QStringLiteral("script_id")).toString(), QString());
}

void EventHistoryServiceTest::aggregateReceiveRateCountsOverlappingSubscriptionsOnce()
{
    Fixture fixture;
    QVERIFY2(fixture.historyStore.isReady(), qPrintable(fixture.historyStore.lastError()));
    fixture.addSubscription(QStringLiteral("devices/#"), static_cast<int>(PayloadFormat::Plaintext));
    fixture.addSubscription(QStringLiteral("devices/+/temp"), static_cast<int>(PayloadFormat::Plaintext));
    QSignalSpy activitySpy(&fixture.service, &EventHistoryService::subscriptionActivityChanged);

    fixture.service.appendIncomingMessage(
        fixture.session.id,
        QStringLiteral("devices/room/temp"),
        QByteArrayLiteral("23"));

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    QCOMPARE(fixture.session.runtime.recentReceivedTraffic.eventCount(nowMs), 1);
    QCOMPARE(
        fixture.session.runtime.recentReceivedTraffic.byteCount(QDateTime::currentMSecsSinceEpoch()),
        qint64(2));
    QCOMPARE(fixture.session.subscriptions.at(0).recentMessages.eventCount(nowMs), 1);
    QCOMPARE(fixture.session.subscriptions.at(1).recentMessages.eventCount(nowMs), 1);
    QCOMPARE(activitySpy.count(), 1);
}

void EventHistoryServiceTest::pausedSubscriptionsDoNotAccumulateReceiveRate()
{
    Fixture fixture;
    QVERIFY2(fixture.historyStore.isReady(), qPrintable(fixture.historyStore.lastError()));
    fixture.addSubscription(QStringLiteral("devices/#"), static_cast<int>(PayloadFormat::Plaintext));
    fixture.session.subscriptions.last().paused = true;
    QSignalSpy activitySpy(&fixture.service, &EventHistoryService::subscriptionActivityChanged);

    fixture.service.appendIncomingMessage(
        fixture.session.id,
        QStringLiteral("devices/temp"),
        QByteArrayLiteral("23"));

    QVERIFY(fixture.session.subscriptions.constLast().recentMessages.isEmpty());
    QCOMPARE(activitySpy.count(), 0);
}

void EventHistoryServiceTest::reusablePayloadLoadsStoredBytesAfterHistoryRowsDropBlobs()
{
    Fixture fixture;
    QVERIFY2(fixture.historyStore.isReady(), qPrintable(fixture.historyStore.lastError()));
    const QByteArray payload("complete payload, not just the rendered preview");

    fixture.service.appendPublishedMessage(
        fixture.session.id,
        QStringLiteral("devices/command"),
        payload,
        static_cast<int>(PayloadFormat::Plaintext));
    fixture.service.flushPendingMessageHistory();

    QTRY_COMPARE(fixture.messages.count(), 1);
    const qint64 historyId = fixture.messages.rowAt(0).value(QStringLiteral("historyId")).toLongLong();
    QVERIFY(historyId > 0);
    QCOMPARE(
        fixture.service.messagePayloadForReuse(
            historyId,
            QStringLiteral("preview only"),
            QStringLiteral("preview only"),
            static_cast<int>(PayloadFormat::Plaintext)),
        QString::fromUtf8(payload));
}

void EventHistoryServiceTest::publishedAndIncomingRowsBothRemainInMessageStream()
{
    Fixture fixture;
    QVERIFY2(fixture.historyStore.isReady(), qPrintable(fixture.historyStore.lastError()));
    fixture.addSubscription(QStringLiteral("devices/command"), static_cast<int>(PayloadFormat::Plaintext));

    fixture.service.appendPublishedMessage(
        fixture.session.id,
        QStringLiteral("devices/command"),
        QByteArrayLiteral("on"),
        static_cast<int>(PayloadFormat::Plaintext));
    fixture.service.appendIncomingMessage(
        fixture.session.id,
        QStringLiteral("devices/command"),
        QByteArrayLiteral("on"));

    QTRY_COMPARE(fixture.messages.count(), 2);
    QCOMPARE(fixture.messages.rowAt(0).value(QStringLiteral("topic")).toString(), QStringLiteral("devices/command"));
    QCOMPARE(fixture.messages.rowAt(0).value(QStringLiteral("payload")).toString(), QStringLiteral("on"));
    QCOMPARE(fixture.messages.rowAt(1).value(QStringLiteral("topic")).toString(), QStringLiteral("devices/command"));
    QCOMPARE(fixture.messages.rowAt(1).value(QStringLiteral("payload")).toString(), QStringLiteral("on"));
}

void EventHistoryServiceTest::pendingVisibleRowsDoNotDuplicateAfterModelRefresh()
{
    Fixture fixture;
    QVERIFY2(fixture.historyStore.isReady(), qPrintable(fixture.historyStore.lastError()));

    fixture.service.appendPublishedMessage(
        fixture.session.id,
        QStringLiteral("devices/command"),
        QByteArrayLiteral("on"),
        static_cast<int>(PayloadFormat::Plaintext));
    fixture.messages.setRows(fixture.session.runtime.messageRows);

    QTRY_COMPARE(fixture.messages.count(), 1);
    QCOMPARE(fixture.messages.rowAt(0).value(QStringLiteral("payload")).toString(), QStringLiteral("on"));
}

void EventHistoryServiceTest::batchedVisibleRowsEmitOneAppendSignalWithCount()
{
    Fixture fixture;
    QVERIFY2(fixture.historyStore.isReady(), qPrintable(fixture.historyStore.lastError()));
    fixture.addSubscription(QStringLiteral("devices/+"), 0);
    QSignalSpy appendSpy(&fixture.service, &EventHistoryService::messageRowsAppended);

    fixture.service.appendIncomingMessage(fixture.session.id, QStringLiteral("devices/one"), QByteArrayLiteral("1"));
    fixture.service.appendIncomingMessage(fixture.session.id, QStringLiteral("devices/two"), QByteArrayLiteral("2"));
    fixture.service.appendIncomingMessage(fixture.session.id, QStringLiteral("devices/three"), QByteArrayLiteral("3"));

    QTRY_COMPARE(fixture.messages.count(), 3);
    QCOMPARE(appendSpy.count(), 1);
    QCOMPARE(appendSpy.first().at(0).toList().size(), 3);
}

void EventHistoryServiceTest::saturatedVisibleWindowUsesIncrementalRows()
{
    Fixture fixture;
    QVERIFY2(fixture.historyStore.isReady(), qPrintable(fixture.historyStore.lastError()));

    QVariantList initialRows;
    initialRows.reserve(1200);
    for (int index = 0; index < 1200; ++index) {
        initialRows.append(QVariantMap {
            {QStringLiteral("historyId"), index + 1},
            {QStringLiteral("kind"), QStringLiteral("message")},
            {QStringLiteral("topic"), QStringLiteral("devices/%1").arg(index)},
            {QStringLiteral("payload"), QString::number(index)},
            {QStringLiteral("payloadFormat"), QStringLiteral("Plaintext")},
            {QStringLiteral("direction"), QStringLiteral("incoming")},
        });
    }
    fixture.session.runtime.messageRows = initialRows;
    fixture.messages.setRows(initialRows);

    QSignalSpy insertSpy(&fixture.messages, &EventStreamModel::rowsInserted);
    QSignalSpy removeSpy(&fixture.messages, &EventStreamModel::rowsRemoved);
    QSignalSpy resetSpy(&fixture.messages, &EventStreamModel::modelReset);
    QSignalSpy dataSpy(&fixture.messages, &EventStreamModel::dataChanged);

    fixture.service.appendPublishedMessage(
        fixture.session.id,
        QStringLiteral("devices/latest"),
        QByteArrayLiteral("latest"),
        static_cast<int>(PayloadFormat::Plaintext));

    QTRY_COMPARE(
        fixture.messages.rowAt(1199).value(QStringLiteral("topic")).toString(),
        QStringLiteral("devices/latest"));
    QCOMPARE(fixture.messages.count(), 1200);
    QCOMPARE(fixture.session.runtime.messageRows.size(), 1200);
    QCOMPARE(removeSpy.count(), 1);
    QCOMPARE(insertSpy.count(), 1);
    QCOMPARE(resetSpy.count(), 0);
    QCOMPARE(dataSpy.count(), 0);
}

void EventHistoryServiceTest::frozenVisibleRowsWaitForResume()
{
    Fixture fixture;
    QVERIFY2(fixture.historyStore.isReady(), qPrintable(fixture.historyStore.lastError()));

    fixture.service.appendPublishedMessage(
        fixture.session.id,
        QStringLiteral("devices/one"),
        QByteArrayLiteral("1"),
        static_cast<int>(PayloadFormat::Plaintext));
    QTRY_COMPARE(fixture.messages.count(), 1);

    fixture.service.setMessageStreamFrozen(true);
    QSignalSpy appendSpy(&fixture.service, &EventHistoryService::messageRowsAppended);

    fixture.service.appendPublishedMessage(
        fixture.session.id,
        QStringLiteral("devices/two"),
        QByteArrayLiteral("2"),
        static_cast<int>(PayloadFormat::Plaintext));
    fixture.service.appendPublishedMessage(
        fixture.session.id,
        QStringLiteral("devices/three"),
        QByteArrayLiteral("3"),
        static_cast<int>(PayloadFormat::Plaintext));

    QTRY_COMPARE(appendSpy.count(), 1);
    QCOMPARE(appendSpy.first().at(0).toList().size(), 2);
    QCOMPARE(fixture.messages.count(), 1);
    QCOMPARE(fixture.session.runtime.messageRows.size(), 3);
    QCOMPARE(fixture.session.runtime.totalMessageCount, 3);

    fixture.service.setMessageStreamFrozen(false);
    QCOMPARE(fixture.messages.count(), 3);
    QCOMPARE(fixture.messages.rowAt(2).value(QStringLiteral("topic")).toString(), QStringLiteral("devices/three"));
}

void EventHistoryServiceTest::frozenHistoryLoadingUsesSnapshotBoundary()
{
    Fixture fixture;
    QVERIFY2(fixture.historyStore.isReady(), qPrintable(fixture.historyStore.lastError()));

    for (int index = 0; index < 3; ++index) {
        MessageRecord record;
        record.sessionId = fixture.session.id;
        record.timestamp = QStringLiteral("2026-07-15T16:00:0%1.000").arg(index);
        record.topic = QStringLiteral("devices/%1").arg(index);
        record.payloadPreview = QString::number(index);
        record.payloadState = QStringLiteral("full");
        QVERIFY(fixture.historyWriter.enqueueMessage(record) > 0);
    }
    fixture.service.reloadCurrentSessionHistory();
    QCOMPARE(fixture.messages.count(), 3);

    fixture.service.setMessageStreamFrozen(true);
    fixture.session.runtime.messageRows.removeFirst();
    fixture.session.runtime.oldestLoadedMessageId = 2;
    fixture.session.runtime.loadedAllMessageHistory = false;

    QCOMPARE(fixture.service.loadOlderCurrentSessionMessages(), 0);
    QCOMPARE(fixture.messages.count(), 3);
}

void EventHistoryServiceTest::totalMessageCountExceedsVisibleWindowAndResets()
{
    Fixture fixture;
    QVERIFY2(fixture.historyStore.isReady(), qPrintable(fixture.historyStore.lastError()));

    for (int index = 0; index < 1201; ++index) {
        fixture.service.appendPublishedMessage(
            fixture.session.id,
            QStringLiteral("devices/%1").arg(index),
            QByteArray::number(index),
            static_cast<int>(PayloadFormat::Plaintext));
    }

    QTRY_COMPARE(fixture.messages.count(), 1200);
    QCOMPARE(fixture.session.runtime.totalMessageCount, 1201);

    QVERIFY(fixture.service.clearCurrentMessages());
    QCOMPARE(fixture.session.runtime.totalMessageCount, 0);
    QCOMPARE(fixture.session.runtime.viewedMessageCount, 0);
    QCOMPARE(fixture.messages.count(), 0);
}

void EventHistoryServiceTest::failedMessageClearKeepsRuntimeAndModel()
{
    Fixture fixture;
    QVERIFY2(fixture.historyStore.isReady(), qPrintable(fixture.historyStore.lastError()));

    fixture.service.appendPublishedMessage(
        fixture.session.id,
        QStringLiteral("devices/one"),
        QByteArrayLiteral("one"),
        static_cast<int>(PayloadFormat::Plaintext));
    QTRY_COMPARE(fixture.messages.count(), 1);

    const QString connectionName = QStringLiteral("fail-service-clear-%1")
                                       .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(fixture.dataDir.filePath(QStringLiteral("history.db")));
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QSqlQuery query(db);
        QVERIFY2(query.exec(QStringLiteral(
                     "CREATE TRIGGER fail_service_total_delete "
                     "BEFORE DELETE ON mqtt_message_totals "
                     "BEGIN SELECT RAISE(ABORT, 'forced delete failure'); END")),
            qPrintable(query.lastError().text()));
    }
    QSqlDatabase::removeDatabase(connectionName);

    QSignalSpy streamSpy(&fixture.service, &EventHistoryService::messageStreamChanged);
    QSignalSpy totalSpy(&fixture.service, &EventHistoryService::totalMessageCountChanged);
    QVERIFY(!fixture.service.clearCurrentMessages());

    QCOMPARE(streamSpy.count(), 0);
    QCOMPARE(totalSpy.count(), 0);
    QCOMPARE(fixture.messages.count(), 1);
    QCOMPARE(fixture.session.runtime.messageRows.size(), 1);
    QCOMPARE(fixture.session.runtime.totalMessageCount, 1);
    QCOMPARE(fixture.historyStore.loadMessages(fixture.session.id, 10).size(), 1);
    QVERIFY(!fixture.session.runtime.logRows.isEmpty());
    QVERIFY(fixture.session.runtime.logRows.last().toMap()
                .value(QStringLiteral("payload"))
                .toString()
                .contains(QStringLiteral("forced delete failure")));
}

void EventHistoryServiceTest::clearAllMessagesDiscardsPendingVisibleRows()
{
    Fixture fixture;
    QVERIFY2(fixture.historyStore.isReady(), qPrintable(fixture.historyStore.lastError()));

    fixture.service.appendPublishedMessage(
        fixture.session.id,
        QStringLiteral("devices/pending"),
        QByteArrayLiteral("pending"),
        static_cast<int>(PayloadFormat::Plaintext));
    QCOMPARE(fixture.session.runtime.messageRows.size(), 1);

    QVERIFY(fixture.service.clearAllMessages());
    QTest::qWait(50);

    QCOMPARE(fixture.historyWriter.pendingMessageCount(), 0);
    QCOMPARE(fixture.historyStore.totalMessageCount(fixture.session.id), 0);
    QCOMPARE(fixture.messages.count(), 0);
    QCOMPARE(fixture.session.runtime.messageRows.size(), 0);
    QCOMPARE(fixture.session.runtime.totalMessageCount, 0);
}

void EventHistoryServiceTest::runtimeFlushDoesNotApplyMessageRetentionLimit()
{
    Fixture fixture;
    QVERIFY2(fixture.historyStore.isReady(), qPrintable(fixture.historyStore.lastError()));
    fixture.preferences.setMessageRetentionLimit(100);

    for (int batch = 0; batch < 10; ++batch) {
        for (int index = 0; index < 15; ++index) {
            const int messageNumber = batch * 15 + index;
            fixture.service.appendPublishedMessage(
                fixture.session.id,
                QStringLiteral("devices/%1").arg(messageNumber),
                QByteArray::number(messageNumber),
                static_cast<int>(PayloadFormat::Plaintext));
        }
        fixture.service.flushPendingMessageHistory();
    }

    QCOMPARE(fixture.historyStore.loadMessages(fixture.session.id, 1000).size(), 150);
    QCOMPARE(fixture.session.runtime.totalMessageCount, 150);
}

void EventHistoryServiceTest::reloadRestoresTotalMessageCount()
{
    Fixture fixture;
    QVERIFY2(fixture.historyStore.isReady(), qPrintable(fixture.historyStore.lastError()));

    MessageRecord first;
    first.sessionId = fixture.session.id;
    first.timestamp = QStringLiteral("2026-07-15T16:00:00.000");
    first.topic = QStringLiteral("devices/one");
    first.payloadPreview = QStringLiteral("one");
    first.payloadState = QStringLiteral("full");
    MessageRecord second = first;
    second.topic = QStringLiteral("devices/two");

    QVERIFY(fixture.historyWriter.enqueueMessage(first) > 0);
    QVERIFY(fixture.historyWriter.enqueueMessage(second) > 0);
    fixture.service.reloadCurrentSessionHistory();

    QCOMPARE(fixture.session.runtime.totalMessageCount, 2);
    QCOMPARE(fixture.messages.count(), 2);
}

QTEST_MAIN(EventHistoryServiceTest)

#include "test_eventhistoryservice.moc"
