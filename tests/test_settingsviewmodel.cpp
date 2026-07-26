#include "usecases/preferencescontroller.h"
#include "models/eventstreammodel.h"
#include "services/storage/historystore.h"
#include "usecases/eventhistoryservice.h"
#include "usecases/scriptservice.h"
#include "usecases/sessionservice.h"
#include "viewmodels/settingsviewmodel.h"

#include <QtTest/QtTest>

#include <QTemporaryDir>

class SettingsFixture
{
public:
    SettingsFixture()
        : settings(dataDir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat)
        , preferencesController(&settings)
        , historyStore(dataDir.path())
        , sessionService(settings, scriptService, historyStore, preferencesController)
        , eventHistoryService(
              sessionService,
              historyStore,
              messages,
              logs,
              scriptService,
              launchTimestamp,
              preferencesController)
    {
    }

    QTemporaryDir dataDir;
    QSettings settings;
    PreferencesController preferencesController;
    HistoryStore historyStore;
    ScriptService scriptService;
    SessionService sessionService;
    EventStreamModel messages;
    EventStreamModel logs;
    QString launchTimestamp = QStringLiteral("2026-07-25T00:00:00.000Z");
    EventHistoryService eventHistoryService;
};

class SettingsOptionsViewModelTest : public QObject
{
    Q_OBJECT

private slots:
    void exposesDefaultSettingIndexes();
    void readsSettings();
    void messageRetentionChangeDefersCleanup();
    void writesSettingsAndClearsHistory();
    void themeChangesEmitSignals();
    void themeColorPersistsAndEmitsSignal();
    void languageChangesEmitSignals();
    void messagePayloadDisplayModePersistsAndEmitsSignal();
};

void SettingsOptionsViewModelTest::exposesDefaultSettingIndexes()
{
    SettingsFixture deps;
    SettingsViewModel settings(
        deps.preferencesController,
        deps.eventHistoryService,
        deps.historyStore,
        deps.sessionService.sessions(),
        deps.settings);

    QCOMPARE(settings.themeModeIndex(), 0);
    QCOMPARE(settings.themeColor(), QStringLiteral("mint"));
    QCOMPARE(settings.languageModeIndex(), 0);
    QCOMPARE(settings.messagePayloadDisplayModeIndex(), 1);
    QCOMPARE(settings.messageRetentionLimitIndex(), 1);
    QCOMPARE(settings.logRetentionLimitIndex(), 1);
    QCOMPARE(settings.historyPageSizeIndex(), 1);
    QCOMPARE(settings.maxIncomingPayloadBytesIndex(), 1);
    QCOMPARE(deps.preferencesController.autoCollapseConnectionListOnConnect(), true);
    QCOMPARE(settings.clearMessagesOnExitIndex(), 0);
    QCOMPARE(settings.clearLogsOnExitIndex(), 0);
    QCOMPARE(deps.preferencesController.subscriptionPaneWidth(), 320);
    QCOMPARE(deps.preferencesController.publishComposerHeight(), 168);
    QCOMPARE(deps.preferencesController.connectionPaneCollapsed(), false);
}

void SettingsOptionsViewModelTest::readsSettings()
{
    SettingsFixture deps;
    deps.preferencesController.setMessageRetentionLimit(10000);
    deps.preferencesController.setLogRetentionLimit(5000);
    deps.preferencesController.setWindowGeometry(1200, 700);
    deps.preferencesController.setWindowMaximized(true);

    deps.settings.setValue(QStringLiteral("appearance/themeMode"), QStringLiteral("dark"));
    deps.settings.setValue(QStringLiteral("appearance/themeColor"), QStringLiteral("violet"));
    deps.settings.setValue(QStringLiteral("appearance/languageMode"), QStringLiteral("zh_CN"));
    deps.settings.setValue(QStringLiteral("workbench/messagePayloadDisplayMode"), QStringLiteral("full"));
    SettingsViewModel settings(
        deps.preferencesController,
        deps.eventHistoryService,
        deps.historyStore,
        deps.sessionService.sessions(),
        deps.settings);

    QCOMPARE(settings.themeModeIndex(), 2);
    QCOMPARE(settings.effectiveTheme(), QStringLiteral("dark"));
    QCOMPARE(settings.themeColor(), QStringLiteral("violet"));
    QCOMPARE(settings.languageModeIndex(), 2);
    QCOMPARE(settings.messagePayloadDisplayModeIndex(), 2);
    QCOMPARE(settings.messageRetentionLimitIndex(), 2);
    QCOMPARE(settings.logRetentionLimitIndex(), 2);
    QCOMPARE(deps.preferencesController.windowWidth(), 1200);
    QCOMPARE(deps.preferencesController.windowHeight(), 700);
    QCOMPARE(deps.preferencesController.windowMaximized(), true);
}

void SettingsOptionsViewModelTest::messageRetentionChangeDefersCleanup()
{
    SettingsFixture deps;
    QVERIFY(deps.dataDir.isValid());
    QVERIFY2(deps.historyStore.isReady(), qPrintable(deps.historyStore.lastError()));

    deps.sessionService.sessions().append(SessionState {});
    deps.sessionService.setCurrentSessionIndex(0);
    SessionState &session = *deps.sessionService.currentSession();
    session.id = QStringLiteral("session-1");

    for (int index = 0; index < 1001; ++index) {
        MessageRecord record;
        record.sessionId = session.id;
        record.timestamp = QString::number(index);
        record.topic = QStringLiteral("topic");
        record.payloadBytes = QByteArray::number(index);
        QVERIFY(deps.historyStore.enqueueMessage(record) > 0);
    }
    QVERIFY(!deps.historyStore.flushPendingMessages().isEmpty());
    QCOMPARE(deps.historyStore.loadMessages(session.id, 2000).size(), 1001);

    MessageRecord pendingRecord;
    pendingRecord.sessionId = session.id;
    pendingRecord.timestamp = QStringLiteral("pending");
    pendingRecord.topic = QStringLiteral("topic");
    pendingRecord.payloadBytes = QByteArrayLiteral("pending");
    QVERIFY(deps.historyStore.enqueueMessage(pendingRecord) > 0);
    QCOMPARE(deps.historyStore.pendingMessageCount(), 1);

    SettingsViewModel settings(
        deps.preferencesController,
        deps.eventHistoryService,
        deps.historyStore,
        deps.sessionService.sessions(),
        deps.settings);
    QSignalSpy messageSpy(&deps.eventHistoryService, &EventHistoryService::messageStreamChanged);

    settings.setMessageRetentionLimitIndex(0);

    QCOMPARE(deps.preferencesController.messageRetentionLimit(), 1000);
    QCOMPARE(deps.historyStore.pendingMessageCount(), 1);
    QCOMPARE(deps.historyStore.loadMessages(session.id, 2000).size(), 1001);
    QCOMPARE(messageSpy.count(), 0);
}

void SettingsOptionsViewModelTest::writesSettingsAndClearsHistory()
{
    SettingsFixture deps;
    deps.sessionService.sessions().append(SessionState {});
    deps.sessionService.setCurrentSessionIndex(0);
    SessionState &session = *deps.sessionService.currentSession();
    session.id = QStringLiteral("session-1");
    session.runtime.totalMessageCount = 12;
    session.runtime.viewedMessageCount = 8;
    SettingsViewModel settings(
        deps.preferencesController,
        deps.eventHistoryService,
        deps.historyStore,
        deps.sessionService.sessions(),
        deps.settings);
    QSignalSpy reloadSpy(&deps.eventHistoryService, &EventHistoryService::totalMessageCountChanged);
    QSignalSpy messageSpy(&deps.eventHistoryService, &EventHistoryService::messageStreamChanged);
    QSignalSpy logSpy(&deps.eventHistoryService, &EventHistoryService::logStreamChanged);

    settings.setThemeModeIndex(1);
    settings.setThemeColor(QStringLiteral("blue"));
    settings.setLanguageModeIndex(1);
    settings.setMessagePayloadDisplayModeIndex(0);
    settings.setMessageRetentionLimitIndex(0);
    settings.setLogRetentionLimitIndex(0);
    settings.setHistoryPageSizeIndex(2);
    settings.setMaxIncomingPayloadBytesIndex(2);
    settings.setClearMessagesOnExitIndex(2);
    settings.setClearLogsOnExitIndex(1);
    deps.preferencesController.setDeleteHistoryWithSession(false);
    deps.preferencesController.setSaveMessagesWhenOutputPaused(false);
    deps.preferencesController.setAutoCollapseConnectionListOnConnect(false);
    deps.preferencesController.setWindowMaximized(true);
    deps.preferencesController.setWindowGeometry(1600, 900);
    deps.preferencesController.setWorkbenchLayout(410, 230, true);
    deps.eventHistoryService.clearAllMessages();
    deps.eventHistoryService.clearAllLogs();
    deps.eventHistoryService.clearAllHistory();

    QCOMPARE(settings.themeMode(), QStringLiteral("light"));
    QCOMPARE(settings.themeColor(), QStringLiteral("blue"));
    QCOMPARE(deps.settings.value(QStringLiteral("appearance/themeColor")).toString(), QStringLiteral("blue"));
    QCOMPARE(settings.languageMode(), QStringLiteral("en"));
    QCOMPARE(settings.messagePayloadDisplayModeIndex(), 0);
    QCOMPARE(deps.settings.value(QStringLiteral("workbench/messagePayloadDisplayMode")).toString(), QStringLiteral("compact"));
    QCOMPARE(deps.preferencesController.messageRetentionLimit(), 1000);
    QCOMPARE(deps.preferencesController.logRetentionLimit(), 500);
    QCOMPARE(deps.preferencesController.historyPageSize(), 1000);
    QCOMPARE(deps.preferencesController.maxIncomingPayloadBytes(), 5242880);
    QCOMPARE(deps.preferencesController.clearMessagesOnExit(), QStringLiteral("all"));
    QCOMPARE(deps.preferencesController.clearLogsOnExit(), QStringLiteral("current"));
    QCOMPARE(deps.preferencesController.deleteHistoryWithSession(), false);
    QCOMPARE(deps.preferencesController.saveMessagesWhenOutputPaused(), false);
    QCOMPARE(deps.preferencesController.autoCollapseConnectionListOnConnect(), false);
    QCOMPARE(deps.preferencesController.windowMaximized(), true);
    QCOMPARE(deps.preferencesController.windowWidth(), 1600);
    QCOMPARE(deps.preferencesController.windowHeight(), 900);
    QCOMPARE(deps.preferencesController.subscriptionPaneWidth(), 410);
    QCOMPARE(deps.preferencesController.publishComposerHeight(), 230);
    QCOMPARE(deps.preferencesController.connectionPaneCollapsed(), true);
    QCOMPARE(deps.settings.value(QStringLiteral("workspace/subscriptionPaneWidth")).toInt(), 410);
    QCOMPARE(deps.settings.value(QStringLiteral("workspace/publishComposerHeight")).toInt(), 230);
    QVERIFY(!deps.settings.contains(QStringLiteral("workspace/subscriptionPaneCollapsed")));
    QCOMPARE(session.runtime.totalMessageCount, 0);
    QCOMPARE(session.runtime.viewedMessageCount, 0);
    QCOMPARE(reloadSpy.count(), 3);
    QCOMPARE(messageSpy.count(), 3);
    QCOMPARE(logSpy.count(), 3);
}

void SettingsOptionsViewModelTest::themeChangesEmitSignals()
{
    SettingsFixture deps;
    SettingsViewModel settings(
        deps.preferencesController,
        deps.eventHistoryService,
        deps.historyStore,
        deps.sessionService.sessions(),
        deps.settings);

    settings.setThemeModeIndex(1);
    QSignalSpy themeSpy(&settings, &SettingsViewModel::themeModeChanged);
    QSignalSpy effectiveSpy(&settings, &SettingsViewModel::effectiveThemeChanged);

    settings.setThemeModeIndex(2);
    settings.setThemeModeIndex(2);
    QCOMPARE(themeSpy.count(), 1);
    QCOMPARE(effectiveSpy.count(), 1);
}

void SettingsOptionsViewModelTest::themeColorPersistsAndEmitsSignal()
{
    SettingsFixture deps;
    SettingsViewModel settings(
        deps.preferencesController,
        deps.eventHistoryService,
        deps.historyStore,
        deps.sessionService.sessions(),
        deps.settings);
    QSignalSpy colorSpy(&settings, &SettingsViewModel::themeColorChanged);

    settings.setThemeColor(QStringLiteral("rose"));
    settings.setThemeColor(QStringLiteral("rose"));
    QCOMPARE(settings.themeColor(), QStringLiteral("rose"));
    QCOMPARE(colorSpy.count(), 1);

    settings.setThemeColor(QStringLiteral("unsupported"));
    QCOMPARE(settings.themeColor(), QStringLiteral("mint"));
    QCOMPARE(colorSpy.count(), 2);
}

void SettingsOptionsViewModelTest::languageChangesEmitSignals()
{
    SettingsFixture deps;
    SettingsViewModel settings(
        deps.preferencesController,
        deps.eventHistoryService,
        deps.historyStore,
        deps.sessionService.sessions(),
        deps.settings);
    QSignalSpy modeSpy(&settings, &SettingsViewModel::languageModeChanged);
    QSignalSpy langSpy(&settings, &SettingsViewModel::languageChanged);

    settings.setLanguageModeIndex(2);
    QCOMPARE(modeSpy.count(), 1);
    QCOMPARE(langSpy.count(), 1);
}

void SettingsOptionsViewModelTest::messagePayloadDisplayModePersistsAndEmitsSignal()
{
    SettingsFixture deps;
    SettingsViewModel settings(
        deps.preferencesController,
        deps.eventHistoryService,
        deps.historyStore,
        deps.sessionService.sessions(),
        deps.settings);
    QSignalSpy modeSpy(&settings, &SettingsViewModel::messagePayloadDisplayModeChanged);

    settings.setMessagePayloadDisplayModeIndex(2);
    settings.setMessagePayloadDisplayModeIndex(2);
    QCOMPARE(settings.messagePayloadDisplayModeIndex(), 2);
    QCOMPARE(modeSpy.count(), 1);
    QCOMPARE(deps.settings.value(QStringLiteral("workbench/messagePayloadDisplayMode")).toString(), QStringLiteral("full"));

    SettingsViewModel restoredSettings(
        deps.preferencesController,
        deps.eventHistoryService,
        deps.historyStore,
        deps.sessionService.sessions(),
        deps.settings);
    QCOMPARE(restoredSettings.messagePayloadDisplayModeIndex(), 2);
}

QTEST_MAIN(SettingsOptionsViewModelTest)

#include "test_settingsviewmodel.moc"
