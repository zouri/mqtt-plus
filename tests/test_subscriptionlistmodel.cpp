#include "domain/session.h"
#include "models/subscriptionfiltermodel.h"
#include "models/subscriptionlistmodel.h"
#include "services/apputils.h"
#include "services/processors/processorlibrary.h"

#include <QDateTime>
#include <QStandardItemModel>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class SubscriptionListModelTest : public QObject
{
    Q_OBJECT

private slots:
    void setSubscriptionsOwnsSnapshot();
    void setSubscriptionsRebuildsProcessorBindingPresentation();
    void sourceSessionChangeClearsTopicRateHistory();
    void samplesTopicRateHistory();
    void separatesNumericRateAndHistoryNotifications();
    void pausedSubscriptionClearsTopicRateHistory();
    void filterSourceChangeNotifiesOnce();
    void filterRowAtUsesPublicRoles();
};

void SubscriptionListModelTest::setSubscriptionsOwnsSnapshot()
{
    SubscriptionListModel model;

    QVector<SubscriptionEntry> subscriptions {
        SubscriptionEntry {.topic = QStringLiteral("devices/first")},
    };
    model.setSubscriptions(QStringLiteral("session-1"), subscriptions, {});
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.rowAt(0).value(QStringLiteral("topic")).toString(), QStringLiteral("devices/first"));

    subscriptions[0].topic = QStringLiteral("devices/second");
    QCOMPARE(model.rowAt(0).value(QStringLiteral("topic")).toString(), QStringLiteral("devices/first"));

    model.setSubscriptions(QStringLiteral("session-1"), subscriptions, {});
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.rowAt(0).value(QStringLiteral("topic")).toString(), QStringLiteral("devices/second"));
}

void SubscriptionListModelTest::setSubscriptionsRebuildsProcessorBindingPresentation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ProcessorLibrary library(directory.path());
    SaveProcessorRevisionCommand command;
    command.name = QStringLiteral("Processor");
    command.content.languageId = QStringLiteral("javascript");
    command.content.runtimeId = QStringLiteral("qt-qjs");
    command.content.entryFile = QStringLiteral("main.js");
    command.content.files = {
        {
            QStringLiteral("main.js"),
            QStringLiteral("text/javascript"),
            QByteArrayLiteral("function process(context) { return context.topic }\n"),
            {},
        },
    };
    const SaveProcessorRevisionResult saved = library.saveRevision(command);
    QVERIFY2(saved.ok, qPrintable(saved.error));

    SubscriptionListModel model;

    SubscriptionEntry subscription;
    subscription.topic = QStringLiteral("devices/temp");
    subscription.processor.processorId = saved.processor.id;
    QVector<SubscriptionEntry> subscriptions {subscription};

    model.setSubscriptions(QStringLiteral("session-1"), subscriptions, &library);
    QCOMPARE(model.rowAt(0).value(QStringLiteral("processorName")).toString(), QStringLiteral("Processor"));
    QVERIFY(model.rowAt(0).value(QStringLiteral("processorBindingAvailable")).toBool());

    QSignalSpy dataSpy(&model, &SubscriptionListModel::dataChanged);
    QSignalSpy resetSpy(&model, &SubscriptionListModel::modelReset);
    QSignalSpy countSpy(&model, &SubscriptionListModel::countChanged);

    command.processorId = saved.processor.id;
    command.name = QStringLiteral("Renamed Processor");
    const SaveProcessorRevisionResult renamed = library.saveRevision(command);
    QVERIFY2(renamed.ok, qPrintable(renamed.error));
    model.setSubscriptions(QStringLiteral("session-1"), subscriptions, &library);

    QCOMPARE(model.rowAt(0).value(QStringLiteral("processorName")).toString(), QStringLiteral("Renamed Processor"));
    QCOMPARE(resetSpy.count(), 0);
    QCOMPARE(countSpy.count(), 0);
    QCOMPARE(dataSpy.count(), 1);
    QCOMPARE(dataSpy.first().at(0).toModelIndex().row(), 0);
    QCOMPARE(dataSpy.first().at(1).toModelIndex().row(), 0);
}

void SubscriptionListModelTest::sourceSessionChangeClearsTopicRateHistory()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    QVector<SubscriptionEntry> subscriptions {
        SubscriptionEntry {.topic = QStringLiteral("devices/temp")},
    };
    subscriptions[0].recentMessages.add(nowMs);
    SubscriptionListModel model;
    model.setSubscriptions(QStringLiteral("session-1"), subscriptions, {});
    QVERIFY(model.updateTopicFps(subscriptions, nowMs));
    QVERIFY(!model.rowAt(0).value(QStringLiteral("topicRateHistory")).toList().isEmpty());

    QSignalSpy dataSpy(&model, &SubscriptionListModel::dataChanged);
    QSignalSpy resetSpy(&model, &SubscriptionListModel::modelReset);
    QSignalSpy countSpy(&model, &SubscriptionListModel::countChanged);
    model.setSubscriptions(QStringLiteral("session-2"), subscriptions, {});

    QVERIFY(model.rowAt(0).value(QStringLiteral("topicRateHistory")).toList().isEmpty());
    QCOMPARE(resetSpy.count(), 1);
    QCOMPARE(dataSpy.count(), 0);
    QCOMPARE(countSpy.count(), 0);
}

void SubscriptionListModelTest::samplesTopicRateHistory()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    SessionState session;
    SubscriptionEntry subscription;
    subscription.topic = QStringLiteral("devices/temp");
    subscription.recentMessages.add(nowMs - 50);
    subscription.recentMessages.add(nowMs - 100);
    session.subscriptions.append(subscription);

    SubscriptionListModel model;
    model.setSubscriptions(QStringLiteral("session-1"), session.subscriptions, {});
    QVERIFY(model.updateTopicFps(session.subscriptions, nowMs));

    QVariantList history = model.rowAt(0).value(QStringLiteral("topicRateHistory")).toList();
    QCOMPARE(history.size(), AppUtils::kSubscriptionRateHistorySampleCount);
    for (int sample = 0; sample < history.size() - 1; ++sample) {
        QCOMPARE(history.at(sample).toReal(), 0.0);
    }
    QCOMPARE(history.constLast().toReal(), 2.0);

    session.subscriptions[0].recentMessages.clear();
    for (int sample = 1; sample < AppUtils::kSubscriptionRateHistorySampleCount; ++sample) {
        QVERIFY(model.updateTopicFps(
            session.subscriptions,
            nowMs + sample * AppUtils::kSubscriptionRateHistorySampleIntervalMs));
    }
    QVERIFY(!model.updateTopicFps(
        session.subscriptions,
        nowMs
        + AppUtils::kSubscriptionRateHistorySampleCount
            * AppUtils::kSubscriptionRateHistorySampleIntervalMs));
    QVERIFY(model.rowAt(0).value(QStringLiteral("topicRateHistory")).toList().isEmpty());

    session.subscriptions[0].topic = QStringLiteral("devices/humidity");
    model.setSubscriptions(QStringLiteral("session-1"), session.subscriptions, {});
    QVERIFY(model.rowAt(0).value(QStringLiteral("topicRateHistory")).toList().isEmpty());

    SessionState otherSession;
    otherSession.subscriptions.append({QStringLiteral("devices/other")});
    model.setSubscriptions(QStringLiteral("session-2"), otherSession.subscriptions, {});
    QVERIFY(model.rowAt(0).value(QStringLiteral("topicRateHistory")).toList().isEmpty());
}

void SubscriptionListModelTest::separatesNumericRateAndHistoryNotifications()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    QVector<SubscriptionEntry> subscriptions {
        SubscriptionEntry {.topic = QStringLiteral("devices/temp")},
    };
    subscriptions[0].recentMessages.add(nowMs);

    SubscriptionListModel model;
    model.setSubscriptions(QStringLiteral("session-1"), subscriptions, {});
    QVERIFY(model.updateTopicFps(subscriptions, nowMs));
    const QVariantList initialHistory = model.rowAt(0)
                                            .value(QStringLiteral("topicRateHistory"))
                                            .toList();

    QSignalSpy dataSpy(&model, &SubscriptionListModel::dataChanged);
    subscriptions[0].recentMessages.add(nowMs + 100);
    QVERIFY(model.updateTopicFps(
        subscriptions,
        nowMs + AppUtils::kSubscriptionFpsRefreshIntervalMs));
    QCOMPARE(model.rowAt(0).value(QStringLiteral("topicFps")).toReal(), 2.0);
    QCOMPARE(
        model.rowAt(0).value(QStringLiteral("topicRateHistory")).toList(),
        initialHistory);
    QCOMPARE(dataSpy.count(), 1);
    QCOMPARE(
        dataSpy.first().at(2).value<QList<int>>(),
        QList<int> {SubscriptionListModel::TopicFpsRole});

    dataSpy.clear();
    QVERIFY(model.updateTopicFps(
        subscriptions,
        nowMs + AppUtils::kSubscriptionFpsRefreshIntervalMs * 2));
    QCOMPARE(dataSpy.count(), 0);

    QVERIFY(model.updateTopicFps(
        subscriptions,
        nowMs + AppUtils::kSubscriptionRateHistorySampleIntervalMs));
    QCOMPARE(dataSpy.count(), 1);
    QCOMPARE(
        dataSpy.first().at(2).value<QList<int>>(),
        QList<int> {SubscriptionListModel::TopicRateHistoryRole});
}

void SubscriptionListModelTest::pausedSubscriptionClearsTopicRateHistory()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    SessionState session;
    SubscriptionEntry subscription;
    subscription.topic = QStringLiteral("devices/temp");
    subscription.recentMessages.add(nowMs);
    session.subscriptions.append(subscription);

    SubscriptionListModel model;
    model.setSubscriptions(QStringLiteral("session-1"), session.subscriptions, {});
    QVERIFY(model.updateTopicFps(session.subscriptions, nowMs));
    QVERIFY(!model.rowAt(0).value(QStringLiteral("topicRateHistory")).toList().isEmpty());

    session.subscriptions[0].paused = true;
    QVERIFY(!model.updateTopicFps(session.subscriptions, nowMs));

    QCOMPARE(model.rowAt(0).value(QStringLiteral("topicFps")).toReal(), 0.0);
    QVERIFY(model.rowAt(0).value(QStringLiteral("topicRateHistory")).toList().isEmpty());
}

void SubscriptionListModelTest::filterSourceChangeNotifiesOnce()
{
    QStandardItemModel firstSource(1, 1);
    QStandardItemModel secondSource(2, 1);
    SubscriptionFilterModel proxy;
    QSignalSpy countSpy(&proxy, &SubscriptionFilterModel::countChanged);

    proxy.setSourceModel(&firstSource);
    QCOMPARE(countSpy.count(), 1);

    countSpy.clear();
    proxy.setSourceModel(&firstSource);
    QCOMPARE(countSpy.count(), 0);

    proxy.setSourceModel(&secondSource);
    QCOMPARE(countSpy.count(), 1);
}

void SubscriptionListModelTest::filterRowAtUsesPublicRoles()
{
    QStandardItemModel source(1, 1);
    source.setItemRoleNames({
        {SubscriptionListModel::TopicRole, "topic"},
        {SubscriptionListModel::AliasRole, "alias"},
        {SubscriptionListModel::DisplayNameRole, "displayName"},
        {SubscriptionListModel::ColorRole, "topicColor"},
        {SubscriptionListModel::PausedRole, "paused"},
        {SubscriptionListModel::StateRole, "subscriptionState"},
    });
    const QModelIndex sourceIndex = source.index(0, 0);
    source.setData(sourceIndex, QStringLiteral("devices/temp"), SubscriptionListModel::TopicRole);
    source.setData(sourceIndex, QStringLiteral("Temperature"), SubscriptionListModel::AliasRole);
    source.setData(sourceIndex, QStringLiteral("Temperature"), SubscriptionListModel::DisplayNameRole);
    source.setData(sourceIndex, QStringLiteral("#336699"), SubscriptionListModel::ColorRole);
    source.setData(sourceIndex, false, SubscriptionListModel::PausedRole);
    source.setData(sourceIndex, QStringLiteral("subscribed"), SubscriptionListModel::StateRole);

    SubscriptionFilterModel proxy;
    proxy.setSourceModel(&source);

    const QVariantMap row = proxy.rowAt(0);
    QCOMPARE(row.value(QStringLiteral("topic")).toString(), QStringLiteral("devices/temp"));
    QCOMPARE(row.value(QStringLiteral("displayName")).toString(), QStringLiteral("Temperature"));
    QCOMPARE(row.value(QStringLiteral("color")).toString(), QStringLiteral("#336699"));
    QCOMPARE(row.value(QStringLiteral("state")).toString(), QStringLiteral("subscribed"));
    QVERIFY(!row.contains(QStringLiteral("topicColor")));
    QVERIFY(!row.contains(QStringLiteral("subscriptionState")));
}

QTEST_MAIN(SubscriptionListModelTest)

#include "test_subscriptionlistmodel.moc"
