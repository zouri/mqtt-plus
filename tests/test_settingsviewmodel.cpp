#include "viewmodels/settingsoptionsviewmodel.h"
#include "viewmodels/settingscoreport.h"
#include "viewmodels/settingsviewmodel.h"

#include <QtTest/QtTest>

class FakeSettingsCore final : public SettingsCorePort
{
public:
    void bindSettingsSignals(QObject *, const SettingsCoreSignalHandlers &newHandlers) override
    {
        handlers = newHandlers;
    }

    QString themeMode() const override { return themeModeValue; }
    QString effectiveTheme() const override { return effectiveThemeValue; }
    QString languageMode() const override { return languageModeValue; }
    int messageRetentionLimit() const override { return messageRetentionLimitValue; }
    int logRetentionLimit() const override { return logRetentionLimitValue; }
    int historyPageSize() const override { return historyPageSizeValue; }
    int maxIncomingPayloadBytes() const override { return maxIncomingPayloadBytesValue; }
    bool deleteHistoryWithSession() const override { return deleteHistoryWithSessionValue; }
    bool saveMessagesWhenOutputPaused() const override { return saveMessagesWhenOutputPausedValue; }
    QString clearMessagesOnExit() const override { return clearMessagesOnExitValue; }
    QString clearLogsOnExit() const override { return clearLogsOnExitValue; }
    int windowWidth() const override { return windowWidthValue; }
    int windowHeight() const override { return windowHeightValue; }
    bool windowMaximized() const override { return windowMaximizedValue; }

    void setThemeMode(const QString &mode) override { themeModeValue = mode; }
    void setLanguageMode(const QString &mode) override { languageModeValue = mode; }
    void setMessageRetentionLimit(int limit) override { messageRetentionLimitValue = limit; }
    void setLogRetentionLimit(int limit) override { logRetentionLimitValue = limit; }
    void setHistoryPageSize(int pageSize) override { historyPageSizeValue = pageSize; }
    void setMaxIncomingPayloadBytes(int bytes) override { maxIncomingPayloadBytesValue = bytes; }
    void setDeleteHistoryWithSession(bool enabled) override { deleteHistoryWithSessionValue = enabled; }
    void setSaveMessagesWhenOutputPaused(bool enabled) override { saveMessagesWhenOutputPausedValue = enabled; }
    void setClearMessagesOnExit(const QString &mode) override { clearMessagesOnExitValue = mode; }
    void setClearLogsOnExit(const QString &mode) override { clearLogsOnExitValue = mode; }
    void setWindowMaximized(bool maximized) override { windowMaximizedValue = maximized; }
    void saveWindowGeometry(int width, int height) override
    {
        windowWidthValue = width;
        windowHeightValue = height;
    }
    void clearAllMessages() override { ++clearMessagesCalls; }
    void clearAllLogs() override { ++clearLogsCalls; }
    void clearAllHistory() override { ++clearHistoryCalls; }

    SettingsCoreSignalHandlers handlers;
    QString themeModeValue = QStringLiteral("system");
    QString effectiveThemeValue = QStringLiteral("light");
    QString languageModeValue = QStringLiteral("system");
    int messageRetentionLimitValue = 5000;
    int logRetentionLimitValue = 2000;
    int historyPageSizeValue = 500;
    int maxIncomingPayloadBytesValue = 1024 * 1024;
    bool deleteHistoryWithSessionValue = true;
    bool saveMessagesWhenOutputPausedValue = true;
    QString clearMessagesOnExitValue = QStringLiteral("never");
    QString clearLogsOnExitValue = QStringLiteral("never");
    int windowWidthValue = 1480;
    int windowHeightValue = 820;
    bool windowMaximizedValue = false;
    int clearMessagesCalls = 0;
    int clearLogsCalls = 0;
    int clearHistoryCalls = 0;
};

class SettingsOptionsViewModelTest : public QObject
{
    Q_OBJECT

private slots:
    void resolvesOptionIndexes();
    void clampsOptionValues();
    void exposesDefaultSettingIndexes();
    void readsSettingsThroughCorePort();
    void writesSettingsThroughCorePort();
    void forwardsCorePortSignals();
};

void SettingsOptionsViewModelTest::resolvesOptionIndexes()
{
    SettingsOptionsViewModel options;

    QCOMPARE(options.optionIndex(QVariantList {QStringLiteral("system"), QStringLiteral("light")}, QStringLiteral("light")), 1);
    QCOMPARE(options.optionIndex(QVariantList {1000, 5000}, 42), 0);
}

void SettingsOptionsViewModelTest::clampsOptionValues()
{
    SettingsOptionsViewModel options;
    const QVariantList values {QStringLiteral("never"), QStringLiteral("current"), QStringLiteral("all")};

    QCOMPARE(options.optionValue(values, -1).toString(), QStringLiteral("never"));
    QCOMPARE(options.optionValue(values, 1).toString(), QStringLiteral("current"));
    QCOMPARE(options.optionValue(values, 99).toString(), QStringLiteral("all"));
}

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

void SettingsOptionsViewModelTest::readsSettingsThroughCorePort()
{
    FakeSettingsCore core;
    core.themeModeValue = QStringLiteral("dark");
    core.effectiveThemeValue = QStringLiteral("dark");
    core.languageModeValue = QStringLiteral("zh_CN");
    core.messageRetentionLimitValue = 10000;
    core.logRetentionLimitValue = 5000;
    core.windowWidthValue = 1200;
    core.windowHeightValue = 700;
    core.windowMaximizedValue = true;
    SettingsViewModel settings(&core);

    QCOMPARE(settings.themeModeIndex(), 2);
    QCOMPARE(settings.effectiveTheme(), QStringLiteral("dark"));
    QCOMPARE(settings.languageModeIndex(), 2);
    QCOMPARE(settings.messageRetentionLimitIndex(), 2);
    QCOMPARE(settings.logRetentionLimitIndex(), 2);
    QCOMPARE(settings.windowWidth(), 1200);
    QCOMPARE(settings.windowHeight(), 700);
    QCOMPARE(settings.windowMaximized(), true);
}

void SettingsOptionsViewModelTest::writesSettingsThroughCorePort()
{
    FakeSettingsCore core;
    SettingsViewModel settings(&core);

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

    QCOMPARE(core.themeModeValue, QStringLiteral("light"));
    QCOMPARE(core.languageModeValue, QStringLiteral("en"));
    QCOMPARE(core.messageRetentionLimitValue, 1000);
    QCOMPARE(core.logRetentionLimitValue, 500);
    QCOMPARE(core.historyPageSizeValue, 1000);
    QCOMPARE(core.maxIncomingPayloadBytesValue, 5242880);
    QCOMPARE(core.clearMessagesOnExitValue, QStringLiteral("all"));
    QCOMPARE(core.clearLogsOnExitValue, QStringLiteral("current"));
    QCOMPARE(core.deleteHistoryWithSessionValue, false);
    QCOMPARE(core.saveMessagesWhenOutputPausedValue, false);
    QCOMPARE(core.windowMaximizedValue, true);
    QCOMPARE(core.windowWidthValue, 1600);
    QCOMPARE(core.windowHeightValue, 900);
    QCOMPARE(core.clearMessagesCalls, 1);
    QCOMPARE(core.clearLogsCalls, 1);
    QCOMPARE(core.clearHistoryCalls, 1);
}

void SettingsOptionsViewModelTest::forwardsCorePortSignals()
{
    FakeSettingsCore core;
    SettingsViewModel settings(&core);
    QSignalSpy themeSpy(&settings, &SettingsViewModel::themeModeChanged);
    QSignalSpy languageSpy(&settings, &SettingsViewModel::languageChanged);
    QSignalSpy windowSpy(&settings, &SettingsViewModel::windowMaximizedChanged);

    QVERIFY(core.handlers.themeModeChanged);
    QVERIFY(core.handlers.languageChanged);
    QVERIFY(core.handlers.windowMaximizedChanged);

    core.handlers.themeModeChanged();
    core.handlers.languageChanged();
    core.handlers.windowMaximizedChanged();

    QCOMPARE(themeSpy.count(), 1);
    QCOMPARE(languageSpy.count(), 1);
    QCOMPARE(windowSpy.count(), 1);
}

QTEST_MAIN(SettingsOptionsViewModelTest)

#include "test_settingsviewmodel.moc"
