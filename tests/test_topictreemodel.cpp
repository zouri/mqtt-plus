#include "models/topictreemodel.h"

#include <QtTest/QtTest>

namespace {
int rowForTopic(const TopicTreeModel &model, const QString &topic)
{
    for (int row = 0; row < model.rowCount(); ++row) {
        if (model.rowAt(row).value(QStringLiteral("fullTopic")).toString() == topic) {
            return row;
        }
    }
    return -1;
}

TopicObservation observation(
    const QString &topic,
    qint64 historyId,
    qint64 observedAtMs,
    const QString &preview)
{
    return {
        .topic = topic,
        .historyId = historyId,
        .observedAtMs = observedAtMs,
        .payloadPreview = preview,
    };
}
} // namespace

class TopicTreeModelTest : public QObject
{
    Q_OBJECT

private slots:
    void buildsHierarchyAndPreservesMqttLevels();
    void searchRevealsAncestorsWithoutChangingExpansion();
    void searchMatchesLatestExactPayload();
    void keepsExactValuesSeparateFromSubtreeActivity();
    void isolatesSessions();
    void preservesExpansionWhenRefreshingSameSession();
    void capsHighCardinalityTopics();
    void replacesOldestTopicWhenCapacityIsFull();
};

void TopicTreeModelTest::buildsHierarchyAndPreservesMqttLevels()
{
    TopicTreeModel model;
    model.resetTopics(QStringLiteral("session-1"), {
        observation(QStringLiteral("sensors"), 1, 100, QStringLiteral("root")),
        observation(QStringLiteral("sensors/room/temp"), 2, 200, QStringLiteral("21")),
        observation(QStringLiteral("sensors/room/humidity"), 3, 300, QStringLiteral("45")),
        observation(QStringLiteral("/leading"), 4, 400, QStringLiteral("leading")),
        observation(QStringLiteral("trailing/"), 5, 500, QStringLiteral("trailing")),
        observation(QStringLiteral("middle//empty"), 6, 600, QStringLiteral("empty")),
    });

    QCOMPARE(model.rowCount(), 4);
    const int sensorsRow = rowForTopic(model, QStringLiteral("sensors"));
    QVERIFY(sensorsRow >= 0);
    const QVariantMap sensors = model.rowAt(sensorsRow);
    QVERIFY(sensors.value(QStringLiteral("isTopic")).toBool());
    QVERIFY(sensors.value(QStringLiteral("hasChildren")).toBool());

    model.toggleExpanded(sensorsRow);
    const int roomRow = rowForTopic(model, QStringLiteral("sensors/room"));
    QVERIFY(roomRow >= 0);
    QCOMPARE(model.rowAt(roomRow).value(QStringLiteral("depth")).toInt(), 1);
    model.toggleExpanded(roomRow);
    QVERIFY(rowForTopic(model, QStringLiteral("sensors/room/temp")) >= 0);
    QVERIFY(rowForTopic(model, QStringLiteral("sensors/room/humidity")) >= 0);

    model.setSearchText(QStringLiteral("/leading"));
    QVERIFY(rowForTopic(model, QStringLiteral("/leading")) >= 0);
    model.setSearchText(QStringLiteral("trailing/"));
    QVERIFY(rowForTopic(model, QStringLiteral("trailing/")) >= 0);
    model.setSearchText(QStringLiteral("middle//empty"));
    QVERIFY(rowForTopic(model, QStringLiteral("middle//empty")) >= 0);
}

void TopicTreeModelTest::searchRevealsAncestorsWithoutChangingExpansion()
{
    TopicTreeModel model;
    model.resetTopics(QStringLiteral("session-1"), {
        observation(QStringLiteral("sensors/room/temp"), 1, 100, QStringLiteral("21")),
        observation(QStringLiteral("devices/status"), 2, 200, QStringLiteral("online")),
    });

    QCOMPARE(model.rowCount(), 2);
    model.setSearchText(QStringLiteral("TEMP"));
    QCOMPARE(model.rowCount(), 3);
    QVERIFY(rowForTopic(model, QStringLiteral("sensors")) >= 0);
    QVERIFY(rowForTopic(model, QStringLiteral("sensors/room")) >= 0);
    QVERIFY(rowForTopic(model, QStringLiteral("sensors/room/temp")) >= 0);

    model.setSearchText({});
    QCOMPARE(model.rowCount(), 2);
    QVERIFY(rowForTopic(model, QStringLiteral("sensors/room")) < 0);
}

void TopicTreeModelTest::searchMatchesLatestExactPayload()
{
    TopicTreeModel model;
    model.resetTopics(QStringLiteral("session-1"), {
        observation(QStringLiteral("sensors/room/temp"), 1, 100, QStringLiteral("boiler-hot")),
        observation(QStringLiteral("devices/status"), 2, 200, QStringLiteral("online")),
    });

    model.setSearchText(QStringLiteral("BOILER"));
    QCOMPARE(model.rowCount(), 3);
    QVERIFY(rowForTopic(model, QStringLiteral("sensors")) >= 0);
    QVERIFY(rowForTopic(model, QStringLiteral("sensors/room")) >= 0);
    QVERIFY(rowForTopic(model, QStringLiteral("sensors/room/temp")) >= 0);
    QVERIFY(rowForTopic(model, QStringLiteral("devices")) < 0);
}

void TopicTreeModelTest::keepsExactValuesSeparateFromSubtreeActivity()
{
    TopicTreeModel model;
    model.resetTopics(QStringLiteral("session-1"), {
        observation(QStringLiteral("sensors"), 1, 100, QStringLiteral("parent")),
        observation(QStringLiteral("sensors/temp"), 2, 200, QStringLiteral("newer")),
        observation(QStringLiteral("sensors/humidity"), 3, 300, QStringLiteral("latest")),
    });

    int sensorsRow = rowForTopic(model, QStringLiteral("sensors"));
    QCOMPARE(model.rowAt(sensorsRow).value(QStringLiteral("latestHistoryId")).toString(), QStringLiteral("1"));
    QCOMPARE(model.rowAt(sensorsRow).value(QStringLiteral("lastSeenMs")).toLongLong(), qint64(100));
    QCOMPARE(model.rowAt(sensorsRow).value(QStringLiteral("latestPayloadPreview")).toString(), QStringLiteral("parent"));
    QCOMPARE(model.rowAt(sensorsRow).value(QStringLiteral("subtreeLastSeenMs")).toLongLong(), qint64(300));

    model.observeTopics(QStringLiteral("session-1"), {
        observation(QStringLiteral("sensors/temp"), 1, 50, QStringLiteral("stale")),
    });
    QCOMPARE(model.rowAt(sensorsRow).value(QStringLiteral("latestPayloadPreview")).toString(), QStringLiteral("parent"));
    QCOMPARE(model.rowAt(sensorsRow).value(QStringLiteral("subtreeLastSeenMs")).toLongLong(), qint64(300));

    model.observeTopics(QStringLiteral("session-1"), {
        observation(QStringLiteral("sensors/temp"), 4, 400, QStringLiteral("hottest")),
    });
    sensorsRow = rowForTopic(model, QStringLiteral("sensors"));
    QCOMPARE(model.rowAt(sensorsRow).value(QStringLiteral("latestHistoryId")).toString(), QStringLiteral("1"));
    QCOMPARE(model.rowAt(sensorsRow).value(QStringLiteral("lastSeenMs")).toLongLong(), qint64(100));
    QCOMPARE(model.rowAt(sensorsRow).value(QStringLiteral("latestPayloadPreview")).toString(), QStringLiteral("parent"));
    QCOMPARE(model.rowAt(sensorsRow).value(QStringLiteral("subtreeLastSeenMs")).toLongLong(), qint64(400));
}

void TopicTreeModelTest::isolatesSessions()
{
    TopicTreeModel model;
    model.resetTopics(QStringLiteral("session-1"), {
        observation(QStringLiteral("one/topic"), 1, 100, QStringLiteral("one")),
    });
    model.observeTopics(QStringLiteral("session-2"), {
        observation(QStringLiteral("two/topic"), 2, 200, QStringLiteral("two")),
    });
    QVERIFY(rowForTopic(model, QStringLiteral("one")) >= 0);
    QVERIFY(rowForTopic(model, QStringLiteral("two")) < 0);

    model.resetTopics(QStringLiteral("session-2"), {
        observation(QStringLiteral("two/topic"), 2, 200, QStringLiteral("two")),
    });
    QVERIFY(rowForTopic(model, QStringLiteral("one")) < 0);
    QVERIFY(rowForTopic(model, QStringLiteral("two")) >= 0);
}

void TopicTreeModelTest::preservesExpansionWhenRefreshingSameSession()
{
    TopicTreeModel model;
    model.resetTopics(QStringLiteral("session-1"), {
        observation(QStringLiteral("devices/one/status"), 1, 100, QStringLiteral("online")),
    });

    const int devicesRow = rowForTopic(model, QStringLiteral("devices"));
    model.toggleExpanded(devicesRow);
    QVERIFY(rowForTopic(model, QStringLiteral("devices/one")) >= 0);

    model.resetTopics(QStringLiteral("session-1"), {
        observation(QStringLiteral("devices/one/status"), 2, 200, QStringLiteral("ready")),
    });
    QVERIFY(rowForTopic(model, QStringLiteral("devices/one")) >= 0);

    model.resetTopics(QStringLiteral("session-2"), {
        observation(QStringLiteral("devices/two/status"), 3, 300, QStringLiteral("online")),
    });
    QVERIFY(rowForTopic(model, QStringLiteral("devices/two")) < 0);
}

void TopicTreeModelTest::capsHighCardinalityTopics()
{
    QVector<TopicObservation> observations;
    observations.reserve(10'001);
    for (int topic = 0; topic < 10'001; ++topic) {
        observations.append(observation(
            QStringLiteral("topic-%1").arg(topic),
            topic + 1,
            topic + 1,
            QString::number(topic)));
    }

    TopicTreeModel model;
    model.resetTopics(QStringLiteral("session-1"), observations);
    QCOMPARE(model.rowCount(), 10'000);
    QVERIFY(model.truncated());
    QVERIFY(rowForTopic(model, QStringLiteral("topic-10000")) >= 0);
    QVERIFY(rowForTopic(model, QStringLiteral("topic-0")) < 0);
}

void TopicTreeModelTest::replacesOldestTopicWhenCapacityIsFull()
{
    QVector<TopicObservation> observations;
    observations.reserve(10'000);
    for (int topic = 0; topic < 10'000; ++topic) {
        observations.append(observation(
            QStringLiteral("topic-%1").arg(topic),
            topic + 1,
            topic + 1,
            QString::number(topic)));
    }

    TopicTreeModel model;
    model.resetTopics(QStringLiteral("session-1"), observations);
    model.observeTopics(QStringLiteral("session-1"), {
        observation(QStringLiteral("topic-0"), 10'001, 10'001, QStringLiteral("updated")),
        observation(QStringLiteral("topic-new"), 10'002, 10'002, QStringLiteral("new")),
    });

    QCOMPARE(model.rowCount(), 10'000);
    QVERIFY(model.truncated());
    QVERIFY(rowForTopic(model, QStringLiteral("topic-new")) >= 0);
    QVERIFY(rowForTopic(model, QStringLiteral("topic-0")) >= 0);
    QVERIFY(rowForTopic(model, QStringLiteral("topic-1")) < 0);
}

QTEST_MAIN(TopicTreeModelTest)

#include "test_topictreemodel.moc"
