#include "models/eventstreammodel.h"
#include "services/apputils.h"
#include "services/storage/historystore.h"
#include "services/storage/historywriterworker.h"
#include "services/parsing/messageparseworker.h"
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

    QByteArray takeWritten()
    {
        const QByteArray written = m_written;
        m_written.clear();
        return written;
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

    qint64 writeData(const char *data, qint64 maxSize) override
    {
        m_written.append(data, maxSize);
        return maxSize;
    }

private:
    QByteArray m_incoming;
    QByteArray m_written;
};

QByteArray acknowledgmentPacket(quint8 packetType, qint32 messageId)
{
    QByteArray packet;
    packet.append(static_cast<char>(packetType));
    packet.append(static_cast<char>(0x02));
    packet.append(static_cast<char>((messageId >> 8) & 0xff));
    packet.append(static_cast<char>(messageId & 0xff));
    return packet;
}

quint16 packetIdentifier(const QByteArray &packet)
{
    if (packet.size() < 4 || (static_cast<quint8>(packet.at(1)) & 0x80) != 0) {
        return 0;
    }
    return static_cast<quint16>(
        (static_cast<quint8>(packet.at(2)) << 8)
        | static_cast<quint8>(packet.at(3)));
}

QStringList publishProgressStates(const QSignalSpy &spy)
{
    QStringList states;
    for (const QList<QVariant> &arguments : spy) {
        states.append(arguments.first().toMap().value(QStringLiteral("state")).toString());
    }
    return states;
}

struct MqttFixture
{
    QTemporaryDir dataDir;
    QSettings settings;
    HistoryStore historyStore;
    HistoryWriterWorker historyWriter;
    MessageParseWorker messageParser;
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
        , historyWriter(dataDir.path(), historyStore.nextMessageId())
        , preferences(&settings)
        , sessions(settings, scripts, historyStore, preferences)
        , events(
              sessions,
              historyStore,
              historyWriter,
              messageParser,
              messages,
              logs,
              scripts,
              launchTimestamp,
              preferences)
        , subscriptions(sessions, scripts, events)
        , mqtt(sessions, subscriptions, events)
    {
        historyWriter.start();
        messageParser.start();
        sessions.setHistoryWriter(&historyWriter);
        sessions.setMessageParser(&messageParser);
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
    void qosTwoPublishCompletesHandshake();
    void qosTwoSubscriptionIsRequestedAndGranted();
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
    QCOMPARE(
        session.runtime.recentPublishedTraffic.eventCount(QDateTime::currentMSecsSinceEpoch()),
        1);
    QCOMPARE(
        session.runtime.recentPublishedTraffic.byteCount(QDateTime::currentMSecsSinceEpoch()),
        qint64(7));
    QCOMPARE(messageSentSpy.count(), 0);
}

void ConnectionErrorTextTest::qosTwoPublishCompletesHandshake()
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
    transport.appendIncoming(QByteArray::fromHex("2003000000"));
    QTRY_COMPARE(client->state(), QMqttClient::Connected);
    transport.takeWritten();
    QSignalSpy progressSpy(&fixture.mqtt, &MqttSessionService::publishProgress);

    QVERIFY(fixture.mqtt.publishCurrentSession(
        QStringLiteral("mqtt-plus/qos2"),
        QStringLiteral("payload"),
        0,
        2,
        false));
    QCOMPARE(session.runtime.publishStatus.qos, 2);
    QCOMPARE(session.runtime.publishStatus.state, QStringLiteral("queued"));
    QVERIFY(session.runtime.publishStatus.messageId > 0);

    const QByteArray publishPacket = transport.takeWritten();
    QVERIFY(!publishPacket.isEmpty());
    QCOMPARE(static_cast<quint8>(publishPacket.front()), quint8(0x34));

    const qint32 messageId = session.runtime.publishStatus.messageId;
    transport.appendIncoming(acknowledgmentPacket(0x50, messageId));
    QTRY_VERIFY(publishProgressStates(progressSpy).contains(QStringLiteral("received")));

    const QByteArray releasePacket = transport.takeWritten();
    QVERIFY(!releasePacket.isEmpty());
    QCOMPARE(static_cast<quint8>(releasePacket.front()), quint8(0x62));

    transport.appendIncoming(acknowledgmentPacket(0x70, messageId));
    QTRY_COMPARE(session.runtime.publishStatus.state, QStringLiteral("completed"));
    QTRY_VERIFY(publishProgressStates(progressSpy).contains(QStringLiteral("completed")));
}

void ConnectionErrorTextTest::qosTwoSubscriptionIsRequestedAndGranted()
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
    transport.appendIncoming(QByteArray::fromHex("2003000000"));
    QTRY_COMPARE(client->state(), QMqttClient::Connected);
    transport.takeWritten();

    QVERIFY(fixture.subscriptions.upsertCurrentSubscription(
        QStringLiteral("mqtt-plus/qos2/#"),
        2,
        0,
        QString(),
        QString(),
        QStringLiteral("QoS 2")));
    QCOMPARE(session.subscriptions.size(), 1);
    QCOMPARE(session.subscriptions.first().requestedQos, 2);

    const QByteArray subscribePacket = transport.takeWritten();
    QVERIFY(subscribePacket.size() >= 5);
    QCOMPARE(static_cast<quint8>(subscribePacket.front()), quint8(0x82));
    QCOMPARE(static_cast<quint8>(subscribePacket.back()) & quint8(0x03), quint8(0x02));
    const quint16 messageId = packetIdentifier(subscribePacket);
    QVERIFY(messageId > 0);

    QByteArray subAck = acknowledgmentPacket(0x90, messageId);
    subAck[1] = static_cast<char>(0x04);
    subAck.append(static_cast<char>(0x00));
    subAck.append(static_cast<char>(0x02));
    transport.appendIncoming(subAck);

    QTRY_COMPARE(session.subscriptions.first().runtimeState, QStringLiteral("subscribed"));
    QCOMPARE(session.subscriptions.first().grantedQos, 2);
}

QTEST_GUILESS_MAIN(ConnectionErrorTextTest)

#include "test_connectionerrortext.moc"
