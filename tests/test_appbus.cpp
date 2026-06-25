#include "app/applicationkernel.h"
#include "app/appbus.h"
#include "app/appruntimestate.h"
#include "models/eventstreammodel.h"
#include "app/uieventhub.h"
#include "models/sessionlistmodel.h"
#include "models/subscriptionfiltermodel.h"

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
        auto *appBus = kernel.appBus();

        QVERIFY(appBus);
        QVERIFY(appBus->sessions());
        QVERIFY(appBus->subscriptions());
        QVERIFY(appBus->connection());
        QVERIFY(appBus->messages());
        QVERIFY(appBus->settings());
        QVERIFY(appBus->scripts());
        QVERIFY(appBus->logs());

        QCOMPARE(QString::fromLatin1(appBus->settings()->metaObject()->className()), QStringLiteral("SettingsBus"));
        QCOMPARE(QString::fromLatin1(appBus->scripts()->metaObject()->className()), QStringLiteral("ScriptsBus"));
        QCOMPARE(QString::fromLatin1(appBus->logs()->metaObject()->className()), QStringLiteral("LogsBus"));

        QVERIFY(appBus->sessions()->sessions());
        QVERIFY(appBus->subscriptions()->filteredSubscriptions());
        QVERIFY(appBus->messages()->messages());
        QVERIFY(appBus->scripts()->scripts());
        QVERIFY(appBus->scripts()->scriptTestSamples());
        QVERIFY(appBus->logs()->logs());
    }

    void kernelExposesUiEventHubInsteadOfLegacyEventBus()
    {
        ApplicationKernel kernel;
        auto *events = kernel.events();
        QVERIFY(events);
        QCOMPARE(QString::fromLatin1(events->metaObject()->className()), QStringLiteral("UiEventHub"));

        const QMetaObject *metaObject = kernel.appBus()->metaObject();
        QCOMPARE(metaObject->indexOfProperty("workbench"), -1);
        QCOMPARE(metaObject->indexOfProperty("appBus"), -1);
        QCOMPARE(metaObject->indexOfProperty("themeMode"), -1);
        QCOMPARE(metaObject->indexOfProperty("currentSessionIndex"), -1);

        QCOMPARE(metaObject->indexOfMethod(QMetaObject::normalizedSignature("connectCurrentSession()")), -1);
        QCOMPARE(metaObject->indexOfMethod(QMetaObject::normalizedSignature("upsertScript(QString,QString,QString,QString)")), -1);
        QCOMPARE(metaObject->indexOfMethod(QMetaObject::normalizedSignature("clearAllHistory()")), -1);
    }

    void appRootBusExposesDomainEntrypoints()
    {
        ApplicationKernel kernel;
        const QMetaObject *metaObject = kernel.appBus()->metaObject();

        QCOMPARE(metaObject->propertyCount() - metaObject->propertyOffset(), 7);

        const QMetaProperty sessionsProperty = metaObject->property(metaObject->indexOfProperty("sessions"));
        const QMetaProperty subscriptionsProperty = metaObject->property(metaObject->indexOfProperty("subscriptions"));
        const QMetaProperty connectionProperty = metaObject->property(metaObject->indexOfProperty("connection"));
        const QMetaProperty messagesProperty = metaObject->property(metaObject->indexOfProperty("messages"));
        const QMetaProperty settingsProperty = metaObject->property(metaObject->indexOfProperty("settings"));
        const QMetaProperty scriptsProperty = metaObject->property(metaObject->indexOfProperty("scripts"));
        const QMetaProperty logsProperty = metaObject->property(metaObject->indexOfProperty("logs"));

        QVERIFY(sessionsProperty.isValid());
        QVERIFY(subscriptionsProperty.isValid());
        QVERIFY(connectionProperty.isValid());
        QVERIFY(messagesProperty.isValid());
        QVERIFY(settingsProperty.isValid());
        QVERIFY(scriptsProperty.isValid());
        QVERIFY(logsProperty.isValid());
        QCOMPARE(QString::fromLatin1(sessionsProperty.typeName()), QStringLiteral("SessionsBus*"));
        QCOMPARE(QString::fromLatin1(subscriptionsProperty.typeName()), QStringLiteral("SubscriptionsBus*"));
        QCOMPARE(QString::fromLatin1(connectionProperty.typeName()), QStringLiteral("ConnectionBus*"));
        QCOMPARE(QString::fromLatin1(messagesProperty.typeName()), QStringLiteral("MessagesBus*"));
        QCOMPARE(QString::fromLatin1(settingsProperty.typeName()), QStringLiteral("SettingsBus*"));
        QCOMPARE(QString::fromLatin1(scriptsProperty.typeName()), QStringLiteral("ScriptsBus*"));
        QCOMPARE(QString::fromLatin1(logsProperty.typeName()), QStringLiteral("LogsBus*"));

        QCOMPARE(metaObject->indexOfProperty("workbench"), -1);
    }

    void runtimeStateDoesNotExposeSurfaceBusProperties()
    {
        ApplicationKernel kernel;
        const QMetaObject *metaObject = kernel.runtime()->state()->metaObject();

        QCOMPARE(metaObject->indexOfProperty("settings"), -1);
        QCOMPARE(metaObject->indexOfProperty("scripts"), -1);
        QCOMPARE(metaObject->indexOfProperty("logs"), -1);
        QCOMPARE(metaObject->indexOfSignal(QMetaObject::normalizedSignature("sessionsChanged()")), -1);
        QCOMPARE(metaObject->indexOfSignal(QMetaObject::normalizedSignature("subscriptionsChanged()")), -1);
        QCOMPARE(metaObject->indexOfSignal(QMetaObject::normalizedSignature("scriptsChanged()")), -1);
        QCOMPARE(metaObject->indexOfSignal(QMetaObject::normalizedSignature("logsChanged()")), -1);

        QVERIFY(kernel.appBus()->settings());
        QVERIFY(kernel.appBus()->scripts());
        QVERIFY(kernel.appBus()->logs());
    }

    void newDomainBusesExposeSplitWorkspaceApi()
    {
        ApplicationKernel kernel;
        auto *appBus = kernel.appBus();
        QVERIFY(appBus);
        QVERIFY(appBus->sessions());
        QVERIFY(appBus->subscriptions());
        QVERIFY(appBus->connection());
        QVERIFY(appBus->messages());

        const QMetaObject *sessionsMetaObject = appBus->sessions()->metaObject();
        const QMetaObject *subscriptionsMetaObject = appBus->subscriptions()->metaObject();
        const QMetaObject *connectionMetaObject = appBus->connection()->metaObject();
        const QMetaObject *messagesMetaObject = appBus->messages()->metaObject();

        QVERIFY(sessionsMetaObject->indexOfProperty("sessions") >= 0);
        QVERIFY(sessionsMetaObject->indexOfProperty("currentSessionIndex") >= 0);
        QVERIFY(sessionsMetaObject->indexOfProperty("currentSession") >= 0);
        QVERIFY(sessionsMetaObject->indexOfMethod(QMetaObject::normalizedSignature("defaultSessionConfig()")) >= 0);
        QVERIFY(sessionsMetaObject->indexOfMethod(QMetaObject::normalizedSignature("sessionConfigAt(int)")) >= 0);
        QVERIFY(sessionsMetaObject->indexOfMethod(QMetaObject::normalizedSignature("updateSessionConfigAt(int,QVariantMap)")) >= 0);
        QVERIFY(sessionsMetaObject->indexOfMethod(QMetaObject::normalizedSignature("addSessionWithConfig(QVariantMap)")) >= 0);
        QVERIFY(sessionsMetaObject->indexOfMethod(QMetaObject::normalizedSignature("duplicateSessionAt(int)")) >= 0);
        QVERIFY(sessionsMetaObject->indexOfMethod(QMetaObject::normalizedSignature("removeSessionAt(int)")) >= 0);
        QVERIFY(sessionsMetaObject->indexOfMethod(QMetaObject::normalizedSignature("showSessionContextMenu(int,QPointF)")) >= 0);
        QVERIFY(sessionsMetaObject->indexOfSignal(QMetaObject::normalizedSignature("sessionsChanged()")) >= 0);
        QVERIFY(sessionsMetaObject->indexOfSignal(QMetaObject::normalizedSignature("currentSessionIndexChanged()")) >= 0);
        QVERIFY(sessionsMetaObject->indexOfSignal(QMetaObject::normalizedSignature("currentSessionChanged()")) >= 0);

        QVERIFY(subscriptionsMetaObject->indexOfProperty("filteredSubscriptions") >= 0);
        QVERIFY(subscriptionsMetaObject->indexOfProperty("payloadFormats") >= 0);
        QVERIFY(subscriptionsMetaObject->indexOfMethod(
                    QMetaObject::normalizedSignature("upsertCurrentSubscription(QString,int,int,QString,QString)"))
                >= 0);
        QVERIFY(subscriptionsMetaObject->indexOfMethod(
                    QMetaObject::normalizedSignature("updateCurrentSubscription(QString,QString,QString,QString)"))
                >= 0);
        QVERIFY(subscriptionsMetaObject->indexOfMethod(QMetaObject::normalizedSignature("removeCurrentSubscription(QString)"))
                >= 0);
        QVERIFY(subscriptionsMetaObject->indexOfMethod(
                    QMetaObject::normalizedSignature("setCurrentSubscriptionPaused(QString,bool)"))
                >= 0);
        QVERIFY(subscriptionsMetaObject->indexOfMethod(
                    QMetaObject::normalizedSignature("showSubscriptionContextMenu(QString,QPointF)"))
                >= 0);
        QVERIFY(subscriptionsMetaObject->indexOfSignal(QMetaObject::normalizedSignature("subscriptionsChanged()")) >= 0);

        QVERIFY(connectionMetaObject->indexOfProperty("sessionStatus") >= 0);
        QVERIFY(connectionMetaObject->indexOfProperty("publishStatus") >= 0);
        QVERIFY(connectionMetaObject->indexOfMethod(QMetaObject::normalizedSignature("connectCurrentSession()")) >= 0);
        QVERIFY(connectionMetaObject->indexOfMethod(QMetaObject::normalizedSignature("disconnectCurrentSession()")) >= 0);
        QVERIFY(connectionMetaObject->indexOfMethod(QMetaObject::normalizedSignature("setCurrentOutputPaused(bool)")) >= 0);
        QVERIFY(connectionMetaObject->indexOfMethod(
                    QMetaObject::normalizedSignature("publishCurrentSession(QString,QString,int,int,bool)"))
                >= 0);
        QVERIFY(connectionMetaObject->indexOfSignal(QMetaObject::normalizedSignature("currentSessionChanged()")) >= 0);
        QVERIFY(connectionMetaObject->indexOfSignal(QMetaObject::normalizedSignature("sessionStatusChanged()")) >= 0);
        QVERIFY(connectionMetaObject->indexOfSignal(QMetaObject::normalizedSignature("publishStatusChanged()")) >= 0);

        QVERIFY(messagesMetaObject->indexOfProperty("messages") >= 0);
        QVERIFY(messagesMetaObject->indexOfMethod(QMetaObject::normalizedSignature("loadOlderCurrentSessionMessages()")) >= 0);
        QVERIFY(messagesMetaObject->indexOfMethod(QMetaObject::normalizedSignature("clearCurrentMessages()")) >= 0);
        QVERIFY(messagesMetaObject->indexOfMethod(QMetaObject::normalizedSignature("copyTextToClipboard(QString)")) >= 0);
        QVERIFY(messagesMetaObject->indexOfSignal(QMetaObject::normalizedSignature("messageStreamChanged()")) >= 0);
        QVERIFY(messagesMetaObject->indexOfSignal(QMetaObject::normalizedSignature("messageStreamRowAppended(QVariantMap)"))
                >= 0);

        QCOMPARE(sessionsMetaObject->indexOfProperty("workbench"), -1);
        QCOMPARE(subscriptionsMetaObject->indexOfProperty("workbench"), -1);
        QCOMPARE(connectionMetaObject->indexOfProperty("workbench"), -1);
        QCOMPARE(messagesMetaObject->indexOfProperty("workbench"), -1);
    }

    void uiEventHubPublishesSettingsAggregateSignals()
    {
        ApplicationKernel kernel;
        auto *uiEvents = kernel.runtime()->uiEvents();
        QVERIFY(uiEvents);

        QSignalSpy settingsSpy(uiEvents, &UiEventHub::settingsChanged);
        QSignalSpy windowMaximizedSpy(uiEvents, &UiEventHub::windowMaximizedChanged);

        uiEvents->publishWindowMaximizedChanged();

        QCOMPARE(windowMaximizedSpy.count(), 1);
        QCOMPARE(settingsSpy.count(), 1);
    }

    void domainBusesExposeDomainSignals()
    {
        ApplicationKernel kernel;
        auto *appBus = kernel.appBus();
        auto *events = kernel.events();
        QVERIFY(appBus);
        QVERIFY(events);
        QVERIFY(appBus->scripts());
        QVERIFY(appBus->logs());

        const QMetaObject *eventsMetaObject = events->metaObject();
        const QMetaObject *scriptsMetaObject = appBus->scripts()->metaObject();
        const QMetaObject *logsMetaObject = appBus->logs()->metaObject();

        QVERIFY(eventsMetaObject->indexOfSignal(QMetaObject::normalizedSignature("scriptsChanged()")) >= 0);
        QVERIFY(eventsMetaObject->indexOfSignal(QMetaObject::normalizedSignature("logsChanged()")) >= 0);
        QVERIFY(eventsMetaObject->indexOfSignal(QMetaObject::normalizedSignature("logsRowAppended(QVariantMap)")) >= 0);

        QVERIFY(scriptsMetaObject->indexOfSignal(QMetaObject::normalizedSignature("scriptsChanged()")) >= 0);
        QVERIFY(logsMetaObject->indexOfSignal(QMetaObject::normalizedSignature("logsChanged()")) >= 0);
        QVERIFY(logsMetaObject->indexOfSignal(QMetaObject::normalizedSignature("logsRowAppended(QVariantMap)")) >= 0);
    }

    void settingsBusChangesPublishOnceThroughUiEventHub()
    {
        ApplicationKernel kernel;
        auto *events = kernel.events();
        auto *settings = kernel.appBus()->settings();
        QVERIFY(events);
        QVERIFY(settings);

        QSignalSpy rootSpy(events, &UiEventHub::windowMaximizedChanged);
        QSignalSpy settingsChangedSpy(events, &UiEventHub::settingsChanged);
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
        auto *sessions = kernel.appBus()->sessions();
        QVERIFY(sessions);
        QVERIFY(sessions->sessions());

        const int initialCount = sessions->sessions()->count();
        QVariantMap config = sessions->defaultSessionConfig();
        config.insert(QStringLiteral("name"), QStringLiteral("Coordinator Test"));
        sessions->addSessionWithConfig(config);

        QCOMPARE(sessions->sessions()->count(), initialCount + 1);
        QCOMPARE(sessions->currentSessionIndex(), initialCount);
        QCOMPARE(sessions->sessions()->rowAt(initialCount).value(QStringLiteral("name")).toString(),
                 QStringLiteral("Coordinator Test"));
        QCOMPARE(sessions->currentSession().value(QStringLiteral("name")).toString(),
                 QStringLiteral("Coordinator Test"));
    }

    void newDomainBusesExecuteWorkspaceActions()
    {
        ApplicationKernel kernel;
        auto *appBus = kernel.appBus();
        QVERIFY(appBus);

        auto *sessions = appBus->sessions();
        auto *subscriptions = appBus->subscriptions();
        auto *connection = appBus->connection();
        auto *messages = appBus->messages();
        QVERIFY(sessions);
        QVERIFY(subscriptions);
        QVERIFY(connection);
        QVERIFY(messages);
        QVERIFY(sessions->sessions());
        QVERIFY(subscriptions->filteredSubscriptions());
        QVERIFY(messages->messages());

        const int initialSessionCount = sessions->sessions()->count();
        QVariantMap config = sessions->defaultSessionConfig();
        config.insert(QStringLiteral("name"), QStringLiteral("Domain Bus Session"));
        sessions->addSessionWithConfig(config);

        QCOMPARE(sessions->sessions()->count(), initialSessionCount + 1);
        QCOMPARE(sessions->currentSessionIndex(), initialSessionCount);
        QCOMPARE(sessions->currentSession().value(QStringLiteral("name")).toString(),
                 QStringLiteral("Domain Bus Session"));
        QCOMPARE(connection->sessionStatus().value(QStringLiteral("state")).toString(), QStringLiteral("disconnected"));
        QCOMPARE(connection->publishStatus().value(QStringLiteral("state")).toString(), QStringLiteral("idle"));
        QCOMPARE(messages->messages()->count(), 0);

        subscriptions->upsertCurrentSubscription(QStringLiteral("domain"), 1, 0);
        QCOMPARE(subscriptions->filteredSubscriptions()->count(), 1);
        QCOMPARE(subscriptions->filteredSubscriptions()->rowAt(0).value(QStringLiteral("topic")).toString(),
                 QStringLiteral("domain"));

        sessions->removeSessionAt(initialSessionCount);
        QCOMPARE(sessions->sessions()->count(), initialSessionCount);
    }

    void sessionLifecycleCreatesDefaultAndPersistsAddDelete()
    {
        ApplicationKernel kernel;
        auto *sessions = kernel.appBus()->sessions();
        QVERIFY(sessions);
        QVERIFY(sessions->sessions());

        const int initialCount = sessions->sessions()->count();
        QVERIFY(initialCount >= 1);
        QCOMPARE(sessions->currentSessionIndex(), 0);
        QVERIFY(!sessions->currentSession().value(QStringLiteral("id")).toString().isEmpty());

        QVariantMap config = sessions->defaultSessionConfig();
        config.insert(QStringLiteral("name"), QStringLiteral("Lifecycle Saved"));
        sessions->addSessionWithConfig(config);

        QCOMPARE(sessions->sessions()->count(), initialCount + 1);
        QCOMPARE(sessions->currentSessionIndex(), initialCount);
        QSettings addedSettings(QStringLiteral("mqtt-plus"), QStringLiteral("mqtt-plus"));
        QCOMPARE(addedSettings.beginReadArray(QStringLiteral("sessions")), initialCount + 1);
        addedSettings.endArray();

        sessions->removeSessionAt(initialCount);

        QCOMPARE(sessions->sessions()->count(), initialCount);
        QCOMPARE(sessions->currentSessionIndex(), initialCount - 1);
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
