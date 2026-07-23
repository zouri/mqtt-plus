#include "app/applicationsessionruntime.h"
#include "services/apputils.h"
#include "usecases/mqttsessionservice.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QMqttClient>
#include <QTranslator>
#include <QtTest/QtTest>

namespace {

class ErrorTextTranslator final : public QTranslator
{
public:
    QString translate(
        const char *context,
        const char *sourceText,
        const char *disambiguation = nullptr,
        int n = -1) const override
    {
        Q_UNUSED(context)
        Q_UNUSED(disambiguation)
        Q_UNUSED(n)
        return sourceText
            ? QStringLiteral("translated: %1").arg(QString::fromUtf8(sourceText))
            : QString();
    }
};

class TranslatorGuard
{
public:
    TranslatorGuard()
    {
        QCoreApplication::installTranslator(&m_translator);
    }

    ~TranslatorGuard()
    {
        QCoreApplication::removeTranslator(&m_translator);
    }

private:
    ErrorTextTranslator m_translator;
};

} // namespace

class ConnectionErrorTextTest : public QObject
{
    Q_OBJECT

private slots:
    void clientErrorNamesIgnoreApplicationTranslator_data();
    void clientErrorNamesIgnoreApplicationTranslator();
    void connectionValidationErrorsIgnoreApplicationTranslator_data();
    void connectionValidationErrorsIgnoreApplicationTranslator();
    void clientErrorUsesSameRawTextForSessionAndLog();
    void connectionTimeoutIgnoresApplicationTranslator();
};

void ConnectionErrorTextTest::clientErrorNamesIgnoreApplicationTranslator_data()
{
    QTest::addColumn<QMqttClient::ClientError>("error");
    QTest::addColumn<QString>("expected");

    QTest::newRow("no-error")
        << QMqttClient::NoError << QStringLiteral("No error");
    QTest::newRow("protocol-version")
        << QMqttClient::InvalidProtocolVersion
        << QStringLiteral("Protocol version rejected by broker");
    QTest::newRow("client-id")
        << QMqttClient::IdRejected << QStringLiteral("Client ID rejected");
    QTest::newRow("broker-unavailable")
        << QMqttClient::ServerUnavailable << QStringLiteral("Broker unavailable");
    QTest::newRow("credentials")
        << QMqttClient::BadUsernameOrPassword
        << QStringLiteral("Username or password rejected");
    QTest::newRow("not-authorized")
        << QMqttClient::NotAuthorized << QStringLiteral("Not authorized");
    QTest::newRow("transport")
        << QMqttClient::TransportInvalid << QStringLiteral("Invalid transport");
    QTest::newRow("protocol-violation")
        << QMqttClient::ProtocolViolation << QStringLiteral("Protocol violation");
    QTest::newRow("unknown")
        << QMqttClient::UnknownError << QStringLiteral("Unknown MQTT error");
    QTest::newRow("mqtt5")
        << QMqttClient::Mqtt5SpecificError
        << QStringLiteral("MQTT 5 broker reported an error");
}

void ConnectionErrorTextTest::clientErrorNamesIgnoreApplicationTranslator()
{
    QFETCH(QMqttClient::ClientError, error);
    QFETCH(QString, expected);

    TranslatorGuard translatorGuard;
    QCOMPARE(AppUtils::clientErrorName(error), expected);
}

void ConnectionErrorTextTest::connectionValidationErrorsIgnoreApplicationTranslator_data()
{
    QTest::addColumn<QString>("host");
    QTest::addColumn<QString>("clientId");
    QTest::addColumn<QString>("expected");

    QTest::newRow("empty-host")
        << QString() << QStringLiteral("client-1")
        << QStringLiteral("Broker host cannot be empty.");
    QTest::newRow("empty-client-id")
        << QStringLiteral("broker.example.com") << QString()
        << QStringLiteral("Client ID cannot be empty.");
}

void ConnectionErrorTextTest::connectionValidationErrorsIgnoreApplicationTranslator()
{
    QFETCH(QString, host);
    QFETCH(QString, clientId);
    QFETCH(QString, expected);

    TranslatorGuard translatorGuard;
    QMqttClient client;
    client.setHostname(host);
    client.setClientId(clientId);

    SessionState session;
    session.id = QStringLiteral("session-1");
    session.runtime.client = &client;

    QString eventChannel;
    QString eventMessage;
    MqttSessionService service;
    MqttSessionService::Dependencies dependencies;
    dependencies.currentSessionState = [&session]() { return &session; };
    dependencies.appendEvent = [&eventChannel, &eventMessage](
                                   SessionState &,
                                   const QString &channel,
                                   const QString &message) {
        eventChannel = channel;
        eventMessage = message;
    };
    service.setDependencies(dependencies);

    service.connectCurrentSession();

    QCOMPARE(session.runtime.lastError, expected);
    QCOMPARE(eventChannel, QStringLiteral("Connection"));
    QCOMPARE(eventMessage, expected);
}

void ConnectionErrorTextTest::clientErrorUsesSameRawTextForSessionAndLog()
{
    TranslatorGuard translatorGuard;
    QMqttClient client;
    SessionState session;
    session.id = QStringLiteral("session-1");
    session.runtime.client = &client;

    QString eventChannel;
    QString eventMessage;
    MqttSessionService service;
    MqttSessionService::Dependencies dependencies;
    dependencies.sessionById = [&session](const QString &sessionId) {
        return sessionId == session.id ? &session : nullptr;
    };
    dependencies.appendEvent = [&eventChannel, &eventMessage](
                                   SessionState &,
                                   const QString &channel,
                                   const QString &message) {
        eventChannel = channel;
        eventMessage = message;
    };
    service.setDependencies(dependencies);
    service.bindSessionSignals(&session);

    client.errorChanged(QMqttClient::TransportInvalid);

    QVERIFY2(session.runtime.lastError.startsWith(QStringLiteral("Invalid transport")),
        qPrintable(session.runtime.lastError));
    QCOMPARE(eventChannel, QStringLiteral("Error"));
    QCOMPARE(eventMessage, session.runtime.lastError);
}

void ConnectionErrorTextTest::connectionTimeoutIgnoresApplicationTranslator()
{
    TranslatorGuard translatorGuard;
    QObject owner;
    QBuffer transport;
    QVERIFY(transport.open(QIODevice::ReadWrite));

    QMqttClient client;
    client.setTransport(&transport, QMqttClient::IODevice);

    SessionState session;
    session.id = QStringLiteral("session-1");
    session.runtime.client = &client;

    QString eventChannel;
    QString eventMessage;
    ApplicationSessionRuntime runtime(
        &owner,
        {
            [&session](const QString &sessionId) {
                return sessionId == session.id ? &session : nullptr;
            },
            [&eventChannel, &eventMessage](
                SessionState &,
                const QString &channel,
                const QString &message) {
                eventChannel = channel;
                eventMessage = message;
            },
            {},
            {},
        });
    runtime.initialize(&session);

    client.connectToHost();
    QCOMPARE(client.state(), QMqttClient::Connecting);
    session.runtime.connectTimeoutTimer->start(0);

    QTRY_COMPARE(session.runtime.lastError, QStringLiteral("Connection timed out."));
    QCOMPARE(eventChannel, QStringLiteral("Error"));
    QCOMPARE(eventMessage, session.runtime.lastError);
}

QTEST_GUILESS_MAIN(ConnectionErrorTextTest)

#include "test_connectionerrortext.moc"
