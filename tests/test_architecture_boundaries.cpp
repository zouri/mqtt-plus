#include <QtTest/QtTest>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QMap>
#include <QStringList>

class ArchitectureBoundariesTest : public QObject
{
    Q_OBJECT

private slots:
    void usecasesDoNotDependOnApplicationCore();
    void usecaseHeadersUseDedicatedDependencies();
    void applicationCoreDoesNotFriendUsecases();
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
    void applicationCoreAppliesMessageRetentionAtLifecycleBoundaries();
    void applicationCoreDelegatesSignalBindings();
    void applicationCoreRemovesWorkspaceDependencyComposition();
    void applicationObjectGraphOwnsApplicationComposition();
    void publishStatusUsesTypedRuntimeState();
    void sessionRuntimeStateIsSeparatedFromPersistentSessionConfig();
    void eventHistoryServiceMatchesSubscriptionsInReceivePath();
    void eventStreamModelUsesTypedRows();
    void eventStreamModelPrependsRowsInBatch();
    void historyStoreListQueriesDoNotProjectPayloadBlobs();
    void eventHistoryServiceDefersRetentionPruneToLifecycle();
    void messageQmlUsesTypedObjectProperties();
    void messageRowsUseHoverHandlerForNestedControls();
    void messageRowsUseButtonTapPolicy();
    void messageRowsDoNotNestPointerHandlingTextEdit();
    void messagePanelUsesSplitViewForComposerResize();
    void eventStreamFollowModeUsesSingleCycleButton();
    void qmlUsesApplicationViewModelRootOnly();
    void applicationUsesSystemFixedFont();
    void translationsDoNotReferenceLegacyFacade();
    void settingsExplainDeferredMessageRetention();
    void addSubscriptionDialogDoesNotBuildScriptOptions();
    void subscriptionsPanelDoesNotReadModelRowsForEditing();
    void subscriptionsPanelDoesNotOwnBusinessState();
    void workbenchMiddlePaneUsesCompactHeaderControls();
    void subscriptionRowsKeepCompactActionGroup();
    void workbenchUsesReferenceMessageWorkspace();
    void messageInspectorPreservesPayloadFormatting();
    void closingMessageInspectorClearsRowSelection();
    void messageInspectorUsesLeftEdgeShadow();
    void qmlUsesNativeFocusManagement();
    void qmlMenusAreApplicationRendered();
    void textEditorsUseNativeContextMenus();
    void workbenchViewsDoNotInterpretContextMenuActions();
    void workbenchViewsDoNotUseDialogBridgeObjects();
    void workbenchViewsUseIntentCommands();
    void eventStreamViewUsesLocalFollowScrollState();
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

void ArchitectureBoundariesTest::usecasesDoNotDependOnApplicationCore()
{
    const QStringList usecaseFiles {
        QStringLiteral("src/usecases/eventhistoryservice.h"),
        QStringLiteral("src/usecases/eventhistoryservice.cpp"),
        QStringLiteral("src/usecases/mqttsessionservice.h"),
        QStringLiteral("src/usecases/mqttsessionservice.cpp"),
        QStringLiteral("src/usecases/sessionservice.h"),
        QStringLiteral("src/usecases/sessionservice.cpp"),
        QStringLiteral("src/usecases/scriptservice.h"),
        QStringLiteral("src/usecases/scriptservice.cpp"),
        QStringLiteral("src/usecases/subscriptionservice.h"),
        QStringLiteral("src/usecases/subscriptionservice.cpp"),
    };

    for (const QString &header : usecaseFiles) {
        QString source;
        QVERIFY2(readSourceFile(header, source), qPrintable(QStringLiteral("Cannot read %1").arg(header)));
        QVERIFY2(!source.contains(QStringLiteral("ApplicationCore")),
            qPrintable(QStringLiteral("%1 must depend on narrow use-case dependencies, not ApplicationCore").arg(header)));
        QVERIFY2(!source.contains(QStringLiteral("#include \"app/")),
            qPrintable(QStringLiteral("%1 must not depend on app-layer headers").arg(header)));
    }
    const QString themeHeaderPath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/usecases/themecontroller.h");
    QVERIFY2(!QFile::exists(themeHeaderPath), "ThemeController is merged into SettingsViewModel");
    const QString themeCppPath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/usecases/themecontroller.cpp");
    QVERIFY2(!QFile::exists(themeCppPath), "ThemeController is merged into SettingsViewModel");
    const QString langHeaderPath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/usecases/languagecontroller.h");
    QVERIFY2(!QFile::exists(langHeaderPath), "LanguageController is merged into SettingsViewModel");
    const QString langCppPath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/usecases/languagecontroller.cpp");
    QVERIFY2(!QFile::exists(langCppPath), "LanguageController is merged into SettingsViewModel");
}

void ArchitectureBoundariesTest::usecaseHeadersUseDedicatedDependencies()
{
    const QMap<QString, QStringList> expectedTokens {
        {
            QStringLiteral("src/usecases/eventhistoryservice.h"),
            {
                QStringLiteral("struct Dependencies"),
                QStringLiteral("void setDependencies(const Dependencies &dependencies)"),
            },
        },
        {
            QStringLiteral("src/usecases/mqttsessionservice.h"),
            {
                QStringLiteral("struct Dependencies"),
                QStringLiteral("void setDependencies(const Dependencies &dependencies)"),
            },
        },
        {
            QStringLiteral("src/usecases/sessionservice.h"),
            {
                QStringLiteral("struct Dependencies"),
                QStringLiteral("void setDependencies(const Dependencies &dependencies)"),
            },
        },
        {
            QStringLiteral("src/usecases/subscriptionservice.h"),
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
                qPrintable(QStringLiteral("%1 must expose dedicated use-case dependencies through %2").arg(it.key(), token)));
        }
        QVERIFY2(!source.contains(QStringLiteral("controllercontext.h")),
            qPrintable(QStringLiteral("%1 must not include deleted controller context headers").arg(it.key())));
        QVERIFY2(!source.contains(QStringLiteral("#include \"app/")),
            qPrintable(QStringLiteral("%1 must not include app-layer headers").arg(it.key())));
    }

    const QString aggregatePath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/usecases/applicationcontext.h");
    QVERIFY2(!QFile::exists(aggregatePath), "Aggregate controller context header must be removed");

    const QStringList deletedContextPaths {
        QStringLiteral("/src/usecases/eventcontrollercontext.h"),
        QStringLiteral("/src/usecases/mqttcontrollercontext.h"),
        QStringLiteral("/src/usecases/sessioncontrollercontext.h"),
        QStringLiteral("/src/usecases/subscriptioncontrollercontext.h"),
        QStringLiteral("/src/app/applicationcontrollercontexts.h"),
        QStringLiteral("/src/app/applicationcontrollercontexts.cpp"),
    };
    for (const QString &path : deletedContextPaths) {
        QVERIFY2(!QFile::exists(QStringLiteral(MQTT_PLUS_SOURCE_DIR) + path),
            qPrintable(QStringLiteral("Deleted controller context artifact must stay removed: %1").arg(path)));
    }
}

void ArchitectureBoundariesTest::applicationCoreDoesNotFriendUsecases()
{
    const QString path = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationcore.h");
    QVERIFY2(!QFile::exists(path), "ApplicationCore header is deleted — use-case services emit their own signals");
}

void ArchitectureBoundariesTest::applicationCoreDoesNotImplementControllerContexts()
{
    const QString corePath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationcore.h");
    QVERIFY2(!QFile::exists(corePath), "ApplicationCore header is deleted");

    QString coreStateSource;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcorestate.cpp"), coreStateSource));
    QVERIFY2(!coreStateSource.contains(QStringLiteral("controllerContexts")),
        "ApplicationCoreState must wire use-case services directly without the deleted adapter bundle");
    QVERIFY2(!coreStateSource.contains(QStringLiteral("setCore(")),
        "SessionService must not retain the old setCore context hook");

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
    const QString corePath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationcore.h");
    QVERIFY2(!QFile::exists(corePath), "ApplicationCore header is deleted — composition is in ApplicationCoreState directly");
}

void ArchitectureBoundariesTest::applicationCoreDoesNotOwnPlatformActions()
{
    const QString coreHeaderPath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationcore.h");
    QVERIFY2(!QFile::exists(coreHeaderPath), "ApplicationCore header is deleted");
    const QString coreSourcePath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationcore.cpp");
    QVERIFY2(!QFile::exists(coreSourcePath), "ApplicationCore source is deleted");

    QString workbenchHeader;
    QVERIFY(readSourceFile(QStringLiteral("src/viewmodels/workbenchviewmodel.h"), workbenchHeader));
    QVERIFY2(workbenchHeader.contains(QStringLiteral("PlatformActions")),
        "WorkbenchViewModel must own workbench platform action integration after deleting WorkbenchWorkspace");

    const QString menuSourcePath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationcoremenus.cpp");
    QVERIFY2(!QFile::exists(menuSourcePath), "Obsolete ApplicationCore menu source file must be removed");
}

void ArchitectureBoundariesTest::applicationCoreDoesNotExposeUnusedScriptSamples()
{
    const QString path = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationcore.h");
    QVERIFY2(!QFile::exists(path), "ApplicationCore header is deleted — script samples owned by ScriptTestSamplesModel");
}

void ArchitectureBoundariesTest::applicationCoreDoesNotImplementWorkbenchPort()
{
    const QString corePath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationcore.h");
    QVERIFY2(!QFile::exists(corePath), "ApplicationCore header is deleted — workbench commands handled by WorkbenchViewModel");

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
    const QString coreHeaderPath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationcore.h");
    QVERIFY2(!QFile::exists(coreHeaderPath), "ApplicationCore header is deleted — UI notifications are controller signals");

    QString coreStateHeader;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcorestate.h"), coreStateHeader));
    QVERIFY2(!coreStateHeader.contains(QStringLiteral("ApplicationCore &")),
        "ApplicationCoreState must not keep ApplicationCore reference member");

    const QStringList deletedPaths {
        QStringLiteral("/src/app/applicationnotifier.h"),
        QStringLiteral("/src/app/applicationnotifier.cpp"),
    };
    for (const QString &path : deletedPaths) {
        QVERIFY2(!QFile::exists(QStringLiteral(MQTT_PLUS_SOURCE_DIR) + path),
            qPrintable(QStringLiteral("Obsolete notifier file must stay removed: %1").arg(path)));
    }
}

void ArchitectureBoundariesTest::applicationCoreDelegatesModelProjection()
{
    QString coreStateHeader;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcorestate.h"), coreStateHeader));
    QVERIFY2(!coreStateHeader.contains(QStringLiteral("ApplicationModelRefresher")),
        "ApplicationCoreState must not delegate model projection via ApplicationModelRefresher — models hold data-source pointers directly");

    const QString coreModelsPath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationcoremodels.cpp");
    QVERIFY2(!QFile::exists(coreModelsPath), "Obsolete ApplicationCore model dependency source file must be removed");

    const QString refresherPath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationmodelrefresher.cpp");
    QVERIFY2(!QFile::exists(refresherPath), "ApplicationModelRefresher must be removed — models hold data-source pointers directly");

    const QString corePath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationcore.h");
    QVERIFY2(!QFile::exists(corePath), "ApplicationCore header is deleted — model projection lives in models");

    QString sessionModel;
    QVERIFY(readSourceFile(QStringLiteral("src/models/sessionlistmodel.cpp"), sessionModel));
    QVERIFY2(sessionModel.contains(QStringLiteral("sessionStateName")),
        "SessionListModel must own session row projection by reading domain data directly");

    QString coreStateSource;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcorestate.cpp"), coreStateSource));
    QVERIFY2(coreStateSource.contains(QStringLiteral("if (!sessionListActivityRefreshTimer.isActive())")),
        "Session-list activity refresh must throttle sustained message traffic instead of debouncing forever");

    const QString refreshCoordPath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationviewrefreshcoordinator.cpp");
    QVERIFY2(!QFile::exists(refreshCoordPath), "ApplicationViewRefreshCoordinator is deleted — controller callbacks call models directly");
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

    const QString coreSourcePath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationcore.cpp");
    QVERIFY2(!QFile::exists(coreSourcePath), "ApplicationCore source is deleted — startup delegated to ApplicationCoreState");

    const QString corePath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationcore.h");
    QVERIFY2(!QFile::exists(corePath), "ApplicationCore header is deleted — persistence delegated to repositories");

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
    const QString coreHeaderPath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationcore.h");
    QVERIFY2(!QFile::exists(coreHeaderPath), "ApplicationCore header is deleted — exit cleanup delegated to ApplicationCoreState");

    QString coreStateSource;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcorestate.cpp"), coreStateSource));
    QVERIFY2(coreStateSource.contains(QStringLiteral("applyExitCleanup")),
        "ApplicationCoreState must own exit cleanup");
    QVERIFY2(coreStateSource.contains(QStringLiteral("flushPendingMessageHistory")),
        "ApplicationCoreState must flush pending history on exit");
}

void ArchitectureBoundariesTest::applicationCoreAppliesMessageRetentionAtLifecycleBoundaries()
{
    QString coreSource;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcorestate.cpp"), coreSource));
    QVERIFY2(coreSource.contains(QStringLiteral("void ApplicationCoreState::applyMessageRetentionLimit()")),
        "ApplicationCoreState must own automatic message retention");
    QVERIFY2(coreSource.contains(QStringLiteral("MessageRetentionLifecycle(historyStore).applyRetention")),
        "ApplicationCoreState must delegate startup retention to the tested lifecycle helper");
    QVERIFY2(coreSource.contains(QStringLiteral("MessageRetentionLifecycle(historyStore).applyExit")),
        "ApplicationCoreState must delegate exit retention and cleanup ordering to the tested lifecycle helper");
    QVERIFY2(coreSource.contains(QStringLiteral("preferencesController.messageRetentionLimit()")),
        "Lifecycle retention must use the configured message limit");
    QVERIFY2(coreSource.contains(QStringLiteral("sessionController.sessions()")),
        "Lifecycle retention must receive every configured connection");
    QVERIFY2(coreSource.contains(QStringLiteral("preferencesController.clearMessagesOnExit()")),
        "Exit lifecycle must preserve the configured message cleanup policy");
    QVERIFY2(coreSource.contains(QStringLiteral("eventController.flushPendingMessageHistory()")),
        "Exit lifecycle must receive the pending-message flush operation");

    QString lifecycleSource;
    QVERIFY(readSourceFile(QStringLiteral("src/app/messageretentionlifecycle.cpp"), lifecycleSource));
    QVERIFY2(lifecycleSource.contains(QStringLiteral("if (limit <= 0)")),
        "Unlimited message retention must skip automatic pruning");
    QVERIFY2(lifecycleSource.contains(QStringLiteral("for (const SessionState &session : sessions)")),
        "Lifecycle retention must cover every configured connection");
    QVERIFY2(lifecycleSource.contains(QStringLiteral("m_historyStore.pruneMessages(session.id, limit)")),
        "Lifecycle retention must prune each configured connection through HistoryStore");

    const qsizetype exitStart = lifecycleSource.indexOf(QStringLiteral("void MessageRetentionLifecycle::applyExit("));
    const qsizetype exitFlush = lifecycleSource.indexOf(QStringLiteral("flushPendingMessages();"), exitStart);
    const qsizetype exitRetention = lifecycleSource.indexOf(QStringLiteral("applyRetention(sessions, limit);"), exitStart);
    const qsizetype exitClear = lifecycleSource.indexOf(QStringLiteral("m_historyStore.clearAllMessages();"), exitStart);
    QVERIFY(exitStart >= 0);
    QVERIFY(exitFlush > exitStart);
    QVERIFY(exitRetention > exitFlush);
    QVERIFY(exitClear > exitRetention);

    const qsizetype startupStart = coreSource.indexOf(QStringLiteral("void ApplicationCoreState::runStartup()"));
    const qsizetype startupLoad = coreSource.indexOf(QStringLiteral("sessionRepository.loadSessions"), startupStart);
    const qsizetype startupRetention = coreSource.indexOf(QStringLiteral("applyMessageRetentionLimit();"), startupStart);
    const qsizetype startupReload = coreSource.indexOf(QStringLiteral("eventController.reloadCurrentSessionHistory();"), startupStart);
    QVERIFY(startupStart >= 0);
    QVERIFY(startupLoad > startupStart);
    QVERIFY(startupRetention > startupLoad);
    QVERIFY(startupReload > startupRetention);
}

void ArchitectureBoundariesTest::applicationCoreDelegatesSignalBindings()
{
    const QString coreSourcePath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationcore.cpp");
    QVERIFY2(!QFile::exists(coreSourcePath), "ApplicationCore source is deleted — signal bindings installed by ApplicationCoreState");

    QString coreStateSource;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcorestate.cpp"), coreStateSource));
    QVERIFY2(coreStateSource.contains(QStringLiteral("ScriptService::storageError")),
        "ApplicationCoreState must bind script storage errors");
    QVERIFY2(coreStateSource.contains(QStringLiteral("QTimer::timeout")),
        "ApplicationCoreState must configure subscription FPS refresh timer");
    QVERIFY2(!QFile::exists(QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationsignalbindings.h")),
        "Deleted ApplicationSignalBindings header must stay removed");
    QVERIFY2(!QFile::exists(QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationsignalbindings.cpp")),
        "Deleted ApplicationSignalBindings source must stay removed");

    const QString refreshCoordPath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationviewrefreshcoordinator.cpp");
    QVERIFY2(!QFile::exists(refreshCoordPath), "ApplicationViewRefreshCoordinator is deleted — language refresh is direct controller-to-ViewModel");
}

void ArchitectureBoundariesTest::applicationCoreRemovesWorkspaceDependencyComposition()
{
    QString coreStateHeader;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcorestate.h"), coreStateHeader));
    QVERIFY2(!coreStateHeader.contains(QStringLiteral("ApplicationWorkspaceDependenciesFactory")),
        "ApplicationCoreState must not keep the deleted workspace dependency factory");

    const QString coreHeaderPath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/app/applicationcore.h");
    QVERIFY2(!QFile::exists(coreHeaderPath), "ApplicationCore header is deleted");

    QString graphHeader;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationobjectgraph.h"), graphHeader));
    QVERIFY2(graphHeader.contains(QStringLiteral("ApplicationViewModel m_viewModel")),
        "ApplicationObjectGraph must own composition directly");

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
    QVERIFY2(graphHeader.contains(QStringLiteral("unique_ptr<ApplicationCoreState> m_state")),
        "ApplicationObjectGraph must own ApplicationCoreState directly");
    QVERIFY2(graphHeader.contains(QStringLiteral("QObject m_owner")),
        "ApplicationObjectGraph must keep a QObject owner for runtime-created QObject children");
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
    QVERIFY2(graphSource.contains(QStringLiteral("std::make_unique<ApplicationCoreState>(&m_owner)")),
        "ApplicationCoreState must receive a real QObject owner so session clients are initialized");
    QVERIFY2(graphSource.contains(QStringLiteral("launchTimestamp = AppUtils::timestampNow()")),
        "ApplicationObjectGraph must initialize the launch timestamp before startup loads history");
    QVERIFY2(graphSource.contains(QStringLiteral("workbenchDependencies(*m_state)")),
        "ApplicationObjectGraph must compose WorkbenchViewModel direct dependencies");
    QVERIFY2(graphSource.contains(QStringLiteral("logsDependencies(*m_state)")),
        "ApplicationObjectGraph must compose LogsViewModel direct dependencies");
    QVERIFY2(graphSource.contains(QStringLiteral("scriptsDependencies(*m_state)")),
        "ApplicationObjectGraph must compose ScriptsViewModel direct dependencies");
    QVERIFY2(graphSource.contains(QStringLiteral("settingsDependencies(*m_state)")),
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

void ArchitectureBoundariesTest::publishStatusUsesTypedRuntimeState()
{
    QString sessionHeader;
    QVERIFY(readSourceFile(QStringLiteral("src/domain/session.h"), sessionHeader));
    QVERIFY2(!sessionHeader.contains(QStringLiteral("QVariantMap publishStatus")),
        "SessionState must not expose publish runtime state as a loose QVariantMap");

    QString runtimeHeader;
    QVERIFY(readSourceFile(QStringLiteral("src/domain/sessionruntime.h"), runtimeHeader));
    QVERIFY2(runtimeHeader.contains(QStringLiteral("PublishStatus publishStatus")),
        "SessionRuntimeState must store publish runtime state as a typed value, not a QVariantMap");
    QVERIFY2(!runtimeHeader.contains(QStringLiteral("QVariantMap publishStatus")),
        "SessionRuntimeState must not expose publish runtime state as a loose QVariantMap");

    QString mqttSource;
    QVERIFY(readSourceFile(QStringLiteral("src/usecases/mqttsessionservice.cpp"), mqttSource));
    QVERIFY2(!mqttSource.contains(QStringLiteral("publishStatus.insert")),
        "MqttSessionService must update publish status through typed fields or helpers");
    QVERIFY2(!mqttSource.contains(QStringLiteral("publishStatus.value")),
        "MqttSessionService must read publish status through typed fields or helpers");
}

void ArchitectureBoundariesTest::sessionRuntimeStateIsSeparatedFromPersistentSessionConfig()
{
    QString sessionHeader;
    QVERIFY(readSourceFile(QStringLiteral("src/domain/session.h"), sessionHeader));
    QVERIFY2(sessionHeader.contains(QStringLiteral("SessionRuntimeState runtime")),
        "SessionState must group runtime-only state under SessionRuntimeState");

    const QStringList runtimeFields {
        QStringLiteral("bool disconnectRequested"),
        QStringLiteral("bool sessionRestored"),
        QStringLiteral("QString lastError"),
        QStringLiteral("QString brokerInfo"),
        QStringLiteral("QHash<QString, int> subscriptionFormats"),
        QStringLiteral("PublishStatus publishStatus"),
        QStringLiteral("QVariantList messageRows"),
        QStringLiteral("QVariantList logRows"),
        QStringLiteral("qint64 oldestLoadedMessageId"),
        QStringLiteral("qint64 oldestLoadedLogId"),
        QStringLiteral("bool loadedAllMessageHistory"),
        QStringLiteral("bool loadedAllLogHistory"),
        QStringLiteral("QMqttClient *client"),
        QStringLiteral("QTimer *connectTimeoutTimer"),
    };
    const int runtimeMemberIndex = sessionHeader.indexOf(QStringLiteral("SessionRuntimeState runtime"));
    QVERIFY(runtimeMemberIndex >= 0);
    const QString persistentSection = sessionHeader.left(runtimeMemberIndex);
    for (const QString &field : runtimeFields) {
        QVERIFY2(!persistentSection.contains(field),
            qPrintable(QStringLiteral("Runtime-only field must not remain in persistent SessionState section: %1").arg(field)));
    }
}

void ArchitectureBoundariesTest::eventHistoryServiceMatchesSubscriptionsInReceivePath()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("src/usecases/eventhistoryservice.cpp"), source));
    QVERIFY2(source.contains(QStringLiteral("MessageSubscriptionMatch")),
        "EventHistoryService should use a local receive-path match result for incoming messages");
    QVERIFY2(!source.contains(QStringLiteral("bestSubscriptionForTopic(*session, topic)")),
        "Incoming message processing must not scan subscriptions once for FPS and again for display selection");
}

void ArchitectureBoundariesTest::eventStreamModelUsesTypedRows()
{
    QString header;
    QVERIFY(readSourceFile(QStringLiteral("src/models/eventstreammodel.h"), header));
    QVERIFY2(header.contains(QStringLiteral("struct EventStreamRow")),
        "EventStreamModel should keep a typed row cache for hot role reads");
    QVERIFY2(!header.contains(QStringLiteral("QVariantList m_rows")),
        "EventStreamModel should not store hot list-model rows as raw QVariantList");
}

void ArchitectureBoundariesTest::eventStreamModelPrependsRowsInBatch()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("src/models/eventstreammodel.cpp"), source));
    QVERIFY2(!source.contains(QStringLiteral("m_rows.prepend")),
        "EventStreamModel::prependRows must insert the incoming batch in one container operation");
}

void ArchitectureBoundariesTest::historyStoreListQueriesDoNotProjectPayloadBlobs()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("src/services/storage/historystore.cpp"), source));

    const int listQueryIndex = source.indexOf(QStringLiteral("QVariantList HistoryStore::loadMessages("));
    const int olderQueryIndex = source.indexOf(QStringLiteral("QVariantList HistoryStore::loadMessagesBefore("));
    const int payloadLookupIndex = source.indexOf(QStringLiteral("QByteArray HistoryStore::loadMessagePayloadBytes("));
    QVERIFY(listQueryIndex >= 0);
    QVERIFY(olderQueryIndex > listQueryIndex);
    QVERIFY(payloadLookupIndex > olderQueryIndex);

    const QString loadMessagesBody = source.mid(listQueryIndex, olderQueryIndex - listQueryIndex);
    const QString loadMessagesBeforeBody = source.mid(olderQueryIndex, payloadLookupIndex - olderQueryIndex);
    QVERIFY2(!loadMessagesBody.contains(QStringLiteral("    payload_bytes")),
        "History message list queries must not project payload_bytes blobs");
    QVERIFY2(!loadMessagesBeforeBody.contains(QStringLiteral("    payload_bytes")),
        "Older message list queries must not project payload_bytes blobs");
}

void ArchitectureBoundariesTest::eventHistoryServiceDefersRetentionPruneToLifecycle()
{
    QString header;
    QVERIFY(readSourceFile(QStringLiteral("src/usecases/eventhistoryservice.h"), header));
    QVERIFY2(!header.contains(QStringLiteral("m_messageRetentionPruneFlushCounts")),
        "EventHistoryService must not track a runtime retention-prune cadence");

    QString source;
    QVERIFY(readSourceFile(QStringLiteral("src/usecases/eventhistoryservice.cpp"), source));
    const qsizetype flushStart = source.indexOf(QStringLiteral("void EventHistoryService::flushPendingMessageHistory()"));
    const qsizetype reportStart = source.indexOf(QStringLiteral("void EventHistoryService::reportMessageStorageError"), flushStart);
    QVERIFY(flushStart >= 0);
    QVERIFY(reportStart > flushStart);
    const QString flushBody = source.mid(flushStart, reportStart - flushStart);
    QVERIFY2(!flushBody.contains(QStringLiteral("pruneMessages")),
        "Runtime message flushes must persist without enforcing retention");
    QVERIFY2(!source.contains(QStringLiteral("shouldPruneMessageHistory")),
        "Runtime retention cadence helper must be removed");
}

void ArchitectureBoundariesTest::messageQmlUsesTypedObjectProperties()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/EventStreamView.qml"), source));
    QVERIFY2(source.contains(QStringLiteral("required property QtObject streamModel")),
        "EventStreamView should type its model dependency as a QObject instead of a dynamic var");
    QVERIFY2(!source.contains(QStringLiteral("required property var streamModel")),
        "EventStreamView should not keep the message model dependency as a dynamic var");
    QVERIFY2(!source.contains(QStringLiteral("required property int historyId")),
        "EventStreamView should not narrow qint64 history ids to a 32-bit QML int");
    QVERIFY2(source.contains(QStringLiteral("required property string historyId")),
        "EventStreamView should carry history ids across the QML boundary without 32-bit narrowing");
}

void ArchitectureBoundariesTest::messageRowsUseHoverHandlerForNestedControls()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/EventStreamView.qml"), source));

    const int messageRowIndex = source.indexOf(QStringLiteral("id: messageRow"));
    QVERIFY(messageRowIndex >= 0);
    const int messageActionsIndex = source.indexOf(QStringLiteral("id: messageActions"), messageRowIndex);
    QVERIFY(messageActionsIndex > messageRowIndex);
    const QString messageRowSource = source.mid(messageRowIndex, messageActionsIndex - messageRowIndex);

    QVERIFY2(messageRowSource.contains(QStringLiteral("id: rowHover")),
        "Message row hover state must come from a row-level HoverHandler so nested text controls and action buttons keep the row highlighted");
    QVERIFY2(messageRowSource.contains(QStringLiteral("rowHover.hovered ?")),
        "Message row background must bind to the HoverHandler hover state");
    QVERIFY2(!messageRowSource.contains(QStringLiteral("rowMouse.containsMouse")),
        "MouseArea.containsMouse is lost when hovering nested controls inside the message row");
}

void ArchitectureBoundariesTest::messageRowsUseButtonTapPolicy()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/EventStreamView.qml"), source));

    const int messageRowIndex = source.indexOf(QStringLiteral("id: messageRow"));
    QVERIFY(messageRowIndex >= 0);
    const int rowBodyIndex = source.indexOf(QStringLiteral("id: rowBody"), messageRowIndex);
    QVERIFY(rowBodyIndex > messageRowIndex);
    const QString messageRowSource = source.mid(messageRowIndex, rowBodyIndex - messageRowIndex);

    QVERIFY2(messageRowSource.contains(QStringLiteral("gesturePolicy: TapHandler.ReleaseWithinBounds")),
        "Message rows should use button-style release handling so a slight pointer movement does not discard the first click");
}

void ArchitectureBoundariesTest::messageRowsDoNotNestPointerHandlingTextEdit()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/EventStreamView.qml"), source));

    const int messageRowIndex = source.indexOf(QStringLiteral("id: messageRow"));
    QVERIFY(messageRowIndex >= 0);
    const int messageActionsIndex = source.indexOf(QStringLiteral("id: messageActions"), messageRowIndex);
    QVERIFY(messageActionsIndex > messageRowIndex);
    const QString messageRowSource = source.mid(messageRowIndex, messageActionsIndex - messageRowIndex);

    QVERIFY2(!messageRowSource.contains(QStringLiteral("TextEdit {")),
        "A selectable TextEdit inside a message row competes with the row TapHandler and makes switching messages depend on the click target");
}

void ArchitectureBoundariesTest::messagePanelUsesSplitViewForComposerResize()
{
    QString panelSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/SessionMessagePanel.qml"), panelSource));
    QVERIFY2(panelSource.contains(QStringLiteral("SplitView {")),
        "SessionMessagePanel should use SplitView between the message stream and publisher");
    QVERIFY2(panelSource.contains(QStringLiteral("orientation: Qt.Vertical")),
        "SessionMessagePanel should split the message stream and publisher vertically");
    QVERIFY2(panelSource.contains(QStringLiteral("SplitView.fillHeight: true")),
        "EventStreamView should consume the flexible side of the vertical split");

    QString composerSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/PublishComposer.qml"), composerSource));
    QVERIFY2(composerSource.contains(QStringLiteral("SplitView.preferredHeight")),
        "PublishComposer should expose SplitView sizing instead of custom drag math");
    QVERIFY2(!composerSource.contains(QStringLiteral("resizeComposerFromDrag")),
        "PublishComposer should not keep a custom drag resize implementation");
    QVERIFY2(!composerSource.contains(QStringLiteral("MouseArea")),
        "PublishComposer should not keep a custom splitter MouseArea");
}

void ArchitectureBoundariesTest::eventStreamFollowModeUsesSingleCycleButton()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/EventStreamView.qml"), source));

    QVERIFY2(source.contains(QStringLiteral("id: followModeButton")),
        "Follow mode should be represented by one toolbar button");
    QVERIFY2(source.contains(QStringLiteral("iconSource: root.ui.materialIcon(\"follow-mode\")")),
        "Follow mode button should use one shared follow icon");
    QVERIFY2(source.contains(QStringLiteral("forceActive: eventList.shouldFollowOutput")),
        "Follow mode button should visually keep the pressed state while smart follow is active");
    QVERIFY2(source.contains(QStringLiteral("onClicked: root.setFollowMode(\"smart\")")),
        "Follow mode button should restore following and scroll to the latest message");
    QVERIFY2(source.contains(QStringLiteral("root.viewModel.setMessageStreamFrozen(true)")),
        "Manual mode should freeze live mutations of the rendered message model");
    QVERIFY2(source.contains(QStringLiteral("root.viewModel.setMessageStreamFrozen(false)")),
        "Smart mode should synchronize and resume the rendered message model");
    QVERIFY2(source.contains(QStringLiteral("allowResume && eventList.userScrollActive")),
        "Only an active user scroll should automatically resume at the snapshot bottom");
    QVERIFY2(source.contains(QStringLiteral("const atBottom = count === 0 || atYEnd")),
        "Moving away from the exact bottom should stop following without a distance threshold");
    QVERIFY2(!source.contains(QStringLiteral("distanceFromBottom <= 24")),
        "Message stream following should not retain the old 24-pixel pause threshold");
    QVERIFY2(source.contains(
                 QStringLiteral("PointerDevice.Mouse | PointerDevice.TouchPad")),
        "Wheel handling should cover both mouse wheels and touchpads");
    QVERIFY2(source.contains(QStringLiteral("root.viewModel.totalMessageCount")),
        "The message badge should use the session total instead of the capped visible model count");
    QVERIFY2(!source.contains(QStringLiteral("checkable: true")),
        "Follow mode button should not let the toolbar disable following directly");
    QVERIFY2(source.contains(QStringLiteral("eventList.shouldFollowOutput = false")),
        "Scrolling away or selecting a row must clear the actual follow state");
    QVERIFY2(source.contains(QStringLiteral("eventList.shouldFollowOutput = true")),
        "Scrolling to latest or enabling smart follow must restore the actual follow state");
    QVERIFY2(!source.contains(QStringLiteral("function nextFollowMode(mode)")),
        "The reference interaction no longer exposes a follow-mode cycle");
    QVERIFY2(!source.contains(QStringLiteral("mode === \"always\"")),
        "Follow mode should no longer expose the removed always mode");
    QVERIFY2(!source.contains(QStringLiteral("follow-always")),
        "Follow mode should no longer reference an always icon");
    QVERIFY2(!source.contains(QStringLiteral("follow-smart")),
        "Follow mode should no longer use a separate smart icon");
    QVERIFY2(!source.contains(QStringLiteral("follow-manual")),
        "Follow mode should no longer use a separate manual icon");
    QVERIFY2(!source.contains(QStringLiteral("model: [\"smart\", \"always\", \"manual\"]")),
        "Follow mode should no longer render a three-option segmented control");
    QVERIFY2(!source.contains(QStringLiteral("id: followModeActions")),
        "Follow mode cycling should not keep the old menu/action model");
}

void ArchitectureBoundariesTest::qmlUsesApplicationViewModelRootOnly()
{
    const QStringList qmlFiles {
        QStringLiteral("qml/Main.qml"),
        QStringLiteral("qml/features/workbench/WorkbenchView.qml"),
        QStringLiteral("qml/features/workbench/SessionSidebar.qml"),
        QStringLiteral("qml/features/workbench/SessionOverviewPanel.qml"),
        QStringLiteral("qml/features/workbench/SubscriptionsPanel.qml"),
        QStringLiteral("qml/features/workbench/SessionMessagePanel.qml"),
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

void ArchitectureBoundariesTest::applicationUsesSystemFixedFont()
{
    QString mainSource;
    QVERIFY(readSourceFile(QStringLiteral("src/app/main.cpp"), mainSource));
    QVERIFY2(mainSource.contains(
                 QStringLiteral("QFontDatabase::systemFont(QFontDatabase::FixedFont)")),
        "The application must use the platform fixed-width font globally");

    QDirIterator qmlFiles(
        QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/qml"),
        {QStringLiteral("*.qml")},
        QDir::Files,
        QDirIterator::Subdirectories);
    while (qmlFiles.hasNext()) {
        QFile file(qmlFiles.next());
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text),
            qPrintable(QStringLiteral("Cannot read %1").arg(file.fileName())));
        const QString source = QString::fromUtf8(file.readAll());
        QVERIFY2(!source.contains(QStringLiteral("\"Menlo\"")),
            qPrintable(QStringLiteral("%1 must inherit the platform fixed-width font")
                           .arg(file.fileName())));
    }
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

void ArchitectureBoundariesTest::settingsExplainDeferredMessageRetention()
{
    QString qml;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/settings/SettingsView.qml"), qml));
    QVERIFY2(qml.contains(QStringLiteral(
        "Maximum MQTT messages kept per connection. Cleanup runs when the app starts or exits.")),
        "Saved-message settings must explain deferred lifecycle cleanup");
    QVERIFY2(qml.contains(QStringLiteral("Clear stored data immediately.")),
        "Manual cleanup must continue to promise immediate deletion");

    QString translations;
    QVERIFY(readSourceFile(QStringLiteral("i18n/mqtt_plus_zh_CN.ts"), translations));
    QVERIFY2(translations.contains(QStringLiteral(
        "每个连接最多保留的 MQTT 消息数；应用将在启动或退出时执行清理。")),
        "Simplified Chinese settings copy must explain deferred lifecycle cleanup");
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

void ArchitectureBoundariesTest::workbenchMiddlePaneUsesCompactHeaderControls()
{
    QString overviewSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/SessionOverviewPanel.qml"), overviewSource));
    QVERIFY2(overviewSource.contains(QStringLiteral("Layout.preferredHeight: 96")),
        "Session overview must leave the subscription list as the primary content area");
    QVERIFY2(overviewSource.contains(QStringLiteral("id: statusDot")),
        "Session overview must expose connection state through a compact color dot");
    QVERIFY2(overviewSource.contains(QStringLiteral("statusToolTipText")),
        "Session overview must keep connection details available in a tooltip");
    QVERIFY2(overviewSource.contains(QStringLiteral("Accessible.name: control.statusToolTipText")),
        "The color-only connection state must have an accessible name");
    QVERIFY2(!overviewSource.contains(QStringLiteral("\"label\": qsTr(\"Status\")")),
        "Session overview must not render a visible connection-state label");

    QString subscriptionsSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/SubscriptionsPanel.qml"), subscriptionsSource));
    QVERIFY2(!subscriptionsSource.contains(QStringLiteral("filterModeLabels")),
        "Subscription toolbar must not expose status filter labels");
    QVERIFY2(!subscriptionsSource.contains(QStringLiteral("AppComboBox")),
        "Subscription toolbar must not expose a status filter combo box");
    QVERIFY2(!subscriptionsSource.contains(QStringLiteral("text: qsTr(\"Subscriptions\")")),
        "Subscription toolbar must not render a redundant title");
    QVERIFY2(subscriptionsSource.contains(QStringLiteral("property: \"filterModeIndex\"")),
        "Subscription panel must lock the hidden status filter to All");
    QVERIFY2(subscriptionsSource.contains(QStringLiteral("Layout.preferredHeight: 46")),
        "Subscription search and add action must share one compact toolbar row");
    QVERIFY2(subscriptionsSource.contains(QStringLiteral("readonly property bool activeTraffic")),
        "Subscription rows must distinguish recent traffic from idle subscriptions");
    QVERIFY2(subscriptionsSource.contains(QStringLiteral("implicitHeight: subscriptionDelegate.hasError ? 60 : 46")),
        "Subscription rows must use the compact management height");
    QVERIFY2(!subscriptionsSource.contains(QStringLiteral("MultiEffect")),
        "Compact subscription rows must remain flat instead of rendering card shadows");
    QVERIFY2(subscriptionsSource.contains(QStringLiteral("readonly property string rateText")),
        "Subscription rows must keep a stable live-rate value");
    QVERIFY2(!subscriptionsSource.contains(QStringLiteral("required property int requestedQos")),
        "Subscription rows must not consume QoS for permanent display");
    QVERIFY2(!subscriptionsSource.contains(QStringLiteral("required property string formatName")),
        "Subscription rows must not duplicate payload format from the message stream");
    QVERIFY2(!subscriptionsSource.contains(QStringLiteral("required property string scriptName")),
        "Subscription rows must not render script metadata as a badge");
    QVERIFY2(!subscriptionsSource.contains(QStringLiteral("SequentialAnimation")),
        "Subscription activity feedback must not use a looping breathing animation");
}

void ArchitectureBoundariesTest::subscriptionRowsKeepCompactActionGroup()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/SubscriptionsPanel.qml"), source));
    QVERIFY2(source.contains(QStringLiteral("id: subscriptionActionGroup")),
        "Subscription rate and row actions must share one layout group");
    const qsizetype rateMetricStart = source.indexOf(QStringLiteral("id: rateMetric"));
    const qsizetype pauseButtonStart = source.indexOf(
        QStringLiteral("id: subscriptionPauseButton"),
        rateMetricStart);
    QVERIFY(rateMetricStart >= 0);
    QVERIFY(pauseButtonStart > rateMetricStart);
    const QString rateMetricSource = source.mid(rateMetricStart, pauseButtonStart - rateMetricStart);
    QVERIFY2(rateMetricSource.contains(QStringLiteral("Layout.preferredWidth: 52"))
            && rateMetricSource.contains(QStringLiteral("Layout.minimumWidth: 52"))
            && rateMetricSource.contains(QStringLiteral("Layout.maximumWidth: 52")),
        "Subscription rate and trend must share one compact fixed-width metric");
    QVERIFY2(source.contains(QStringLiteral("spacing: 2")),
        "Subscription action group must use compact spacing");
    QVERIFY2(source.contains(QStringLiteral("readonly property bool subscriptionActive: !subscriptionDelegate.paused")),
        "The subscription row should derive its selected state from the active subscription state");
    QVERIFY2(source.contains(QStringLiteral("color: subscriptionDelegate.subscriptionActive")),
        "An active subscription row should show the selected container treatment");
    QVERIFY2(source.contains(QStringLiteral("subscriptionDelegate.subscriptionActive ? 1 : 0")),
        "A paused subscription row should not keep the selected container outline");
}

void ArchitectureBoundariesTest::workbenchUsesReferenceMessageWorkspace()
{
    QString workbenchSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/WorkbenchView.qml"), workbenchSource));
    QVERIFY(workbenchSource.contains(QStringLiteral("expandedConnectionPaneWidth: 208")));
    QVERIFY(workbenchSource.contains(QStringLiteral("subscriptionPaneWidth: root.settingsViewModel.subscriptionPaneWidth")));
    QVERIFY(workbenchSource.contains(QStringLiteral("saveWorkbenchLayout")));
    QVERIFY(workbenchSource.contains(QStringLiteral("function persistLayout()")));

    QString mainSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/Main.qml"), mainSource));
    QVERIFY2(mainSource.contains(QStringLiteral("workbenchPage.persistLayout();")),
        "Window shutdown must flush pending workbench layout changes");

    QString subscriptionsSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/SubscriptionsPanel.qml"), subscriptionsSource));
    QVERIFY(subscriptionsSource.contains(QStringLiteral("signal replaceMessageTopicFilter(string topic)")));
    QVERIFY(subscriptionsSource.contains(QStringLiteral("setAllCurrentSubscriptionsPaused")));
    QVERIFY2(subscriptionsSource.contains(QStringLiteral("topicRateHistory")),
        "Subscription rows must expose a compact recent-rate sparkline");

    QString sessionSidebarSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/SessionSidebar.qml"), sessionSidebarSource));
    QVERIFY2(sessionSidebarSource.contains(QStringLiteral("qsTr(\"Connection error · See Logs\")"))
            && sessionSidebarSource.contains(QStringLiteral("Accessible.description: sessionDelegate.endpointText")),
        "Connection rows must direct users to Logs instead of truncating technical errors");
    QVERIFY2(!sessionSidebarSource.contains(
                 QStringLiteral("sessionDelegate.lastError.length > 0 ? sessionDelegate.lastError")),
        "Connection rows must not render raw technical errors in the compact sidebar");

    QString panelSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/SessionMessagePanel.qml"), panelSource));
    QVERIFY(panelSource.contains(QStringLiteral("MessageInspector")));
    QVERIFY(panelSource.contains(QStringLiteral("streamModel: root.viewModel.filteredMessages")));

    QString streamSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/EventStreamView.qml"), streamSource));
    QVERIFY(streamSource.contains(QStringLiteral("MessageFilterPopover")));
    QVERIFY(streamSource.contains(QStringLiteral("filteredMessageCount")));
    QVERIFY(streamSource.contains(QStringLiteral("totalMessageCount")));
    QVERIFY(streamSource.contains(QStringLiteral("filterSummaryText")));
    QVERIFY(streamSource.contains(QStringLiteral("Accessible.role: Accessible.Button")));
    QVERIFY(streamSource.contains(QStringLiteral("Keys.onPressed")));
    QVERIFY(streamSource.contains(QStringLiteral("streamActionsMenu.openForItem(streamActionsButton)")));
    QVERIFY(streamSource.contains(QStringLiteral("accessibleName: qsTr(\"More message actions\")")));
    QVERIFY2(streamSource.contains(QStringLiteral("AppEmptyState {")),
        "The message workspace must guide users when no rows are visible");
    QVERIFY2(streamSource.contains(QStringLiteral("id: clearMessagesDialog")),
        "Clearing message history must require confirmation");
    QVERIFY2(streamSource.contains(QStringLiteral("? 12"))
            && streamSource.contains(QStringLiteral(": 3")),
        "Large payloads must stay clamped until the row is selected or inspected");
    QVERIFY2(streamSource.contains(QStringLiteral("required property int payloadDisplayMode"))
            && streamSource.contains(QStringLiteral("? 2147483647")),
        "Message payload clamping must support the persisted full-content display mode");
    QVERIFY2(streamSource.contains(QStringLiteral("selectAdjacentMessage"))
            && streamSource.contains(QStringLiteral("Qt.Key_Up"))
            && streamSource.contains(QStringLiteral("Qt.Key_Down")),
        "Message rows must support keyboard navigation");
    QVERIFY2(streamSource.contains(QStringLiteral("Math.max(metadataRow.implicitWidth,")),
        "Message-row quick actions must participate in width calculation");
    const int metadataStart = streamSource.indexOf(QStringLiteral("id: messageActions"));
    const int metadataEnd = streamSource.indexOf(QStringLiteral("id: followButton"), metadataStart);
    QVERIFY(metadataStart >= 0 && metadataEnd > metadataStart);
    QVERIFY2(!streamSource.mid(metadataStart, metadataEnd - metadataStart).contains(QStringLiteral("AppBadge {")),
        "Message format metadata should remain plain text like the reference row");

    QString composerSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/PublishComposer.qml"), composerSource));
    QVERIFY2(composerSource.contains(QStringLiteral("recentPublishes"))
            && composerSource.contains(QStringLiteral("Publish again")),
        "The composer must expose reusable publish history and one-click republish");
    QVERIFY2(composerSource.contains(QStringLiteral("id: publishPulseTimer"))
            && composerSource.contains(QStringLiteral("interval: 300")),
        "Publish completion must provide short-lived button feedback");

    QVERIFY2(workbenchSource.contains(QStringLiteral("id: workbenchStatusBar"))
            && workbenchSource.contains(QStringLiteral("currentIncomingMessageRate"))
            && workbenchSource.contains(QStringLiteral("currentOutgoingMessageRate")),
        "The workbench must expose aggregate traffic and history state");
    QVERIFY2(workbenchSource.contains(QStringLiteral("text: root.liveConnectionStatusText"))
            && workbenchSource.contains(QStringLiteral("connectedAtMs"))
            && workbenchSource.contains(QStringLiteral("connectionStartedAtMs")),
        "The bottom status bar must include live connection duration or timeout context");

    QString filterSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/MessageFilterPopover.qml"), filterSource));
    QVERIFY(filterSource.contains(QStringLiteral("receiveStateText")));
    QVERIFY2(filterSource.contains(QStringLiteral("delegate: AppCheckBox")),
        "Topic options should use the compact application checkbox instead of the platform default");

    QVERIFY(workbenchSource.contains(QStringLiteral("connectionPaneAutoHidden")));
    QVERIFY(workbenchSource.contains(QStringLiteral("subscriptionPaneAutoHidden")));
    QVERIFY2(!workbenchSource.contains(QStringLiteral("subscriptionPaneCollapsed"))
            && !workbenchSource.contains(QStringLiteral("Show subscription list")),
        "The subscription pane must not expose a manual collapsed state or restore action");

    QVERIFY2(!subscriptionsSource.contains(QStringLiteral("signal collapseRequested"))
            && !subscriptionsSource.contains(QStringLiteral("Hide subscription list")),
        "The subscription panel must not expose a manual collapse action");

    QVERIFY(panelSource.contains(QStringLiteral("function closeInspector()")));
    QVERIFY(panelSource.contains(QStringLiteral("selectedMessageHistoryId = \"\"")));

    const QString inspectorPath = QStringLiteral(MQTT_PLUS_SOURCE_DIR)
        + QStringLiteral("/qml/features/workbench/MessageInspector.qml");
    QVERIFY(QFile::exists(inspectorPath));

    QString inspectorSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/MessageInspector.qml"), inspectorSource));
    QVERIFY(inspectorSource.contains(QStringLiteral("qsTr(\"Message Viewer\")")));
    QVERIFY(inspectorSource.contains(QStringLiteral("id: metadataSeparator1")));
    QVERIFY(inspectorSource.contains(QStringLiteral("component InspectorActionButton: AppButton")));
    QVERIFY2(!inspectorSource.contains(QStringLiteral("primary: true")),
        "Inspector actions should use the same neutral bordered weight as the reference");
}

void ArchitectureBoundariesTest::messageInspectorPreservesPayloadFormatting()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/MessageInspector.qml"), source));

    QVERIFY2(source.count(QStringLiteral("textFormat: TextEdit.PlainText")) >= 2,
        "Inspector payload fields must render message data as literal text so embedded markup and newlines are preserved");
    QVERIFY2(source.contains(QStringLiteral("Layout.preferredHeight: Math.max(40, payloadBodyText.contentHeight + 20)")),
        "The payload container must grow to its wrapped content height instead of clipping multiline data");
    QVERIFY2(source.contains(QStringLiteral("Layout.preferredHeight: Math.max(40, parsedResultText.contentHeight + 20)")),
        "The parsed-result container must grow to its wrapped content height instead of clipping multiline data");
    QVERIFY2(source.contains(QStringLiteral("contentWidth: availableWidth")),
        "The inspector scroller must constrain content to its viewport so long payload lines can wrap");
    QVERIFY2(source.contains(QStringLiteral("width: inspectorScroll.availableWidth")),
        "The inspector content column must use the available viewport width for deterministic wrapping");

    const int payloadSection = source.indexOf(QStringLiteral("qsTr(\"Payload preview\")"));
    const int parsedSection = source.indexOf(QStringLiteral("qsTr(\"Parsed result\")"));
    QVERIFY(payloadSection >= 0);
    QVERIFY(parsedSection >= 0);
    QVERIFY2(payloadSection < parsedSection,
        "The original Payload section must stay first, with optional script output displayed below it");
}

void ArchitectureBoundariesTest::closingMessageInspectorClearsRowSelection()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/EventStreamView.qml"), source));

    const int clearSelection = source.indexOf(QStringLiteral("function clearMessageSelection()"));
    const int nextFunction = source.indexOf(QStringLiteral("function requestFollowScroll()"), clearSelection);
    QVERIFY(clearSelection >= 0);
    QVERIFY(nextFunction > clearSelection);
    const QString clearSelectionSource = source.mid(clearSelection, nextFunction - clearSelection);

    QVERIFY2(clearSelectionSource.contains(QStringLiteral("root.selectedHistoryId = \"\"")),
        "Closing the inspector must clear the selected message id");
    QVERIFY2(!clearSelectionSource.contains(QStringLiteral("forceActiveFocus()")),
        "Closing the inspector must clear selection without moving component focus");
    QVERIFY2(!source.contains(QStringLiteral("selectedMessageTrigger")),
        "The closed inspector must not retain or refocus the previously selected delegate");
}

void ArchitectureBoundariesTest::messageInspectorUsesLeftEdgeShadow()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/MessageInspector.qml"), source));

    QVERIFY2(source.contains(QStringLiteral("import QtQuick.Effects")),
        "The inspector should use the existing Qt Quick effect stack for elevation");
    QVERIFY2(source.contains(QStringLiteral("layer.enabled: control.visible")),
        "The inspector shadow layer should only be active while the panel is visible");
    QVERIFY2(source.contains(QStringLiteral("shadowEnabled: true")),
        "The inspector should render an elevation shadow");
    QVERIFY2(source.contains(QStringLiteral("shadowHorizontalOffset: -8")),
        "The inspector shadow should project toward the message list on its left");
}

void ArchitectureBoundariesTest::qmlUsesNativeFocusManagement()
{
    const QStringList forbiddenFocusTokens {
        QStringLiteral("showFocusIndicators"),
        QStringLiteral("focusIndicatorVisible"),
        QStringLiteral("focusRingColor"),
        QStringLiteral("focusRingWidth"),
        QStringLiteral("fieldFocusBorder"),
    };

    const QString qmlRoot = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/qml");
    QDirIterator qmlFiles(qmlRoot, { QStringLiteral("*.qml") }, QDir::Files, QDirIterator::Subdirectories);
    while (qmlFiles.hasNext()) {
        const QString path = qmlFiles.next();
        QFile file(path);
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(QStringLiteral("Cannot read %1").arg(path)));
        const QString source = QString::fromUtf8(file.readAll());
        for (const QString &token : forbiddenFocusTokens) {
            QVERIFY2(!source.contains(token),
                qPrintable(QStringLiteral("%1 still contains custom focus-management token '%2'").arg(path, token)));
        }
    }

    QString streamSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/EventStreamView.qml"), streamSource));
    QVERIFY2(streamSource.contains(QStringLiteral("activeFocusOnTab: eventDelegate.isMessage")),
        "Message rows must participate in native keyboard focus traversal");

    QString subscriptionSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/SubscriptionsPanel.qml"), subscriptionSource));
    QVERIFY2(subscriptionSource.contains(QStringLiteral("activeFocusOnTab: true")),
        "Subscription rows must participate in native keyboard focus traversal");
}

void ArchitectureBoundariesTest::qmlMenusAreApplicationRendered()
{
    QString menuSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/components/AppMenu.qml"), menuSource));
    QVERIFY2(menuSource.contains(QStringLiteral("import QtQuick.Controls.Basic")),
        "Application menus must use the stable Basic control style");
    QVERIFY2(menuSource.contains(QStringLiteral("popupType: Popup.Item")),
        "Application menus must stay inside the QML scene instead of using native popups");
    QVERIFY2(menuSource.contains(QStringLiteral("function openAtPoint")),
        "Context menus must preserve the pointer position");
    QVERIFY2(menuSource.contains(QStringLiteral("function openForItem")),
        "More menus must anchor to their trigger item");
    QVERIFY2(!menuSource.contains(QStringLiteral("Popup.Native")),
        "Application menus must never opt into native rendering");

    const QString removedMenuPath = QStringLiteral(MQTT_PLUS_SOURCE_DIR)
        + QStringLiteral("/qml/components/AppPlatformMenu.qml");
    QVERIFY2(!QFile::exists(removedMenuPath), "The native platform menu wrapper must stay removed");

    const QString qmlRoot = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/qml");
    QDirIterator qmlFiles(qmlRoot, { QStringLiteral("*.qml") }, QDir::Files, QDirIterator::Subdirectories);
    while (qmlFiles.hasNext()) {
        const QString path = qmlFiles.next();
        QFile file(path);
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(QStringLiteral("Cannot read %1").arg(path)));
        const QString source = QString::fromUtf8(file.readAll());
        QVERIFY2(!source.contains(QStringLiteral("Qt.labs.platform")),
            qPrintable(QStringLiteral("%1 must not import native platform menus").arg(path)));
        QVERIFY2(!source.contains(QStringLiteral("Platform.Menu")),
            qPrintable(QStringLiteral("%1 must not create native platform menus").arg(path)));
        QVERIFY2(!source.contains(QStringLiteral("AppPlatformMenu")),
            qPrintable(QStringLiteral("%1 must use AppMenu instead of the removed native wrapper").arg(path)));
    }
}

void ArchitectureBoundariesTest::textEditorsUseNativeContextMenus()
{
    QString nativeMenuSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/components/AppNativeTextMenu.qml"), nativeMenuSource));
    QVERIFY2(!nativeMenuSource.contains(QStringLiteral("QtQuick.Controls.impl")),
        "The native text menu must use only Qt's public QML API");
    QVERIFY2(nativeMenuSource.contains(QStringLiteral("required property var editor")),
        "The native text menu must target the text editor that opened it");
    QVERIFY2(nativeMenuSource.contains(QStringLiteral("popupType: Popup.Native")),
        "Text-editing context menus must be rendered by the platform");

    const QStringList expectedShortcuts {
        QStringLiteral("StandardKey.Undo"),
        QStringLiteral("StandardKey.Redo"),
        QStringLiteral("StandardKey.Cut"),
        QStringLiteral("StandardKey.Copy"),
        QStringLiteral("StandardKey.Paste"),
        QStringLiteral("StandardKey.Delete"),
        QStringLiteral("StandardKey.SelectAll"),
    };
    QCOMPARE(nativeMenuSource.count(QStringLiteral("Action {")), expectedShortcuts.size());
    for (const QString &shortcut : expectedShortcuts) {
        QVERIFY2(nativeMenuSource.contains(QStringLiteral("shortcut: ") + shortcut),
            qPrintable(QStringLiteral("AppNativeTextMenu must expose %1").arg(shortcut)));
    }

    const QMap<QString, QString> sharedEditors {
        {
            QStringLiteral("qml/components/AppTextField.qml"),
            QStringLiteral("editor: control"),
        },
        {
            QStringLiteral("qml/components/AppTextArea.qml"),
            QStringLiteral("editor: textArea"),
        },
    };
    for (auto it = sharedEditors.cbegin(); it != sharedEditors.cend(); ++it) {
        QString source;
        QVERIFY2(readSourceFile(it.key(), source), qPrintable(QStringLiteral("Cannot read %1").arg(it.key())));
        QCOMPARE(source.count(QStringLiteral("ContextMenu.menu: AppNativeTextMenu {")), 1);
        QVERIFY2(source.contains(it.value()),
            qPrintable(QStringLiteral("%1 must bind its native menu to the wrapped editor").arg(it.key())));
        QVERIFY2(!source.contains(QStringLiteral("ContextMenu.onRequested:")),
            qPrintable(QStringLiteral("%1 must not change popup type while a context request is opening").arg(it.key())));
    }

    QString inspectorSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/MessageInspector.qml"), inspectorSource));
    QCOMPARE(inspectorSource.count(QStringLiteral("TextEdit {")), 2);
    QCOMPARE(inspectorSource.count(QStringLiteral("ContextMenu.menu: AppNativeTextMenu {")), 2);
    QVERIFY2(inspectorSource.contains(QStringLiteral("editor: payloadBodyText")),
        "The payload text editor must use the native text context menu");
    QVERIFY2(inspectorSource.contains(QStringLiteral("editor: parsedResultText")),
        "The parsed-result text editor must use the native text context menu");
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
            qPrintable(QStringLiteral("%1 must keep subscription menu presentation local to QML").arg(path)));
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
            QStringLiteral("qml/features/workbench/SessionMessagePanel.qml"),
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

void ArchitectureBoundariesTest::eventStreamViewUsesLocalFollowScrollState()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/EventStreamView.qml"), source));

    QVERIFY2(source.contains(QStringLiteral("property string followMode: \"smart\"")),
        "EventStreamView must keep the non-persistent follow mode local to the page");
    QVERIFY2(source.contains(QStringLiteral("property bool bottomAnchorActive")),
        "EventStreamView must track whether model changes should keep the list anchored to the bottom");
    QVERIFY2(source.contains(QStringLiteral("property bool followScrollQueued")),
        "EventStreamView must coalesce follow-scroll requests");
    QVERIFY2(source.contains(QStringLiteral("onContentHeightChanged")),
        "EventStreamView must re-check the bottom anchor after large delegates settle");
    QVERIFY2(!source.contains(QStringLiteral("followScrollTimer")),
        "EventStreamView must not add another 16ms follow-scroll timer on top of the model flush timer");
    QVERIFY2(!source.contains(QStringLiteral("forceLayout()")),
        "EventStreamView must not force synchronous ListView layout to follow new messages");
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
    QVERIFY2(header.contains(QStringLiteral("Q_PROPERTY(PublishDraftViewModel* publisher")),
        "WorkbenchViewModel must expose a dedicated publish draft ViewModel");
    QVERIFY2(header.contains(QStringLiteral("SessionService *sessionController")),
        "WorkbenchViewModel must receive session controller directly");
    QVERIFY2(header.contains(QStringLiteral("MqttSessionService *mqttController")),
        "WorkbenchViewModel must receive MQTT controller directly");
    QVERIFY2(header.contains(QStringLiteral("SubscriptionService *subscriptionController")),
        "WorkbenchViewModel must receive subscription controller directly");
    QVERIFY2(header.contains(QStringLiteral("EventHistoryService *eventController")),
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
    QVERIFY2(source.contains(QStringLiteral("m_themeMode = sanitizeThemeMode")),
        "SettingsViewModel must own theme mode logic directly");
    QVERIFY2(source.contains(QStringLiteral("m_languageMode = sanitizeLanguageMode")),
        "SettingsViewModel must own language mode logic directly");
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
    QVERIFY2(source.contains(QStringLiteral("m_settings(settingsDependencies, settings, this)")),
        "ApplicationViewModel must wire SettingsViewModel from direct dependencies with QSettings");

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
        QStringLiteral("Q_INVOKABLE void handleSessionContextMenu"),
        QStringLiteral("Q_INVOKABLE QString showSessionContextMenu"),
        QStringLiteral("Q_INVOKABLE QString showSubscriptionContextMenu"),
        QStringLiteral("Q_INVOKABLE void handleSubscriptionContextMenu"),
        QStringLiteral("Q_INVOKABLE void connectCurrentSession"),
        QStringLiteral("Q_INVOKABLE void disconnectCurrentSession"),
        QStringLiteral("Q_INVOKABLE void setCurrentOutputPaused"),
        QStringLiteral("Q_INVOKABLE void removeCurrentSubscription"),
        QStringLiteral("Q_INVOKABLE void setCurrentSubscriptionPaused"),
        QStringLiteral("Q_INVOKABLE void setPublishDraft"),
        QStringLiteral("Q_INVOKABLE void useMessageAsPublishDraft"),
        QStringLiteral("Q_INVOKABLE bool publishDraft"),
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
        QStringLiteral("Q_PROPERTY(QString subscriptionFilterText"),
        QStringLiteral("Q_PROPERTY(QString subscriptionFilterMode"),
        QStringLiteral("Q_PROPERTY(int subscriptionFilterModeIndex"),
        QStringLiteral("Q_PROPERTY(bool hasSubscriptionFilter"),
        QStringLiteral("SubscriptionListModel *subscriptions()"),
        QStringLiteral("ScriptLibraryModel *scripts()"),
        QStringLiteral("QString subscriptionFilterText()"),
        QStringLiteral("QString subscriptionFilterMode()"),
        QStringLiteral("int subscriptionFilterModeIndex()"),
        QStringLiteral("bool hasSubscriptionFilter()"),
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
    const QString optionsPath = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src/viewmodels/settingsoptionsviewmodel.h");
    QVERIFY2(!QFile::exists(optionsPath), "SettingsOptionsViewModel is deleted — option helpers are free functions in SettingsViewModel implementation");

    QString source;
    QVERIFY(readSourceFile(QStringLiteral("src/viewmodels/settingsviewmodel.h"), source));

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
