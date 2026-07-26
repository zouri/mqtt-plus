#include "usecases/eventhistoryservice.h"
#include "usecases/preferencescontroller.h"
#include "usecases/scriptservice.h"
#include "usecases/sessionservice.h"
#include "models/eventstreammodel.h"
#include "presentation/eventrenderer.h"
#include "services/payload/payloadcodec.h"
#include "services/storage/historystore.h"

#include <QtTest/QtTest>

#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>

class EventHistoryServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void liveRowsDecodeConfiguredPayloadFormatWithoutScript();
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
    PreferencesController preferences;
    EventStreamModel messages;
    EventStreamModel logs;
    ScriptService scripts;
    QString launchTimestamp = QStringLiteral("2026-07-04T00:00:00.000Z");
    SessionService sessions;
    SessionState &session;
    EventHistoryService service;

    Fixture()
        : settings(dataDir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat)
        , historyStore(dataDir.path())
        , preferences(&settings)
        , sessions(settings, scripts, historyStore, preferences)
        , session(initializeSession(sessions))
        , service(
              sessions,
              historyStore,
              messages,
              logs,
              scripts,
              launchTimestamp,
              preferences)
    {
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
    const QByteArray payload(80, '\0');
    QString decodeError;
    const QString expectedPayload = PayloadCodec::decodeForDisplay(PayloadFormat::Hex, payload, decodeError);
    QVERIFY2(decodeError.isEmpty(), qPrintable(decodeError));

    fixture.service.appendIncomingMessage(
        fixture.session.id,
        QStringLiteral("devices/binary"),
        payload);

    QTRY_COMPARE(fixture.messages.count(), 1);
    const QVariantMap row = fixture.messages.rowAt(0);
    QCOMPARE(row.value(QStringLiteral("payload")).toString(), expectedPayload);
    QCOMPARE(row.value(QStringLiteral("payloadFormat")).toString(), QStringLiteral("Hex"));
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
    fixture.service.flushPendingMessageHistory();
    fixture.session.runtime.messageRows.clear();
    fixture.messages.clear();

    fixture.service.reloadCurrentSessionHistory();

    QCOMPARE(fixture.messages.count(), 1);
    const QVariantMap row = fixture.messages.rowAt(0);
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

    QCOMPARE(fixture.session.runtime.recentReceivedTimestampsMs.size(), 1);
    QCOMPARE(fixture.session.subscriptions.at(0).recentMessageTimestampsMs.size(), 1);
    QCOMPARE(fixture.session.subscriptions.at(1).recentMessageTimestampsMs.size(), 1);
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

    QVERIFY(fixture.session.subscriptions.constLast().recentMessageTimestampsMs.isEmpty());
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
    QCOMPARE(appendSpy.first().at(0).toInt(), 3);
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
    QCOMPARE(appendSpy.first().at(0).toInt(), 2);
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
        QVERIFY(fixture.historyStore.enqueueMessage(record) > 0);
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

    QCOMPARE(fixture.historyStore.pendingMessageCount(), 0);
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

    QVERIFY(fixture.historyStore.enqueueMessage(first) > 0);
    QVERIFY(fixture.historyStore.enqueueMessage(second) > 0);
    fixture.service.reloadCurrentSessionHistory();

    QCOMPARE(fixture.session.runtime.totalMessageCount, 2);
    QCOMPARE(fixture.messages.count(), 2);
}

QTEST_MAIN(EventHistoryServiceTest)

#include "test_eventhistoryservice.moc"
