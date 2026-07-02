#include "controllers/preferencescontroller.h"
#include "models/eventstreammodel.h"
#include "viewmodels/settingsviewmodel.h"

#include <QtTest/QtTest>

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
    void writesSettingsThroughDependencies();
    void forwardsDependencySignals();
    void themeChangesEmitSignals();
    void languageChangesEmitSignals();
};

void SettingsOptionsViewModelTest::exposesDefaultSettingIndexes()
{
    FakeSettingsDeps deps;
    SettingsViewModel settings(deps.dependencies(), &deps.settings);

    QCOMPARE(settings.themeModeIndex(), 0);
    QCOMPARE(settings.languageModeIndex(), 0);
    QCOMPARE(settings.messageRetentionLimitIndex(), 1);
    QCOMPARE(settings.logRetentionLimitIndex(), 1);
    QCOMPARE(settings.historyPageSizeIndex(), 1);
    QCOMPARE(settings.maxIncomingPayloadBytesIndex(), 1);
    QCOMPARE(settings.clearMessagesOnExitIndex(), 0);
    QCOMPARE(settings.clearLogsOnExitIndex(), 0);
}

void SettingsOptionsViewModelTest::readsSettingsThroughDependencies()
{
    FakeSettingsDeps deps;
    deps.preferencesController.setMessageRetentionLimit(10000);
    deps.preferencesController.setLogRetentionLimit(5000);
    deps.preferencesController.setWindowGeometry(1200, 700);
    deps.preferencesController.setWindowMaximized(true);

    deps.settings.setValue(QStringLiteral("appearance/themeMode"), QStringLiteral("dark"));
    deps.settings.setValue(QStringLiteral("appearance/languageMode"), QStringLiteral("zh_CN"));
    SettingsViewModel settings(deps.dependencies(), &deps.settings);

    QCOMPARE(settings.themeModeIndex(), 2);
    QCOMPARE(settings.effectiveTheme(), QStringLiteral("dark"));
    QCOMPARE(settings.languageModeIndex(), 2);
    QCOMPARE(settings.messageRetentionLimitIndex(), 2);
    QCOMPARE(settings.logRetentionLimitIndex(), 2);
    QCOMPARE(settings.windowWidth(), 1200);
    QCOMPARE(settings.windowHeight(), 700);
    QCOMPARE(settings.windowMaximized(), true);
}

void SettingsOptionsViewModelTest::writesSettingsThroughDependencies()
{
    FakeSettingsDeps deps;
    SettingsViewModel settings(deps.dependencies(), &deps.settings);

    settings.setThemeModeIndex(1);
    settings.setLanguageModeIndex(1);
    settings.setMessageRetentionLimitIndex(0);
    settings.setLogRetentionLimitIndex(0);
    settings.setHistoryPageSizeIndex(2);
    settings.setMaxIncomingPayloadBytesIndex(2);
    settings.setClearMessagesOnExitIndex(2);
    settings.setClearLogsOnExitIndex(1);
    settings.setDeleteHistoryWithSession(false);
    settings.setSaveMessagesWhenOutputPaused(false);
    settings.setWindowMaximized(true);
    settings.saveWindowGeometry(1600, 900);
    settings.clearAllMessages();
    settings.clearAllLogs();
    settings.clearAllHistory();

    QCOMPARE(settings.themeMode(), QStringLiteral("light"));
    QCOMPARE(settings.languageMode(), QStringLiteral("en"));
    QCOMPARE(deps.preferencesController.messageRetentionLimit(), 1000);
    QCOMPARE(deps.preferencesController.logRetentionLimit(), 500);
    QCOMPARE(deps.preferencesController.historyPageSize(), 1000);
    QCOMPARE(deps.preferencesController.maxIncomingPayloadBytes(), 5242880);
    QCOMPARE(deps.preferencesController.clearMessagesOnExit(), QStringLiteral("all"));
    QCOMPARE(deps.preferencesController.clearLogsOnExit(), QStringLiteral("current"));
    QCOMPARE(deps.preferencesController.deleteHistoryWithSession(), false);
    QCOMPARE(deps.preferencesController.saveMessagesWhenOutputPaused(), false);
    QCOMPARE(deps.preferencesController.windowMaximized(), true);
    QCOMPARE(deps.preferencesController.windowWidth(), 1600);
    QCOMPARE(deps.preferencesController.windowHeight(), 900);
    QCOMPARE(deps.reloadHistoryCalls, 2);
    QCOMPARE(deps.refreshScriptSamplesCalls, 2);
    QCOMPARE(deps.messageStreamChangedCalls, 3);
    QCOMPARE(deps.logStreamChangedCalls, 3);
}

void SettingsOptionsViewModelTest::forwardsDependencySignals()
{
    FakeSettingsDeps deps;
    SettingsViewModel settings(deps.dependencies(), &deps.settings);
    QSignalSpy windowSpy(&settings, &SettingsViewModel::windowMaximizedChanged);

    QVERIFY(deps.windowMaximizedChanged);

    deps.windowMaximizedChanged();
    QCOMPARE(windowSpy.count(), 1);
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

QTEST_MAIN(SettingsOptionsViewModelTest)

#include "test_settingsviewmodel.moc"
