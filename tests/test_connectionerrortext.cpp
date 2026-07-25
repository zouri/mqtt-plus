#include "models/eventstreammodel.h"
#include "services/apputils.h"
#include "services/storage/historystore.h"
#include "usecases/eventhistoryservice.h"
#include "usecases/mqttsessionservice.h"
#include "usecases/preferencescontroller.h"
#include "usecases/scriptservice.h"
#include "usecases/sessionservice.h"
#include "usecases/subscriptionservice.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QMqttClient>
#include <QSettings>
#include <QTemporaryDir>
#include <QTranslator>
#include <QtTest/QtTest>

#include <cstring>

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

class TestMqttTransport final : public QIODevice
{
public:
    void appendIncoming(const QByteArray &data)
    {
        m_incoming.append(data);
        emit readyRead();
    }

    qint64 bytesAvailable() const override
    {
        return m_incoming.size() + QIODevice::bytesAvailable();
    }

protected:
    qint64 readData(char *data, qint64 maxSize) override
    {
        const qint64 bytesToRead = qMin<qint64>(maxSize, m_incoming.size());
        if (bytesToRead == 0) {
            return 0;
        }
        std::memcpy(data, m_incoming.constData(), static_cast<size_t>(bytesToRead));
        m_incoming.remove(0, bytesToRead);
        return bytesToRead;
    }

    qint64 writeData(const char *, qint64 maxSize) override
    {
        return maxSize;
    }

private:
    QByteArray m_incoming;
};

struct MqttFixture
{
    QTemporaryDir dataDir;
    QSettings settings;
    HistoryStore historyStore;
    PreferencesController preferences;
    ScriptService scripts;
    SessionService sessions;
    EventStreamModel messages;
    EventStreamModel logs;
    QString launchTimestamp = QStringLiteral("2026-07-25T00:00:00.000Z");
    EventHistoryService events;
    SubscriptionService subscriptions;
    MqttSessionService mqtt;

    explicit MqttFixture(bool bindMqttSignals = true)
        : settings(dataDir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat)
        , historyStore(dataDir.path())
        , preferences(&settings)
        , sessions(settings, scripts, historyStore, preferences)
        , events(
              sessions,
              historyStore,
              messages,
              logs,
              scripts,
              launchTimestamp,
              preferences)
        , subscriptions(sessions, scripts, events)
        , mqtt(sessions, subscriptions, events)
    {
        if (bindMqttSignals) {
            QObject::connect(
                &sessions,
                &SessionService::sessionRuntimeReady,
                &mqtt,
                &MqttSessionService::bindSessionSignals);
        }
        QObject::connect(
            &sessions,
            &SessionService::runtimeError,
            &events,
            [this](
                const QString &sessionId,
                const QString &channel,
                const QString &message) {
                if (auto *session = sessions.sessionById(sessionId)) {
                    events.appendEvent(*session, channel, message);
                }
            });
    }

    bool initialize()
    {
        if (!historyStore.isReady() || !sessions.loadSessions()) {
            return false;
        }
        sessions.setCurrentSessionIndex(0);
        return sessions.currentSession() != nullptr;
    }
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
    void pendingReconnectStartsAfterDisconnectedSignal();
    void qosZeroPublishDoesNotRemainQueued();
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
    MqttFixture fixture(false);
    QVERIFY2(fixture.initialize(), qPrintable(fixture.historyStore.lastError()));
    SessionState &session = *fixture.sessions.currentSession();
    session.runtime.client->setHostname(host);
    session.runtime.client->setClientId(clientId);
    QSignalSpy logSpy(&fixture.events, &EventHistoryService::logAppended);

    fixture.mqtt.connectCurrentSession();

    QCOMPARE(session.runtime.lastError, expected);
    QCOMPARE(logSpy.count(), 1);
    const QVariantMap row = logSpy.takeFirst().at(0).toMap();
    QCOMPARE(row.value(QStringLiteral("title")).toString(), QStringLiteral("Connection"));
    QCOMPARE(row.value(QStringLiteral("payload")).toString(), expected);
}

void ConnectionErrorTextTest::clientErrorUsesSameRawTextForSessionAndLog()
{
    TranslatorGuard translatorGuard;
    MqttFixture fixture;
    QVERIFY2(fixture.initialize(), qPrintable(fixture.historyStore.lastError()));
    SessionState &session = *fixture.sessions.currentSession();
    QSignalSpy logSpy(&fixture.events, &EventHistoryService::logAppended);

    session.runtime.client->errorChanged(QMqttClient::TransportInvalid);

    QVERIFY2(session.runtime.lastError.startsWith(QStringLiteral("Invalid transport")),
        qPrintable(session.runtime.lastError));
    QCOMPARE(logSpy.count(), 1);
    const QVariantMap row = logSpy.takeFirst().at(0).toMap();
    QCOMPARE(row.value(QStringLiteral("title")).toString(), QStringLiteral("Error"));
    QCOMPARE(row.value(QStringLiteral("payload")).toString(), session.runtime.lastError);
}

void ConnectionErrorTextTest::connectionTimeoutIgnoresApplicationTranslator()
{
    TranslatorGuard translatorGuard;
    QBuffer transport;
    QVERIFY(transport.open(QIODevice::ReadWrite));
    MqttFixture fixture(false);
    QVERIFY2(fixture.initialize(), qPrintable(fixture.historyStore.lastError()));
    SessionState &session = *fixture.sessions.currentSession();
    auto *client = session.runtime.client;
    QVERIFY(client);
    QVERIFY(session.runtime.connectTimeoutTimer);
    client->setTransport(&transport, QMqttClient::IODevice);
    QSignalSpy runtimeErrorSpy(&fixture.sessions, &SessionService::runtimeError);
    QSignalSpy logSpy(&fixture.events, &EventHistoryService::logAppended);

    client->connectToHost();
    QCOMPARE(client->state(), QMqttClient::Connecting);
    session.runtime.connectTimeoutTimer->start(0);

    QTRY_COMPARE(session.runtime.lastError, QStringLiteral("Connection timed out."));
    QCOMPARE(runtimeErrorSpy.count(), 1);
    const QList<QVariant> arguments = runtimeErrorSpy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), session.id);
    QCOMPARE(arguments.at(1).toString(), QStringLiteral("Error"));
    QCOMPARE(arguments.at(2).toString(), session.runtime.lastError);
    QVERIFY(logSpy.count() >= 1);
    const QVariantMap row = logSpy.takeFirst().at(0).toMap();
    QCOMPARE(row.value(QStringLiteral("title")).toString(), QStringLiteral("Error"));
    QCOMPARE(row.value(QStringLiteral("payload")).toString(), session.runtime.lastError);
}

void ConnectionErrorTextTest::pendingReconnectStartsAfterDisconnectedSignal()
{
    QBuffer transport;
    QVERIFY(transport.open(QIODevice::ReadWrite));
    MqttFixture fixture;
    QVERIFY2(fixture.initialize(), qPrintable(fixture.historyStore.lastError()));
    SessionState &session = *fixture.sessions.currentSession();
    auto *client = session.runtime.client;
    QVERIFY(client);
    client->setTransport(&transport, QMqttClient::IODevice);
    session.runtime.disconnectRequested = true;
    session.runtime.reconnectPending = true;

    client->disconnected();

    QVERIFY(!session.runtime.disconnectRequested);
    QVERIFY(!session.runtime.reconnectPending);
    QCOMPARE(client->state(), QMqttClient::Connecting);
    QVERIFY(session.runtime.connectionStartedAtMs > 0);
}

void ConnectionErrorTextTest::qosZeroPublishDoesNotRemainQueued()
{
    TestMqttTransport transport;
    QVERIFY(transport.open(QIODevice::ReadWrite));
    MqttFixture fixture;
    QVERIFY2(fixture.initialize(), qPrintable(fixture.historyStore.lastError()));
    SessionState &session = *fixture.sessions.currentSession();
    auto *client = session.runtime.client;
    QVERIFY(client);
    client->setTransport(&transport, QMqttClient::IODevice);
    client->setProtocolVersion(QMqttClient::MQTT_5_0);

    fixture.mqtt.connectCurrentSession();
    QCOMPARE(client->state(), QMqttClient::Connecting);
    transport.appendIncoming(QByteArray::fromHex("2003000000"));
    QTRY_COMPARE(client->state(), QMqttClient::Connected);
    QSignalSpy messageSentSpy(client, &QMqttClient::messageSent);

    QVERIFY(fixture.mqtt.publishCurrentSession(
        QStringLiteral("mqtt-plus/test"),
        QStringLiteral("payload"),
        0,
        0,
        false));
    QCOMPARE(session.runtime.publishStatus.messageId, 0);
    QCOMPARE(session.runtime.publishStatus.state, QStringLiteral("sent"));
    QCOMPARE(messageSentSpy.count(), 0);
}

QTEST_GUILESS_MAIN(ConnectionErrorTextTest)

#include "test_connectionerrortext.moc"
