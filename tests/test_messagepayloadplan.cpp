#include "services/messaging/messagepayloadplan.h"

#include <QtTest/QtTest>

class MessagePayloadPlanTest : public QObject
{
    Q_OBJECT

private slots:
    void keepsTextPayloadsForStorage();
    void identifiesBinaryPayloads();
    void rejectsPayloadsAboveTheConfiguredLimit();
    void boundsPressurePreviews();
    void identifiesBackgroundParseFormats();
};

void MessagePayloadPlanTest::keepsTextPayloadsForStorage()
{
    const QByteArray payload("hello");
    const MessagePayload::Plan plan = MessagePayload::planStorage(
        QStringLiteral("devices/a"), payload, 1024);

    QCOMPARE(plan.storedBytes, payload);
    QCOMPARE(plan.preview, QStringLiteral("hello"));
    QCOMPARE(plan.state, QStringLiteral("full"));
    QVERIFY(plan.allowFullProcessing);
    QVERIFY(!plan.shouldReport);
}

void MessagePayloadPlanTest::identifiesBinaryPayloads()
{
    const QByteArray payload = QByteArray::fromHex("00010203");
    const MessagePayload::Plan plan = MessagePayload::planStorage(
        QStringLiteral("devices/binary"), payload, 1024);

    QCOMPARE(plan.storedBytes, payload);
    QCOMPARE(plan.preview, QStringLiteral("00 01 02 03"));
    QCOMPARE(plan.state, QStringLiteral("raw_only"));
    QVERIFY(plan.shouldReport);
}

void MessagePayloadPlanTest::rejectsPayloadsAboveTheConfiguredLimit()
{
    const QByteArray payload("payload");
    const MessagePayload::Plan plan = MessagePayload::planStorage(
        QStringLiteral("devices/large"), payload, 3);

    QVERIFY(plan.storedBytes.isEmpty());
    QCOMPARE(plan.state, QStringLiteral("skipped"));
    QVERIFY(!plan.allowFullProcessing);
    QCOMPARE(plan.hash.size(), 64);
    QVERIFY(plan.reportMessage.contains(QStringLiteral("devices/large")));
}

void MessagePayloadPlanTest::boundsPressurePreviews()
{
    const QByteArray payload(5000, 'x');
    const MessagePayload::Plan plan = MessagePayload::planStorage(
        QStringLiteral("devices/pressure"), payload, 10000, true);

    QCOMPARE(plan.storedBytes, payload);
    QCOMPARE(plan.preview.size(), 4 * 1024);
    QCOMPARE(plan.state, QStringLiteral("full"));
}

void MessagePayloadPlanTest::identifiesBackgroundParseFormats()
{
    QVERIFY(MessagePayload::requiresBackgroundParse(PayloadFormat::Json));
    QVERIFY(MessagePayload::requiresBackgroundParse(PayloadFormat::Cbor));
    QVERIFY(MessagePayload::requiresBackgroundParse(PayloadFormat::MsgPack));
    QVERIFY(!MessagePayload::requiresBackgroundParse(PayloadFormat::Plaintext));
    QVERIFY(!MessagePayload::requiresBackgroundParse(PayloadFormat::Base64));
    QVERIFY(!MessagePayload::requiresBackgroundParse(PayloadFormat::Hex));
}

QTEST_MAIN(MessagePayloadPlanTest)

#include "test_messagepayloadplan.moc"
