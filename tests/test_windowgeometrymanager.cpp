#include "app/windowgeometrymanager.h"
#include "usecases/preferencescontroller.h"

#include <QtTest/QtTest>

#include <QSettings>
#include <QTemporaryDir>

class WindowGeometryManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void centersAndClampsDefaultGeometry();
    void centersAndClampsSavedSizeIncludingWindowFrame();
    void classifiesWindowStates();
    void windowStatePersistsAndRejectsInvalidSize();
};

void WindowGeometryManagerTest::centersAndClampsDefaultGeometry()
{
    const QRect geometry = WindowGeometryManager::centeredGeometry(
        QSize(1480, 820),
        QRect(0, 23, 1366, 745),
        QSize(1100, 600));

    QCOMPARE(geometry, QRect(0, 23, 1366, 745));
}

void WindowGeometryManagerTest::centersAndClampsSavedSizeIncludingWindowFrame()
{
    const QRect geometry = WindowGeometryManager::centeredGeometry(
        QSize(1600, 1000),
        QRect(0, 20, 1440, 860),
        QSize(1100, 600),
        QMargins(8, 28, 8, 8));

    QCOMPARE(geometry, QRect(8, 48, 1424, 824));
}

void WindowGeometryManagerTest::classifiesWindowStates()
{
    using SaveDisposition = WindowGeometryManager::SaveDisposition;

    QCOMPARE(
        WindowGeometryManager::saveDisposition(QWindow::Windowed, Qt::WindowNoState),
        SaveDisposition::Normal);
    QCOMPARE(
        WindowGeometryManager::saveDisposition(QWindow::Maximized, Qt::WindowMaximized),
        SaveDisposition::Maximized);
    QCOMPARE(
        WindowGeometryManager::saveDisposition(QWindow::FullScreen, Qt::WindowFullScreen),
        SaveDisposition::Preserve);
    QCOMPARE(
        WindowGeometryManager::saveDisposition(QWindow::Minimized, Qt::WindowMinimized),
        SaveDisposition::Preserve);
    QCOMPARE(
        WindowGeometryManager::saveDisposition(QWindow::Hidden, Qt::WindowNoState),
        SaveDisposition::Preserve);
}

void WindowGeometryManagerTest::windowStatePersistsAndRejectsInvalidSize()
{
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());
    const QString settingsPath = dataDir.filePath(QStringLiteral("settings.ini"));

    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        PreferencesController preferences(&settings);
        preferences.setWindowState(QSize(1400, 780), true);
    }

    QSettings restoredSettings(settingsPath, QSettings::IniFormat);
    PreferencesController restoredPreferences(&restoredSettings);
    QCOMPARE(restoredPreferences.windowSize(), QSize(1400, 780));
    QCOMPARE(restoredPreferences.windowMaximized(), true);
    QVERIFY(!restoredSettings.contains(QStringLiteral("window/geometry")));
    QVERIFY(!restoredSettings.contains(QStringLiteral("window/screenIdentifier")));

    restoredPreferences.setWindowState(QSize(0, 780), false);
    QCOMPARE(restoredPreferences.windowSize(), QSize(1400, 780));
    QCOMPARE(restoredPreferences.windowMaximized(), true);
}

QTEST_APPLESS_MAIN(WindowGeometryManagerTest)

#include "test_windowgeometrymanager.moc"
