#include "usecases/subscriptionservice.h"

#include "models/eventstreammodel.h"
#include "services/apputils.h"
#include "services/storage/historystore.h"
#include "services/storage/historywriterworker.h"
#include "services/parsing/messageparseworker.h"
#include "usecases/eventhistoryservice.h"
#include "usecases/preferencescontroller.h"
#include "usecases/scriptservice.h"
#include "usecases/sessionservice.h"

#include <QtTest/QtTest>

#include <QDateTime>
#include <QSettings>
#include <QTemporaryDir>

#include <utility>

class SubscriptionServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void updateCurrentSubscriptionEditsQosAndFormat();
    void setsAllCurrentSubscriptionsPausedWithSingleSignal();
    void detectsActiveCurrentSubscriptionFps();
};

namespace {

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
    QString launchTimestamp = QStringLiteral("2026-07-25T00:00:00.000Z");
    SessionService sessions;
    EventHistoryService eventHistory;
    SubscriptionService service;

    Fixture()
        : settings(dataDir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat)
        , historyStore(dataDir.path())
        , historyWriter(dataDir.path(), historyStore.nextMessageId())
        , preferences(&settings)
        , sessions(settings, scripts, historyStore, preferences)
        , eventHistory(
              sessions,
              historyStore,
              historyWriter,
              messageParser,
              messages,
              logs,
              scripts,
              launchTimestamp,
              preferences)
        , service(sessions, scripts, eventHistory)
    {
        historyWriter.start();
        messageParser.start();
        sessions.setHistoryWriter(&historyWriter);
        sessions.setMessageParser(&messageParser);
    }

    SessionState &setCurrentSession(SessionState session)
    {
        sessions.sessions().append(std::move(session));
        sessions.setCurrentSessionIndex(0);
        return *sessions.currentSession();
    }
};

} // namespace

void SubscriptionServiceTest::updateCurrentSubscriptionEditsQosAndFormat()
{
    Fixture fixture;
    SessionState session;
    session.id = QStringLiteral("session-1");
    session.name = QStringLiteral("Session 1");
    SubscriptionEntry entry;
    entry.topic = QStringLiteral("devices/temp");
    entry.alias = QStringLiteral("Temperature");
    entry.requestedQos = 0;
    entry.format = 0;
    entry.color = QStringLiteral("#0071E3");
    session.subscriptions.append(entry);
    session.runtime.subscriptionFormats.insert(entry.topic, entry.format);
    SessionState &currentSession = fixture.setCurrentSession(std::move(session));
    QSignalSpy changedSpy(&fixture.service, &SubscriptionService::subscriptionsChanged);

    QVERIFY(fixture.service.updateCurrentSubscription(
        QStringLiteral("devices/temp"),
        QStringLiteral("devices/temp"),
        QStringLiteral("Temperature"),
        1,
        2,
        QString(),
        QStringLiteral("#0071E3")));

    QCOMPARE(currentSession.subscriptions.size(), 1);
    QCOMPARE(currentSession.subscriptions.first().requestedQos, 1);
    QCOMPARE(currentSession.subscriptions.first().format, 2);
    QCOMPARE(currentSession.runtime.subscriptionFormats.value(QStringLiteral("devices/temp")), 2);
    QCOMPARE(changedSpy.count(), 1);
}

void SubscriptionServiceTest::setsAllCurrentSubscriptionsPausedWithSingleSignal()
{
    Fixture fixture;
    SessionState session;
    session.id = QStringLiteral("session-1");
    session.name = QStringLiteral("Session 1");
    session.subscriptions = {
        SubscriptionEntry {.topic = QStringLiteral("devices/one")},
        SubscriptionEntry {.topic = QStringLiteral("devices/two")},
    };
    session.subscriptions[0].recentMessages.add(1);
    session.subscriptions[0].recentMessages.add(2);
    session.subscriptions[1].recentMessages.add(3);
    session.subscriptions[1].recentMessages.add(4);
    SessionState &currentSession = fixture.setCurrentSession(std::move(session));
    QSignalSpy changedSpy(&fixture.service, &SubscriptionService::subscriptionsChanged);

    fixture.service.setAllCurrentSubscriptionsPaused(true);

    QVERIFY(currentSession.subscriptions.at(0).paused);
    QVERIFY(currentSession.subscriptions.at(1).paused);
    QVERIFY(currentSession.subscriptions.at(0).recentMessages.isEmpty());
    QVERIFY(currentSession.subscriptions.at(1).recentMessages.isEmpty());
    QCOMPARE(changedSpy.count(), 1);

    currentSession.subscriptions[0].recentMessages.add(5);
    currentSession.subscriptions[1].recentMessages.add(6);
    fixture.service.setAllCurrentSubscriptionsPaused(false);

    QVERIFY(!currentSession.subscriptions.at(0).paused);
    QVERIFY(!currentSession.subscriptions.at(1).paused);
    QVERIFY(currentSession.subscriptions.at(0).recentMessages.isEmpty());
    QVERIFY(currentSession.subscriptions.at(1).recentMessages.isEmpty());
    QCOMPARE(changedSpy.count(), 2);
}

void SubscriptionServiceTest::detectsActiveCurrentSubscriptionFps()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    Fixture fixture;
    SessionState session;
    session.id = QStringLiteral("session-1");
    session.name = QStringLiteral("Session 1");
    SubscriptionEntry entry;
    entry.topic = QStringLiteral("devices/temp");
    entry.recentMessages.add(nowMs);
    session.subscriptions.append(entry);
    SessionState &currentSession = fixture.setCurrentSession(std::move(session));

    QVERIFY(fixture.service.currentSessionHasActiveSubscriptionFps(nowMs));

    currentSession.subscriptions[0].paused = true;
    QVERIFY(!fixture.service.currentSessionHasActiveSubscriptionFps(nowMs));

    currentSession.subscriptions[0].paused = false;
    QVERIFY(!fixture.service.currentSessionHasActiveSubscriptionFps(
        nowMs + AppUtils::kSubscriptionFpsWindowMs + 1));
}

QTEST_MAIN(SubscriptionServiceTest)

#include "test_subscriptionservice.moc"
