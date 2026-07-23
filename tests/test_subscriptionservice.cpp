#include "usecases/subscriptionservice.h"

#include "models/subscriptionlistmodel.h"
#include "services/apputils.h"
#include "usecases/scriptservice.h"

#include <QtTest/QtTest>

#include <QDateTime>
#include <QTimer>

class SubscriptionServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void updateCurrentSubscriptionEditsQosAndFormat();
    void setsAllCurrentSubscriptionsPausedWithSingleRefresh();
    void refreshTimerStopsAfterRateHistoryDrains();
};

void SubscriptionServiceTest::updateCurrentSubscriptionEditsQosAndFormat()
{
    SessionState session;
    SubscriptionEntry entry;
    entry.topic = QStringLiteral("devices/temp");
    entry.alias = QStringLiteral("Temperature");
    entry.requestedQos = 0;
    entry.format = 0;
    entry.scriptId = QString();
    entry.color = QStringLiteral("#0071E3");
    session.subscriptions.append(entry);
    session.runtime.subscriptionFormats.insert(entry.topic, entry.format);

    bool saved = false;
    bool refreshed = false;
    ScriptService scripts;
    SubscriptionService service;
    service.setDependencies({
        nullptr,
        &scripts,
        nullptr,
        nullptr,
        [&session]() { return &session; },
        {},
        [&saved]() {
            saved = true;
            return true;
        },
        [&refreshed]() { refreshed = true; },
    });

    QVERIFY(service.updateCurrentSubscription(
        QStringLiteral("devices/temp"),
        QStringLiteral("devices/temp"),
        QStringLiteral("Temperature"),
        1,
        2,
        QString(),
        QStringLiteral("#0071E3")));

    QCOMPARE(session.subscriptions.size(), 1);
    QCOMPARE(session.subscriptions.first().requestedQos, 1);
    QCOMPARE(session.subscriptions.first().format, 2);
    QCOMPARE(session.runtime.subscriptionFormats.value(QStringLiteral("devices/temp")), 2);
    QVERIFY(saved);
    QVERIFY(refreshed);
}

void SubscriptionServiceTest::setsAllCurrentSubscriptionsPausedWithSingleRefresh()
{
    SessionState session;
    session.subscriptions = {
        SubscriptionEntry {.topic = QStringLiteral("devices/one")},
        SubscriptionEntry {.topic = QStringLiteral("devices/two")},
    };
    session.subscriptions[0].recentMessageTimestampsMs = {1, 2};
    session.subscriptions[1].recentMessageTimestampsMs = {3, 4};

    int saveCount = 0;
    int refreshCount = 0;
    SubscriptionService service;
    service.setDependencies({
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        [&session]() { return &session; },
        {},
        [&saveCount]() {
            ++saveCount;
            return true;
        },
        [&refreshCount]() { ++refreshCount; },
    });

    service.setAllCurrentSubscriptionsPaused(true);

    QVERIFY(session.subscriptions.at(0).paused);
    QVERIFY(session.subscriptions.at(1).paused);
    QVERIFY(session.subscriptions.at(0).recentMessageTimestampsMs.isEmpty());
    QVERIFY(session.subscriptions.at(1).recentMessageTimestampsMs.isEmpty());
    QCOMPARE(saveCount, 1);
    QCOMPARE(refreshCount, 1);

    session.subscriptions[0].recentMessageTimestampsMs = {5};
    session.subscriptions[1].recentMessageTimestampsMs = {6};
    service.setAllCurrentSubscriptionsPaused(false);

    QVERIFY(!session.subscriptions.at(0).paused);
    QVERIFY(!session.subscriptions.at(1).paused);
    QVERIFY(session.subscriptions.at(0).recentMessageTimestampsMs.isEmpty());
    QVERIFY(session.subscriptions.at(1).recentMessageTimestampsMs.isEmpty());
    QCOMPARE(saveCount, 2);
    QCOMPARE(refreshCount, 2);
}

void SubscriptionServiceTest::refreshTimerStopsAfterRateHistoryDrains()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    SessionState session;
    SubscriptionEntry entry;
    entry.topic = QStringLiteral("devices/temp");
    entry.recentMessageTimestampsMs = {nowMs};
    session.subscriptions.append(entry);

    SubscriptionListModel model;
    model.setSource(&session);
    QVERIFY(model.updateTopicFps(nowMs));
    session.subscriptions[0].recentMessageTimestampsMs.clear();

    QTimer refreshTimer;
    refreshTimer.setInterval(60'000);
    refreshTimer.start();

    SubscriptionService service;
    service.setDependencies({
        &model,
        nullptr,
        nullptr,
        &refreshTimer,
        [&session]() { return &session; },
        {},
        {},
        {},
    });

    for (int sample = 1; sample < AppUtils::kSubscriptionRateHistorySampleCount; ++sample) {
        service.refreshSubscriptionFps();
        QVERIFY(refreshTimer.isActive());
    }

    service.refreshSubscriptionFps();
    QVERIFY(!refreshTimer.isActive());
    QVERIFY(model.rowAt(0).value(QStringLiteral("topicRateHistory")).toList().isEmpty());
}

QTEST_MAIN(SubscriptionServiceTest)

#include "test_subscriptionservice.moc"
