#include "models/eventstreammodel.h"
#include "models/messagefiltermodel.h"

#include <QtTest/QtTest>

class MessageFilterModelTest : public QObject
{
    Q_OBJECT

private slots:
    void filtersTextTopicsAndDirection();
    void hidesDividersOnlyWhileFiltering();
    void reportsVisibleAndTotalMessageCounts();
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

QTEST_MAIN(MessageFilterModelTest)

#include "test_messagefiltermodel.moc"
