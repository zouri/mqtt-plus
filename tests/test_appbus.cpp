#include "app/applicationkernel.h"
#include "models/sessionlistmodel.h"

#include <QDir>
#include <QGuiApplication>
#include <QMetaProperty>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

class AppBusTest : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QSettings settings(QStringLiteral("mqtt-plus"), QStringLiteral("mqtt-plus"));
        settings.clear();
        settings.sync();
    }

    void childBusesExposeDomainModels()
    {
        ApplicationKernel kernel;
        auto *bus = kernel.bus();

        QVERIFY(bus->workbench());
        QVERIFY(bus->settings());
        QVERIFY(bus->scripts());
        QVERIFY(bus->logs());

        QCOMPARE(QString::fromLatin1(bus->workbench()->metaObject()->className()), QStringLiteral("WorkbenchBus"));
        QCOMPARE(QString::fromLatin1(bus->settings()->metaObject()->className()), QStringLiteral("SettingsBus"));
        QCOMPARE(QString::fromLatin1(bus->scripts()->metaObject()->className()), QStringLiteral("ScriptsBus"));
        QCOMPARE(QString::fromLatin1(bus->logs()->metaObject()->className()), QStringLiteral("LogsBus"));

        QVERIFY(bus->workbench()->sessions());
        QVERIFY(bus->workbench()->filteredSubscriptions());
        QVERIFY(bus->workbench()->messages());
        QVERIFY(bus->scripts()->scripts());
        QVERIFY(bus->scripts()->scriptTestSamples());
        QVERIFY(bus->logs()->logs());
    }

    void rootBusOnlyExposesDomainEntrypoints()
    {
        ApplicationKernel kernel;
        const QMetaObject *metaObject = kernel.bus()->metaObject();

        QCOMPARE(metaObject->propertyCount() - metaObject->propertyOffset(), 4);

        const QMetaProperty workbenchProperty = metaObject->property(metaObject->indexOfProperty("workbench"));
        const QMetaProperty settingsProperty = metaObject->property(metaObject->indexOfProperty("settings"));
        const QMetaProperty scriptsProperty = metaObject->property(metaObject->indexOfProperty("scripts"));
        const QMetaProperty logsProperty = metaObject->property(metaObject->indexOfProperty("logs"));

        QVERIFY(workbenchProperty.isValid());
        QVERIFY(settingsProperty.isValid());
        QVERIFY(scriptsProperty.isValid());
        QVERIFY(logsProperty.isValid());
        QCOMPARE(QString::fromLatin1(workbenchProperty.typeName()), QStringLiteral("WorkbenchBus*"));
        QCOMPARE(QString::fromLatin1(settingsProperty.typeName()), QStringLiteral("SettingsBus*"));
        QCOMPARE(QString::fromLatin1(scriptsProperty.typeName()), QStringLiteral("ScriptsBus*"));
        QCOMPARE(QString::fromLatin1(logsProperty.typeName()), QStringLiteral("LogsBus*"));

        QCOMPARE(metaObject->indexOfProperty("scriptLibrary"), -1);
        QCOMPARE(metaObject->indexOfProperty("logStream"), -1);
        QCOMPARE(metaObject->indexOfProperty("sessions"), -1);
        QCOMPARE(metaObject->indexOfProperty("themeMode"), -1);
        QCOMPARE(metaObject->indexOfProperty("currentSessionIndex"), -1);
        QCOMPARE(metaObject->indexOfProperty("windowMaximized"), -1);

        QCOMPARE(metaObject->indexOfMethod(QMetaObject::normalizedSignature("connectCurrentSession()")), -1);
        QCOMPARE(metaObject->indexOfMethod(QMetaObject::normalizedSignature("upsertScript(QString,QString,QString,QString)")), -1);
        QCOMPARE(metaObject->indexOfMethod(QMetaObject::normalizedSignature("clearAllHistory()")), -1);
    }

    void domainBusesExposeDomainSignals()
    {
        ApplicationKernel kernel;
        auto *bus = kernel.bus();
        QVERIFY(bus);
        QVERIFY(bus->scripts());
        QVERIFY(bus->logs());

        const QMetaObject *rootMetaObject = bus->metaObject();
        const QMetaObject *scriptsMetaObject = bus->scripts()->metaObject();
        const QMetaObject *logsMetaObject = bus->logs()->metaObject();

        QVERIFY(rootMetaObject->indexOfSignal(QMetaObject::normalizedSignature("scriptsChanged()")) >= 0);
        QVERIFY(rootMetaObject->indexOfSignal(QMetaObject::normalizedSignature("logsChanged()")) >= 0);
        QVERIFY(rootMetaObject->indexOfSignal(QMetaObject::normalizedSignature("logsRowAppended(QVariantMap)")) >= 0);

        QVERIFY(scriptsMetaObject->indexOfSignal(QMetaObject::normalizedSignature("scriptsChanged()")) >= 0);
        QVERIFY(logsMetaObject->indexOfSignal(QMetaObject::normalizedSignature("logsChanged()")) >= 0);
        QVERIFY(logsMetaObject->indexOfSignal(QMetaObject::normalizedSignature("logsRowAppended(QVariantMap)")) >= 0);
    }

    void settingsBusChangesPublishOnceThroughRootBus()
    {
        ApplicationKernel kernel;
        auto *bus = kernel.bus();
        auto *settings = bus->settings();
        QVERIFY(settings);

        QSignalSpy rootSpy(bus, &AppEventBus::windowMaximizedChanged);
        QSignalSpy settingsChangedSpy(bus, &AppEventBus::settingsChanged);
        QSignalSpy settingsSpy(settings, &SettingsBus::windowMaximizedChanged);

        const bool maximized = !settings->windowMaximized();
        settings->setWindowMaximized(maximized);

        QCOMPARE(settings->windowMaximized(), maximized);
        QCOMPARE(rootSpy.count(), 1);
        QCOMPARE(settingsChangedSpy.count(), 1);
        QCOMPARE(settingsSpy.count(), 1);
    }

    void sessionCollectionRefreshUpdatesCurrentSessionModels()
    {
        ApplicationKernel kernel;
        auto *bus = kernel.bus();
        auto *workbench = bus->workbench();
        QVERIFY(workbench);
        QVERIFY(workbench->sessions());

        const int initialCount = workbench->sessions()->count();
        QVariantMap config = workbench->defaultSessionConfig();
        config.insert(QStringLiteral("name"), QStringLiteral("Coordinator Test"));
        workbench->addSessionWithConfig(config);

        QCOMPARE(workbench->sessions()->count(), initialCount + 1);
        QCOMPARE(workbench->currentSessionIndex(), initialCount);
        QCOMPARE(workbench->sessions()->rowAt(initialCount).value(QStringLiteral("name")).toString(),
                 QStringLiteral("Coordinator Test"));
        QCOMPARE(workbench->currentSession().value(QStringLiteral("name")).toString(),
                 QStringLiteral("Coordinator Test"));
    }

    void sessionLifecycleCreatesDefaultAndPersistsAddDelete()
    {
        ApplicationKernel kernel;
        auto *workbench = kernel.bus()->workbench();
        QVERIFY(workbench);
        QVERIFY(workbench->sessions());

        const int initialCount = workbench->sessions()->count();
        QVERIFY(initialCount >= 1);
        QCOMPARE(workbench->currentSessionIndex(), 0);
        QVERIFY(!workbench->currentSession().value(QStringLiteral("id")).toString().isEmpty());

        QVariantMap config = workbench->defaultSessionConfig();
        config.insert(QStringLiteral("name"), QStringLiteral("Lifecycle Saved"));
        workbench->addSessionWithConfig(config);

        QCOMPARE(workbench->sessions()->count(), initialCount + 1);
        QCOMPARE(workbench->currentSessionIndex(), initialCount);
        QSettings addedSettings(QStringLiteral("mqtt-plus"), QStringLiteral("mqtt-plus"));
        QCOMPARE(addedSettings.beginReadArray(QStringLiteral("sessions")), initialCount + 1);
        addedSettings.endArray();

        workbench->removeSessionAt(initialCount);

        QCOMPARE(workbench->sessions()->count(), initialCount);
        QCOMPARE(workbench->currentSessionIndex(), initialCount - 1);
        QSettings removedSettings(QStringLiteral("mqtt-plus"), QStringLiteral("mqtt-plus"));
        QCOMPARE(removedSettings.beginReadArray(QStringLiteral("sessions")), initialCount);
        removedSettings.endArray();
    }
};

int main(int argc, char **argv)
{
    const QString testHome = QDir::temp().filePath(
        QStringLiteral("mqtt_plus_appbus_test_%1").arg(QCoreApplication::applicationPid()));
    QDir().mkpath(testHome);
    qputenv("HOME", QFile::encodeName(testHome));
    qputenv("XDG_CONFIG_HOME", QFile::encodeName(QDir(testHome).filePath(QStringLiteral(".config"))));
    qputenv("XDG_DATA_HOME", QFile::encodeName(QDir(testHome).filePath(QStringLiteral(".local/share"))));

    QGuiApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);

    AppBusTest test;
    const int result = QTest::qExec(&test, argc, argv);
    QDir(testHome).removeRecursively();
    return result;
}

#include "test_appbus.moc"
