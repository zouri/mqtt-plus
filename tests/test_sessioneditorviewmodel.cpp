#include "viewmodels/sessioneditorviewmodel.h"

#include <QtTest/QtTest>

class SessionEditorViewModelTest : public QObject
{
    Q_OBJECT

private slots:
    void opensForCreateAndEdit();
    void validatesRequiredFields();
    void validatesIntegerRanges();
    void collectsSanitizedConfig();
    void selectsTransportByIndex();
    void collectsAdvancedConfig();
};

void SessionEditorViewModelTest::opensForCreateAndEdit()
{
    SessionEditorViewModel editor;
    SessionConnectionConfig existing = SessionEditorViewModel::defaultConfig(3);
    existing.name = QStringLiteral("Existing");

    editor.openForCreate(SessionEditorViewModel::defaultConfig(1));
    QVERIFY(!editor.editMode());
    QCOMPARE(editor.targetIndex(), -1);
    QCOMPARE(editor.title(), QStringLiteral("New Connection"));

    editor.openForEdit(2, existing);
    QVERIFY(editor.editMode());
    QCOMPARE(editor.targetIndex(), 2);
    QCOMPARE(editor.title(), QStringLiteral("Edit Connection"));
    QCOMPARE(editor.name(), QStringLiteral("Existing"));
}

void SessionEditorViewModelTest::validatesRequiredFields()
{
    SessionEditorViewModel editor;

    editor.setName(QString());
    editor.setHost(QStringLiteral("broker.emqx.io"));

    QVERIFY(!editor.validate());
    QCOMPARE(editor.validationError(), QStringLiteral("Name is required."));

    editor.setName(QStringLiteral("Local"));
    editor.setHost(QString());

    QVERIFY(!editor.validate());
    QCOMPARE(editor.validationError(), QStringLiteral("Server address is required."));

    editor.setHost(QStringLiteral("broker.example.test"));
    editor.setWillEnabled(true);
    editor.setWillTopic(QStringLiteral("clients/+"));
    QVERIFY(!editor.validate());
    QCOMPARE(editor.validationError(), QStringLiteral("Last Will topic is invalid."));
}

void SessionEditorViewModelTest::validatesIntegerRanges()
{
    SessionEditorViewModel editor;
    editor.loadConfig(SessionEditorViewModel::defaultConfig(1));

    editor.setPortText(QStringLiteral("70000"));
    QVERIFY(!editor.validate());
    QCOMPARE(editor.validationError(), QStringLiteral("Port must be between 1 and 65535."));

    editor.setPortText(QStringLiteral("1883"));
    editor.setConnectTimeoutText(QStringLiteral("0"));
    QVERIFY(!editor.validate());
    QCOMPARE(editor.validationError(), QStringLiteral("Connection timeout must be between 1 and 300."));

    editor.setConnectTimeoutText(QStringLiteral("10"));
    editor.setKeepAliveText(QStringLiteral("4"));
    QVERIFY(!editor.validate());
    QCOMPARE(editor.validationError(), QStringLiteral("Keep Alive must be between 5 and 1200."));

    editor.setKeepAliveText(QStringLiteral("30"));
    editor.setReceiveMaximumText(QStringLiteral("70000"));
    QVERIFY(!editor.validate());
    QCOMPARE(editor.validationError(), QStringLiteral("Receive maximum must be between 1 and 65535."));
}

void SessionEditorViewModelTest::collectsSanitizedConfig()
{
    SessionEditorViewModel editor;
    editor.loadConfig(SessionEditorViewModel::defaultConfig(2));
    editor.setName(QStringLiteral("Production"));
    editor.setHost(QStringLiteral(" mqtt.example.com "));
    editor.setTransport(QStringLiteral("tls"));
    editor.setPortText(QStringLiteral("8883"));
    editor.setProtocolVersion(4);
    editor.setClientId(QStringLiteral("client-a"));

    const SessionConnectionConfig config = editor.collectedConfig();

    QCOMPARE(config.name, QStringLiteral("Production"));
    QCOMPARE(config.host, QStringLiteral("mqtt.example.com"));
    QCOMPARE(config.transport, QStringLiteral("tls"));
    QCOMPARE(config.port, 8883);
    QCOMPARE(config.protocolVersion, 4);
    QCOMPARE(config.clientId, QStringLiteral("client-a"));
}

void SessionEditorViewModelTest::selectsTransportByIndex()
{
    SessionEditorViewModel editor;
    QCOMPARE(editor.transportSchemes().size(), 4);

    editor.setTransportIndex(2);
    QCOMPARE(editor.transport(), QStringLiteral("ws"));
    QCOMPARE(editor.transportIndex(), 2);
    QCOMPARE(editor.portText(), QStringLiteral("8083"));
    QVERIFY(editor.webSocketTransport());

    editor.setPortText(QStringLiteral("9001"));
    editor.setTransportIndex(3);
    QCOMPARE(editor.transport(), QStringLiteral("wss"));
    QCOMPARE(editor.portText(), QStringLiteral("9001"));
}

void SessionEditorViewModelTest::collectsAdvancedConfig()
{
    SessionEditorViewModel editor;
    editor.loadConfig(SessionEditorViewModel::defaultConfig(1));

    editor.setSslSecure(false);
    editor.setAlpn(QStringLiteral("mqtt"));
    editor.setCertificateType(QStringLiteral("self"));
    editor.setCaFile(QStringLiteral("/tmp/ca.pem"));
    editor.setClientCertificateFile(QStringLiteral("/tmp/client.pem"));
    editor.setClientKeyFile(QStringLiteral("/tmp/client.key"));
    editor.setUsername(QStringLiteral("user"));
    editor.setPassword(QStringLiteral("secret"));
    editor.setKeepAliveText(QStringLiteral("45"));
    editor.setCleanSession(false);
    editor.setSessionExpiryText(QStringLiteral("60"));
    editor.setReceiveMaximumText(QStringLiteral("100"));
    editor.setMaximumPacketSizeText(QStringLiteral("1024"));
    editor.setTopicAliasMaximumText(QStringLiteral("5"));
    editor.setRequestResponseInformation(true);
    editor.setRequestProblemInformation(true);
    editor.setAuthenticationMethod(QStringLiteral("token"));
    editor.setAuthenticationData(QStringLiteral("data"));
    editor.setTransport(QStringLiteral("wss"));
    editor.setWebSocketPath(QStringLiteral("mqtt/v5"));
    editor.setUserPropertiesText(QStringLiteral("client=desktop\nregion=cn"));
    editor.setWillEnabled(true);
    editor.setWillTopic(QStringLiteral("clients/offline"));
    editor.setWillPayload(QStringLiteral("offline"));
    editor.setWillQos(1);
    editor.setWillRetain(true);
    editor.setWillDelayText(QStringLiteral("10"));
    editor.setWillContentType(QStringLiteral("text/plain"));
    editor.setWillUserPropertiesText(QStringLiteral("reason=disconnect"));

    const SessionConnectionConfig config = editor.collectedConfig();

    QCOMPARE(config.sslSecure, false);
    QCOMPARE(config.alpn, QStringLiteral("mqtt"));
    QCOMPARE(config.certificateType, QStringLiteral("self"));
    QCOMPARE(config.caFile, QStringLiteral("/tmp/ca.pem"));
    QCOMPARE(config.clientCertificateFile, QStringLiteral("/tmp/client.pem"));
    QCOMPARE(config.clientKeyFile, QStringLiteral("/tmp/client.key"));
    QCOMPARE(config.username, QStringLiteral("user"));
    QCOMPARE(config.password, QStringLiteral("secret"));
    QCOMPARE(config.keepAliveSeconds, 45);
    QCOMPARE(config.cleanSession, false);
    QCOMPARE(config.sessionExpiryInterval, quint32(60));
    QCOMPARE(config.receiveMaximum, quint16(100));
    QCOMPARE(config.maximumPacketSize, quint32(1024));
    QCOMPARE(config.topicAliasMaximum, quint16(5));
    QCOMPARE(config.requestResponseInformation, true);
    QCOMPARE(config.requestProblemInformation, true);
    QCOMPARE(config.authenticationMethod, QStringLiteral("token"));
    QCOMPARE(config.authenticationData, QStringLiteral("data"));
    QCOMPARE(config.transport, QStringLiteral("wss"));
    QCOMPARE(config.webSocketPath, QStringLiteral("/mqtt/v5"));
    QCOMPARE(config.userProperties.size(), 2);
    QVERIFY(config.lastWill.enabled);
    QCOMPARE(config.lastWill.topic, QStringLiteral("clients/offline"));
    QCOMPARE(config.lastWill.qos, 1);
    QVERIFY(config.lastWill.retain);
    QCOMPARE(config.lastWill.properties.delayInterval, quint32(10));
    QCOMPARE(config.lastWill.properties.contentType, QStringLiteral("text/plain"));
    QCOMPARE(config.lastWill.properties.userProperties.size(), 1);
}

QTEST_MAIN(SessionEditorViewModelTest)

#include "test_sessioneditorviewmodel.moc"
