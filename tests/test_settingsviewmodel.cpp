#include "viewmodels/settingsoptionsviewmodel.h"
#include "viewmodels/settingsviewmodel.h"

#include <QtTest/QtTest>

class SettingsOptionsViewModelTest : public QObject
{
    Q_OBJECT

private slots:
    void resolvesOptionIndexes();
    void clampsOptionValues();
    void exposesDefaultSettingIndexes();
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

QTEST_MAIN(SettingsOptionsViewModelTest)

#include "test_settingsviewmodel.moc"
