#include "controllers/eventhistoryservice.h"
#include "controllers/preferencescontroller.h"
#include "controllers/scriptservice.h"
#include "controllers/subscriptionservice.h"
#include "models/eventstreammodel.h"
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
    void batchedVisibleRowsEmitOneAppendSignalPerRow();
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

void EventHistoryServiceTest::batchedVisibleRowsEmitOneAppendSignalPerRow()
{
    Fixture fixture;
    QVERIFY2(fixture.historyStore.isReady(), qPrintable(fixture.historyStore.lastError()));
    fixture.addSubscription(QStringLiteral("devices/+"), 0);
    QSignalSpy appendSpy(&fixture.service, &EventHistoryService::messageAppended);

    fixture.service.appendIncomingMessage(fixture.session.id, QStringLiteral("devices/one"), QByteArrayLiteral("1"));
    fixture.service.appendIncomingMessage(fixture.session.id, QStringLiteral("devices/two"), QByteArrayLiteral("2"));
    fixture.service.appendIncomingMessage(fixture.session.id, QStringLiteral("devices/three"), QByteArrayLiteral("3"));

    QTRY_COMPARE(fixture.messages.count(), 3);
    QCOMPARE(appendSpy.count(), 3);
}

QTEST_MAIN(EventHistoryServiceTest)

#include "test_eventhistoryservice.moc"
