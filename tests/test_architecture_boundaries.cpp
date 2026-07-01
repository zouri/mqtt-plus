#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QMap>
#include <QStringList>

class ArchitectureBoundariesTest : public QObject
{
    Q_OBJECT

private slots:
    void controllersDoNotDependOnApplicationCore();
    void controllerHeadersUseDedicatedDependencies();
    void applicationCoreDoesNotFriendControllers();
    void applicationCoreDoesNotImplementControllerContexts();
    void applicationCoreHeaderKeepsOnlyCompositionBoundary();
    void applicationCoreDoesNotOwnPlatformActions();
    void applicationCoreDoesNotExposeUnusedScriptSamples();
    void applicationCoreDoesNotImplementWorkbenchPort();
    void applicationCoreUsesNotifierForUiNotifications();
    void applicationCoreDelegatesModelProjection();
    void applicationCoreDelegatesSessionConfiguration();
    void applicationCoreDelegatesSessionRuntimeAndPersistence();
    void applicationCoreDelegatesExitCleanup();
    void applicationCoreDelegatesSignalBindings();
    void applicationCoreRemovesWorkspaceDependencyComposition();
    void applicationObjectGraphOwnsApplicationComposition();
    void qmlUsesApplicationViewModelRootOnly();
    void translationsDoNotReferenceLegacyFacade();
    void addSubscriptionDialogDoesNotBuildScriptOptions();
    void subscriptionsPanelDoesNotReadModelRowsForEditing();
    void subscriptionsPanelDoesNotOwnBusinessState();
    void workbenchViewsDoNotInterpretContextMenuActions();
    void workbenchViewsDoNotUseDialogBridgeObjects();
    void workbenchViewsUseIntentCommands();
    void workbenchViewModelUsesDirectDependencies();
    void logsViewModelUsesDirectDependencies();
    void scriptsViewModelUsesDirectDependencies();
    void settingsViewModelUsesDirectDependencies();
    void applicationViewModelUsesDirectDependencies();
    void workbenchViewModelDoesNotExposeLegacyCommands();
    void workbenchViewModelDoesNotExposeUnusedRawModels();
    void workbenchViewModelDoesNotForwardNonWorkbenchSignals();
    void featureViewModelsDoNotDependOnApplicationCore();
    void editorViewModelsDoNotExposeInternalWorkflowHelpers();
    void scriptEditorViewModelDoesNotExposeInternalWorkflowHelpers();
    void scriptsViewModelDoesNotExposeCoreScriptCrud();
    void settingsViewModelDoesNotExposeWritableRawOptions();
    void settingsViewModelDoesNotExposeInternalOptionHelpers();

private:
    bool readSourceFile(const QString &relativePath, QString &source) const;
};

bool ArchitectureBoundariesTest::readSourceFile(const QString &relativePath, QString &source) const
{
    QFile file(QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QLatin1Char('/') + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    source = QString::fromUtf8(file.readAll());
    return true;
}

void ArchitectureBoundariesTest::controllersDoNotDependOnApplicationCore()
{
    const QStringList controllerFiles {
        QStringLiteral("src/controllers/eventcontroller.h"),
        QStringLiteral("src/controllers/eventcontroller.cpp"),
        QStringLiteral("src/controllers/mqttcontroller.h"),
        QStringLiteral("src/controllers/mqttcontroller.cpp"),
        QStringLiteral("src/controllers/sessioncontroller.h"),
        QStringLiteral("src/controllers/sessioncontroller.cpp"),
        QStringLiteral("src/controllers/scriptcontroller.h"),
        QStringLiteral("src/controllers/scriptcontroller.cpp"),
        QStringLiteral("src/controllers/subscriptioncontroller.h"),
        QStringLiteral("src/controllers/subscriptioncontroller.cpp"),
        QStringLiteral("src/controllers/themecontroller.h"),
        QStringLiteral("src/controllers/themecontroller.cpp"),
    };

    for (const QString &header : controllerFiles) {
        QString source;
        QVERIFY2(readSourceFile(header, source), qPrintable(QStringLiteral("Cannot read %1").arg(header)));
        QVERIFY2(!source.contains(QStringLiteral("ApplicationCore")),
            qPrintable(QStringLiteral("%1 must depend on a narrow controller context, not ApplicationCore").arg(header)));
        QVERIFY2(!source.contains(QStringLiteral("#include \"app/")),
            qPrintable(QStringLiteral("%1 must not depend on app-layer headers").arg(header)));
    }
}

void ArchitectureBoundariesTest::controllerHeadersUseDedicatedDependencies()
{
    const QMap<QString, QStringList> expectedTokens {
        {
            QStringLiteral("src/controllers/eventcontroller.h"),
            {
                QStringLiteral("struct Dependencies"),
                QStringLiteral("void setDependencies(const Dependencies &dependencies)"),
            },
        },
        {
            QStringLiteral("src/controllers/mqttcontroller.h"),
            {
                QStringLiteral("struct Dependencies"),
                QStringLiteral("void setDependencies(const Dependencies &dependencies)"),
            },
        },
        {
            QStringLiteral("src/controllers/sessioncontroller.h"),
            {
                QStringLiteral("struct Dependencies"),
                QStringLiteral("void setDependencies(const Dependencies &dependencies)"),
            },
        },
        {
            QStringLiteral("src/controllers/subscriptioncontroller.h"),
            {
                QStringLiteral("struct Dependencies"),
                QStringLiteral("void setDependencies(const Dependencies &dependencies)"),
            },
        },
    };

    for (auto it = expectedTokens.cbegin(); it != expectedTokens.cend(); ++it) {
        QString source;
        QVERIFY2(readSourceFile(it.key(), source), qPrintable(QStringLiteral("Cannot read %1").arg(it.key())));
        for (const QString &token : it.value()) {
            QVERIFY2(source.contains(token),
                qPrintable(QStringLiteral("%1 must expose dedicated controller dependencies through %2").arg(it.key(), token)));
        }
        QVERIFY2(!source.contains(QStringLiteral("controllercontext.h")),
            qPrintable(QStringLiteral("%1 must not include deleted controller context headers").arg(it.key())));
        QVERIFY2(!source.contains(QStringLiteral("#include \"app/")),
            qPrintable(QStringLiteral("%1 must not include app-layer headers").arg(it.key())));
    }

    const QString aggregatePath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/controllers/applicationcontext.h");
    QVERIFY2(!QFile::exists(aggregatePath), "Aggregate controller context header must be removed");

    const QStringList deletedContextPaths {
        QStringLiteral("/src/controllers/eventcontrollercontext.h"),
        QStringLiteral("/src/controllers/mqttcontrollercontext.h"),
        QStringLiteral("/src/controllers/sessioncontrollercontext.h"),
        QStringLiteral("/src/controllers/subscriptioncontrollercontext.h"),
        QStringLiteral("/src/app/applicationcontrollercontexts.h"),
        QStringLiteral("/src/app/applicationcontrollercontexts.cpp"),
    };
    for (const QString &path : deletedContextPaths) {
        QVERIFY2(!QFile::exists(QStringLiteral(MQTT_PLUS_SOURCE_DIR) + path),
            qPrintable(QStringLiteral("Deleted controller context artifact must stay removed: %1").arg(path)));
    }
}

void ArchitectureBoundariesTest::applicationCoreDoesNotFriendControllers()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcore.h"), source));

    QVERIFY(!source.contains(QStringLiteral("friend class MqttController")));
    QVERIFY(!source.contains(QStringLiteral("friend class SubscriptionController")));
    QVERIFY(!source.contains(QStringLiteral("friend class EventController")));
    QVERIFY(!source.contains(QStringLiteral("friend class SessionController")));
}

void ArchitectureBoundariesTest::applicationCoreDoesNotImplementControllerContexts()
{
    QString coreHeader;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcore.h"), coreHeader));

    const QStringList forbiddenContextTokens {
        QStringLiteral("SessionControllerContext"),
        QStringLiteral("MqttControllerContext"),
        QStringLiteral("EventControllerContext"),
        QStringLiteral("SubscriptionControllerContext"),
        QStringLiteral("ApplicationControllerContexts"),
    };
    for (const QString &token : forbiddenContextTokens) {
        QVERIFY2(!coreHeader.contains(token),
            qPrintable(QStringLiteral("ApplicationCore must not retain deleted controller context token %1").arg(token)));
    }
    QVERIFY2(coreHeader.contains(QStringLiteral("std::unique_ptr<ApplicationCoreState> m_state")),
        "ApplicationCore must keep concrete runtime ownership behind ApplicationCoreState");

    QString coreStateSource;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcorestate.cpp"), coreStateSource));
    QVERIFY2(!coreStateSource.contains(QStringLiteral("controllerContexts")),
        "ApplicationCoreState must wire controllers directly without the deleted adapter bundle");
    QVERIFY2(!coreStateSource.contains(QStringLiteral("setCore(")),
        "SessionController must not retain the old setCore context hook");

    const QStringList expectedDependencyCalls {
        QStringLiteral("sessionController.setDependencies"),
        QStringLiteral("mqttController.setDependencies"),
        QStringLiteral("eventController.setDependencies"),
        QStringLiteral("subscriptionController.setDependencies"),
    };
    for (const QString &token : expectedDependencyCalls) {
        QVERIFY2(coreStateSource.contains(token),
            qPrintable(QStringLiteral("ApplicationCoreState must inject dedicated controller dependencies through %1").arg(token)));
    }

    const QString oldAdapterHeaderPath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationcontrollercontextadapter.h");
    QVERIFY2(!QFile::exists(oldAdapterHeaderPath), "Obsolete aggregate ApplicationControllerContextAdapter header must be removed");
    const QString oldAdapterSourcePath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationcontrollercontextadapter.cpp");
    QVERIFY2(!QFile::exists(oldAdapterSourcePath), "Obsolete aggregate ApplicationControllerContextAdapter source must be removed");
    const QString controllerContextsHeaderPath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationcontrollercontexts.h");
    QVERIFY2(!QFile::exists(controllerContextsHeaderPath), "ApplicationControllerContexts header must stay removed");
    const QString controllerContextsSourcePath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationcontrollercontexts.cpp");
    QVERIFY2(!QFile::exists(controllerContextsSourcePath), "ApplicationControllerContexts source must stay removed");

    const QString eventsSourcePath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationcoreevents.cpp");
    QVERIFY2(!QFile::exists(eventsSourcePath), "Obsolete ApplicationCore event forwarding source file must be removed");
    const QString subscriptionsSourcePath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationcoresubscriptions.cpp");
    QVERIFY2(!QFile::exists(subscriptionsSourcePath), "Obsolete ApplicationCore subscription forwarding source file must be removed");
}

void ArchitectureBoundariesTest::applicationCoreHeaderKeepsOnlyCompositionBoundary()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcore.h"), source));

    QVERIFY2(source.contains(QStringLiteral("std::unique_ptr<ApplicationCoreState> m_state")),
        "ApplicationCore must hide concrete runtime state behind ApplicationCoreState");

    const QStringList allowedForwardDeclarations {
        QStringLiteral("struct ApplicationCoreState;"),
    };
    for (const QString &token : allowedForwardDeclarations) {
        QVERIFY2(source.contains(token),
            qPrintable(QStringLiteral("ApplicationCore header must keep forward declaration %1").arg(token)));
    }

    const QStringList forbiddenIncludePrefixes {
        QStringLiteral("#include \"app/"),
        QStringLiteral("#include \"controllers/"),
        QStringLiteral("#include \"domain/"),
        QStringLiteral("#include \"models/"),
        QStringLiteral("#include \"services/"),
        QStringLiteral("#include <QSettings>"),
        QStringLiteral("#include <QTimer>"),
    };
    for (const QString &token : forbiddenIncludePrefixes) {
        QVERIFY2(!source.contains(token),
            qPrintable(QStringLiteral("ApplicationCore header must not include concrete dependency %1").arg(token)));
    }

    const QStringList forbiddenConcreteMembers {
        QStringLiteral("ApplicationNotifier"),
        QStringLiteral("ApplicationControllerContextAdapter"),
        QStringLiteral("ApplicationModelRefresher"),
        QStringLiteral("ApplicationSessionRepository"),
        QStringLiteral("ApplicationSessionRuntime"),
        QStringLiteral("SessionController"),
        QStringLiteral("ScriptController"),
        QStringLiteral("SubscriptionController"),
        QStringLiteral("MqttController"),
        QStringLiteral("EventController"),
        QStringLiteral("ThemeController"),
        QStringLiteral("LanguageController"),
        QStringLiteral("PreferencesController"),
        QStringLiteral("HistoryStore"),
        QStringLiteral("SessionListModel"),
        QStringLiteral("SubscriptionListModel"),
        QStringLiteral("SubscriptionFilterModel"),
        QStringLiteral("EventStreamModel"),
        QStringLiteral("ScriptLibraryModel"),
        QStringLiteral("ScriptTestSamplesModel"),
        QStringLiteral("QSettings"),
        QStringLiteral("QTimer"),
    };
    for (const QString &token : forbiddenConcreteMembers) {
        QVERIFY2(!source.contains(token),
            qPrintable(QStringLiteral("ApplicationCore header must hide concrete member %1 in ApplicationCoreState").arg(token)));
    }
}

void ArchitectureBoundariesTest::applicationCoreDoesNotOwnPlatformActions()
{
    const QMap<QString, QStringList> forbiddenTokens {
        {
            QStringLiteral("src/app/applicationcore.h"),
            {
                QStringLiteral("PlatformActions"),
                QStringLiteral("showSessionContextMenu"),
                QStringLiteral("showSubscriptionContextMenu"),
                QStringLiteral("copyTextToClipboard"),
            },
        },
        {
            QStringLiteral("src/app/applicationcore.cpp"),
            {
                QStringLiteral("QClipboard"),
                QStringLiteral("QGuiApplication"),
                QStringLiteral("PlatformActions"),
                QStringLiteral("ApplicationCore::copyTextToClipboard"),
            },
        },
    };

    for (auto it = forbiddenTokens.cbegin(); it != forbiddenTokens.cend(); ++it) {
        QString source;
        QVERIFY2(readSourceFile(it.key(), source), qPrintable(QStringLiteral("Cannot read %1").arg(it.key())));
        for (const QString &token : it.value()) {
            QVERIFY2(!source.contains(token),
                qPrintable(QStringLiteral("%1 must not own platform action API %2").arg(it.key(), token)));
        }
    }

    QString workbenchHeader;
    QVERIFY(readSourceFile(QStringLiteral("src/viewmodels/workbenchviewmodel.h"), workbenchHeader));
    QVERIFY2(workbenchHeader.contains(QStringLiteral("PlatformActions")),
        "WorkbenchViewModel must own workbench platform action integration after deleting WorkbenchWorkspace");

    const QString menuSourcePath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationcoremenus.cpp");
    QVERIFY2(!QFile::exists(menuSourcePath), "Obsolete ApplicationCore menu source file must be removed");
}

void ArchitectureBoundariesTest::applicationCoreDoesNotExposeUnusedScriptSamples()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcore.h"), source));

    const QStringList forbiddenTokens {
        QStringLiteral("ScriptTestSamplesModel *scriptTestSamples()"),
        QStringLiteral("void scriptTestSamplesChanged()"),
        QStringLiteral("emitScriptTestSamplesChanged"),
        QStringLiteral("bool deleteScript(const QString &id)"),
        QStringLiteral("QVariantMap testScript("),
        QStringLiteral("QString effectiveLanguage() const"),
        QStringLiteral("QVariantList availableLanguages() const"),
    };

    for (const QString &token : forbiddenTokens) {
        QVERIFY2(!source.contains(token),
            qPrintable(QStringLiteral("ApplicationCore must not expose unused script sample surface %1").arg(token)));
    }
}

void ArchitectureBoundariesTest::applicationCoreDoesNotImplementWorkbenchPort()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcore.h"), source));

    QVERIFY2(!source.contains(QStringLiteral("WorkbenchCorePort")),
        "ApplicationCore must not implement page-specific ViewModel ports");
    QVERIFY2(!source.contains(QStringLiteral("workbenchcoreport.h")),
        "ApplicationCore must not include page-specific ViewModel port headers");

    const QStringList forbiddenTokens {
        QStringLiteral("SessionListModel *sessions()"),
        QStringLiteral("SubscriptionFilterModel *filteredSubscriptions()"),
        QStringLiteral("EventStreamModel *messages()"),
        QStringLiteral("int currentSessionIndex() const"),
        QStringLiteral("QVariantMap currentSession() const"),
        QStringLiteral("QVariantMap sessionStatus() const"),
        QStringLiteral("QVariantMap publishStatus() const"),
        QStringLiteral("QStringList payloadFormats() const"),
        QStringLiteral("void setCurrentSessionIndex"),
        QStringLiteral("QVariantMap defaultSessionConfig() const"),
        QStringLiteral("QVariantMap sessionConfigAt"),
        QStringLiteral("bool updateSessionConfigAt"),
        QStringLiteral("void addSessionWithConfig"),
        QStringLiteral("void duplicateSessionAt"),
        QStringLiteral("void removeSessionAt"),
        QStringLiteral("void connectCurrentSession"),
        QStringLiteral("void disconnectCurrentSession"),
        QStringLiteral("void setCurrentOutputPaused"),
        QStringLiteral("bool upsertCurrentSubscription"),
        QStringLiteral("bool updateCurrentSubscription"),
        QStringLiteral("void removeCurrentSubscription"),
        QStringLiteral("void setCurrentSubscriptionPaused"),
        QStringLiteral("void publishCurrentSession"),
        QStringLiteral("void clearCurrentMessages"),
        QStringLiteral("int loadOlderCurrentSessionMessages"),
    };

    for (const QString &token : forbiddenTokens) {
        QVERIFY2(!source.contains(token),
            qPrintable(QStringLiteral("ApplicationCore must not expose Workbench page API %1").arg(token)));
    }

    const int privateIndex = source.indexOf(QStringLiteral("private:"));
    QVERIFY2(privateIndex > 0, "ApplicationCore header must keep a private section");
    const QString publicSurface = source.left(privateIndex);
    const QStringList forbiddenFeatureTokens {
        QStringLiteral("EventStreamModel *logs()"),
        QStringLiteral("ScriptLibraryModel *scripts()"),
        QStringLiteral("QString themeMode() const"),
        QStringLiteral("QString effectiveTheme() const"),
        QStringLiteral("QString languageMode() const"),
        QStringLiteral("QString clearMessagesOnExit() const"),
        QStringLiteral("QString clearLogsOnExit() const"),
        QStringLiteral("void setThemeMode"),
        QStringLiteral("void setLanguageMode"),
        QStringLiteral("void clearCurrentLogs"),
        QStringLiteral("void clearAllMessages"),
        QStringLiteral("void clearAllLogs"),
        QStringLiteral("void clearAllHistory"),
        QStringLiteral("int loadOlderCurrentSessionLogs"),
        QStringLiteral("QString upsertScript"),
        QStringLiteral("void saveWindowGeometry"),
    };

    for (const QString &token : forbiddenFeatureTokens) {
        QVERIFY2(!publicSurface.contains(token),
            qPrintable(QStringLiteral("ApplicationCore public API must expose dependencies instead of feature command %1").arg(token)));
    }

    const QStringList forbiddenViewModelDependencyTokens {
        QStringLiteral("LogsViewModelDependencies"),
        QStringLiteral("ScriptsViewModelDependencies"),
        QStringLiteral("SettingsViewModelDependencies"),
        QStringLiteral("logsDependencies()"),
        QStringLiteral("scriptsDependencies()"),
        QStringLiteral("settingsDependencies()"),
    };

    for (const QString &token : forbiddenViewModelDependencyTokens) {
        QVERIFY2(!publicSurface.contains(token),
            qPrintable(QStringLiteral("ApplicationCore must expose workspace dependencies instead of ViewModel dependency %1").arg(token)));
    }

    const QStringList workspaceFiles {
        QStringLiteral("src/app/workbenchworkspace.h"),
        QStringLiteral("src/app/workbenchworkspace.cpp"),
    };

    for (const QString &path : workspaceFiles) {
        QVERIFY2(!QFile::exists(QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QLatin1Char('/') + path),
            qPrintable(QStringLiteral("Deleted Workbench workspace file must not return: %1").arg(path)));
    }
}

void ArchitectureBoundariesTest::applicationCoreUsesNotifierForUiNotifications()
{
    QString coreHeader;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcore.h"), coreHeader));
    QVERIFY2(coreHeader.contains(QStringLiteral("signals:")),
        "ApplicationCore must own UI notification signals after deleting ApplicationNotifier");
    QVERIFY2(coreHeader.contains(QStringLiteral("void sessionsChanged()")),
        "ApplicationCore must expose session change notifications");
    QVERIFY2(coreHeader.contains(QStringLiteral("void logStreamRowAppended")),
        "ApplicationCore must expose row-append notifications for stream consumers");
    QVERIFY2(coreHeader.contains(QStringLiteral("void notifySessionsChanged()")),
        "ApplicationCore must own session notification helpers");

    QString coreStateHeader;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcorestate.h"), coreStateHeader));
    QVERIFY2(coreStateHeader.contains(QStringLiteral("ApplicationCore &core")),
        "ApplicationCoreState must keep ApplicationCore as the UI notification outlet");
    QVERIFY2(!coreStateHeader.contains(QStringLiteral("ApplicationNotifier")),
        "ApplicationCoreState must not keep the deleted ApplicationNotifier");

    const QStringList deletedPaths {
        QStringLiteral("/src/app/applicationnotifier.h"),
        QStringLiteral("/src/app/applicationnotifier.cpp"),
    };
    for (const QString &path : deletedPaths) {
        QVERIFY2(!QFile::exists(QStringLiteral(MQTT_PLUS_SOURCE_DIR) + path),
            qPrintable(QStringLiteral("Deleted notifier artifact must stay removed: %1").arg(path)));
    }

    QString objectGraph;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationobjectgraph.cpp"), objectGraph));
    QVERIFY2(objectGraph.contains(QStringLiteral("ApplicationCore::")),
        "ApplicationObjectGraph direct dependencies must bind ViewModel callbacks to ApplicationCore");
    QVERIFY2(objectGraph.contains(QStringLiteral("&ApplicationCore::currentSessionChanged")),
        "ViewModel dependencies must bind current-session notifications to ApplicationCore UI signals");
    QVERIFY2(objectGraph.contains(QStringLiteral("&ApplicationCore::logStreamChanged")),
        "ViewModel dependencies must bind log-stream notifications to ApplicationCore UI signals");
    QVERIFY2(objectGraph.contains(QStringLiteral("&ApplicationCore::scriptLibraryChanged")),
        "ViewModel dependencies must bind script-library notifications to ApplicationCore UI signals");
}

void ArchitectureBoundariesTest::applicationCoreDelegatesModelProjection()
{
    QString coreStateHeader;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcorestate.h"), coreStateHeader));
    QVERIFY2(coreStateHeader.contains(QStringLiteral("ApplicationModelRefresher modelRefresher")),
        "ApplicationCoreState must delegate list-model projection to ApplicationModelRefresher");

    const QString coreModelsPath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationcoremodels.cpp");
    QVERIFY2(!QFile::exists(coreModelsPath), "Obsolete ApplicationCore model dependency source file must be removed");

    QString coreHeader;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcore.h"), coreHeader));
    const QStringList forbiddenProjectionTokens {
        QStringLiteral("SessionListRow row"),
        QStringLiteral("SubscriptionListRow row"),
        QStringLiteral("ScriptLibraryRow row"),
        QStringLiteral("ScriptTestSampleRow sample"),
        QStringLiteral("PayloadCodec::formatName"),
        QStringLiteral("ScriptStore::scriptFilePath"),
    };
    for (const QString &token : forbiddenProjectionTokens) {
        QVERIFY2(!coreHeader.contains(token),
            qPrintable(QStringLiteral("ApplicationCore must not build model projection row %1").arg(token)));
    }
    QVERIFY2(!coreHeader.contains(QStringLiteral("void ApplicationCore::refreshSessionsModel")),
        "ApplicationCore must not retain session model refresh forwarding methods");
    QVERIFY2(!coreHeader.contains(QStringLiteral("void ApplicationCore::refreshSubscriptionsModel")),
        "ApplicationCore must not retain subscription model refresh forwarding methods");

    QString refresher;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationmodelrefresher.cpp"), refresher));
    QVERIFY2(refresher.contains(QStringLiteral("SessionListRow row")),
        "ApplicationModelRefresher must own session row projection");
    QVERIFY2(refresher.contains(QStringLiteral("SubscriptionListRow row")),
        "ApplicationModelRefresher must own subscription row projection");
    QVERIFY2(refresher.contains(QStringLiteral("ScriptLibraryRow row")),
        "ApplicationModelRefresher must own script row projection");
    QVERIFY2(refresher.contains(QStringLiteral("ScriptTestSampleRow sample")),
        "ApplicationModelRefresher must own script sample row projection");

    QString refreshCoordinator;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationviewrefreshcoordinator.cpp"), refreshCoordinator));
    QVERIFY2(refreshCoordinator.contains(QStringLiteral("m_dependencies.modelRefresher->refreshSessions")),
        "ApplicationViewRefreshCoordinator must route session model refreshes to ApplicationModelRefresher");
    QVERIFY2(refreshCoordinator.contains(QStringLiteral("m_dependencies.modelRefresher->refreshSubscriptions")),
        "ApplicationViewRefreshCoordinator must route subscription model refreshes to ApplicationModelRefresher");
}

void ArchitectureBoundariesTest::applicationCoreDelegatesSessionConfiguration()
{
    QString coreStateSource;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcorestate.cpp"), coreStateSource));

    const QStringList forbiddenConfigTokens {
        QStringLiteral("QMqttConnectionProperties connectionProperties"),
        QStringLiteral("setAuthenticationMethod"),
        QStringLiteral("setConnectionProperties"),
        QStringLiteral("sanitizeOptionalUInt32"),
        QStringLiteral("sanitizeOptionalUInt16"),
    };
    for (const QString &token : forbiddenConfigTokens) {
        QVERIFY2(!coreStateSource.contains(token),
            qPrintable(QStringLiteral("ApplicationCore must not own session configuration detail %1").arg(token)));
    }

    QString configurator;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationsessionconfigurator.cpp"), configurator));
    QVERIFY2(configurator.contains(QStringLiteral("QMqttConnectionProperties connectionProperties")),
        "ApplicationSessionConfigurator must own MQTT connection property construction");
    QVERIFY2(configurator.contains(QStringLiteral("setConnectionProperties")),
        "ApplicationSessionConfigurator must apply MQTT connection properties");

    QVERIFY2(coreStateSource.contains(QStringLiteral("ApplicationSessionConfigurator::applyConfig")),
        "ApplicationCoreState must route session controller configuration dependency to ApplicationSessionConfigurator");
}

void ArchitectureBoundariesTest::applicationCoreDelegatesSessionRuntimeAndPersistence()
{
    QString coreStateHeader;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcorestate.h"), coreStateHeader));
    QVERIFY2(coreStateHeader.contains(QStringLiteral("ApplicationSessionRuntime sessionRuntime")),
        "ApplicationCoreState must own a dedicated session runtime collaborator");
    QVERIFY2(coreStateHeader.contains(QStringLiteral("ApplicationSessionRepository sessionRepository")),
        "ApplicationCoreState must own a dedicated session persistence collaborator");

    QString coreSource;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcore.cpp"), coreSource));
    QVERIFY2(coreSource.contains(QStringLiteral("m_state->runStartup()")),
        "ApplicationCore startup must delegate startup loading to ApplicationCoreState");
    QVERIFY2(!coreSource.contains(QStringLiteral("loadSessions()")),
        "ApplicationCore must not retain session startup helpers");
    QVERIFY2(!coreSource.contains(QStringLiteral("loadScripts()")),
        "ApplicationCore must not retain script startup helpers");

    QString coreHeader;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcore.h"), coreHeader));
    QVERIFY2(!coreHeader.contains(QStringLiteral("loadSessions")),
        "ApplicationCore header must not expose startup helpers");
    QVERIFY2(!coreHeader.contains(QStringLiteral("loadScripts")),
        "ApplicationCore header must not expose startup helpers");

    QString coreStateSource;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcorestate.cpp"), coreStateSource));
    QVERIFY2(coreStateSource.contains(QStringLiteral("sessionRepository.loadSessions")),
        "ApplicationCoreState startup must delegate persistence work to ApplicationSessionRepository");

    const QStringList forbiddenRuntimeTokens {
        QStringLiteral("m_sessionRuntime.initialize"),
        QStringLiteral("m_sessionRuntime.destroy"),
        QStringLiteral("m_sessionRepository.saveSessions"),
        QStringLiteral("new QMqttClient"),
        QStringLiteral("new QTimer"),
        QStringLiteral("deleteLater"),
        QStringLiteral("QUuid::createUuid"),
    };
    for (const QString &token : forbiddenRuntimeTokens) {
        QVERIFY2(!coreStateSource.contains(token),
            qPrintable(QStringLiteral("ApplicationCoreState startup must not own session runtime detail %1").arg(token)));
    }

    const QStringList forbiddenPersistenceTokens {
        QStringLiteral("beginReadArray"),
        QStringLiteral("endArray"),
        QStringLiteral("SessionSettingsStore::readSession"),
        QStringLiteral("SessionSettingsStore::writeSessions"),
    };
    for (const QString &token : forbiddenPersistenceTokens) {
        QVERIFY2(!coreStateSource.contains(token),
            qPrintable(QStringLiteral("ApplicationCoreState startup must not own session persistence detail %1").arg(token)));
    }

    QVERIFY2(!QFile::exists(QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationstartup.h")),
        "Deleted ApplicationStartup header must stay removed");
    QVERIFY2(!QFile::exists(QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationstartup.cpp")),
        "Deleted ApplicationStartup source must stay removed");

    const QString oldSessionsSourcePath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationcoresessions.cpp");
    QVERIFY2(!QFile::exists(oldSessionsSourcePath), "Obsolete ApplicationCore session startup source file must be removed");
    const QString oldScriptsSourcePath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationcorescripts.cpp");
    QVERIFY2(!QFile::exists(oldScriptsSourcePath), "Obsolete ApplicationCore script startup source file must be removed");

    QString runtime;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationsessionruntime.cpp"), runtime));
    QVERIFY2(runtime.contains(QStringLiteral("new QMqttClient")),
        "ApplicationSessionRuntime must own MQTT client creation");
    QVERIFY2(runtime.contains(QStringLiteral("deleteLater")),
        "ApplicationSessionRuntime must own runtime object destruction");

    QVERIFY2(coreStateSource.contains(QStringLiteral("sessionRuntime.initialize")),
        "ApplicationCoreState must route session controller runtime initialization to ApplicationSessionRuntime");
    QVERIFY2(coreStateSource.contains(QStringLiteral("sessionRuntime.destroy")),
        "ApplicationCoreState must route session controller runtime destruction to ApplicationSessionRuntime");
    QVERIFY2(coreStateSource.contains(QStringLiteral("sessionRepository.saveSessions")),
        "ApplicationCoreState must route controller session saving to ApplicationSessionRepository");

    QString repository;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationsessionrepository.cpp"), repository));
    QVERIFY2(repository.contains(QStringLiteral("SessionSettingsStore::readSession")),
        "ApplicationSessionRepository must own session loading");
    QVERIFY2(repository.contains(QStringLiteral("SessionSettingsStore::writeSessions")),
        "ApplicationSessionRepository must own session saving");
}

void ArchitectureBoundariesTest::applicationCoreDelegatesExitCleanup()
{
    QString coreHeader;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcore.h"), coreHeader));
    QVERIFY2(!coreHeader.contains(QStringLiteral("clearMessagesOnExit() const")),
        "ApplicationCore must not expose exit cleanup preference helpers");
    QVERIFY2(!coreHeader.contains(QStringLiteral("clearLogsOnExit() const")),
        "ApplicationCore must not expose exit cleanup preference helpers");
    QVERIFY2(!coreHeader.contains(QStringLiteral("applyExitCleanup")),
        "ApplicationCore must delegate exit cleanup to ApplicationCoreState");

    QString coreSource;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcore.cpp"), coreSource));
    QVERIFY2(coreSource.contains(QStringLiteral("m_state->applyExitCleanup()")),
        "ApplicationCore destructor must delegate exit cleanup");

    const QStringList forbiddenCoreTokens {
        QStringLiteral("flushPendingMessageHistory"),
        QStringLiteral("clearAllMessages"),
        QStringLiteral("clearMessages(session->id)"),
        QStringLiteral("clearAllLogs"),
        QStringLiteral("clearLogs(session->id)"),
        QStringLiteral("clearMessagesOnExit()"),
        QStringLiteral("clearLogsOnExit()"),
    };
    for (const QString &token : forbiddenCoreTokens) {
        QVERIFY2(!coreSource.contains(token),
            qPrintable(QStringLiteral("ApplicationCore must not own exit cleanup detail %1").arg(token)));
    }

    QString coreStateHeader;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcorestate.h"), coreStateHeader));
    QVERIFY2(coreStateHeader.contains(QStringLiteral("void applyExitCleanup()")),
        "ApplicationCoreState must own the exit cleanup entry point");
    QVERIFY2(!coreStateHeader.contains(QStringLiteral("ApplicationExitCleanup")),
        "ApplicationCoreState must not keep the deleted exit cleanup collaborator");

    QString coreStateSource;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcorestate.cpp"), coreStateSource));
    QVERIFY2(coreStateSource.contains(QStringLiteral("flushPendingMessageHistory")),
        "ApplicationCoreState exit cleanup must flush pending history before cleanup");
    QVERIFY2(coreStateSource.contains(QStringLiteral("clearAllMessages")),
        "ApplicationCoreState exit cleanup must own message cleanup policy");
    QVERIFY2(coreStateSource.contains(QStringLiteral("clearAllLogs")),
        "ApplicationCoreState exit cleanup must own log cleanup policy");
    QVERIFY2(coreStateSource.contains(QStringLiteral("clearMessagesOnExit")),
        "ApplicationCoreState exit cleanup must read exit cleanup preferences");
    QVERIFY2(coreStateSource.contains(QStringLiteral("clearLogsOnExit")),
        "ApplicationCoreState exit cleanup must read exit cleanup preferences");
    QVERIFY2(!QFile::exists(QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationexitcleanup.h")),
        "Deleted ApplicationExitCleanup header must stay removed");
    QVERIFY2(!QFile::exists(QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationexitcleanup.cpp")),
        "Deleted ApplicationExitCleanup source must stay removed");
}

void ArchitectureBoundariesTest::applicationCoreDelegatesSignalBindings()
{
    QString coreSource;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcore.cpp"), coreSource));
    QVERIFY2(coreSource.contains(QStringLiteral("m_state->installSignalBindings()")),
        "ApplicationCore constructor must delegate signal binding installation to ApplicationCoreState");

    const QStringList forbiddenCoreTokens {
        QStringLiteral("connect(&m_state->scriptController"),
        QStringLiteral("connect(&m_state->themeController"),
        QStringLiteral("connect(&m_state->languageController"),
        QStringLiteral("connect(&m_state->preferencesController"),
        QStringLiteral("subscriptionFpsRefreshTimer.setInterval"),
        QStringLiteral("&QTimer::timeout"),
    };
    for (const QString &token : forbiddenCoreTokens) {
        QVERIFY2(!coreSource.contains(token),
            qPrintable(QStringLiteral("ApplicationCore must not own signal/timer binding detail %1").arg(token)));
    }

    QString coreStateHeader;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcorestate.h"), coreStateHeader));
    QVERIFY2(coreStateHeader.contains(QStringLiteral("void installSignalBindings()")),
        "ApplicationCoreState must own the signal binding entry point");
    QVERIFY2(!coreStateHeader.contains(QStringLiteral("ApplicationSignalBindings")),
        "ApplicationCoreState must not keep the deleted signal binding collaborator");

    QString coreStateSource;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcorestate.cpp"), coreStateSource));
    QVERIFY2(coreStateSource.contains(QStringLiteral("ScriptController::storageError")),
        "ApplicationCoreState must bind script storage errors");
    QVERIFY2(coreStateSource.contains(QStringLiteral("ThemeController::modeChanged")),
        "ApplicationCoreState must bind theme notifications");
    QVERIFY2(coreStateSource.contains(QStringLiteral("ApplicationCore::notifyThemeModeChanged")),
        "ApplicationCoreState must route theme notifications through ApplicationCore");
    QVERIFY2(coreStateSource.contains(QStringLiteral("LanguageController::languageChanged")),
        "ApplicationCoreState must bind language refresh notifications");
    QVERIFY2(coreStateSource.contains(QStringLiteral("PreferencesController::historyPageSizeChanged")),
        "ApplicationCoreState must bind history page-size notifications");
    QVERIFY2(coreStateSource.contains(QStringLiteral("subscriptionFpsRefreshTimer.setInterval")),
        "ApplicationCoreState must configure subscription FPS refresh timer");
    QVERIFY2(coreStateSource.contains(QStringLiteral("SubscriptionController::refreshSubscriptionFps")),
        "ApplicationCoreState must connect subscription FPS refresh timer");
    QVERIFY2(!QFile::exists(QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationsignalbindings.h")),
        "Deleted ApplicationSignalBindings header must stay removed");
    QVERIFY2(!QFile::exists(QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationsignalbindings.cpp")),
        "Deleted ApplicationSignalBindings source must stay removed");

    QString refreshCoordinator;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationviewrefreshcoordinator.cpp"), refreshCoordinator));
    QVERIFY2(refreshCoordinator.contains(QStringLiteral("notifyLanguageChanged")),
        "ApplicationViewRefreshCoordinator must own language refresh orchestration");
    QVERIFY2(refreshCoordinator.contains(QStringLiteral("notifyHistoryPageSizeChanged")),
        "ApplicationViewRefreshCoordinator must own history page-size refresh orchestration");
}

void ArchitectureBoundariesTest::applicationCoreRemovesWorkspaceDependencyComposition()
{
    QString coreStateHeader;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcorestate.h"), coreStateHeader));
    QVERIFY2(!coreStateHeader.contains(QStringLiteral("ApplicationWorkspaceDependenciesFactory")),
        "ApplicationCoreState must not keep the deleted workspace dependency factory");

    QString coreHeader;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcore.h"), coreHeader));
    const QStringList deletedMethods {
        QStringLiteral("workbenchDependencies()"),
        QStringLiteral("logsWorkspaceDependencies()"),
        QStringLiteral("scriptsWorkspaceDependencies()"),
        QStringLiteral("settingsWorkspaceDependencies()"),
    };
    for (const QString &token : deletedMethods) {
        QVERIFY2(!coreHeader.contains(token),
            qPrintable(QStringLiteral("ApplicationCore must not keep deleted workspace dependency method %1").arg(token)));
    }

    const QStringList deletedPaths {
        QStringLiteral("/src/app/applicationworkspacedependenciesfactory.h"),
        QStringLiteral("/src/app/applicationworkspacedependenciesfactory.cpp"),
        QStringLiteral("/src/app/applicationcoremodels.cpp"),
        QStringLiteral("/src/app/applicationcorepreferences.cpp"),
    };
    for (const QString &path : deletedPaths) {
        QVERIFY2(!QFile::exists(QStringLiteral(MQTT_PLUS_SOURCE_DIR) + path),
            qPrintable(QStringLiteral("Deleted workspace composition artifact must stay removed: %1").arg(path)));
    }
}

void ArchitectureBoundariesTest::applicationObjectGraphOwnsApplicationComposition()
{
    QString graphHeader;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationobjectgraph.h"), graphHeader));
    QVERIFY2(graphHeader.contains(QStringLiteral("class ApplicationObjectGraph")),
        "ApplicationObjectGraph must own application-level object composition");
    QVERIFY2(graphHeader.contains(QStringLiteral("ApplicationCore m_core")),
        "ApplicationObjectGraph must own ApplicationCore");
    QVERIFY2(!graphHeader.contains(QStringLiteral("WorkbenchWorkspace")),
        "ApplicationObjectGraph must not keep the deleted WorkbenchWorkspace");
    QVERIFY2(!graphHeader.contains(QStringLiteral("LogsWorkspace")),
        "ApplicationObjectGraph must not keep the deleted LogsWorkspace");
    QVERIFY2(!graphHeader.contains(QStringLiteral("ScriptsWorkspace")),
        "ApplicationObjectGraph must not keep the deleted ScriptsWorkspace");
    QVERIFY2(!graphHeader.contains(QStringLiteral("SettingsWorkspace")),
        "ApplicationObjectGraph must not keep the deleted SettingsWorkspace");
    QVERIFY2(graphHeader.contains(QStringLiteral("ApplicationViewModel m_viewModel")),
        "ApplicationObjectGraph must own ApplicationViewModel");

    QString graphSource;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationobjectgraph.cpp"), graphSource));
    QVERIFY2(graphSource.contains(QStringLiteral("workbenchDependencies(*m_core.m_state)")),
        "ApplicationObjectGraph must compose WorkbenchViewModel direct dependencies");
    QVERIFY2(graphSource.contains(QStringLiteral("logsDependencies(*m_core.m_state)")),
        "ApplicationObjectGraph must compose LogsViewModel direct dependencies");
    QVERIFY2(graphSource.contains(QStringLiteral("scriptsDependencies(*m_core.m_state)")),
        "ApplicationObjectGraph must compose ScriptsViewModel direct dependencies");
    QVERIFY2(graphSource.contains(QStringLiteral("settingsDependencies(*m_core.m_state)")),
        "ApplicationObjectGraph must compose SettingsViewModel direct dependencies");
    QVERIFY2(!graphSource.contains(QStringLiteral("&m_logsWorkspace")),
        "ApplicationObjectGraph must not pass the deleted LogsWorkspace");
    QVERIFY2(!graphSource.contains(QStringLiteral("&m_scriptsWorkspace")),
        "ApplicationObjectGraph must not pass the deleted ScriptsWorkspace");
    QVERIFY2(!graphSource.contains(QStringLiteral("&m_workbenchWorkspace")),
        "ApplicationObjectGraph must not pass the deleted WorkbenchWorkspace");
    QVERIFY2(!graphSource.contains(QStringLiteral("&m_settingsWorkspace")),
        "ApplicationObjectGraph must not pass the deleted SettingsWorkspace");
}

void ArchitectureBoundariesTest::qmlUsesApplicationViewModelRootOnly()
{
    const QStringList qmlFiles {
        QStringLiteral("qml/Main.qml"),
        QStringLiteral("qml/features/workbench/WorkbenchView.qml"),
        QStringLiteral("qml/features/workbench/SessionSidebar.qml"),
        QStringLiteral("qml/features/workbench/SessionOverviewPanel.qml"),
        QStringLiteral("qml/features/workbench/SubscriptionsPanel.qml"),
        QStringLiteral("qml/features/workbench/SessionActivityPanel.qml"),
        QStringLiteral("qml/features/workbench/EventStreamView.qml"),
        QStringLiteral("qml/features/workbench/PublishComposer.qml"),
        QStringLiteral("qml/features/workbench/SessionEditorDialog.qml"),
        QStringLiteral("qml/features/workbench/AddSubscriptionDialog.qml"),
        QStringLiteral("qml/features/logs/LogsView.qml"),
        QStringLiteral("qml/features/scripts/ScriptsView.qml"),
        QStringLiteral("qml/features/scripts/ScriptListPane.qml"),
        QStringLiteral("qml/features/settings/SettingsView.qml"),
    };

    for (const QString &path : qmlFiles) {
        QString source;
        QVERIFY2(readSourceFile(path, source), qPrintable(QStringLiteral("Cannot read %1").arg(path)));
        QVERIFY2(!source.contains(QStringLiteral("appController")),
            qPrintable(QStringLiteral("%1 must use the ApplicationViewModel root property `app`, not appController").arg(path)));
        QVERIFY2(!source.contains(QStringLiteral("AppFacade")),
            qPrintable(QStringLiteral("%1 must not reference the legacy AppFacade").arg(path)));
    }

    QString mainSource;
    QVERIFY(readSourceFile(QStringLiteral("src/app/main.cpp"), mainSource));
    QVERIFY2(mainSource.contains(QStringLiteral("\"app\"")),
        "main.cpp must inject ApplicationViewModel as the QML `app` root property");
    QVERIFY2(!mainSource.contains(QStringLiteral("appController")),
        "main.cpp must not inject the legacy appController root property");
    QVERIFY2(mainSource.contains(QStringLiteral("ApplicationObjectGraph objectGraph")),
        "main.cpp must delegate application object composition to ApplicationObjectGraph");
    QVERIFY2(mainSource.contains(QStringLiteral("objectGraph.viewModel()")),
        "main.cpp must inject the ViewModel owned by ApplicationObjectGraph");
    QVERIFY2(mainSource.contains(QStringLiteral("objectGraph.settingsViewModel()")),
        "main.cpp must use ApplicationObjectGraph for settings ViewModel access");
    QVERIFY2(!mainSource.contains(QStringLiteral("ApplicationCore core")),
        "main.cpp must not compose ApplicationCore directly");
    QVERIFY2(!mainSource.contains(QStringLiteral("WorkbenchWorkspace")),
        "main.cpp must not compose WorkbenchWorkspace directly");
    QVERIFY2(!mainSource.contains(QStringLiteral("LogsWorkspace")),
        "main.cpp must not compose LogsWorkspace directly");
    QVERIFY2(!mainSource.contains(QStringLiteral("ScriptsWorkspace")),
        "main.cpp must not compose ScriptsWorkspace directly");
    QVERIFY2(!mainSource.contains(QStringLiteral("SettingsWorkspace")),
        "main.cpp must not compose SettingsWorkspace directly");
}

void ArchitectureBoundariesTest::translationsDoNotReferenceLegacyFacade()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("i18n/mqtt_plus_zh_CN.ts"), source));

    const QStringList forbiddenTokens {
        QStringLiteral("AppFacade"),
        QStringLiteral("appfacade"),
        QStringLiteral("<name>ApplicationCore</name>"),
        QStringLiteral("type=\"vanished\""),
        QStringLiteral("type=\"obsolete\""),
        QStringLiteral("applicationcoremodels.cpp"),
        QStringLiteral("applicationcoreutils.cpp"),
        QStringLiteral("applicationcoreevents.cpp"),
        QStringLiteral("applicationcoremenus.cpp"),
        QStringLiteral("applicationcoremqtt.cpp"),
        QStringLiteral("applicationcorescripts.cpp"),
        QStringLiteral("applicationcoresessions.cpp"),
        QStringLiteral("applicationcoresubscriptions.cpp"),
        QStringLiteral("applicationcoretheme.cpp"),
    };

    for (const QString &token : forbiddenTokens) {
        QVERIFY2(!source.contains(token),
            qPrintable(QStringLiteral("Translation file must not keep legacy facade token %1").arg(token)));
    }
}

void ArchitectureBoundariesTest::addSubscriptionDialogDoesNotBuildScriptOptions()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/AddSubscriptionDialog.qml"), source));

    QVERIFY2(!source.contains(QStringLiteral("rowAt(")),
        "AddSubscriptionDialog.qml must not read script model rows to build editor options");
    QVERIFY2(!source.contains(QStringLiteral("setScriptOptions")),
        "AddSubscriptionDialog.qml must not push script options into the editor ViewModel");
    QVERIFY2(!source.contains(QStringLiteral("scriptLibraryChanged")),
        "AddSubscriptionDialog.qml must not synchronize script library state");
}

void ArchitectureBoundariesTest::subscriptionsPanelDoesNotReadModelRowsForEditing()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/SubscriptionsPanel.qml"), source));

    QVERIFY2(!source.contains(QStringLiteral("rowAt(")),
        "SubscriptionsPanel.qml must ask WorkbenchViewModel to prepare editor state instead of reading model rows");
}

void ArchitectureBoundariesTest::subscriptionsPanelDoesNotOwnBusinessState()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/SubscriptionsPanel.qml"), source));

    const QStringList forbiddenTokens {
        QStringLiteral("property string filterText"),
        QStringLiteral("property string filterMode"),
        QStringLiteral("filterModeValues"),
        QStringLiteral("pendingDeleteTopic"),
        QStringLiteral("pendingDeleteDisplayName"),
        QStringLiteral("property: \"filterText\""),
        QStringLiteral("property: \"filterMode\""),
    };

    for (const QString &token : forbiddenTokens) {
        QVERIFY2(!source.contains(token),
            qPrintable(QStringLiteral("SubscriptionsPanel.qml must keep %1 in WorkbenchViewModel").arg(token)));
    }
}

void ArchitectureBoundariesTest::workbenchViewsDoNotInterpretContextMenuActions()
{
    const QStringList files {
        QStringLiteral("qml/features/workbench/SessionSidebar.qml"),
        QStringLiteral("qml/features/workbench/SubscriptionsPanel.qml"),
    };

    for (const QString &path : files) {
        QString source;
        QVERIFY2(readSourceFile(path, source), qPrintable(QStringLiteral("Cannot read %1").arg(path)));
        QVERIFY2(!source.contains(QStringLiteral("showSessionContextMenu")),
            qPrintable(QStringLiteral("%1 must delegate session menu action handling to WorkbenchViewModel").arg(path)));
        QVERIFY2(!source.contains(QStringLiteral("showSubscriptionContextMenu")),
            qPrintable(QStringLiteral("%1 must delegate subscription menu action handling to WorkbenchViewModel").arg(path)));
        QVERIFY2(!source.contains(QStringLiteral("action ===")),
            qPrintable(QStringLiteral("%1 must not branch on platform menu action strings").arg(path)));
    }
}

void ArchitectureBoundariesTest::workbenchViewsDoNotUseDialogBridgeObjects()
{
    const QMap<QString, QStringList> forbiddenTokens {
        {
            QStringLiteral("qml/features/workbench/WorkbenchView.qml"),
            {
                QStringLiteral("sessionEditorBridge"),
                QStringLiteral("addSubscriptionDialogBridge"),
                QStringLiteral("sessionEditor:"),
                QStringLiteral("addSubscriptionDialog:"),
            },
        },
        {
            QStringLiteral("qml/features/workbench/SessionSidebar.qml"),
            {
                QStringLiteral("required property var sessionEditor"),
                QStringLiteral("sessionEditor.openFor"),
            },
        },
        {
            QStringLiteral("qml/features/workbench/SessionOverviewPanel.qml"),
            {
                QStringLiteral("required property var sessionEditor"),
                QStringLiteral("sessionEditor.openFor"),
            },
        },
        {
            QStringLiteral("qml/features/workbench/SubscriptionsPanel.qml"),
            {
                QStringLiteral("required property var addSubscriptionDialog"),
                QStringLiteral("addSubscriptionDialog.openFor"),
            },
        },
    };

    for (auto it = forbiddenTokens.cbegin(); it != forbiddenTokens.cend(); ++it) {
        QString source;
        QVERIFY2(readSourceFile(it.key(), source), qPrintable(QStringLiteral("Cannot read %1").arg(it.key())));
        for (const QString &token : it.value()) {
            QVERIFY2(!source.contains(token),
                qPrintable(QStringLiteral("%1 must expose edit/create intents instead of dialog bridge %2").arg(it.key(), token)));
        }
    }
}

void ArchitectureBoundariesTest::workbenchViewsUseIntentCommands()
{
    const QMap<QString, QStringList> forbiddenTokens {
        {
            QStringLiteral("qml/features/workbench/SessionOverviewPanel.qml"),
            {
                QStringLiteral("connectCurrentSession("),
                QStringLiteral("disconnectCurrentSession("),
            },
        },
        {
            QStringLiteral("qml/features/workbench/EventStreamView.qml"),
            {
                QStringLiteral("setCurrentOutputPaused("),
                QStringLiteral("copyTextToClipboard("),
                QStringLiteral("loadOlderRows"),
                QStringLiteral("clearRows"),
                QStringLiteral("publishDraftRequested"),
            },
        },
        {
            QStringLiteral("qml/features/workbench/WorkbenchView.qml"),
            {
                QStringLiteral("onLogStreamChanged"),
            },
        },
        {
            QStringLiteral("qml/features/workbench/SessionActivityPanel.qml"),
            {
                QStringLiteral("loadOlderRows"),
                QStringLiteral("clearRows"),
                QStringLiteral("loadOlderCurrentSessionMessages("),
                QStringLiteral("clearCurrentMessages("),
                QStringLiteral("setDraft("),
            },
        },
        {
            QStringLiteral("qml/features/workbench/SubscriptionsPanel.qml"),
            {
                QStringLiteral("setCurrentSubscriptionPaused("),
            },
        },
    };

    for (auto it = forbiddenTokens.cbegin(); it != forbiddenTokens.cend(); ++it) {
        QString source;
        QVERIFY2(readSourceFile(it.key(), source), qPrintable(QStringLiteral("Cannot read %1").arg(it.key())));
        for (const QString &token : it.value()) {
            QVERIFY2(!source.contains(token),
                qPrintable(QStringLiteral("%1 must use an intent-style WorkbenchViewModel command instead of %2").arg(it.key(), token)));
        }
    }
}

void ArchitectureBoundariesTest::workbenchViewModelUsesDirectDependencies()
{
    const QStringList files {
        QStringLiteral("src/viewmodels/workbenchviewmodel.h"),
        QStringLiteral("src/viewmodels/workbenchviewmodel.cpp"),
    };

    for (const QString &path : files) {
        QString source;
        QVERIFY2(readSourceFile(path, source), qPrintable(QStringLiteral("Cannot read %1").arg(path)));
        QVERIFY2(!source.contains(QStringLiteral("ApplicationCore")),
            qPrintable(QStringLiteral("%1 must depend on direct dependencies, not ApplicationCore").arg(path)));
        QVERIFY2(!source.contains(QStringLiteral("app/applicationcore.h")),
            qPrintable(QStringLiteral("%1 must not include the aggregate ApplicationCore header").arg(path)));
        QVERIFY2(!source.contains(QStringLiteral("WorkbenchCorePort")),
            qPrintable(QStringLiteral("%1 must not depend on the deleted WorkbenchCorePort").arg(path)));
    }

    QString header;
    QVERIFY(readSourceFile(QStringLiteral("src/viewmodels/workbenchviewmodel.h"), header));
    QVERIFY2(header.contains(QStringLiteral("struct Dependencies")),
        "WorkbenchViewModel must expose a direct dependency struct");
    QVERIFY2(header.contains(QStringLiteral("SessionController *sessionController")),
        "WorkbenchViewModel must receive session controller directly");
    QVERIFY2(header.contains(QStringLiteral("MqttController *mqttController")),
        "WorkbenchViewModel must receive MQTT controller directly");
    QVERIFY2(header.contains(QStringLiteral("SubscriptionController *subscriptionController")),
        "WorkbenchViewModel must receive subscription controller directly");
    QVERIFY2(header.contains(QStringLiteral("EventController *eventController")),
        "WorkbenchViewModel must receive event controller directly");
    QVERIFY2(header.contains(QStringLiteral("SessionListModel *sessions")),
        "WorkbenchViewModel must receive session model directly");
    QVERIFY2(header.contains(QStringLiteral("SubscriptionFilterModel *filteredSubscriptions")),
        "WorkbenchViewModel must receive filtered subscription model directly");
    QVERIFY2(header.contains(QStringLiteral("EventStreamModel *messages")),
        "WorkbenchViewModel must receive message model directly");
    QVERIFY2(header.contains(QStringLiteral("ScriptLibraryModel *scripts")),
        "WorkbenchViewModel must receive script model directly");

    QString source;
    QVERIFY(readSourceFile(QStringLiteral("src/viewmodels/workbenchviewmodel.cpp"), source));
    QVERIFY2(source.contains(QStringLiteral("m_dependencies.bindCurrentSessionChanged")),
        "WorkbenchViewModel must bind workbench notifications through direct dependencies");
    QVERIFY2(source.contains(QStringLiteral("m_dependencies.sessionController")),
        "WorkbenchViewModel must route session commands through direct dependencies");
    QVERIFY2(source.contains(QStringLiteral("m_dependencies.mqttController")),
        "WorkbenchViewModel must route MQTT commands through direct dependencies");
    QVERIFY2(source.contains(QStringLiteral("m_dependencies.subscriptionController")),
        "WorkbenchViewModel must route subscription commands through direct dependencies");
    QVERIFY2(source.contains(QStringLiteral("m_dependencies.eventController")),
        "WorkbenchViewModel must route event commands through direct dependencies");

    const QStringList deletedPaths {
        QStringLiteral("/src/viewmodels/workbenchcoreport.h"),
        QStringLiteral("/src/app/workbenchworkspace.h"),
        QStringLiteral("/src/app/workbenchworkspace.cpp"),
        QStringLiteral("/src/app/workbenchworkspacedependencies.h"),
    };
    for (const QString &path : deletedPaths) {
        QVERIFY2(!QFile::exists(QStringLiteral(MQTT_PLUS_SOURCE_DIR) + path),
            qPrintable(QStringLiteral("Obsolete workbench abstraction must stay removed: %1").arg(path)));
    }
}

void ArchitectureBoundariesTest::logsViewModelUsesDirectDependencies()
{
    QString header;
    QVERIFY(readSourceFile(QStringLiteral("src/viewmodels/logsviewmodel.h"), header));
    QVERIFY2(header.contains(QStringLiteral("struct Dependencies")),
        "LogsViewModel must expose a small direct dependency struct");
    QVERIFY2(header.contains(QStringLiteral("EventStreamModel *logs")),
        "LogsViewModel must receive the log model directly");
    QVERIFY2(header.contains(QStringLiteral("std::function<void()> clearCurrentLogs")),
        "LogsViewModel must receive clear command dependency directly");
    QVERIFY2(header.contains(QStringLiteral("std::function<int()> loadOlderCurrentSessionLogs")),
        "LogsViewModel must receive history loading command dependency directly");
    QVERIFY2(!header.contains(QStringLiteral("LogsCorePort")),
        "LogsViewModel must not depend on the deleted LogsCorePort");

    QString source;
    QVERIFY(readSourceFile(QStringLiteral("src/viewmodels/logsviewmodel.cpp"), source));
    QVERIFY2(source.contains(QStringLiteral("m_dependencies.bindLogStreamChanged")),
        "LogsViewModel must bind log stream changes through direct dependencies");
    QVERIFY2(source.contains(QStringLiteral("m_dependencies.clearCurrentLogs()")),
        "LogsViewModel must route clear through direct dependencies");
    QVERIFY2(source.contains(QStringLiteral("m_dependencies.loadOlderCurrentSessionLogs()")),
        "LogsViewModel must route history loading through direct dependencies");
    QVERIFY2(!source.contains(QStringLiteral("m_core")),
        "LogsViewModel must not retain the old CorePort member");

    const QStringList deletedPaths {
        QStringLiteral("/src/viewmodels/logscoreport.h"),
        QStringLiteral("/src/app/logsworkspace.h"),
        QStringLiteral("/src/app/logsworkspace.cpp"),
    };
    for (const QString &path : deletedPaths) {
        QVERIFY2(!QFile::exists(QStringLiteral(MQTT_PLUS_SOURCE_DIR) + path),
            qPrintable(QStringLiteral("Obsolete logs abstraction must stay removed: %1").arg(path)));
    }
}

void ArchitectureBoundariesTest::scriptsViewModelUsesDirectDependencies()
{
    QString header;
    QVERIFY(readSourceFile(QStringLiteral("src/viewmodels/scriptsviewmodel.h"), header));
    QVERIFY2(header.contains(QStringLiteral("struct Dependencies")),
        "ScriptsViewModel must expose a small direct dependency struct");
    QVERIFY2(header.contains(QStringLiteral("ScriptLibraryModel *scripts")),
        "ScriptsViewModel must receive the script model directly");
    QVERIFY2(header.contains(QStringLiteral("std::function<QString(")),
        "ScriptsViewModel must receive script save command dependency directly");
    QVERIFY2(!header.contains(QStringLiteral("ScriptsCorePort")),
        "ScriptsViewModel must not depend on the deleted ScriptsCorePort");

    QString source;
    QVERIFY(readSourceFile(QStringLiteral("src/viewmodels/scriptsviewmodel.cpp"), source));
    QVERIFY2(source.contains(QStringLiteral("m_dependencies.bindScriptLibraryChanged")),
        "ScriptsViewModel must bind script library changes through direct dependencies");
    QVERIFY2(source.contains(QStringLiteral("m_dependencies.upsertScript")),
        "ScriptsViewModel must save through direct dependencies");
    QVERIFY2(!source.contains(QStringLiteral("m_core")),
        "ScriptsViewModel must not retain the old CorePort member");

    const QStringList deletedPaths {
        QStringLiteral("/src/viewmodels/scriptscoreport.h"),
        QStringLiteral("/src/app/scriptsworkspace.h"),
        QStringLiteral("/src/app/scriptsworkspace.cpp"),
    };
    for (const QString &path : deletedPaths) {
        QVERIFY2(!QFile::exists(QStringLiteral(MQTT_PLUS_SOURCE_DIR) + path),
            qPrintable(QStringLiteral("Obsolete scripts abstraction must stay removed: %1").arg(path)));
    }
}

void ArchitectureBoundariesTest::settingsViewModelUsesDirectDependencies()
{
    QString header;
    QVERIFY(readSourceFile(QStringLiteral("src/viewmodels/settingsviewmodel.h"), header));
    QVERIFY2(header.contains(QStringLiteral("struct Dependencies")),
        "SettingsViewModel must expose a direct dependency struct");
    QVERIFY2(header.contains(QStringLiteral("ThemeController *themeController")),
        "SettingsViewModel must receive theme controller directly");
    QVERIFY2(header.contains(QStringLiteral("LanguageController *languageController")),
        "SettingsViewModel must receive language controller directly");
    QVERIFY2(header.contains(QStringLiteral("PreferencesController *preferencesController")),
        "SettingsViewModel must receive preferences controller directly");
    QVERIFY2(header.contains(QStringLiteral("HistoryStore *historyStore")),
        "SettingsViewModel must receive history store directly");
    QVERIFY2(header.contains(QStringLiteral("EventStreamModel *messages")),
        "SettingsViewModel must receive message stream model directly");
    QVERIFY2(header.contains(QStringLiteral("EventStreamModel *logs")),
        "SettingsViewModel must receive log stream model directly");
    QVERIFY2(!header.contains(QStringLiteral("SettingsCorePort")),
        "SettingsViewModel must not depend on the deleted SettingsCorePort");
    QVERIFY2(!header.contains(QStringLiteral("SettingsViewModelDependencies")),
        "SettingsViewModel must not depend on SettingsViewModelDependencies");

    QString source;
    QVERIFY(readSourceFile(QStringLiteral("src/viewmodels/settingsviewmodel.cpp"), source));
    QVERIFY2(source.contains(QStringLiteral("m_dependencies.bindThemeModeChanged")),
        "SettingsViewModel must bind settings notifications through direct dependencies");
    QVERIFY2(source.contains(QStringLiteral("m_dependencies.themeController->setMode")),
        "SettingsViewModel must route theme writes through direct dependencies");
    QVERIFY2(source.contains(QStringLiteral("m_dependencies.preferencesController->setMessageRetentionLimit")),
        "SettingsViewModel must route preference writes through direct dependencies");
    QVERIFY2(source.contains(QStringLiteral("m_dependencies.historyStore->clearAllMessages")),
        "SettingsViewModel must route cleanup commands through direct dependencies");
    QVERIFY2(!source.contains(QStringLiteral("m_core")),
        "SettingsViewModel must not retain the old CorePort member");

    const QString oldDependenciesPath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/viewmodels/settingsviewmodeldependencies.h");
    QVERIFY2(!QFile::exists(oldDependenciesPath), "Obsolete SettingsViewModelDependencies header must be removed");

    const QStringList deletedPaths {
        QStringLiteral("/src/viewmodels/settingscoreport.h"),
        QStringLiteral("/src/app/settingsworkspace.h"),
        QStringLiteral("/src/app/settingsworkspace.cpp"),
    };
    for (const QString &path : deletedPaths) {
        QVERIFY2(!QFile::exists(QStringLiteral(MQTT_PLUS_SOURCE_DIR) + path),
            qPrintable(QStringLiteral("Obsolete settings abstraction must stay removed: %1").arg(path)));
    }
}

void ArchitectureBoundariesTest::applicationViewModelUsesDirectDependencies()
{
    QString header;
    QVERIFY(readSourceFile(QStringLiteral("src/viewmodels/applicationviewmodel.h"), header));

    const QStringList expectedPorts {
        QStringLiteral("const WorkbenchViewModel::Dependencies &workbenchDependencies"),
        QStringLiteral("const LogsViewModel::Dependencies &logsDependencies"),
        QStringLiteral("const ScriptsViewModel::Dependencies &scriptsDependencies"),
        QStringLiteral("const SettingsViewModel::Dependencies &settingsDependencies"),
    };
    for (const QString &token : expectedPorts) {
        QVERIFY2(header.contains(token),
            qPrintable(QStringLiteral("ApplicationViewModel constructor must receive %1").arg(token)));
    }

    const QStringList forbiddenViewModelDependencies {
        QStringLiteral("WorkbenchCorePort *workbenchCore"),
        QStringLiteral("LogsCorePort *logsCore"),
        QStringLiteral("ScriptsCorePort *scriptsCore"),
        QStringLiteral("SettingsCorePort *settingsCore"),
        QStringLiteral("SettingsViewModelDependencies"),
    };
    for (const QString &token : forbiddenViewModelDependencies) {
        QVERIFY2(!header.contains(token),
            qPrintable(QStringLiteral("ApplicationViewModel must not receive %1").arg(token)));
    }

    QString source;
    QVERIFY(readSourceFile(QStringLiteral("src/viewmodels/applicationviewmodel.cpp"), source));
    QVERIFY2(source.contains(QStringLiteral("m_workbench(workbenchDependencies, this)")),
        "ApplicationViewModel must wire WorkbenchViewModel from direct dependencies");
    QVERIFY2(source.contains(QStringLiteral("m_logs(logsDependencies, this)")),
        "ApplicationViewModel must wire LogsViewModel from direct dependencies");
    QVERIFY2(source.contains(QStringLiteral("m_scripts(scriptsDependencies, this)")),
        "ApplicationViewModel must wire ScriptsViewModel from direct dependencies");
    QVERIFY2(source.contains(QStringLiteral("m_settings(settingsDependencies, this)")),
        "ApplicationViewModel must wire SettingsViewModel from direct dependencies");

    const QStringList removedDependencyHeaders {
        QStringLiteral("/src/viewmodels/logsviewmodeldependencies.h"),
        QStringLiteral("/src/viewmodels/scriptsviewmodeldependencies.h"),
        QStringLiteral("/src/viewmodels/settingsviewmodeldependencies.h"),
    };
    for (const QString &path : removedDependencyHeaders) {
        QVERIFY2(!QFile::exists(QStringLiteral(MQTT_PLUS_SOURCE_DIR) + path),
            qPrintable(QStringLiteral("Obsolete ViewModel dependency header must be removed: %1").arg(path)));
    }
}

void ArchitectureBoundariesTest::workbenchViewModelDoesNotExposeLegacyCommands()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("src/viewmodels/workbenchviewmodel.h"), source));

    const QStringList forbiddenInvokables {
        QStringLiteral("Q_INVOKABLE void duplicateSessionAt"),
        QStringLiteral("Q_INVOKABLE void removeSessionAt"),
        QStringLiteral("Q_INVOKABLE QString showSessionContextMenu"),
        QStringLiteral("Q_INVOKABLE QString showSubscriptionContextMenu"),
        QStringLiteral("Q_INVOKABLE void connectCurrentSession"),
        QStringLiteral("Q_INVOKABLE void disconnectCurrentSession"),
        QStringLiteral("Q_INVOKABLE void setCurrentOutputPaused"),
        QStringLiteral("Q_INVOKABLE void removeCurrentSubscription"),
        QStringLiteral("Q_INVOKABLE void setCurrentSubscriptionPaused"),
        QStringLiteral("Q_INVOKABLE void setPublishDraft"),
        QStringLiteral("Q_INVOKABLE void copyTextToClipboard"),
        QStringLiteral("Q_INVOKABLE void clearCurrentMessages"),
        QStringLiteral("Q_INVOKABLE int loadOlderCurrentSessionMessages"),
    };

    for (const QString &token : forbiddenInvokables) {
        QVERIFY2(!source.contains(token),
            qPrintable(QStringLiteral("WorkbenchViewModel must expose intent commands instead of %1").arg(token)));
    }
}

void ArchitectureBoundariesTest::workbenchViewModelDoesNotExposeUnusedRawModels()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("src/viewmodels/workbenchviewmodel.h"), source));

    const QStringList forbiddenTokens {
        QStringLiteral("Q_PROPERTY(SubscriptionListModel* subscriptions"),
        QStringLiteral("Q_PROPERTY(ScriptLibraryModel* scripts"),
        QStringLiteral("SubscriptionListModel *subscriptions()"),
        QStringLiteral("ScriptLibraryModel *scripts()"),
        QStringLiteral("Q_INVOKABLE void refreshSubscriptionEditorScriptOptions"),
    };

    for (const QString &token : forbiddenTokens) {
        QVERIFY2(!source.contains(token),
            qPrintable(QStringLiteral("WorkbenchViewModel must not expose unused raw model %1").arg(token)));
    }
}

void ArchitectureBoundariesTest::workbenchViewModelDoesNotForwardNonWorkbenchSignals()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("src/viewmodels/workbenchviewmodel.h"), source));

    const QStringList forbiddenSignals {
        QStringLiteral("void subscriptionsChanged()"),
        QStringLiteral("void logStreamChanged()"),
        QStringLiteral("void logStreamRowAppended"),
        QStringLiteral("void scriptLibraryChanged()"),
        QStringLiteral("void messageStreamRowAppended(const QVariantMap"),
    };

    for (const QString &token : forbiddenSignals) {
        QVERIFY2(!source.contains(token),
            qPrintable(QStringLiteral("WorkbenchViewModel must not forward non-workbench signal %1").arg(token)));
    }
}

void ArchitectureBoundariesTest::featureViewModelsDoNotDependOnApplicationCore()
{
    const QStringList files {
        QStringLiteral("src/viewmodels/applicationviewmodel.h"),
        QStringLiteral("src/viewmodels/applicationviewmodel.cpp"),
        QStringLiteral("src/viewmodels/logsviewmodel.h"),
        QStringLiteral("src/viewmodels/logsviewmodel.cpp"),
        QStringLiteral("src/viewmodels/scriptsviewmodel.h"),
        QStringLiteral("src/viewmodels/scriptsviewmodel.cpp"),
        QStringLiteral("src/viewmodels/settingsviewmodel.h"),
        QStringLiteral("src/viewmodels/settingsviewmodel.cpp"),
    };

    for (const QString &path : files) {
        QString source;
        QVERIFY2(readSourceFile(path, source), qPrintable(QStringLiteral("Cannot read %1").arg(path)));
        QVERIFY2(!source.contains(QStringLiteral("ApplicationCore")),
            qPrintable(QStringLiteral("%1 must depend on narrow ViewModel dependencies, not ApplicationCore").arg(path)));
        QVERIFY2(!source.contains(QStringLiteral("ViewModelDependencies")),
            qPrintable(QStringLiteral("%1 must use inline direct dependency structs, not legacy ViewModel dependency bags").arg(path)));
        QVERIFY2(!source.contains(QStringLiteral("app/applicationcore.h")),
            qPrintable(QStringLiteral("%1 must not include the aggregate ApplicationCore header").arg(path)));
    }
}

void ArchitectureBoundariesTest::editorViewModelsDoNotExposeInternalWorkflowHelpers()
{
    const QMap<QString, QStringList> forbiddenInvokables {
        {
            QStringLiteral("src/viewmodels/sessioneditorviewmodel.h"),
            {
                QStringLiteral("Q_INVOKABLE void openForCreate"),
                QStringLiteral("Q_INVOKABLE void openForEdit"),
                QStringLiteral("Q_INVOKABLE void loadConfig"),
                QStringLiteral("Q_INVOKABLE QVariantMap collectedConfig"),
                QStringLiteral("Q_INVOKABLE bool validate"),
            },
        },
        {
            QStringLiteral("src/viewmodels/subscriptioneditorviewmodel.h"),
            {
                QStringLiteral("Q_INVOKABLE void openForCreate"),
                QStringLiteral("Q_INVOKABLE void openForEdit"),
                QStringLiteral("Q_INVOKABLE void setScriptOptions"),
                QStringLiteral("Q_INVOKABLE QVariantMap submission"),
            },
        },
    };

    for (auto it = forbiddenInvokables.cbegin(); it != forbiddenInvokables.cend(); ++it) {
        QString source;
        QVERIFY2(readSourceFile(it.key(), source), qPrintable(QStringLiteral("Cannot read %1").arg(it.key())));
        for (const QString &token : it.value()) {
            QVERIFY2(!source.contains(token),
                qPrintable(QStringLiteral("%1 must not expose internal workflow helper %2").arg(it.key(), token)));
        }
    }
}

void ArchitectureBoundariesTest::scriptEditorViewModelDoesNotExposeInternalWorkflowHelpers()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("src/viewmodels/scripteditorviewmodel.h"), source));

    const QStringList forbiddenInvokables {
        QStringLiteral("Q_INVOKABLE QString defaultCode"),
        QStringLiteral("Q_INVOKABLE void loadScript"),
        QStringLiteral("Q_INVOKABLE void newScript"),
        QStringLiteral("Q_INVOKABLE bool validateStructure"),
        QStringLiteral("Q_INVOKABLE void markSaved"),
    };

    for (const QString &token : forbiddenInvokables) {
        QVERIFY2(!source.contains(token),
            qPrintable(QStringLiteral("ScriptEditorViewModel must not expose internal workflow helper %1").arg(token)));
    }
}

void ArchitectureBoundariesTest::scriptsViewModelDoesNotExposeCoreScriptCrud()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("src/viewmodels/scriptsviewmodel.h"), source));

    const QStringList forbiddenTokens {
        QStringLiteral("Q_INVOKABLE QString upsertScript"),
        QStringLiteral("Q_INVOKABLE bool deleteScript"),
        QStringLiteral("Q_INVOKABLE QVariantMap testScript"),
        QStringLiteral("Q_PROPERTY(ScriptTestSamplesModel* scriptTestSamples"),
        QStringLiteral("Q_PROPERTY(QStringList payloadFormats"),
        QStringLiteral("ScriptTestSamplesModel *scriptTestSamples()"),
    };

    for (const QString &token : forbiddenTokens) {
        QVERIFY2(!source.contains(token),
            qPrintable(QStringLiteral("ScriptsViewModel must expose page intent commands instead of core script API %1").arg(token)));
    }
}

void ArchitectureBoundariesTest::settingsViewModelDoesNotExposeWritableRawOptions()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("src/viewmodels/settingsviewmodel.h"), source));

    const QStringList forbiddenTokens {
        QStringLiteral("Q_PROPERTY(QString themeMode READ themeMode"),
        QStringLiteral("Q_PROPERTY(QString languageMode READ languageMode"),
        QStringLiteral("Q_PROPERTY(QString effectiveLanguage READ effectiveLanguage"),
        QStringLiteral("Q_PROPERTY(QVariantList availableLanguages READ availableLanguages"),
        QStringLiteral("Q_PROPERTY(int messageRetentionLimit READ messageRetentionLimit"),
        QStringLiteral("Q_PROPERTY(int logRetentionLimit READ logRetentionLimit"),
        QStringLiteral("Q_PROPERTY(int historyPageSize READ historyPageSize"),
        QStringLiteral("Q_PROPERTY(int maxIncomingPayloadBytes READ maxIncomingPayloadBytes"),
        QStringLiteral("Q_PROPERTY(QString clearMessagesOnExit READ clearMessagesOnExit"),
        QStringLiteral("Q_PROPERTY(QString clearLogsOnExit READ clearLogsOnExit"),
        QStringLiteral("Q_PROPERTY(QString themeMode READ themeMode WRITE"),
        QStringLiteral("Q_PROPERTY(QString languageMode READ languageMode WRITE"),
        QStringLiteral("Q_PROPERTY(int messageRetentionLimit READ messageRetentionLimit WRITE"),
        QStringLiteral("Q_PROPERTY(int logRetentionLimit READ logRetentionLimit WRITE"),
        QStringLiteral("Q_PROPERTY(int historyPageSize READ historyPageSize WRITE"),
        QStringLiteral("Q_PROPERTY(int maxIncomingPayloadBytes READ maxIncomingPayloadBytes WRITE"),
        QStringLiteral("Q_PROPERTY(QString clearMessagesOnExit READ clearMessagesOnExit WRITE"),
        QStringLiteral("Q_PROPERTY(QString clearLogsOnExit READ clearLogsOnExit WRITE"),
    };

    for (const QString &token : forbiddenTokens) {
        QVERIFY2(!source.contains(token),
            qPrintable(QStringLiteral("SettingsViewModel must update option state through index commands instead of writable raw property %1").arg(token)));
    }
}

void ArchitectureBoundariesTest::settingsViewModelDoesNotExposeInternalOptionHelpers()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("src/viewmodels/settingsoptionsviewmodel.h"), source));

    const QStringList forbiddenInvokables {
        QStringLiteral("Q_INVOKABLE int optionIndex"),
        QStringLiteral("Q_INVOKABLE QVariant optionValue"),
    };

    for (const QString &token : forbiddenInvokables) {
        QVERIFY2(!source.contains(token),
            qPrintable(QStringLiteral("SettingsViewModel must not expose internal option helper %1").arg(token)));
    }
}

QTEST_MAIN(ArchitectureBoundariesTest)

#include "test_architecture_boundaries.moc"
