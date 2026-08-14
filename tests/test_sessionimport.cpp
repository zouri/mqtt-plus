#include "services/storage/historystore.h"
#include "usecases/preferencescontroller.h"
#include "usecases/sessionservice.h"

#include <QSettings>
#include <QCborValue>
#include <QTemporaryDir>
#include <QtTest>

namespace {
int controlledSettingsWriteCount = 0;
int failControlledSettingsWriteAt = 0;
QSettings::SettingsMap lastSuccessfulSettings;

bool readControlledSettings(QIODevice &, QSettings::SettingsMap &settings)
{
    settings.clear();
    return true;
}

bool writeControlledSettings(
    QIODevice &device,
    const QSettings::SettingsMap &settings)
{
    ++controlledSettingsWriteCount;
    if (controlledSettingsWriteCount == failControlledSettingsWriteAt) {
        return false;
    }
    lastSuccessfulSettings = settings;
    return device.write("ok") == 2;
}

QSettings::Format controlledSettingsFormat()
{
    static const QSettings::Format format = QSettings::registerFormat(
        QStringLiteral("mqtt-plus-controlled-session-import-settings"),
        readControlledSettings,
        writeControlledSettings);
    return format;
}
}

class SessionImportTest : public QObject
{
    Q_OBJECT

private slots:
    void mergesPersistsAndRollsBackImportedSessions();
    void restoresSettingsCacheWhenImportWriteFails();
    void persistsProcessorReference();
};

void SessionImportTest::mergesPersistsAndRollsBackImportedSessions()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(
        directory.filePath(QStringLiteral("settings.ini")),
        QSettings::IniFormat);
    PreferencesController preferences(&settings);
    HistoryStore historyStore(directory.filePath(QStringLiteral("history")));
    SessionService service(settings, historyStore, preferences);

    QVERIFY(service.loadSessions());
    QCOMPARE(service.sessions().size(), 1);
    service.setCurrentSessionIndex(0);
    const QString originalSessionId = service.currentSession()->id;

    SessionImportRequest request;
    request.id = QStringLiteral("imported-session");
    request.config.name = QStringLiteral("Session 1");
    request.config.host = QStringLiteral("broker.example.test");
    request.config.port = 8883;
    request.config.transport = QStringLiteral("tls");
    request.config.protocolVersion = 5;
    request.config.clientId = QStringLiteral("imported-client");
    request.config.username = QStringLiteral("user");
    request.config.password = QStringLiteral("secret");
    request.config.cleanSession = false;
    request.config.keepAliveSeconds = 45;
    request.config.connectTimeoutSeconds = 25;
    request.config.sessionExpiryInterval = 60;
    request.config.receiveMaximum = 100;
    request.config.maximumPacketSize = 1024;
    request.config.topicAliasMaximum = 5;
    request.config.requestResponseInformation = true;
    request.config.requestProblemInformation = true;
    request.config.authenticationMethod = QStringLiteral("token");
    request.config.authenticationData = QStringLiteral("auth-data");
    SubscriptionEntry active;
    active.topic = QStringLiteral("devices/+/state");
    active.alias = QStringLiteral("Devices");
    active.requestedQos = 1;
    active.processor.processorId = QStringLiteral("missing-processor");
    SubscriptionEntry paused;
    paused.topic = QStringLiteral("alerts/#");
    paused.paused = true;
    SubscriptionEntry invalid;
    invalid.topic = QStringLiteral("invalid/#/topic");
    request.subscriptions = {active, paused, invalid};

    QStringList importedIds;
    QString errorMessage;
    QVERIFY(service.importSessions({request}, importedIds, errorMessage));
    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
    QCOMPARE(importedIds, QStringList {QStringLiteral("imported-session")});
    QCOMPARE(service.sessions().size(), 2);
    QCOMPARE(service.currentSession()->id, originalSessionId);

    const SessionState *imported = service.sessionById(QStringLiteral("imported-session"));
    QVERIFY(imported);
    QCOMPARE(imported->name, QStringLiteral("Session 1 (Imported 2)"));
    QCOMPARE(imported->transport, QStringLiteral("tls"));
    QCOMPARE(imported->runtime.client->hostname(), QStringLiteral("broker.example.test"));
    QCOMPARE(imported->runtime.client->port(), 8883);
    QCOMPARE(imported->runtime.client->keepAlive(), 45);
    QCOMPARE(imported->runtime.client->cleanSession(), false);
    QCOMPARE(imported->connectTimeoutSeconds, 25);
    QCOMPARE(imported->sessionExpiryInterval, quint32(60));
    QCOMPARE(imported->receiveMaximum, quint16(100));
    QCOMPARE(imported->maximumPacketSize, quint32(1024));
    QCOMPARE(imported->topicAliasMaximum, quint16(5));
    QCOMPARE(imported->requestResponseInformation, true);
    QCOMPARE(imported->requestProblemInformation, true);
    QCOMPARE(imported->authenticationMethod, QStringLiteral("token"));
    QCOMPARE(imported->authenticationData, QStringLiteral("auth-data"));
    QCOMPARE(imported->runtime.client->state(), QMqttClient::Disconnected);
    QCOMPARE(imported->subscriptions.size(), 2);
    QCOMPARE(
        imported->subscriptions.first().processor.processorId,
        QStringLiteral("missing-processor"));
    QCOMPARE(imported->subscriptions.last().paused, true);

    {
        PreferencesController reloadedPreferences(&settings);
        HistoryStore reloadedHistory(directory.filePath(QStringLiteral("reloaded-history")));
        SessionService reloaded(
            settings,
            reloadedHistory,
            reloadedPreferences);
        QVERIFY(reloaded.loadSessions());
        QCOMPARE(reloaded.sessions().size(), 2);
        const SessionState *stored = reloaded.sessionById(QStringLiteral("imported-session"));
        QVERIFY(stored);
        QCOMPARE(stored->runtime.client->keepAlive(), 45);
        QCOMPARE(stored->connectTimeoutSeconds, 25);
        QCOMPARE(stored->sessionExpiryInterval, quint32(60));
        QCOMPARE(stored->receiveMaximum, quint16(100));
        QCOMPARE(stored->maximumPacketSize, quint32(1024));
        QCOMPARE(stored->topicAliasMaximum, quint16(5));
        QCOMPARE(stored->authenticationMethod, QStringLiteral("token"));
        QCOMPARE(stored->authenticationData, QStringLiteral("auth-data"));
        QCOMPARE(stored->subscriptions.size(), 2);
        QCOMPARE(
            stored->subscriptions.first().processor.processorId,
            QStringLiteral("missing-processor"));
        QCOMPARE(stored->subscriptions.last().paused, true);
    }

    service.setCurrentSessionIndex(1);
    QSignalSpy historyReloadSpy(
        &service,
        &SessionService::currentSessionHistoryReloadRequested);
    QSignalSpy indexChangedSpy(
        &service,
        &SessionService::currentSessionIndexChanged);
    QSignalSpy currentChangedSpy(
        &service,
        &SessionService::currentSessionChanged);
    QVERIFY(service.rollbackImportedSessions(importedIds, errorMessage));
    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
    QCOMPARE(service.sessions().size(), 1);
    QVERIFY(!service.sessionById(QStringLiteral("imported-session")));
    QCOMPARE(service.currentIndex(), 0);
    QCOMPARE(service.currentSession()->id, originalSessionId);
    QCOMPARE(historyReloadSpy.size(), 1);
    QCOMPARE(indexChangedSpy.size(), 1);
    QCOMPARE(currentChangedSpy.size(), 1);

    PreferencesController finalPreferences(&settings);
    HistoryStore finalHistory(directory.filePath(QStringLiteral("final-history")));
    SessionService finalService(
        settings,
        finalHistory,
        finalPreferences);
    QVERIFY(finalService.loadSessions());
    QCOMPARE(finalService.sessions().size(), 1);
    QCOMPARE(finalService.sessions().first().id, originalSessionId);
}

void SessionImportTest::persistsProcessorReference()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));

    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        PreferencesController preferences(&settings);
        HistoryStore historyStore(directory.filePath(QStringLiteral("history")));
        SessionService service(settings, historyStore, preferences);
        QVERIFY(service.loadSessions());
        service.setCurrentSessionIndex(0);

        SubscriptionEntry entry;
        entry.topic = QStringLiteral("devices/processor");
        entry.processor.processorId = QStringLiteral("processor-1");
        entry.processor.parameters.insert(QStringLiteral("gain"), 2);
        entry.processor.parameters.insert(QStringLiteral("unit"), QStringLiteral("C"));
        service.currentSession()->subscriptions.append(entry);
        QVERIFY(service.saveSessions());
    }

    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        PreferencesController preferences(&settings);
        HistoryStore historyStore(directory.filePath(QStringLiteral("reloaded-history")));
        SessionService service(settings, historyStore, preferences);
        QVERIFY(service.loadSessions());
        QCOMPARE(service.sessions().size(), 1);
        QCOMPARE(service.sessions().first().subscriptions.size(), 1);
        const SubscriptionEntry &entry = service.sessions().first().subscriptions.first();
        QCOMPARE(entry.processor.processorId, QStringLiteral("processor-1"));
        QCOMPARE(entry.processor.parameters.value(QStringLiteral("gain")).toInteger(), qint64(2));
        QCOMPARE(entry.processor.parameters.value(QStringLiteral("unit")).toString(), QStringLiteral("C"));
    }
}

void SessionImportTest::restoresSettingsCacheWhenImportWriteFails()
{
    const QSettings::Format format = controlledSettingsFormat();
    QVERIFY(format != QSettings::InvalidFormat);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    controlledSettingsWriteCount = 0;
    failControlledSettingsWriteAt = 0;
    lastSuccessfulSettings.clear();
    QSettings settings(
        directory.filePath(QStringLiteral("settings.controlled")),
        format);
    PreferencesController preferences(&settings);
    HistoryStore historyStore(directory.filePath(QStringLiteral("history")));
    SessionService service(settings, historyStore, preferences);
    QVERIFY(service.loadSessions());
    QCOMPARE(service.sessions().size(), 1);

    SessionImportRequest request;
    request.id = QStringLiteral("failed-import");
    request.config.name = QStringLiteral("Failed import");
    request.config.host = QStringLiteral("broker.example.test");
    QStringList importedIds;
    QString errorMessage;
    controlledSettingsWriteCount = 0;
    failControlledSettingsWriteAt = 1;
    lastSuccessfulSettings.clear();

    QVERIFY(!service.importSessions({request}, importedIds, errorMessage));
    QCOMPARE(service.sessions().size(), 1);
    QVERIFY(importedIds.isEmpty());
    QCOMPARE(controlledSettingsWriteCount, 2);
    QCOMPARE(lastSuccessfulSettings.value(QStringLiteral("sessions/size")).toInt(), 1);
    for (auto it = lastSuccessfulSettings.cbegin(); it != lastSuccessfulSettings.cend(); ++it) {
        QVERIFY(it.value().toString() != QStringLiteral("failed-import"));
    }
}

QTEST_GUILESS_MAIN(SessionImportTest)
#include "test_sessionimport.moc"
