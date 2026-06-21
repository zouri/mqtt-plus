#include "domain/sessionconfig.h"

#include <QtTest/QtTest>

class SessionConfigTest : public QObject
{
    Q_OBJECT

private slots:
    void sanitizePortUsesTransportDefaults();
    void sanitizePortClampsValidRange();
    void sanitizeKeepAliveClampsValidRange();
    void sanitizeOptionalIntegers();
    void sanitizeEnumsAndModes();
    void defaultConfigContainsExpectedBaseline();
};

void SessionConfigTest::sanitizePortUsesTransportDefaults()
{
    QCOMPARE(SessionConfig::sanitizePort(QStringLiteral("not-a-port"), QStringLiteral("tcp")), 1883);
    QCOMPARE(SessionConfig::sanitizePort(QStringLiteral("not-a-port"), QStringLiteral("tls")), 8883);
}

void SessionConfigTest::sanitizePortClampsValidRange()
{
    QCOMPARE(SessionConfig::sanitizePort(0, QStringLiteral("tcp")), 1);
    QCOMPARE(SessionConfig::sanitizePort(70000, QStringLiteral("tcp")), 65535);
    QCOMPARE(SessionConfig::sanitizePort(1884, QStringLiteral("tcp")), 1884);
}

void SessionConfigTest::sanitizeKeepAliveClampsValidRange()
{
    QCOMPARE(SessionConfig::sanitizeKeepAlive(QStringLiteral("bad")), SessionConfig::kDefaultKeepAlive);
    QCOMPARE(SessionConfig::sanitizeKeepAlive(1), 5);
    QCOMPARE(SessionConfig::sanitizeKeepAlive(5000), 1200);
    QCOMPARE(SessionConfig::sanitizeKeepAlive(60), 60);
}

void SessionConfigTest::sanitizeOptionalIntegers()
{
    QCOMPARE(SessionConfig::sanitizeOptionalUInt16(QString()), quint16(0));
    QCOMPARE(SessionConfig::sanitizeOptionalUInt16(QStringLiteral("bad")), quint16(0));
    QCOMPARE(SessionConfig::sanitizeOptionalUInt16(QStringLiteral("70000")), quint16(65535));
    QCOMPARE(SessionConfig::sanitizeOptionalUInt16(QStringLiteral("12")), quint16(12));

    QCOMPARE(SessionConfig::sanitizeOptionalUInt32(QString()), quint32(0));
    QCOMPARE(SessionConfig::sanitizeOptionalUInt32(QStringLiteral("bad")), quint32(0));
    QCOMPARE(SessionConfig::sanitizeOptionalUInt32(QStringLiteral("42949672960")), quint32(4294967295U));
    QCOMPARE(SessionConfig::sanitizeOptionalUInt32(QStringLiteral("120")), quint32(120));
}

void SessionConfigTest::sanitizeEnumsAndModes()
{
    QCOMPARE(SessionConfig::sanitizeQos(-1), 0);
    QCOMPARE(SessionConfig::sanitizeQos(0), 0);
    QCOMPARE(SessionConfig::sanitizeQos(1), 1);
    QCOMPARE(SessionConfig::sanitizeQos(2), 1);

    QCOMPARE(SessionConfig::sanitizeTransport(QStringLiteral(" TLS ")), QStringLiteral("tls"));
    QCOMPARE(SessionConfig::sanitizeTransport(QStringLiteral("websocket")), QStringLiteral("tcp"));

    QCOMPARE(SessionConfig::sanitizeProtocolVersion(4), 4);
    QCOMPARE(SessionConfig::sanitizeProtocolVersion(5), 5);
    QCOMPARE(SessionConfig::sanitizeProtocolVersion(QStringLiteral("bad")), 5);
}

void SessionConfigTest::defaultConfigContainsExpectedBaseline()
{
    const QVariantMap config = SessionConfig::defaultConfig(3);
    QCOMPARE(config.value(QStringLiteral("name")).toString(), QStringLiteral("Session 3"));
    QCOMPARE(config.value(QStringLiteral("host")).toString(), QStringLiteral("broker.emqx.io"));
    QCOMPARE(config.value(QStringLiteral("port")).toInt(), SessionConfig::kDefaultPort);
    QCOMPARE(config.value(QStringLiteral("transport")).toString(), QStringLiteral("tcp"));
    QCOMPARE(config.value(QStringLiteral("protocolVersion")).toInt(), 5);
    QCOMPARE(config.value(QStringLiteral("sslSecure")).toBool(), true);
    QCOMPARE(config.value(QStringLiteral("keepAliveSeconds")).toInt(), SessionConfig::kDefaultKeepAlive);
    QVERIFY(config.value(QStringLiteral("clientId")).toString().startsWith(QStringLiteral("mqtt-plus-")));
}

QTEST_MAIN(SessionConfigTest)

#include "test_sessionconfig.moc"
