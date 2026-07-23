#include "usecases/sessionservice.h"
#include "domain/session.h"
#include "models/subscriptionlistmodel.h"
#include "services/apputils.h"

#include <QtTest/QtTest>
#include <QDateTime>

class SubscriptionListModelTest : public QObject
{
    Q_OBJECT

private slots:
    void refreshBindsCurrentSession();
    void refreshRebuildsScriptNameCache();
    void samplesTopicRateHistory();
    void pausedSubscriptionClearsTopicRateHistory();
};

void SubscriptionListModelTest::refreshBindsCurrentSession()
{
    SessionService sessions;
    SubscriptionListModel model;

    SessionState first;
    first.subscriptions.append({QStringLiteral("devices/first")});
    SessionState second;
    second.subscriptions.append({QStringLiteral("devices/second")});
    sessions.appendSession(first);
    sessions.appendSession(second);

    sessions.setCurrentIndex(0);
    model.setSource(sessions.currentSession());
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.rowAt(0).value(QStringLiteral("topic")).toString(), QStringLiteral("devices/first"));

    sessions.setCurrentIndex(1);
    model.setSource(sessions.currentSession());
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.rowAt(0).value(QStringLiteral("topic")).toString(), QStringLiteral("devices/second"));
}

void SubscriptionListModelTest::refreshRebuildsScriptNameCache()
{
    SessionService sessions;
    SubscriptionListModel model;
    QString scriptName = QStringLiteral("Decoder");

    model.setScriptNameLookup([&scriptName](const QString &id) {
        return id == QStringLiteral("script-1") ? scriptName : QString();
    });

    SessionState session;
    SubscriptionEntry subscription;
    subscription.topic = QStringLiteral("devices/temp");
    subscription.scriptId = QStringLiteral("script-1");
    session.subscriptions.append(subscription);
    sessions.appendSession(session);
    sessions.setCurrentIndex(0);

    model.setSource(sessions.currentSession());
    QCOMPARE(model.rowAt(0).value(QStringLiteral("scriptName")).toString(), QStringLiteral("Decoder"));

    QSignalSpy dataSpy(&model, &SubscriptionListModel::dataChanged);
    QSignalSpy resetSpy(&model, &SubscriptionListModel::modelReset);
    QSignalSpy countSpy(&model, &SubscriptionListModel::countChanged);

    scriptName = QStringLiteral("Pretty Decoder");
    model.setSource(sessions.currentSession());

    QCOMPARE(model.rowAt(0).value(QStringLiteral("scriptName")).toString(), QStringLiteral("Pretty Decoder"));
    QCOMPARE(resetSpy.count(), 0);
    QCOMPARE(countSpy.count(), 0);
    QCOMPARE(dataSpy.count(), 1);
    QCOMPARE(dataSpy.first().at(0).toModelIndex().row(), 0);
    QCOMPARE(dataSpy.first().at(1).toModelIndex().row(), 0);
}

void SubscriptionListModelTest::samplesTopicRateHistory()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    SessionState session;
    SubscriptionEntry subscription;
    subscription.topic = QStringLiteral("devices/temp");
    subscription.recentMessageTimestampsMs = {nowMs - 50, nowMs - 100};
    session.subscriptions.append(subscription);

    SubscriptionListModel model;
    model.setSource(&session);
    QVERIFY(model.updateTopicFps(nowMs));

    QVariantList history = model.rowAt(0).value(QStringLiteral("topicRateHistory")).toList();
    QCOMPARE(history.size(), AppUtils::kSubscriptionRateHistorySampleCount);
    for (int sample = 0; sample < history.size() - 1; ++sample) {
        QCOMPARE(history.at(sample).toReal(), 0.0);
    }
    QCOMPARE(history.constLast().toReal(), 2.0);

    session.subscriptions[0].recentMessageTimestampsMs.clear();
    for (int sample = 1; sample < AppUtils::kSubscriptionRateHistorySampleCount; ++sample) {
        QVERIFY(model.updateTopicFps(
            nowMs + sample * AppUtils::kSubscriptionFpsRefreshIntervalMs));
    }
    QVERIFY(!model.updateTopicFps(
        nowMs
        + AppUtils::kSubscriptionRateHistorySampleCount
            * AppUtils::kSubscriptionFpsRefreshIntervalMs));
    QVERIFY(model.rowAt(0).value(QStringLiteral("topicRateHistory")).toList().isEmpty());

    session.subscriptions[0].topic = QStringLiteral("devices/humidity");
    model.setSource(&session);
    QVERIFY(model.rowAt(0).value(QStringLiteral("topicRateHistory")).toList().isEmpty());

    SessionState otherSession;
    otherSession.subscriptions.append({QStringLiteral("devices/other")});
    model.setSource(&otherSession);
    QVERIFY(model.rowAt(0).value(QStringLiteral("topicRateHistory")).toList().isEmpty());
}

void SubscriptionListModelTest::pausedSubscriptionClearsTopicRateHistory()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    SessionState session;
    SubscriptionEntry subscription;
    subscription.topic = QStringLiteral("devices/temp");
    subscription.recentMessageTimestampsMs = {nowMs};
    session.subscriptions.append(subscription);

    SubscriptionListModel model;
    model.setSource(&session);
    QVERIFY(model.updateTopicFps(nowMs));
    QVERIFY(!model.rowAt(0).value(QStringLiteral("topicRateHistory")).toList().isEmpty());

    session.subscriptions[0].paused = true;
    model.notifyRefresh();

    QCOMPARE(model.rowAt(0).value(QStringLiteral("topicFps")).toReal(), 0.0);
    QVERIFY(model.rowAt(0).value(QStringLiteral("topicRateHistory")).toList().isEmpty());
}

QTEST_MAIN(SubscriptionListModelTest)

#include "test_subscriptionlistmodel.moc"
