#include "usecases/preferencescontroller.h"
#include "models/eventstreammodel.h"
#include "services/storage/historystore.h"
#include "usecases/eventhistoryservice.h"
#include "viewmodels/settingsviewmodel.h"

#include <QtTest/QtTest>

#include <QTemporaryDir>

#include <utility>

class FakeSettingsDeps
{
public:
    FakeSettingsDeps()
        : settings(QStringLiteral("mqtt-plus-test"), QStringLiteral("settings-viewmodel-test"))
        , preferencesController(&settings)
    {
        settings.clear();
    }

    SettingsViewModel::Dependencies dependencies()
    {
        return {
            &preferencesController,
            nullptr,
            nullptr,
            &sessions,
            &messages,
            &logs,
            [this](QObject *, std::function<void()> handler) { messageRetentionLimitChanged = std::move(handler); },
            [this](QObject *, std::function<void()> handler) { logRetentionLimitChanged = std::move(handler); },
            [this](QObject *, std::function<void()> handler) { historyPageSizeChanged = std::move(handler); },
            [this](QObject *, std::function<void()> handler) { maxIncomingPayloadBytesChanged = std::move(handler); },
            [this](QObject *, std::function<void()> handler) { deleteHistoryWithSessionChanged = std::move(handler); },
            [this](QObject *, std::function<void()> handler) { saveMessagesWhenOutputPausedChanged = std::move(handler); },
            [this](QObject *, std::function<void()> handler) { autoCollapseConnectionListOnConnectChanged = std::move(handler); },
            [this](QObject *, std::function<void()> handler) { clearMessagesOnExitChanged = std::move(handler); },
            [this](QObject *, std::function<void()> handler) { clearLogsOnExitChanged = std::move(handler); },
            [this](QObject *, std::function<void()> handler) { windowWidthChanged = std::move(handler); },
            [this](QObject *, std::function<void()> handler) { windowHeightChanged = std::move(handler); },
            [this](QObject *, std::function<void()> handler) { windowMaximizedChanged = std::move(handler); },
            [this]() { ++reloadHistoryCalls; },
            [this]() { ++refreshScriptSamplesCalls; },
            [this]() { ++messageStreamChangedCalls; },
            [this]() { ++logStreamChangedCalls; },
        };
    }

    QSettings settings;
    PreferencesController preferencesController;
    QVector<SessionState> sessions;
    EventStreamModel messages;
    EventStreamModel logs;
    std::function<void()> messageRetentionLimitChanged;
    std::function<void()> logRetentionLimitChanged;
    std::function<void()> historyPageSizeChanged;
    std::function<void()> maxIncomingPayloadBytesChanged;
    std::function<void()> deleteHistoryWithSessionChanged;
    std::function<void()> saveMessagesWhenOutputPausedChanged;
    std::function<void()> autoCollapseConnectionListOnConnectChanged;
    std::function<void()> clearMessagesOnExitChanged;
    std::function<void()> clearLogsOnExitChanged;
    std::function<void()> windowWidthChanged;
    std::function<void()> windowHeightChanged;
    std::function<void()> windowMaximizedChanged;
    int reloadHistoryCalls = 0;
    int refreshScriptSamplesCalls = 0;
    int messageStreamChangedCalls = 0;
    int logStreamChangedCalls = 0;
};

class SettingsOptionsViewModelTest : public QObject
{
    Q_OBJECT

private slots:
    void exposesDefaultSettingIndexes();
    void readsSettingsThroughDependencies();
    void messageRetentionChangeDefersCleanup();
    void writesSettingsThroughDependencies();
    void forwardsDependencySignals();
    void themeChangesEmitSignals();
    void themeColorPersistsAndEmitsSignal();
    void languageChangesEmitSignals();
    void messagePayloadDisplayModePersistsAndEmitsSignal();
};

void SettingsOptionsViewModelTest::exposesDefaultSettingIndexes()
{
    FakeSettingsDeps deps;
    SettingsViewModel settings(deps.dependencies(), &deps.settings);

    QCOMPARE(settings.themeModeIndex(), 0);
    QCOMPARE(settings.themeColor(), QStringLiteral("mint"));
    QCOMPARE(settings.languageModeIndex(), 0);
    QCOMPARE(settings.messagePayloadDisplayModeIndex(), 1);
    QCOMPARE(settings.messageRetentionLimitIndex(), 1);
    QCOMPARE(settings.logRetentionLimitIndex(), 1);
    QCOMPARE(settings.historyPageSizeIndex(), 1);
    QCOMPARE(settings.maxIncomingPayloadBytesIndex(), 1);
    QCOMPARE(settings.autoCollapseConnectionListOnConnect(), true);
    QCOMPARE(settings.clearMessagesOnExitIndex(), 0);
    QCOMPARE(settings.clearLogsOnExitIndex(), 0);
    QCOMPARE(settings.subscriptionPaneWidth(), 320);
    QCOMPARE(settings.publishComposerHeight(), 168);
    QCOMPARE(settings.connectionPaneCollapsed(), false);
    QCOMPARE(settings.subscriptionPaneCollapsed(), false);
}

void SettingsOptionsViewModelTest::readsSettingsThroughDependencies()
{
    FakeSettingsDeps deps;
    deps.preferencesController.setMessageRetentionLimit(10000);
    deps.preferencesController.setLogRetentionLimit(5000);
    deps.preferencesController.setWindowGeometry(1200, 700);
    deps.preferencesController.setWindowMaximized(true);

    deps.settings.setValue(QStringLiteral("appearance/themeMode"), QStringLiteral("dark"));
    deps.settings.setValue(QStringLiteral("appearance/themeColor"), QStringLiteral("violet"));
    deps.settings.setValue(QStringLiteral("appearance/languageMode"), QStringLiteral("zh_CN"));
    deps.settings.setValue(QStringLiteral("workbench/messagePayloadDisplayMode"), QStringLiteral("full"));
    SettingsViewModel settings(deps.dependencies(), &deps.settings);

    QCOMPARE(settings.themeModeIndex(), 2);
    QCOMPARE(settings.effectiveTheme(), QStringLiteral("dark"));
    QCOMPARE(settings.themeColor(), QStringLiteral("violet"));
    QCOMPARE(settings.languageModeIndex(), 2);
    QCOMPARE(settings.messagePayloadDisplayModeIndex(), 2);
    QCOMPARE(settings.messageRetentionLimitIndex(), 2);
    QCOMPARE(settings.logRetentionLimitIndex(), 2);
    QCOMPARE(settings.windowWidth(), 1200);
    QCOMPARE(settings.windowHeight(), 700);
    QCOMPARE(settings.windowMaximized(), true);
}

void SettingsOptionsViewModelTest::messageRetentionChangeDefersCleanup()
{
    FakeSettingsDeps deps;
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());

    HistoryStore historyStore(dataDir.path());
    QVERIFY2(historyStore.isReady(), qPrintable(historyStore.lastError()));

    deps.sessions.resize(1);
    SessionState &session = deps.sessions[0];
    session.id = QStringLiteral("session-1");

    for (int index = 0; index < 1001; ++index) {
        QVERIFY(historyStore.enqueueMessage(
                    session.id,
                    QString::number(index),
                    QStringLiteral("topic"),
                    QByteArray::number(index))
                > 0);
    }
    QVERIFY(!historyStore.flushPendingMessages().isEmpty());
    QCOMPARE(historyStore.loadMessages(session.id, 2000).size(), 1001);

    EventHistoryService eventHistoryService;
    EventHistoryService::Dependencies eventHistoryDependencies;
    eventHistoryDependencies.historyStore = &historyStore;
    eventHistoryDependencies.currentSessionState = [&session]() { return &session; };
    eventHistoryService.setDependencies(eventHistoryDependencies);

    QVERIFY(historyStore.enqueueMessage(
                session.id,
                QStringLiteral("pending"),
                QStringLiteral("topic"),
                QByteArrayLiteral("pending"))
            > 0);
    QCOMPARE(historyStore.pendingMessageCount(), 1);

    SettingsViewModel::Dependencies settingsDependencies = deps.dependencies();
    settingsDependencies.eventController = &eventHistoryService;
    settingsDependencies.historyStore = &historyStore;
    SettingsViewModel settings(settingsDependencies, &deps.settings);

    settings.setMessageRetentionLimitIndex(0);

    QCOMPARE(deps.preferencesController.messageRetentionLimit(), 1000);
    QCOMPARE(historyStore.pendingMessageCount(), 1);
    QCOMPARE(historyStore.loadMessages(session.id, 2000).size(), 1001);
    QCOMPARE(deps.reloadHistoryCalls, 0);
    QCOMPARE(deps.messageStreamChangedCalls, 0);
}

void SettingsOptionsViewModelTest::writesSettingsThroughDependencies()
{
    FakeSettingsDeps deps;
    deps.sessions.resize(1);
    deps.sessions[0].runtime.totalMessageCount = 12;
    deps.sessions[0].runtime.viewedMessageCount = 8;
    SettingsViewModel settings(deps.dependencies(), &deps.settings);

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
    settings.setDeleteHistoryWithSession(false);
    settings.setSaveMessagesWhenOutputPaused(false);
    settings.setAutoCollapseConnectionListOnConnect(false);
    settings.setWindowMaximized(true);
    settings.saveWindowGeometry(1600, 900);
    settings.saveWorkbenchLayout(410, 230, true, true);
    settings.clearAllMessages();
    settings.clearAllLogs();
    settings.clearAllHistory();

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
    QCOMPARE(deps.preferencesController.subscriptionPaneCollapsed(), true);
    QCOMPARE(deps.settings.value(QStringLiteral("workspace/subscriptionPaneWidth")).toInt(), 410);
    QCOMPARE(deps.settings.value(QStringLiteral("workspace/publishComposerHeight")).toInt(), 230);
    QCOMPARE(deps.sessions[0].runtime.totalMessageCount, 0);
    QCOMPARE(deps.sessions[0].runtime.viewedMessageCount, 0);
    QCOMPARE(deps.reloadHistoryCalls, 1);
    QCOMPARE(deps.refreshScriptSamplesCalls, 2);
    QCOMPARE(deps.messageStreamChangedCalls, 2);
    QCOMPARE(deps.logStreamChangedCalls, 3);
}

void SettingsOptionsViewModelTest::forwardsDependencySignals()
{
    FakeSettingsDeps deps;
    SettingsViewModel settings(deps.dependencies(), &deps.settings);
    QSignalSpy windowSpy(&settings, &SettingsViewModel::windowMaximizedChanged);
    QSignalSpy autoCollapseSpy(&settings, &SettingsViewModel::autoCollapseConnectionListOnConnectChanged);

    QVERIFY(deps.windowMaximizedChanged);
    QVERIFY(deps.autoCollapseConnectionListOnConnectChanged);

    deps.windowMaximizedChanged();
    deps.autoCollapseConnectionListOnConnectChanged();
    QCOMPARE(windowSpy.count(), 1);
    QCOMPARE(autoCollapseSpy.count(), 1);
}

void SettingsOptionsViewModelTest::themeChangesEmitSignals()
{
    FakeSettingsDeps deps;
    SettingsViewModel settings(deps.dependencies(), &deps.settings);

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
    FakeSettingsDeps deps;
    SettingsViewModel settings(deps.dependencies(), &deps.settings);
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
    FakeSettingsDeps deps;
    SettingsViewModel settings(deps.dependencies(), &deps.settings);
    QSignalSpy modeSpy(&settings, &SettingsViewModel::languageModeChanged);
    QSignalSpy langSpy(&settings, &SettingsViewModel::languageChanged);

    settings.setLanguageModeIndex(2);
    QCOMPARE(modeSpy.count(), 1);
    QCOMPARE(langSpy.count(), 1);
}

void SettingsOptionsViewModelTest::messagePayloadDisplayModePersistsAndEmitsSignal()
{
    FakeSettingsDeps deps;
    SettingsViewModel settings(deps.dependencies(), &deps.settings);
    QSignalSpy modeSpy(&settings, &SettingsViewModel::messagePayloadDisplayModeChanged);

    settings.setMessagePayloadDisplayModeIndex(2);
    settings.setMessagePayloadDisplayModeIndex(2);
    QCOMPARE(settings.messagePayloadDisplayModeIndex(), 2);
    QCOMPARE(modeSpy.count(), 1);
    QCOMPARE(deps.settings.value(QStringLiteral("workbench/messagePayloadDisplayMode")).toString(), QStringLiteral("full"));

    SettingsViewModel restoredSettings(deps.dependencies(), &deps.settings);
    QCOMPARE(restoredSettings.messagePayloadDisplayModeIndex(), 2);
}

QTEST_MAIN(SettingsOptionsViewModelTest)

#include "test_settingsviewmodel.moc"
