#include "services/configuration/configurationadapters.h"
#include "services/mqtt/qtmqttpropertycodec.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHostAddress>
#include <QMqttClient>
#include <QPointer>
#include <QWebSocket>
#include <QWebSocketHandshakeOptions>
#include <QWebSocketServer>
#include <QtTest>

class ConfigurationAdaptersTest : public QObject
{
    Q_OBJECT

private slots:
    void importsMqttxConnectionsAndWarnings();
    void doesNotReportMissingAdvancedSubscriptionFields();
    void roundTripsNativeConfiguration();
    void roundTripsQtPublishProperties();
    void connectsOverWebSocketWithMqttSubprotocol();
    void rejectsFutureNativeVersion();
    void rejectsPreviousNativeVersion();
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
    const QJsonObject webSocket {
        {QStringLiteral("name"), QStringLiteral("WebSocket")},
        {QStringLiteral("host"), QStringLiteral("broker.example.test")},
        {QStringLiteral("port"), 8083},
        {QStringLiteral("protocol"), QStringLiteral("ws")},
        {QStringLiteral("path"), QStringLiteral("mqtt-stream")},
        {QStringLiteral("mqttVersion"), QStringLiteral("5.0")},
        {QStringLiteral("will"), QJsonObject {
             {QStringLiteral("lastWillTopic"), QStringLiteral("clients/offline")},
             {QStringLiteral("lastWillPayload"), QStringLiteral("gone")},
             {QStringLiteral("lastWillQos"), 1},
             {QStringLiteral("willDelayInterval"), 15},
             {QStringLiteral("payloadFormatIndicator"), true},
         }},
    };

    const auto result = MqttxConfigAdapter::parse(
        QJsonDocument(QJsonArray {supported, webSocket}).toJson());

    QVERIFY(result.ok);
    QCOMPARE(result.format, QStringLiteral("mqttx"));
    QCOMPARE(result.bundle.sessions.size(), 2);
    QCOMPARE(result.sensitiveFieldCount, 1);
    const auto &session = result.bundle.sessions.first();
    QCOMPARE(session.transport, QStringLiteral("tls"));
    QCOMPARE(session.protocolVersion, 5);
    QCOMPARE(session.connectTimeoutSeconds, 10);
    QCOMPARE(session.sslSecure, false);
    QCOMPARE(session.subscriptions.size(), 1);
    QCOMPARE(session.subscriptions.first().qos, 2);
    QCOMPARE(session.subscriptions.first().paused, true);
    const auto &webSocketSession = result.bundle.sessions.at(1);
    QCOMPARE(webSocketSession.transport, QStringLiteral("ws"));
    QCOMPARE(webSocketSession.webSocketPath, QStringLiteral("/mqtt-stream"));
    QVERIFY(webSocketSession.lastWill.enabled);
    QCOMPARE(webSocketSession.lastWill.topic, QStringLiteral("clients/offline"));
    QCOMPARE(webSocketSession.lastWill.properties.delayInterval, quint32(15));
    QVERIFY(webSocketSession.lastWill.properties.payloadFormatIndicator
        == MqttPayloadFormatIndicator::Utf8);
    QVERIFY(result.warnings.join(QLatin1Char('\n')).contains(QStringLiteral("reconnect"), Qt::CaseInsensitive));
    QVERIFY(result.warnings.join(QLatin1Char('\n')).contains(QStringLiteral("not supported"), Qt::CaseInsensitive));
    QVERIFY(!result.warnings.join(QLatin1Char('\n')).contains(QStringLiteral("QoS 2")));
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

void ConfigurationAdaptersTest::roundTripsNativeConfiguration()
{
    ConfigurationTransfer::Bundle bundle;
    ConfigurationTransfer::SessionData session;
    session.sourceId = QStringLiteral("session-1");
    session.name = QStringLiteral("Local broker");
    session.host = QStringLiteral("localhost");
    session.port = 8883;
    session.transport = QStringLiteral("wss");
    session.webSocketPath = QStringLiteral("/mqtt/v5");
    session.userProperties.append({QStringLiteral("client"), QStringLiteral("desktop")});
    session.lastWill.enabled = true;
    session.lastWill.topic = QStringLiteral("clients/offline");
    session.lastWill.payload = QStringLiteral("offline");
    session.lastWill.qos = 1;
    session.lastWill.properties.payloadFormatIndicator =
        MqttPayloadFormatIndicator::Unspecified;
    session.lastWill.properties.messageExpiryInterval = 45;
    session.lastWill.properties.contentType = QStringLiteral("text/plain");
    session.password = QStringLiteral("password");
    session.caCertificate = QByteArrayLiteral("CA DATA");
    session.clientKey = QByteArrayLiteral("KEY DATA");
    ConfigurationTransfer::SubscriptionData subscription;
    subscription.topic = QStringLiteral("sensors/#");
    subscription.alias = QStringLiteral("Sensors");
    subscription.qos = 2;
    subscription.paused = true;
    subscription.options.noLocal = true;
    subscription.options.subscriptionIdentifier = 42;
    session.subscriptions.append(subscription);
    bundle.sessions.append(session);

    PublishDraft draft;
    draft.id = QStringLiteral("draft-1");
    draft.name = QStringLiteral("Command");
    draft.payload = QStringLiteral("{}");
    draft.formatId = QStringLiteral("json");
    draft.qos = 2;
    draft.properties.responseTopic = QStringLiteral("commands/replies");
    draft.properties.correlationData = QByteArrayLiteral("request-42");
    bundle.drafts.append(draft);
    bundle.preferences.insert(QStringLiteral("appearance/themeMode"), QStringLiteral("dark"));

    const auto serialized = MqttPlusConfigAdapter::serialize(bundle, true);
    QVERIFY(serialized.ok);
    const QJsonObject serializedRoot = QJsonDocument::fromJson(serialized.content).object();
    QCOMPARE(serializedRoot.value(QStringLiteral("version")).toInt(), 3);
    const auto parsed = MqttPlusConfigAdapter::parse(serialized.content);

    QVERIFY(parsed.ok);
    QCOMPARE(parsed.bundle.sessions.size(), 1);
    QCOMPARE(parsed.bundle.sessions.first().caCertificate, QByteArrayLiteral("CA DATA"));
    QCOMPARE(parsed.bundle.sessions.first().clientKey, QByteArrayLiteral("KEY DATA"));
    QCOMPARE(parsed.bundle.sessions.first().transport, QStringLiteral("wss"));
    QCOMPARE(parsed.bundle.sessions.first().webSocketPath, QStringLiteral("/mqtt/v5"));
    QCOMPARE(parsed.bundle.sessions.first().userProperties.size(), 1);
    QVERIFY(parsed.bundle.sessions.first().lastWill.enabled);
    QCOMPARE(parsed.bundle.sessions.first().lastWill, session.lastWill);
    QCOMPARE(parsed.bundle.sessions.first().subscriptions.first().topic, QStringLiteral("sensors/#"));
    QCOMPARE(parsed.bundle.sessions.first().subscriptions.first().qos, 2);
    QVERIFY(parsed.bundle.sessions.first().subscriptions.first().options.noLocal);
    QCOMPARE(parsed.bundle.sessions.first().subscriptions.first().options.subscriptionIdentifier,
        quint32(42));
    QCOMPARE(parsed.bundle.drafts.first().name, draft.name);
    QCOMPARE(parsed.bundle.drafts.first().qos, 2);
    QCOMPARE(parsed.bundle.drafts.first().properties, draft.properties);
    QCOMPARE(parsed.bundle.preferences.value(QStringLiteral("appearance/themeMode")).toString(), QStringLiteral("dark"));
    QCOMPARE(parsed.sensitiveFieldCount, 2);
}

void ConfigurationAdaptersTest::roundTripsQtPublishProperties()
{
    MqttPublishProperties properties;
    properties.payloadFormatIndicator = MqttPayloadFormatIndicator::Utf8;
    properties.messageExpiryInterval = 120;
    properties.topicAlias = 7;
    properties.responseTopic = QStringLiteral("requests/reply");
    properties.correlationData = QByteArrayLiteral("request-7");
    properties.contentType = QStringLiteral("application/json");
    properties.userProperties.append({QStringLiteral("trace"), QStringLiteral("abc")});

    const MqttPublishProperties roundTripped = QtMqttPropertyCodec::fromQtPublishProperties(
        QtMqttPropertyCodec::toQtPublishProperties(properties));
    QCOMPARE(roundTripped, properties);

    const QString encoded = mqttPublishPropertiesToBase64Cbor(properties);
    QVERIFY(mqttPublishPropertiesFromBase64Cbor(encoded) == properties);
    QVERIFY(!mqttPublishPropertiesFromBase64Cbor(QStringLiteral("not base64")));

    MqttSubscriptionOptions subscription;
    subscription.noLocal = true;
    subscription.subscriptionIdentifier = 42;
    subscription.userProperties = properties.userProperties;
    const QMqttSubscriptionProperties qtSubscription =
        QtMqttPropertyCodec::toQtSubscriptionProperties(subscription);
    QVERIFY(qtSubscription.noLocal());
    QCOMPARE(qtSubscription.subscriptionIdentifier(), quint32(42));
    QCOMPARE(qtSubscription.userProperties().size(), 1);
}

void ConfigurationAdaptersTest::connectsOverWebSocketWithMqttSubprotocol()
{
    QWebSocketServer server(
        QStringLiteral("MQTT WebSocket test"),
        QWebSocketServer::NonSecureMode);
    server.setSupportedSubprotocols({QStringLiteral("mqtt")});
    if (!server.listen(QHostAddress::LocalHost, 0)) {
        QSKIP(qPrintable(QStringLiteral("Loopback listener unavailable: %1")
                             .arg(server.errorString())));
    }

    QPointer<QWebSocket> acceptedSocket;
    QUrl acceptedUrl;
    connect(&server, &QWebSocketServer::newConnection, this, [&] {
        acceptedSocket = server.nextPendingConnection();
        QVERIFY(acceptedSocket);
        acceptedUrl = acceptedSocket->requestUrl();
        connect(acceptedSocket, &QWebSocket::binaryMessageReceived, this,
            [acceptedSocket](const QByteArray &packet) {
                if (acceptedSocket && !packet.isEmpty() && quint8(packet.at(0)) == 0x10) {
                    acceptedSocket->sendBinaryMessage(QByteArray::fromHex("2003000000"));
                }
            });
    });

    QMqttClient client;
    client.setProtocolVersion(QMqttClient::MQTT_5_0);
    QWebSocket webSocket;
    client.connectToHostWebSocket(&webSocket);

    QWebSocketHandshakeOptions options;
    options.setSubprotocols({QStringLiteral("mqtt")});
    webSocket.open(
        QUrl(QStringLiteral("ws://127.0.0.1:%1/mqtt/v5").arg(server.serverPort())),
        options);

    QTRY_COMPARE_WITH_TIMEOUT(client.state(), QMqttClient::Connected, 3000);
    QVERIFY(acceptedSocket);
    QCOMPARE(acceptedSocket->subprotocol(), QStringLiteral("mqtt"));
    QCOMPARE(acceptedUrl.path(), QStringLiteral("/mqtt/v5"));
    client.disconnectFromHost();
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

void ConfigurationAdaptersTest::rejectsPreviousNativeVersion()
{
    const QJsonObject root {
        {QStringLiteral("format"), QStringLiteral("mqtt-plus-config")},
        {QStringLiteral("version"), 1},
        {QStringLiteral("sessions"), QJsonArray {}},
        {QStringLiteral("drafts"), QJsonArray {}},
        {QStringLiteral("preferences"), QJsonObject {}},
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
