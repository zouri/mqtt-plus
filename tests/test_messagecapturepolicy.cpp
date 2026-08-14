#include "domain/messagecapturepolicy.h"
#include "domain/messagerecord.h"

#include <QtTest/QtTest>

class MessageCapturePolicyTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultsToCapturingBothDirections();
    void appliesIncludeAndExcludeTopicFilters();
    void normalizesTopicFilters();
};

void MessageCapturePolicyTest::defaultsToCapturingBothDirections()
{
    MessageCapturePolicy policy;
    QVERIFY(policy.accepts(MessageDirection::Incoming, QStringLiteral("devices/a/temp")));
    QVERIFY(policy.accepts(MessageDirection::Outgoing, QStringLiteral("devices/a/set")));

    policy.captureOutgoing = false;
    QVERIFY(policy.accepts(MessageDirection::Incoming, QStringLiteral("devices/a/temp")));
    QVERIFY(!policy.accepts(MessageDirection::Outgoing, QStringLiteral("devices/a/set")));
}

void MessageCapturePolicyTest::appliesIncludeAndExcludeTopicFilters()
{
    MessageCapturePolicy policy;
    policy.includeTopicFilters = {QStringLiteral("devices/+/temp"), QStringLiteral("alerts/#")};
    policy.excludeTopicFilters = {QStringLiteral("devices/private/#")};

    QVERIFY(policy.accepts(MessageDirection::Incoming, QStringLiteral("devices/room/temp")));
    QVERIFY(policy.accepts(MessageDirection::Incoming, QStringLiteral("alerts/site/critical")));
    QVERIFY(!policy.accepts(MessageDirection::Incoming, QStringLiteral("devices/room/humidity")));
    QVERIFY(!policy.accepts(MessageDirection::Incoming, QStringLiteral("devices/private/temp")));
}

void MessageCapturePolicyTest::normalizesTopicFilters()
{
    MessageCapturePolicy policy;
    policy.includeTopicFilters = {
        QStringLiteral(" devices/# "),
        QString(),
        QStringLiteral("devices/#"),
    };
    policy.excludeTopicFilters = {QStringLiteral("  devices/noisy/#  ")};

    const MessageCapturePolicy normalized = policy.normalized();
    QCOMPARE(normalized.includeTopicFilters, QStringList {QStringLiteral("devices/#")});
    QCOMPARE(normalized.excludeTopicFilters, QStringList {QStringLiteral("devices/noisy/#")});
    QVERIFY(normalized.accepts(MessageDirection::Incoming, QStringLiteral("devices/ok")));
    QVERIFY(!normalized.accepts(MessageDirection::Incoming, QStringLiteral("devices/noisy/a")));
}

QTEST_MAIN(MessageCapturePolicyTest)

#include "test_messagecapturepolicy.moc"
