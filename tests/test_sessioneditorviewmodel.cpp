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
    void collectsAdvancedConfig();
};

void SessionEditorViewModelTest::opensForCreateAndEdit()
{
    SessionEditorViewModel editor;
    QVariantMap existing = SessionEditorViewModel::defaultConfig(3);
    existing.insert(QStringLiteral("name"), QStringLiteral("Existing"));

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

    const QVariantMap config = editor.collectedConfig();

    QCOMPARE(config.value(QStringLiteral("name")).toString(), QStringLiteral("Production"));
    QCOMPARE(config.value(QStringLiteral("host")).toString(), QStringLiteral("mqtt.example.com"));
    QCOMPARE(config.value(QStringLiteral("transport")).toString(), QStringLiteral("tls"));
    QCOMPARE(config.value(QStringLiteral("port")).toInt(), 8883);
    QCOMPARE(config.value(QStringLiteral("protocolVersion")).toInt(), 4);
    QCOMPARE(config.value(QStringLiteral("clientId")).toString(), QStringLiteral("client-a"));
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

    const QVariantMap config = editor.collectedConfig();

    QCOMPARE(config.value(QStringLiteral("sslSecure")).toBool(), false);
    QCOMPARE(config.value(QStringLiteral("alpn")).toString(), QStringLiteral("mqtt"));
    QCOMPARE(config.value(QStringLiteral("certificateType")).toString(), QStringLiteral("self"));
    QCOMPARE(config.value(QStringLiteral("caFile")).toString(), QStringLiteral("/tmp/ca.pem"));
    QCOMPARE(config.value(QStringLiteral("clientCertificateFile")).toString(), QStringLiteral("/tmp/client.pem"));
    QCOMPARE(config.value(QStringLiteral("clientKeyFile")).toString(), QStringLiteral("/tmp/client.key"));
    QCOMPARE(config.value(QStringLiteral("username")).toString(), QStringLiteral("user"));
    QCOMPARE(config.value(QStringLiteral("password")).toString(), QStringLiteral("secret"));
    QCOMPARE(config.value(QStringLiteral("keepAliveSeconds")).toInt(), 45);
    QCOMPARE(config.value(QStringLiteral("cleanSession")).toBool(), false);
    QCOMPARE(config.value(QStringLiteral("sessionExpiryInterval")).toString(), QStringLiteral("60"));
    QCOMPARE(config.value(QStringLiteral("receiveMaximum")).toString(), QStringLiteral("100"));
    QCOMPARE(config.value(QStringLiteral("maximumPacketSize")).toString(), QStringLiteral("1024"));
    QCOMPARE(config.value(QStringLiteral("topicAliasMaximum")).toString(), QStringLiteral("5"));
    QCOMPARE(config.value(QStringLiteral("requestResponseInformation")).toBool(), true);
    QCOMPARE(config.value(QStringLiteral("requestProblemInformation")).toBool(), true);
    QCOMPARE(config.value(QStringLiteral("authenticationMethod")).toString(), QStringLiteral("token"));
    QCOMPARE(config.value(QStringLiteral("authenticationData")).toString(), QStringLiteral("data"));
}

QTEST_MAIN(SessionEditorViewModelTest)

#include "test_sessioneditorviewmodel.moc"
