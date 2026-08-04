#include "services/payload/payloadcodec.h"

#include <QtTest/QtTest>

class PayloadCodecTest : public QObject
{
    Q_OBJECT

private slots:
    void formatMapping();
    void stableFormatIds();
    void encodePlaintextAndJson();
    void emptyPayloadIsValidForEveryFormat();
    void rejectInvalidStructuredPayloads();
    void encodeBinaryTextFormats();
    void decodeDisplayFormats();
    void matchTopicFilters();
    void scoreTopicSpecificity();
    void resolveMostSpecificTopicFormat();
};

void PayloadCodecTest::formatMapping()
{
    QCOMPARE(PayloadCodec::formatNames(), QStringList({
        QStringLiteral("Plaintext"),
        QStringLiteral("JSON"),
        QStringLiteral("Base64"),
        QStringLiteral("Hex"),
        QStringLiteral("CBOR"),
        QStringLiteral("MsgPack"),
    }));
    QCOMPARE(PayloadCodec::formatFromInt(0), PayloadFormat::Plaintext);
    QCOMPARE(PayloadCodec::formatFromInt(5), PayloadFormat::MsgPack);
    QCOMPARE(PayloadCodec::formatFromInt(99), PayloadFormat::Plaintext);
    QCOMPARE(PayloadCodec::formatName(PayloadFormat::Cbor), QStringLiteral("CBOR"));
}

void PayloadCodecTest::stableFormatIds()
{
    const QList<PayloadFormat> formats {
        PayloadFormat::Plaintext,
        PayloadFormat::Json,
        PayloadFormat::Base64,
        PayloadFormat::Hex,
        PayloadFormat::Cbor,
        PayloadFormat::MsgPack,
    };
    for (PayloadFormat format : formats) {
        bool ok = false;
        QCOMPARE(PayloadCodec::formatFromId(PayloadCodec::formatId(format), &ok), format);
        QVERIFY(ok);
    }
}

void PayloadCodecTest::encodePlaintextAndJson()
{
    QByteArray output;
    QString error;

    QVERIFY(PayloadCodec::encodeForPublish(PayloadFormat::Plaintext, QStringLiteral("hello"), output, error));
    QCOMPARE(output, QByteArray("hello"));
    QVERIFY(error.isEmpty());

    QVERIFY(PayloadCodec::encodeForPublish(PayloadFormat::Json, QStringLiteral("{\"a\": 1}"), output, error));
    QCOMPARE(output, QByteArray("{\"a\":1}"));
    QVERIFY(error.isEmpty());
}

void PayloadCodecTest::emptyPayloadIsValidForEveryFormat()
{
    for (int value = 0; value <= 5; ++value) {
        QByteArray output("stale");
        QString error(QStringLiteral("stale"));
        QVERIFY(PayloadCodec::encodeForPublish(
            PayloadCodec::formatFromInt(value), QString(), output, error));
        QVERIFY(output.isEmpty());
        QVERIFY(error.isEmpty());
    }
}

void PayloadCodecTest::rejectInvalidStructuredPayloads()
{
    QByteArray output;
    QString error;

    QVERIFY(!PayloadCodec::encodeForPublish(PayloadFormat::Json, QStringLiteral("not json"), output, error));
    QVERIFY(!error.isEmpty());

    QVERIFY(!PayloadCodec::encodeForPublish(PayloadFormat::Base64, QStringLiteral("***"), output, error));
    QVERIFY(!error.isEmpty());

    QVERIFY(!PayloadCodec::encodeForPublish(PayloadFormat::Hex, QStringLiteral("abc"), output, error));
    QVERIFY(!error.isEmpty());
}

void PayloadCodecTest::encodeBinaryTextFormats()
{
    QByteArray output;
    QString error;

    QVERIFY(PayloadCodec::encodeForPublish(PayloadFormat::Base64, QStringLiteral("aGVsbG8="), output, error));
    QCOMPARE(output, QByteArray("hello"));

    QVERIFY(PayloadCodec::encodeForPublish(PayloadFormat::Hex, QStringLiteral("68 65 6c 6c 6f"), output, error));
    QCOMPARE(output, QByteArray("hello"));
}

void PayloadCodecTest::decodeDisplayFormats()
{
    QString error;

    QCOMPARE(PayloadCodec::decodeForDisplay(PayloadFormat::Plaintext, QByteArray("hello"), error), QStringLiteral("hello"));
    QVERIFY(error.isEmpty());

    const QString json = PayloadCodec::decodeForDisplay(PayloadFormat::Json, QByteArray("{\"a\":1}"), error);
    QVERIFY(error.isEmpty());
    QVERIFY(json.contains(QStringLiteral("\"a\": 1")));

    QCOMPARE(PayloadCodec::decodeForDisplay(PayloadFormat::Base64, QByteArray("hello"), error), QStringLiteral("aGVsbG8="));
    QCOMPARE(PayloadCodec::decodeForDisplay(PayloadFormat::Hex, QByteArray("hello"), error), QStringLiteral("68 65 6C 6C 6F"));
}

void PayloadCodecTest::matchTopicFilters()
{
    QVERIFY(PayloadCodec::topicFilterMatches(QStringLiteral("devices/+/temp"), QStringLiteral("devices/a/temp")));
    QVERIFY(!PayloadCodec::topicFilterMatches(QStringLiteral("devices/+/temp"), QStringLiteral("devices/a/humidity")));
    QVERIFY(PayloadCodec::topicFilterMatches(QStringLiteral("devices/#"), QStringLiteral("devices/a/temp")));
    QVERIFY(!PayloadCodec::topicFilterMatches(QStringLiteral("devices/#/temp"), QStringLiteral("devices/a/temp")));
    QVERIFY(!PayloadCodec::topicFilterMatches(QString(), QStringLiteral("devices/a/temp")));
}

void PayloadCodecTest::scoreTopicSpecificity()
{
    QCOMPARE(PayloadCodec::topicSpecificityScore(QStringLiteral("devices/#")), 8);
    QCOMPARE(PayloadCodec::topicSpecificityScore(QStringLiteral("devices/+/temp")), 13);
    QCOMPARE(PayloadCodec::topicSpecificityScore(QStringLiteral("devices/a/temp")), 14);
}

void PayloadCodecTest::resolveMostSpecificTopicFormat()
{
    QHash<QString, int> formats;
    formats.insert(QStringLiteral("devices/#"), 1);
    formats.insert(QStringLiteral("devices/a/+"), 3);
    formats.insert(QStringLiteral("devices/a/temp"), 2);

    QCOMPARE(PayloadCodec::resolveTopicFormat(formats, QStringLiteral("devices/a/temp")), PayloadFormat::Base64);
    QCOMPARE(PayloadCodec::resolveTopicFormat(formats, QStringLiteral("devices/a/humidity")), PayloadFormat::Hex);
    QCOMPARE(PayloadCodec::resolveTopicFormat(formats, QStringLiteral("devices/b/temp")), PayloadFormat::Json);
    QCOMPARE(PayloadCodec::resolveTopicFormat(formats, QStringLiteral("other/topic")), PayloadFormat::Plaintext);
}

QTEST_MAIN(PayloadCodecTest)

#include "test_payloadcodec.moc"
