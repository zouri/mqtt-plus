#include "domain/mqtttopicfilter.h"

#include <QtTest/QtTest>

class MqttTopicFilterTest : public QObject
{
    Q_OBJECT

private slots:
    void matchesMqttWildcards();
    void rejectsInvalidMultiLevelWildcardPlacement();
    void scoresSpecificity();
};

void MqttTopicFilterTest::matchesMqttWildcards()
{
    QVERIFY(MqttTopicFilter::matches(
        QStringLiteral("devices/+/temp"),
        QStringLiteral("devices/a/temp")));
    QVERIFY(!MqttTopicFilter::matches(
        QStringLiteral("devices/+/temp"),
        QStringLiteral("devices/a/humidity")));
    QVERIFY(MqttTopicFilter::matches(
        QStringLiteral("devices/#"),
        QStringLiteral("devices/a/temp")));
    QVERIFY(!MqttTopicFilter::matches(
        QString(),
        QStringLiteral("devices/a/temp")));
}

void MqttTopicFilterTest::rejectsInvalidMultiLevelWildcardPlacement()
{
    QVERIFY(!MqttTopicFilter::matches(
        QStringLiteral("devices/#/temp"),
        QStringLiteral("devices/a/temp")));
}

void MqttTopicFilterTest::scoresSpecificity()
{
    QCOMPARE(MqttTopicFilter::specificityScore(QStringLiteral("devices/#")), 8);
    QCOMPARE(MqttTopicFilter::specificityScore(QStringLiteral("devices/+/temp")), 13);
    QCOMPARE(MqttTopicFilter::specificityScore(QStringLiteral("devices/a/temp")), 14);
}

QTEST_MAIN(MqttTopicFilterTest)

#include "test_mqtttopicfilter.moc"
