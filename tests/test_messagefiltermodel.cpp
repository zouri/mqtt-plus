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
    void countsRowsAcceptedByCurrentFilter();
    void findsHistoryIdInFilteredRows();
};

namespace {
EventRow messageRow(
    const QString &topic,
    const QString &alias,
    const QString &payload,
    const QString &direction)
{
    EventRow row;
    row.kind = QStringLiteral("message");
    row.topic = topic;
    row.alias = alias;
    row.payload = payload;
    row.payloadFormat = QStringLiteral("JSON");
    row.direction = direction;
    return row;
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
        EventRow {.kind = QStringLiteral("divider")},
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
        EventRow {.kind = QStringLiteral("divider")},
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
    QTRY_COMPARE(messageCountsSpy.count(), 1);

    countSpy.clear();
    messageCountsSpy.clear();
    proxy.setSourceModel(&firstSource);
    QCOMPARE(countSpy.count(), 0);
    QCOMPARE(messageCountsSpy.count(), 0);

    proxy.setSourceModel(&secondSource);
    QCOMPARE(countSpy.count(), 1);
    QTRY_COMPARE(messageCountsSpy.count(), 1);

    countSpy.clear();
    messageCountsSpy.clear();
    firstSource.appendRow(messageRow(
        QStringLiteral("first/stale"), {}, {}, QStringLiteral("incoming")));
    QCOMPARE(countSpy.count(), 0);
    QCOMPARE(messageCountsSpy.count(), 0);

    secondSource.appendRow(messageRow(
        QStringLiteral("second/new"), {}, {}, QStringLiteral("incoming")));
    QCOMPARE(countSpy.count(), 1);
    QTRY_COMPARE(messageCountsSpy.count(), 1);

    messageCountsSpy.clear();
    proxy.setDirection(QStringLiteral("outgoing"));
    QTRY_COMPARE(messageCountsSpy.count(), 1);
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

void MessageFilterModelTest::countsRowsAcceptedByCurrentFilter()
{
    MessageFilterModel proxy;
    proxy.setSelectedTopics({QStringLiteral("home/+/temp")});
    proxy.setDirection(QStringLiteral("incoming"));

    QCOMPARE(
        proxy.matchingMessageCount(QVector<EventRow> {
            messageRow(
                QStringLiteral("home/kitchen/temp"),
                QStringLiteral("Kitchen"),
                QStringLiteral("23.7"),
                QStringLiteral("incoming")),
            messageRow(
                QStringLiteral("home/kitchen/temp"),
                QStringLiteral("Kitchen"),
                QStringLiteral("23.8"),
                QStringLiteral("outgoing")),
            messageRow(
                QStringLiteral("home/light"),
                QStringLiteral("Light"),
                QStringLiteral("on"),
                QStringLiteral("incoming")),
        }),
        1);
}

void MessageFilterModelTest::findsHistoryIdInFilteredRows()
{
    EventStreamModel source;
    EventRow first = messageRow(
        QStringLiteral("home/kitchen/temp"),
        QStringLiteral("Kitchen"),
        QStringLiteral("23.7"),
        QStringLiteral("incoming"));
    first.historyId = 41;
    EventRow second = messageRow(
        QStringLiteral("home/light"),
        QStringLiteral("Light"),
        QStringLiteral("on"),
        QStringLiteral("incoming"));
    second.historyId = 42;
    source.setRows({first, second});

    MessageFilterModel proxy;
    proxy.setSourceModel(&source);
    proxy.setSelectedTopics({QStringLiteral("home/+/temp")});

    QCOMPARE(proxy.indexOfHistoryId(QStringLiteral("41")), 0);
    QCOMPARE(proxy.indexOfHistoryId(QStringLiteral("42")), -1);
}

QTEST_MAIN(MessageFilterModelTest)

#include "test_messagefiltermodel.moc"
