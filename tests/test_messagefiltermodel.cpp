#include "models/eventstreammodel.h"
#include "models/messagefiltermodel.h"

#include <QStandardItemModel>
#include <QtTest/QtTest>

class MessageFilterModelTest : public QObject
{
    Q_OBJECT

private slots:
    void filtersTextTopicsAndDirection();
    void hidesDividersOnlyWhileFiltering();
    void reportsVisibleAndTotalMessageCounts();
    void sourceChangesNotifyOnceAndDisconnectOldSource();
    void rowAtUsesPublicRoles();
};

namespace {
QVariantMap messageRow(
    const QString &topic,
    const QString &alias,
    const QString &payload,
    const QString &direction)
{
    return {
        {QStringLiteral("kind"), QStringLiteral("message")},
        {QStringLiteral("topic"), topic},
        {QStringLiteral("alias"), alias},
        {QStringLiteral("payload"), payload},
        {QStringLiteral("payloadFormat"), QStringLiteral("JSON")},
        {QStringLiteral("direction"), direction},
    };
}
}

void MessageFilterModelTest::filtersTextTopicsAndDirection()
{
    EventStreamModel source;
    source.setRows({
        messageRow(QStringLiteral("home/kitchen/temp"), QStringLiteral("Kitchen"), QStringLiteral("23.7"), QStringLiteral("incoming")),
        messageRow(QStringLiteral("home/light/set"), QStringLiteral("Light"), QStringLiteral("on"), QStringLiteral("outgoing")),
    });

    MessageFilterModel proxy;
    proxy.setSourceModel(&source);
    proxy.setFilterText(QStringLiteral("kitchen"));
    QCOMPARE(proxy.count(), 1);
    QCOMPARE(proxy.rowAt(0).value(QStringLiteral("topic")).toString(), QStringLiteral("home/kitchen/temp"));

    proxy.setFilterText({});
    proxy.setSelectedTopics({QStringLiteral("home/+/temp")});
    QCOMPARE(proxy.count(), 1);

    proxy.setSelectedTopics({});
    proxy.setDirection(QStringLiteral("outgoing"));
    QCOMPARE(proxy.count(), 1);
    QCOMPARE(proxy.rowAt(0).value(QStringLiteral("direction")).toString(), QStringLiteral("outgoing"));
}

void MessageFilterModelTest::hidesDividersOnlyWhileFiltering()
{
    EventStreamModel source;
    source.setRows({
        QVariantMap {{QStringLiteral("kind"), QStringLiteral("divider")}},
        messageRow(QStringLiteral("home/light"), QStringLiteral("Light"), QStringLiteral("off"), QStringLiteral("incoming")),
    });

    MessageFilterModel proxy;
    proxy.setSourceModel(&source);
    QCOMPARE(proxy.count(), 2);

    proxy.setFilterText(QStringLiteral("light"));
    QCOMPARE(proxy.count(), 1);
    QCOMPARE(proxy.rowAt(0).value(QStringLiteral("kind")).toString(), QStringLiteral("message"));
}

void MessageFilterModelTest::reportsVisibleAndTotalMessageCounts()
{
    EventStreamModel source;
    source.setRows({
        QVariantMap {{QStringLiteral("kind"), QStringLiteral("divider")}},
        messageRow(QStringLiteral("home/light"), QStringLiteral("Light"), QStringLiteral("off"), QStringLiteral("incoming")),
        messageRow(QStringLiteral("home/light/set"), QString(), QStringLiteral("on"), QStringLiteral("outgoing")),
    });

    MessageFilterModel proxy;
    proxy.setSourceModel(&source);
    QCOMPARE(proxy.filteredMessageCount(), 2);
    QCOMPARE(proxy.totalMessageCount(), 2);

    proxy.setDirection(QStringLiteral("outgoing"));
    QCOMPARE(proxy.filteredMessageCount(), 1);
    QCOMPARE(proxy.totalMessageCount(), 2);

    source.appendRow(messageRow(
        QStringLiteral("home/kitchen/temp"),
        QStringLiteral("Kitchen"),
        QStringLiteral("23.7"),
        QStringLiteral("outgoing")));
    QCOMPARE(proxy.filteredMessageCount(), 2);
    QCOMPARE(proxy.totalMessageCount(), 3);
}

void MessageFilterModelTest::sourceChangesNotifyOnceAndDisconnectOldSource()
{
    EventStreamModel firstSource;
    firstSource.setRows({
        messageRow(QStringLiteral("first/topic"), {}, {}, QStringLiteral("incoming")),
    });
    EventStreamModel secondSource;
    secondSource.setRows({
        messageRow(QStringLiteral("second/topic"), {}, {}, QStringLiteral("incoming")),
        messageRow(QStringLiteral("second/other"), {}, {}, QStringLiteral("outgoing")),
    });

    MessageFilterModel proxy;
    QSignalSpy countSpy(&proxy, &MessageFilterModel::countChanged);
    QSignalSpy messageCountsSpy(&proxy, &MessageFilterModel::messageCountsChanged);

    proxy.setSourceModel(&firstSource);
    QCOMPARE(countSpy.count(), 1);
    QCOMPARE(messageCountsSpy.count(), 1);

    countSpy.clear();
    messageCountsSpy.clear();
    proxy.setSourceModel(&firstSource);
    QCOMPARE(countSpy.count(), 0);
    QCOMPARE(messageCountsSpy.count(), 0);

    proxy.setSourceModel(&secondSource);
    QCOMPARE(countSpy.count(), 1);
    QCOMPARE(messageCountsSpy.count(), 1);

    countSpy.clear();
    messageCountsSpy.clear();
    firstSource.appendRow(messageRow(
        QStringLiteral("first/stale"), {}, {}, QStringLiteral("incoming")));
    QCOMPARE(countSpy.count(), 0);
    QCOMPARE(messageCountsSpy.count(), 0);

    secondSource.appendRow(messageRow(
        QStringLiteral("second/new"), {}, {}, QStringLiteral("incoming")));
    QCOMPARE(countSpy.count(), 1);
    QCOMPARE(messageCountsSpy.count(), 1);

    messageCountsSpy.clear();
    proxy.setDirection(QStringLiteral("outgoing"));
    QCOMPARE(messageCountsSpy.count(), 1);
}

void MessageFilterModelTest::rowAtUsesPublicRoles()
{
    QStandardItemModel source(1, 1);
    source.setItemRoleNames({
        {EventStreamModel::KindRole, "kind"},
        {EventStreamModel::TopicRole, "topic"},
        {EventStreamModel::PayloadRole, "payload"},
        {EventStreamModel::DirectionRole, "direction"},
    });
    const QModelIndex sourceIndex = source.index(0, 0);
    source.setData(sourceIndex, QStringLiteral("message"), EventStreamModel::KindRole);
    source.setData(sourceIndex, QStringLiteral("generic/topic"), EventStreamModel::TopicRole);
    source.setData(sourceIndex, QStringLiteral("payload"), EventStreamModel::PayloadRole);
    source.setData(sourceIndex, QStringLiteral("incoming"), EventStreamModel::DirectionRole);

    MessageFilterModel proxy;
    proxy.setSourceModel(&source);

    const QVariantMap row = proxy.rowAt(0);
    QCOMPARE(row.value(QStringLiteral("kind")).toString(), QStringLiteral("message"));
    QCOMPARE(row.value(QStringLiteral("topic")).toString(), QStringLiteral("generic/topic"));
    QCOMPARE(row.value(QStringLiteral("payload")).toString(), QStringLiteral("payload"));
    QCOMPARE(row.value(QStringLiteral("direction")).toString(), QStringLiteral("incoming"));
}

QTEST_MAIN(MessageFilterModelTest)

#include "test_messagefiltermodel.moc"
