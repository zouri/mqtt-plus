#include "app/appfacade.h"

#include <QDir>
#include <QGuiApplication>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

class AppFacadeFacadesTest : public QObject
{
    Q_OBJECT

private slots:
    void childFacadesExposeDomainModels()
    {
        AppFacade facade;

        QVERIFY(facade.workbench());
        QVERIFY(facade.settings());
        QVERIFY(facade.scriptLibrary());
        QVERIFY(facade.logStream());

        QCOMPARE(facade.workbench()->parent(), &facade);
        QCOMPARE(facade.settings()->parent(), &facade);
        QCOMPARE(facade.scriptLibrary()->parent(), &facade);
        QCOMPARE(facade.logStream()->parent(), &facade);

        QVERIFY(facade.workbench()->sessions());
        QVERIFY(facade.workbench()->filteredSubscriptions());
        QVERIFY(facade.workbench()->messages());
        QVERIFY(facade.scriptLibrary()->scripts());
        QVERIFY(facade.scriptLibrary()->scriptTestSamples());
        QVERIFY(facade.logStream()->logs());
    }

    void rootFacadeOnlyExposesDomainEntrypoints()
    {
        AppFacade facade;
        const QMetaObject *metaObject = facade.metaObject();

        QVERIFY(metaObject->indexOfProperty("workbench") >= 0);
        QVERIFY(metaObject->indexOfProperty("settings") >= 0);
        QVERIFY(metaObject->indexOfProperty("scriptLibrary") >= 0);
        QVERIFY(metaObject->indexOfProperty("logStream") >= 0);

        QCOMPARE(metaObject->indexOfProperty("sessions"), -1);
        QCOMPARE(metaObject->indexOfProperty("scripts"), -1);
        QCOMPARE(metaObject->indexOfProperty("themeMode"), -1);
        QCOMPARE(metaObject->indexOfProperty("currentSessionIndex"), -1);
        QCOMPARE(metaObject->indexOfProperty("windowMaximized"), -1);

        QCOMPARE(metaObject->indexOfMethod(QMetaObject::normalizedSignature("connectCurrentSession()")), -1);
        QCOMPARE(metaObject->indexOfMethod(QMetaObject::normalizedSignature("upsertScript(QString,QString,QString,QString)")), -1);
        QCOMPARE(metaObject->indexOfMethod(QMetaObject::normalizedSignature("clearAllHistory()")), -1);
    }

    void settingsFacadeForwardsPropertyChanges()
    {
        AppFacade facade;
        auto *settings = facade.settings();
        QVERIFY(settings);

        QSignalSpy rootSpy(&facade, &AppFacade::windowMaximizedChanged);
        QSignalSpy settingsSpy(settings, &AppSettingsFacade::windowMaximizedChanged);

        const bool maximized = !settings->windowMaximized();
        settings->setWindowMaximized(maximized);

        QCOMPARE(settings->windowMaximized(), maximized);
        QCOMPARE(rootSpy.count(), 1);
        QCOMPARE(settingsSpy.count(), 1);
    }
};

int main(int argc, char **argv)
{
    const QString testHome = QDir::temp().filePath(
        QStringLiteral("mqtt_plus_appfacade_test_%1").arg(QCoreApplication::applicationPid()));
    QDir().mkpath(testHome);
    qputenv("HOME", QFile::encodeName(testHome));
    qputenv("XDG_CONFIG_HOME", QFile::encodeName(QDir(testHome).filePath(QStringLiteral(".config"))));
    qputenv("XDG_DATA_HOME", QFile::encodeName(QDir(testHome).filePath(QStringLiteral(".local/share"))));

    QGuiApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);

    AppFacadeFacadesTest test;
    const int result = QTest::qExec(&test, argc, argv);
    QDir(testHome).removeRecursively();
    return result;
}

#include "test_appfacadefacades.moc"
