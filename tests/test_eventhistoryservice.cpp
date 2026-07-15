#include "usecases/eventhistoryservice.h"
#include "usecases/preferencescontroller.h"
#include "usecases/scriptservice.h"
#include "usecases/subscriptionservice.h"
#include "models/eventstreammodel.h"
#include "presentation/eventrenderer.h"
#include "services/payload/payloadcodec.h"
#include "services/storage/historystore.h"

#include <QtTest/QtTest>

#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>

class EventHistoryServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void liveRowsDecodeConfiguredPayloadFormatWithoutScript();
    void rawOnlyRowsKeepPayloadBodyFreeOfStatusText();
    void publishedRowsAppearInMessageStream();
    void publishedRowsKeepFormatAfterHistoryReload();
    void largePublishedRowsShowPreviewAfterHistoryReload();
    void messageDetailsExposeFullPendingPayload();
    void previewOnlyRowsKeepPayloadBodyFreeOfStatusText();
    void skippedRowsKeepPayloadBodyEmpty();
    void publishedRowsAppearWhenOutputPaused();
    void pausedIncomingRowsAreStoredWithoutScriptParsing();
    void publishedAndIncomingRowsBothRemainInMessageStream();
    void pendingVisibleRowsDoNotDuplicateAfterModelRefresh();
    void batchedVisibleRowsEmitOneAppendSignalWithCount();
    void reusablePayloadLoadsStoredBytesAfterHistoryRowsDropBlobs();
};

namespace {

struct Fixture {
    QTemporaryDir dataDir;
    QSettings settings;
    HistoryStore historyStore;
    PreferencesController preferences;
    EventStreamModel messages;
    EventStreamModel logs;
    ScriptService scripts;
    SubscriptionService subscriptions;
    QTimer fpsTimer;
    QString launchTimestamp = QStringLiteral("2026-07-04T00:00:00.000Z");
    SessionState session;
    EventHistoryService service;

    Fixture()
        : settings(dataDir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat)
        , historyStore(dataDir.path())
        , preferences(&settings)
    {
        session.id = QStringLiteral("session-1");
        session.name = QStringLiteral("Session 1");

        EventHistoryService::Dependencies dependencies;
        dependencies.historyStore = &historyStore;
        dependencies.messagesModel = &messages;
        dependencies.logsModel = &logs;
        dependencies.scriptController = &scripts;
        dependencies.subscriptionController = &subscriptions;
        dependencies.subscriptionFpsRefreshTimer = &fpsTimer;
        dependencies.launchTimestamp = &launchTimestamp;
        dependencies.preferencesController = &preferences;
        dependencies.currentSessionState = [this]() { return &session; };
        dependencies.sessionById = [this](const QString &id) -> SessionState * {
            return id == session.id ? &session : nullptr;
        };
        dependencies.refreshSubscriptionsModel = []() {};
        dependencies.refreshScriptTestSamplesModel = []() {};
        service.setDependencies(dependencies);
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

    fixture.service.appendIncomingMessage(fixture.session.id, QStringLiteral("devices/temp"), payload);

    QTRY_COMPARE(fixture.messages.count(), 1);
    QCOMPARE(fixture.messages.rowAt(0).value(QStringLiteral("payload")).toString(), expectedPayload);
    QCOMPARE(fixture.messages.rowAt(0).value(QStringLiteral("payloadFormat")).toString(), QStringLiteral("JSON"));
}

void EventHistoryServiceTest::rawOnlyRowsKeepPayloadBodyFreeOfStatusText()
{
    Fixture fixture;
    QVERIFY2(fixture.historyStore.isReady(), qPrintable(fixture.historyStore.lastError()));
    fixture.addSubscription(QStringLiteral("devices/binary"), static_cast<int>(PayloadFormat::Hex));

    fixture.service.appendIncomingMessage(
        fixture.session.id,
        QStringLiteral("devices/binary"),
        QByteArray::fromHex("00017F"));

    QTRY_COMPARE(fixture.messages.count(), 1);
    const QVariantMap row = fixture.messages.rowAt(0);
    QCOMPARE(row.value(QStringLiteral("payload")).toString(), QStringLiteral("00 01 7F"));
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

void EventHistoryServiceTest::largePublishedRowsShowPreviewAfterHistoryReload()
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
    fixture.service.flushPendingMessageHistory();
    fixture.session.runtime.messageRows.clear();
    fixture.messages.clear();

    fixture.service.reloadCurrentSessionHistory();

    QCOMPARE(fixture.messages.count(), 1);
    const QVariantMap row = fixture.messages.rowAt(0);
    QCOMPARE(row.value(QStringLiteral("payload")).toString(), QString(64 * 1024, QLatin1Char('x')));
    QCOMPARE(row.value(QStringLiteral("payloadFormat")).toString(), QStringLiteral("Truncated"));
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

    QCOMPARE(fixture.historyStore.pendingMessageCount(), 1);
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

    const QVariantList rows = fixture.historyStore.loadMessages(fixture.session.id, 10);
    QCOMPARE(rows.size(), 1);
    const QVariantMap row = rows.first().toMap();
    QCOMPARE(row.value(QStringLiteral("topic")).toString(), QStringLiteral("devices/paused"));
    QCOMPARE(row.value(QStringLiteral("parse_error")).toString(), QString());
    QCOMPARE(row.value(QStringLiteral("script_id")).toString(), QString());
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
    QCOMPARE(appendSpy.first().at(0).toInt(), 3);
}

QTEST_MAIN(EventHistoryServiceTest)

#include "test_eventhistoryservice.moc"
