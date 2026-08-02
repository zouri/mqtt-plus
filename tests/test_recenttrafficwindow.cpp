#include "domain/recenttrafficwindow.h"

#include <QtTest/QtTest>

class RecentTrafficWindowTest : public QObject
{
    Q_OBJECT

private slots:
    void aggregatesRecentEventsAndBytes();
    void keepsExactInclusiveWindowBoundary();
    void storageRemainsFixedUnderHighTraffic();
};

void RecentTrafficWindowTest::aggregatesRecentEventsAndBytes()
{
    RecentTrafficWindow window;
    window.add(10'000, 100);
    window.add(10'050, 200);
    window.add(10'900, 300);

    QCOMPARE(window.eventCount(10'900), 3);
    QCOMPARE(window.byteCount(10'900), qint64(600));
    QCOMPARE(window.eventCount(11'101), 1);
    QCOMPARE(window.byteCount(11'101), qint64(300));

    window.clear();
    QVERIFY(window.isEmpty());
}

void RecentTrafficWindowTest::keepsExactInclusiveWindowBoundary()
{
    RecentTrafficWindow window;
    window.add(10'037, 25);

    QCOMPARE(window.eventCount(11'037), 1);
    QCOMPARE(window.byteCount(11'037), qint64(25));
    QCOMPARE(window.eventCount(11'038), 0);
    QCOMPARE(window.byteCount(11'038), qint64(0));
}

void RecentTrafficWindowTest::storageRemainsFixedUnderHighTraffic()
{
    RecentTrafficWindow window;
    for (int index = 0; index < 100'000; ++index) {
        window.add(20'000 + index % 1000, 1);
    }

    QCOMPARE(window.eventCount(20'999), 100'000);
    QCOMPARE(window.byteCount(20'999), qint64(100'000));
    QVERIFY(window.activeBucketCount() <= RecentTrafficWindow::kBucketCount);

    window.add(22'000, 10);
    QCOMPARE(window.eventCount(22'000), 1);
    QCOMPARE(window.byteCount(22'000), qint64(10));
    QVERIFY(window.activeBucketCount() <= RecentTrafficWindow::kBucketCount);
}

QTEST_MAIN(RecentTrafficWindowTest)

#include "test_recenttrafficwindow.moc"
