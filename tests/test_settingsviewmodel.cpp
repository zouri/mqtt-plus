#include "controllers/languagecontroller.h"
#include "controllers/preferencescontroller.h"
#include "controllers/themecontroller.h"
#include "models/eventstreammodel.h"
#include "viewmodels/settingsviewmodel.h"

#include <QtTest/QtTest>

#include <utility>

class FakeSettingsDeps
{
public:
    FakeSettingsDeps()
        : settings(QStringLiteral("mqtt-plus-test"), QStringLiteral("settings-viewmodel-test"))
        , themeController(&settings)
        , languageController(&settings)
        , preferencesController(&settings)
    {
        settings.clear();
    }

    SettingsViewModel::Dependencies dependencies()
    {
        return {
            &themeController,
            &languageController,
            &preferencesController,
            nullptr,
            nullptr,
            &sessions,
            &messages,
            &logs,
            [this](QObject *, std::function<void()> handler) { themeModeChanged = std::move(handler); },
            [this](QObject *, std::function<void()> handler) { effectiveThemeChanged = std::move(handler); },
            [this](QObject *, std::function<void()> handler) { languageModeChanged = std::move(handler); },
            [this](QObject *, std::function<void()> handler) { languageChanged = std::move(handler); },
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
    ThemeController themeController;
    LanguageController languageController;
    PreferencesController preferencesController;
    QVector<SessionState> sessions;
    EventStreamModel messages;
    EventStreamModel logs;
    std::function<void()> themeModeChanged;
    std::function<void()> effectiveThemeChanged;
    std::function<void()> languageModeChanged;
    std::function<void()> languageChanged;
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
};

void SettingsOptionsViewModelTest::exposesDefaultSettingIndexes()
{
    SettingsViewModel settings;

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
    deps.themeController.setMode(QStringLiteral("dark"));
    deps.languageController.setMode(QStringLiteral("zh_CN"));
    deps.preferencesController.setMessageRetentionLimit(10000);
    deps.preferencesController.setLogRetentionLimit(5000);
    deps.preferencesController.setWindowGeometry(1200, 700);
    deps.preferencesController.setWindowMaximized(true);
    SettingsViewModel settings(deps.dependencies());

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
    SettingsViewModel settings(deps.dependencies());

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

    QCOMPARE(deps.themeController.mode(), QStringLiteral("light"));
    QCOMPARE(deps.languageController.mode(), QStringLiteral("en"));
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
    SettingsViewModel settings(deps.dependencies());
    QSignalSpy themeSpy(&settings, &SettingsViewModel::themeModeChanged);
    QSignalSpy languageSpy(&settings, &SettingsViewModel::languageChanged);
    QSignalSpy windowSpy(&settings, &SettingsViewModel::windowMaximizedChanged);

    QVERIFY(deps.themeModeChanged);
    QVERIFY(deps.languageChanged);
    QVERIFY(deps.windowMaximizedChanged);

    deps.themeModeChanged();
    deps.languageChanged();
    deps.windowMaximizedChanged();

    QCOMPARE(themeSpy.count(), 1);
    QCOMPARE(languageSpy.count(), 1);
    QCOMPARE(windowSpy.count(), 1);
}

QTEST_MAIN(SettingsOptionsViewModelTest)

#include "test_settingsviewmodel.moc"
