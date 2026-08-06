#include "services/configuration/configurationadapters.h"
#include "services/storage/historystore.h"
#include "usecases/configurationtransferservice.h"
#include "usecases/draftlibraryservice.h"
#include "usecases/preferencescontroller.h"
#include "usecases/sessionservice.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

namespace {
int controlledSettingsWriteCount = 0;
int failControlledSettingsWriteAt = 0;

bool readControlledSettings(QIODevice &, QSettings::SettingsMap &settings)
{
    settings.clear();
    return true;
}

bool writeControlledSettings(QIODevice &device, const QSettings::SettingsMap &)
{
    ++controlledSettingsWriteCount;
    if (controlledSettingsWriteCount == failControlledSettingsWriteAt) {
        return false;
    }
    return device.write("ok") == 2;
}

QSettings::Format controlledSettingsFormat()
{
    static const QSettings::Format format = QSettings::registerFormat(
        QStringLiteral("mqtt-plus-controlled-transfer-settings"),
        readControlledSettings,
        writeControlledSettings);
    return format;
}
}

class ConfigurationTransferServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void previewsImportsAndExportsWithoutCredentials();
    void rollsBackSessionsSettingsAndCertificatesWhenDraftSaveFails();
    void retainsCertificatesWhenSessionRollbackFails();
};

void ConfigurationTransferServiceTest::previewsImportsAndExportsWithoutCredentials()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(
        directory.filePath(QStringLiteral("settings.ini")),
        QSettings::IniFormat);
    PreferencesController preferences(&settings);
    HistoryStore historyStore(directory.filePath(QStringLiteral("history")));
    DraftLibraryService draftService(directory.filePath(QStringLiteral("drafts")));
    draftService.load();
    QTRY_VERIFY(draftService.ready());
    SessionService sessionService(
        settings,
        historyStore,
        preferences);
    QVERIFY(sessionService.loadSessions());

    ConfigurationTransferService transfer(
        sessionService,
        draftService,
        preferences,
        settings,
        directory.filePath(QStringLiteral("imported-certificates")));
    QSignalSpy operationSpy(
        &transfer,
        &ConfigurationTransferService::operationFinished);
    QSignalSpy previewSpy(
        &transfer,
        &ConfigurationTransferService::importPreviewReady);

    const QJsonObject subscription {
        {QStringLiteral("topic"), QStringLiteral("devices/#")},
        {QStringLiteral("alias"), QStringLiteral("Devices")},
        {QStringLiteral("qos"), 0},
        {QStringLiteral("disabled"), true},
    };
    const QJsonObject connection {
        {QStringLiteral("id"), QStringLiteral("mqttx-id")},
        {QStringLiteral("name"), QStringLiteral("Imported")},
        {QStringLiteral("host"), QStringLiteral("broker.example.test")},
        {QStringLiteral("port"), 1883},
        {QStringLiteral("protocol"), QStringLiteral("mqtt")},
        {QStringLiteral("mqttVersion"), QStringLiteral("5.0")},
        {QStringLiteral("clientId"), QStringLiteral("client")},
        {QStringLiteral("password"), QStringLiteral("secret")},
        {QStringLiteral("reconnect"), true},
        {QStringLiteral("subscriptions"), QJsonArray {subscription}},
    };
    const QString importPath = directory.filePath(QStringLiteral("mqttx.json"));
    QFile importFile(importPath);
    QVERIFY(importFile.open(QIODevice::WriteOnly));
    importFile.write(QJsonDocument(QJsonArray {connection}).toJson());
    importFile.close();

    QVERIFY(transfer.inspectImportFile(QUrl::fromLocalFile(importPath)));
    QTRY_COMPARE(previewSpy.size(), 1);
    QCOMPARE(transfer.previewFormat(), QStringLiteral("mqttx"));
    QCOMPARE(transfer.previewConnectionCount(), 1);
    QCOMPARE(transfer.previewSubscriptionCount(), 1);
    QVERIFY(transfer.previewContainsSensitiveData());
    QVERIFY(!transfer.previewWarnings().isEmpty());

    transfer.importPreview(false);
    QCOMPARE(operationSpy.size(), 1);
    QCOMPARE(operationSpy.at(0).at(0).toBool(), true);
    QCOMPARE(sessionService.sessions().size(), 2);
    const SessionState &imported = sessionService.sessions().last();
    QCOMPARE(imported.runtime.client->hostname(), QStringLiteral("broker.example.test"));
    QCOMPARE(imported.runtime.client->password(), QString());
    QCOMPARE(imported.subscriptions.size(), 1);
    QVERIFY(imported.subscriptions.first().paused);

    const QString exportPath = directory.filePath(QStringLiteral("backup.mqttplus.json"));
    QVERIFY(transfer.exportConfiguration(QUrl::fromLocalFile(exportPath), false));
    QCOMPARE(operationSpy.size(), 2);
    const QFileDevice::Permissions exportedPermissions = QFileInfo(exportPath).permissions();
    QVERIFY(!(exportedPermissions
        & (QFileDevice::ReadGroup
            | QFileDevice::WriteGroup
            | QFileDevice::ReadOther
            | QFileDevice::WriteOther)));
    QFile exportFile(exportPath);
    QVERIFY(exportFile.open(QIODevice::ReadOnly));
    const auto parsed = MqttPlusConfigAdapter::parse(exportFile.readAll());
    QVERIFY(parsed.ok);
    QCOMPARE(parsed.bundle.sessions.size(), 2);
    QCOMPARE(parsed.bundle.sessions.last().password, QString());
    QCOMPARE(parsed.bundle.sessions.last().subscriptions.size(), 1);
}

void ConfigurationTransferServiceTest::rollsBackSessionsSettingsAndCertificatesWhenDraftSaveFails()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(
        directory.filePath(QStringLiteral("settings.ini")),
        QSettings::IniFormat);
    settings.setValue(QStringLiteral("appearance/themeMode"), QStringLiteral("light"));
    settings.setValue(QStringLiteral("history/messageRetentionLimit"), 1000);
    settings.sync();

    PreferencesController preferences(&settings);
    HistoryStore historyStore(directory.filePath(QStringLiteral("history")));
    const QString blockedDraftRoot =
        directory.filePath(QStringLiteral("draft-storage-blocker"));
    QFile blocker(blockedDraftRoot);
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    blocker.write("x");
    blocker.close();
    DraftLibraryService draftService(blockedDraftRoot);
    draftService.load();
    QTRY_VERIFY(draftService.ready());
    SessionService sessionService(
        settings,
        historyStore,
        preferences);
    QVERIFY(sessionService.loadSessions());
    QCOMPARE(sessionService.sessions().size(), 1);

    const QString importedCertificateRoot =
        directory.filePath(QStringLiteral("imported-certificates"));
    ConfigurationTransferService transfer(
        sessionService,
        draftService,
        preferences,
        settings,
        importedCertificateRoot);
    QSignalSpy operationSpy(
        &transfer,
        &ConfigurationTransferService::operationFinished);
    QSignalSpy previewSpy(
        &transfer,
        &ConfigurationTransferService::importPreviewReady);

    ConfigurationTransfer::Bundle bundle;
    ConfigurationTransfer::SessionData session;
    session.name = QStringLiteral("Rollback broker");
    session.host = QStringLiteral("broker.example.test");
    session.caCertificate = QByteArrayLiteral(
        "-----BEGIN CERTIFICATE-----\nTEST\n-----END CERTIFICATE-----\n");
    bundle.sessions.append(session);
    PublishDraft draft;
    draft.id = QStringLiteral("rollback-draft");
    draft.name = QStringLiteral("Rollback draft");
    draft.formatId = QStringLiteral("text");
    bundle.drafts.append(draft);
    bundle.preferences.insert(
        QStringLiteral("appearance/themeMode"),
        QStringLiteral("dark"));
    bundle.preferences.insert(
        QStringLiteral("history/messageRetentionLimit"),
        5000);

    const auto serialized = MqttPlusConfigAdapter::serialize(bundle, true);
    QVERIFY(serialized.ok);
    const QString importPath =
        directory.filePath(QStringLiteral("rollback.mqttplus.json"));
    QFile importFile(importPath);
    QVERIFY(importFile.open(QIODevice::WriteOnly));
    QCOMPARE(importFile.write(serialized.content), serialized.content.size());
    importFile.close();

    QDir importedCertificateDirectory(importedCertificateRoot);
    const QStringList certificateDirectoriesBefore = importedCertificateDirectory.entryList(
        QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::Name);

    QVERIFY(transfer.inspectImportFile(QUrl::fromLocalFile(importPath)));
    QTRY_COMPARE(previewSpy.size(), 1);
    transfer.importPreview(true);

    QTRY_COMPARE(operationSpy.size(), 1);
    QCOMPARE(operationSpy.first().at(0).toBool(), false);
    QCOMPARE(sessionService.sessions().size(), 1);
    QCOMPARE(
        settings.value(QStringLiteral("appearance/themeMode")).toString(),
        QStringLiteral("light"));
    QCOMPARE(preferences.messageRetentionLimit(), 1000);
    QVERIFY(draftService.drafts().isEmpty());
    importedCertificateDirectory.refresh();
    QCOMPARE(
        importedCertificateDirectory.entryList(
            QDir::Dirs | QDir::NoDotAndDotDot,
            QDir::Name),
        certificateDirectoriesBefore);
}

void ConfigurationTransferServiceTest::retainsCertificatesWhenSessionRollbackFails()
{
    const QSettings::Format format = controlledSettingsFormat();
    QVERIFY(format != QSettings::InvalidFormat);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    controlledSettingsWriteCount = 0;
    failControlledSettingsWriteAt = 0;
    QSettings settings(
        directory.filePath(QStringLiteral("settings.controlled")),
        format);
    PreferencesController preferences(&settings);
    HistoryStore historyStore(directory.filePath(QStringLiteral("history")));
    const QString blockedDraftRoot = directory.filePath(QStringLiteral("draft-blocker"));
    QFile blocker(blockedDraftRoot);
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    blocker.write("x");
    blocker.close();
    DraftLibraryService draftService(blockedDraftRoot);
    draftService.load();
    QTRY_VERIFY(draftService.ready());
    SessionService sessionService(
        settings,
        historyStore,
        preferences);
    QVERIFY(sessionService.loadSessions());
    QCOMPARE(sessionService.sessions().size(), 1);

    const QString importedCertificateRoot =
        directory.filePath(QStringLiteral("imported-certificates"));
    ConfigurationTransferService transfer(
        sessionService,
        draftService,
        preferences,
        settings,
        importedCertificateRoot);
    QSignalSpy operationSpy(
        &transfer,
        &ConfigurationTransferService::operationFinished);
    QSignalSpy previewSpy(
        &transfer,
        &ConfigurationTransferService::importPreviewReady);

    ConfigurationTransfer::Bundle bundle;
    ConfigurationTransfer::SessionData session;
    session.name = QStringLiteral("Retained broker");
    session.host = QStringLiteral("broker.example.test");
    session.caCertificate = QByteArrayLiteral(
        "-----BEGIN CERTIFICATE-----\nTEST\n-----END CERTIFICATE-----\n");
    bundle.sessions.append(session);
    PublishDraft draft;
    draft.id = QStringLiteral("failed-rollback-draft");
    draft.name = QStringLiteral("Failed rollback draft");
    draft.formatId = QStringLiteral("text");
    bundle.drafts.append(draft);

    const auto serialized = MqttPlusConfigAdapter::serialize(bundle, true);
    QVERIFY(serialized.ok);
    const QString importPath = directory.filePath(QStringLiteral("retained.json"));
    QFile importFile(importPath);
    QVERIFY(importFile.open(QIODevice::WriteOnly));
    QCOMPARE(importFile.write(serialized.content), serialized.content.size());
    importFile.close();

    controlledSettingsWriteCount = 0;
    failControlledSettingsWriteAt = 2;
    QVERIFY(transfer.inspectImportFile(QUrl::fromLocalFile(importPath)));
    QTRY_COMPARE(previewSpy.size(), 1);
    transfer.importPreview(true);

    QTRY_COMPARE(operationSpy.size(), 1);
    QCOMPARE(operationSpy.first().at(0).toBool(), false);
    QCOMPARE(sessionService.sessions().size(), 2);
    const SessionState &retained = sessionService.sessions().last();
    QVERIFY(!retained.caFile.isEmpty());
    QVERIFY(QFileInfo::exists(retained.caFile));

    failControlledSettingsWriteAt = 0;
    QDir certificateDirectory = QFileInfo(retained.caFile).dir();
    QVERIFY(certificateDirectory.removeRecursively());
}

QTEST_GUILESS_MAIN(ConfigurationTransferServiceTest)
#include "test_configurationtransferservice.moc"
