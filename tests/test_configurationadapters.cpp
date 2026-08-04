#include "services/configuration/configurationadapters.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

class ConfigurationAdaptersTest : public QObject
{
    Q_OBJECT

private slots:
    void importsMqttxConnectionsAndWarnings();
    void doesNotReportMissingAdvancedSubscriptionFields();
    void inspectsExternalMqttxSampleWhenProvided();
    void roundTripsNativeConfiguration();
    void rejectsFutureNativeVersion();
    void rejectsInvalidNativeStructure();
};

void ConfigurationAdaptersTest::importsMqttxConnectionsAndWarnings()
{
    const QJsonObject subscription {
        {QStringLiteral("topic"), QStringLiteral("devices/+/state")},
        {QStringLiteral("alias"), QStringLiteral("Device state")},
        {QStringLiteral("qos"), 2},
        {QStringLiteral("color"), QStringLiteral("#123456")},
        {QStringLiteral("disabled"), true},
        {QStringLiteral("nl"), true},
        {QStringLiteral("rap"), false},
        {QStringLiteral("rh"), 0},
    };
    const QJsonObject supported {
        {QStringLiteral("id"), QStringLiteral("mqttx-source")},
        {QStringLiteral("name"), QStringLiteral("Imported broker")},
        {QStringLiteral("host"), QStringLiteral("broker.example.test")},
        {QStringLiteral("port"), 8883},
        {QStringLiteral("protocol"), QStringLiteral("mqtts")},
        {QStringLiteral("mqttVersion"), QStringLiteral("5.0")},
        {QStringLiteral("clientId"), QStringLiteral("client-a")},
        {QStringLiteral("username"), QStringLiteral("user")},
        {QStringLiteral("password"), QStringLiteral("secret")},
        {QStringLiteral("clean"), false},
        {QStringLiteral("keepalive"), 60},
        {QStringLiteral("connectTimeout"), 10000},
        {QStringLiteral("reconnect"), true},
        {QStringLiteral("reconnectPeriod"), 4000},
        {QStringLiteral("rejectUnauthorized"), false},
        {QStringLiteral("ALPNProtocols"), QStringLiteral("mqtt")},
        {QStringLiteral("subscriptions"), QJsonArray {subscription}},
    };
    const QJsonObject unsupported {
        {QStringLiteral("name"), QStringLiteral("WebSocket")},
        {QStringLiteral("host"), QStringLiteral("broker.example.test")},
        {QStringLiteral("port"), 8083},
        {QStringLiteral("protocol"), QStringLiteral("ws")},
        {QStringLiteral("mqttVersion"), QStringLiteral("5.0")},
    };

    const auto result = MqttxConfigAdapter::parse(
        QJsonDocument(QJsonArray {supported, unsupported}).toJson());

    QVERIFY(result.ok);
    QCOMPARE(result.format, QStringLiteral("mqttx"));
    QCOMPARE(result.bundle.sessions.size(), 1);
    QCOMPARE(result.sensitiveFieldCount, 1);
    const auto &session = result.bundle.sessions.first();
    QCOMPARE(session.transport, QStringLiteral("tls"));
    QCOMPARE(session.protocolVersion, 5);
    QCOMPARE(session.connectTimeoutSeconds, 10);
    QCOMPARE(session.sslSecure, false);
    QCOMPARE(session.subscriptions.size(), 1);
    QCOMPARE(session.subscriptions.first().qos, 1);
    QCOMPARE(session.subscriptions.first().paused, true);
    QVERIFY(result.warnings.join(QLatin1Char('\n')).contains(QStringLiteral("reconnect"), Qt::CaseInsensitive));
    QVERIFY(result.warnings.join(QLatin1Char('\n')).contains(QStringLiteral("QoS 2")));
    QVERIFY(result.warnings.join(QLatin1Char('\n')).contains(QStringLiteral("unsupported"), Qt::CaseInsensitive));
}

void ConfigurationAdaptersTest::doesNotReportMissingAdvancedSubscriptionFields()
{
    const QJsonObject subscription {
        {QStringLiteral("topic"), QStringLiteral("devices/#")},
        {QStringLiteral("qos"), 0},
    };
    const QJsonObject connection {
        {QStringLiteral("name"), QStringLiteral("Basic broker")},
        {QStringLiteral("host"), QStringLiteral("broker.example.test")},
        {QStringLiteral("protocol"), QStringLiteral("mqtt")},
        {QStringLiteral("mqttVersion"), QStringLiteral("5.0")},
        {QStringLiteral("subscriptions"), QJsonArray {subscription}},
    };

    const auto result = MqttxConfigAdapter::parse(
        QJsonDocument(QJsonArray {connection}).toJson());

    QVERIFY(result.ok);
    QVERIFY(!result.warnings.join(QLatin1Char('\n')).contains(
        QStringLiteral("Advanced MQTT 5 options")));
}

void ConfigurationAdaptersTest::inspectsExternalMqttxSampleWhenProvided()
{
    const QString path = qEnvironmentVariable("MQTT_PLUS_MQTTX_SAMPLE_PATH");
    if (path.isEmpty()) {
        QSKIP("MQTT_PLUS_MQTTX_SAMPLE_PATH is not set.");
    }
    QFile file(path);
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.errorString()));
    const auto result = MqttxConfigAdapter::parse(file.readAll());
    QVERIFY2(result.ok, qPrintable(result.errorMessage));
    QCOMPARE(result.bundle.sessions.size(), 5);
    int subscriptionCount = 0;
    for (const auto &session : result.bundle.sessions) {
        subscriptionCount += session.subscriptions.size();
    }
    QCOMPARE(subscriptionCount, 35);
    QCOMPARE(result.sensitiveFieldCount, 3);
}

void ConfigurationAdaptersTest::roundTripsNativeConfiguration()
{
    ConfigurationTransfer::Bundle bundle;
    ConfigurationTransfer::SessionData session;
    session.sourceId = QStringLiteral("session-1");
    session.name = QStringLiteral("Local broker");
    session.host = QStringLiteral("localhost");
    session.port = 8883;
    session.transport = QStringLiteral("tls");
    session.password = QStringLiteral("password");
    session.caCertificate = QByteArrayLiteral("CA DATA");
    session.clientKey = QByteArrayLiteral("KEY DATA");
    ConfigurationTransfer::SubscriptionData subscription;
    subscription.topic = QStringLiteral("sensors/#");
    subscription.alias = QStringLiteral("Sensors");
    subscription.qos = 1;
    subscription.scriptId = QStringLiteral("script-that-must-not-be-exported");
    subscription.paused = true;
    session.subscriptions.append(subscription);
    bundle.sessions.append(session);

    PublishDraft draft;
    draft.id = QStringLiteral("draft-1");
    draft.name = QStringLiteral("Command");
    draft.payload = QStringLiteral("{}");
    draft.formatId = QStringLiteral("json");
    draft.qos = 1;
    bundle.drafts.append(draft);
    bundle.preferences.insert(QStringLiteral("appearance/themeMode"), QStringLiteral("dark"));

    const auto serialized = MqttPlusConfigAdapter::serialize(bundle, true);
    QVERIFY(serialized.ok);
    QVERIFY(!serialized.content.contains("script-that-must-not-be-exported"));
    QVERIFY(!serialized.content.contains("scriptId"));
    const auto parsed = MqttPlusConfigAdapter::parse(serialized.content);

    QVERIFY(parsed.ok);
    QCOMPARE(parsed.bundle.sessions.size(), 1);
    QCOMPARE(parsed.bundle.sessions.first().caCertificate, QByteArrayLiteral("CA DATA"));
    QCOMPARE(parsed.bundle.sessions.first().clientKey, QByteArrayLiteral("KEY DATA"));
    QCOMPARE(parsed.bundle.sessions.first().subscriptions.first().topic, QStringLiteral("sensors/#"));
    QCOMPARE(parsed.bundle.drafts.first().name, draft.name);
    QCOMPARE(parsed.bundle.preferences.value(QStringLiteral("appearance/themeMode")).toString(), QStringLiteral("dark"));
    QCOMPARE(parsed.sensitiveFieldCount, 2);
}

void ConfigurationAdaptersTest::rejectsFutureNativeVersion()
{
    const QJsonObject root {
        {QStringLiteral("format"), QStringLiteral("mqtt-plus-config")},
        {QStringLiteral("version"), MqttPlusConfigAdapter::kSchemaVersion + 1},
    };
    const auto result = MqttPlusConfigAdapter::parse(QJsonDocument(root).toJson());
    QVERIFY(!result.ok);
    QVERIFY(!result.errorMessage.isEmpty());
}

void ConfigurationAdaptersTest::rejectsInvalidNativeStructure()
{
    const QJsonObject validRoot {
        {QStringLiteral("format"), QStringLiteral("mqtt-plus-config")},
        {QStringLiteral("version"), MqttPlusConfigAdapter::kSchemaVersion},
        {QStringLiteral("sessions"), QJsonArray {}},
        {QStringLiteral("drafts"), QJsonArray {}},
        {QStringLiteral("preferences"), QJsonObject {}},
    };

    const QVector<QPair<QString, QJsonValue>> invalidFields {
        {QStringLiteral("sessions"), QJsonObject {}},
        {QStringLiteral("drafts"), QJsonObject {}},
        {QStringLiteral("preferences"), QJsonArray {}},
        {QStringLiteral("sessions"), QJsonArray {QStringLiteral("invalid")}},
        {QStringLiteral("drafts"), QJsonArray {QStringLiteral("invalid")}},
    };
    for (const auto &[key, value] : invalidFields) {
        QJsonObject root = validRoot;
        root.insert(key, value);
        const auto result = MqttPlusConfigAdapter::parse(QJsonDocument(root).toJson());
        QVERIFY2(!result.ok, qPrintable(key));
        QVERIFY2(!result.errorMessage.isEmpty(), qPrintable(key));
    }
}

QTEST_GUILESS_MAIN(ConfigurationAdaptersTest)
#include "test_configurationadapters.moc"
