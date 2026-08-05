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
    void usecasesDoNotDependOnApplicationLayer();
    void messageHistoryWritesUseDedicatedWorker();
    void messageAdmissionChecksMetadataBeforePayloadWork();
    void messageQmlUsesTypedObjectProperties();
    void qmlUsesApplicationViewModelRootOnly();
    void messageProfilerUsesIsolatedApplicationData();
    void addSubscriptionDialogDoesNotBuildScriptOptions();
    void subscriptionsPanelDoesNotReadModelRowsForEditing();
    void subscriptionsPanelDoesNotOwnBusinessState();
    void workbenchViewsDoNotInterpretContextMenuActions();
    void workbenchViewsDoNotUseDialogBridgeObjects();
    void workbenchViewsUseIntentCommands();
    void messageWorkspaceSeparatesDisplayAndCaptureFilters();
    void workbenchViewModelDoesNotExposeLegacyCommands();
    void workbenchViewModelDoesNotExposeUnusedRawModels();
    void workbenchViewModelDoesNotForwardNonWorkbenchSignals();
    void featureViewModelsDoNotDependOnApplicationLayer();
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

void ArchitectureBoundariesTest::usecasesDoNotDependOnApplicationLayer()
{
    const QString usecaseRoot = QStringLiteral(MQTT_PLUS_SOURCE_DIR)
        + QStringLiteral("/src/usecases");
    QDirIterator sourceFiles(
        usecaseRoot,
        {QStringLiteral("*.h"), QStringLiteral("*.cpp")},
        QDir::Files,
        QDirIterator::Subdirectories);

    while (sourceFiles.hasNext()) {
        const QString path = sourceFiles.next();
        QFile file(path);
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text),
            qPrintable(QStringLiteral("Cannot read %1").arg(path)));
        const QString source = QString::fromUtf8(file.readAll());
        QVERIFY2(!source.contains(QStringLiteral("#include \"app/")),
            qPrintable(QStringLiteral("%1 must not depend on application-layer headers").arg(path)));
    }
}

void ArchitectureBoundariesTest::messageHistoryWritesUseDedicatedWorker()
{
    QString eventHistorySource;
    QVERIFY(readSourceFile(QStringLiteral("src/usecases/eventhistoryservice.cpp"), eventHistorySource));
    QVERIFY2(eventHistorySource.contains(QStringLiteral("m_historyWriter.enqueueMessage(record)")),
        "Message capture must enqueue through the bounded history writer");
    QVERIFY2(!eventHistorySource.contains(QStringLiteral("m_historyStore.enqueueMessage(record)")),
        "EventHistoryService must not keep the GUI-thread HistoryStore pending queue");
    QVERIFY2(!eventHistorySource.contains(QStringLiteral("m_messageHistoryFlushTimer")),
        "Message persistence scheduling belongs to HistoryWriterWorker");

    QString applicationSource;
    QVERIFY(readSourceFile(QStringLiteral("src/app/application.cpp"), applicationSource));
    QVERIFY2(applicationSource.contains(QStringLiteral("m_historyWriter->moveToThread(&m_historyWriterThread)")),
        "Application must give the history writer a dedicated thread");
    QVERIFY2(applicationSource.contains(QStringLiteral("m_messageParser->moveToThread(&m_messageParserThread)")),
        "Application must give payload parsing a dedicated thread");
    QVERIFY2(!applicationSource.contains(QStringLiteral("m_eventHistoryService.moveToThread")),
        "The UI-facing EventHistoryService must stay on the GUI thread");

    const int destructorStart = applicationSource.indexOf(QStringLiteral("Application::~Application()"));
    const int nextFunction = applicationSource.indexOf(
        QStringLiteral("ApplicationViewModel *Application::viewModel()"),
        destructorStart);
    QVERIFY(destructorStart >= 0);
    QVERIFY(nextFunction > destructorStart);
    const QString destructorSource = applicationSource.mid(
        destructorStart,
        nextFunction - destructorStart);
    const int parserShutdown = destructorSource.indexOf(QStringLiteral("&MessageParseWorker::shutdown"));
    const int finalWriterDrain = destructorSource.indexOf(QStringLiteral("m_historyWriter->drain()"));
    const int writerShutdown = destructorSource.indexOf(QStringLiteral("&HistoryWriterWorker::shutdown"));
    QVERIFY2(parserShutdown >= 0
            && finalWriterDrain > parserShutdown
            && writerShutdown > finalWriterDrain,
        "Application shutdown must persist parse results emitted by the final parser batch before closing the writer");

}

void ArchitectureBoundariesTest::messageAdmissionChecksMetadataBeforePayloadWork()
{
    QString eventHistorySource;
    QVERIFY(readSourceFile(QStringLiteral("src/usecases/eventhistoryservice.cpp"), eventHistorySource));
    const int incomingIndex = eventHistorySource.indexOf(
        QStringLiteral("void EventHistoryService::appendIncomingMessage"));
    const int outgoingIndex = eventHistorySource.indexOf(
        QStringLiteral("void EventHistoryService::appendPublishedMessage"), incomingIndex);
    QVERIFY(incomingIndex >= 0);
    QVERIFY(outgoingIndex > incomingIndex);
    const QString incomingSource = eventHistorySource.mid(
        incomingIndex,
        outgoingIndex - incomingIndex);
    const int capturePolicyIndex = incomingSource.indexOf(QStringLiteral("shouldCaptureMessage"));
    const int payloadPlanIndex = incomingSource.indexOf(QStringLiteral("makePayloadStoragePlan"));
    const int captureEnqueueIndex = incomingSource.indexOf(QStringLiteral("m_historyWriter.enqueueMessage"));
    const int parseEnqueueIndex = incomingSource.indexOf(QStringLiteral("enqueueMessageParsing"));
    QVERIFY2(capturePolicyIndex >= 0 && capturePolicyIndex < payloadPlanIndex,
        "Capture policy must reject by topic/direction before payload preview, hashing, or DTO work");
    QVERIFY2(captureEnqueueIndex >= 0 && captureEnqueueIndex < parseEnqueueIndex,
        "Raw capture admission must happen before optional structured parsing");

    QString mqttSessionSource;
    QVERIFY(readSourceFile(QStringLiteral("src/usecases/mqttsessionservice.cpp"), mqttSessionSource));
    const int callbackIndex = mqttSessionSource.indexOf(QStringLiteral("&QMqttClient::messageReceived"));
    QVERIFY(callbackIndex >= 0);
    const QString callbackSource = mqttSessionSource.mid(callbackIndex, 500);
    QVERIFY2(!callbackSource.contains(QStringLiteral("BlockingQueuedConnection")),
        "The MQTT receive callback must not wait for storage or parsing workers");
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

void ArchitectureBoundariesTest::qmlUsesApplicationViewModelRootOnly()
{
    const QStringList forbiddenRootDependencies {
        QStringLiteral("appController"),
        QStringLiteral("AppFacade"),
        QStringLiteral("SessionService"),
        QStringLiteral("MqttSessionService"),
        QStringLiteral("SubscriptionService"),
        QStringLiteral("EventHistoryService"),
        QStringLiteral("HistoryStore"),
        QStringLiteral("QSettings"),
    };

    const QString qmlRoot = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/qml");
    QDirIterator qmlFiles(
        qmlRoot,
        {QStringLiteral("*.qml")},
        QDir::Files,
        QDirIterator::Subdirectories);
    while (qmlFiles.hasNext()) {
        const QString path = qmlFiles.next();
        QFile file(path);
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text),
            qPrintable(QStringLiteral("Cannot read %1").arg(path)));
        const QString source = QString::fromUtf8(file.readAll());
        for (const QString &token : forbiddenRootDependencies) {
            QVERIFY2(!source.contains(token),
                qPrintable(QStringLiteral("%1 must use QML-facing ViewModels instead of %2")
                               .arg(path, token)));
        }
    }

    QString qmlMain;
    QVERIFY(readSourceFile(QStringLiteral("qml/Main.qml"), qmlMain));
    QVERIFY2(qmlMain.contains(QStringLiteral("required property var app")),
        "Main.qml must expose the ApplicationViewModel through the `app` root property");

    QString mainSource;
    QVERIFY(readSourceFile(QStringLiteral("src/app/main.cpp"), mainSource));
    QVERIFY2(mainSource.contains(QStringLiteral("\"app\"")),
        "main.cpp must inject ApplicationViewModel as the QML `app` root property");
    QVERIFY2(!mainSource.contains(QStringLiteral("appController")),
        "main.cpp must not inject the legacy appController root property");
}

void ArchitectureBoundariesTest::messageProfilerUsesIsolatedApplicationData()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("src/app/main.cpp"), source));

    const int testModeIndex = source.indexOf(
        QStringLiteral("QStandardPaths::setTestModeEnabled(true)"));
    const int applicationIndex = source.indexOf(QStringLiteral("Application application"));
    QVERIFY2(testModeIndex >= 0 && applicationIndex > testModeIndex,
        "The profiler driver must redirect application data before constructing Application");
    QVERIFY2(source.contains(QStringLiteral("#ifdef QT_QML_DEBUG")),
        "Synthetic message profiling must remain gated behind QML debugging builds");
    QVERIFY2(source.contains(QStringLiteral("--profile-message-stream")),
        "The profiling build must expose an explicit opt-in message stream driver");

    QString cmakeSource;
    QVERIFY(readSourceFile(QStringLiteral("CMakeLists.txt"), cmakeSource));
    QVERIFY2(cmakeSource.contains(QStringLiteral("\"src/app/messagestreamprofiledriver.cpp\"")),
        "The application-only profiling driver must be excluded from shared test sources");
    QVERIFY2(cmakeSource.contains(QStringLiteral("\"src/app/messagestreamprofiledriver.h\"")),
        "The profiling driver header must be excluded from shared test sources");
    QVERIFY2(cmakeSource.contains(QStringLiteral("$<$<CONFIG:Debug>:QT_QML_DEBUG>")),
        "The profiling driver must be enabled by the repository debug preset");
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
                QStringLiteral("setDraft("),
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

void ArchitectureBoundariesTest::messageWorkspaceSeparatesDisplayAndCaptureFilters()
{
    QString filterSource;
    QVERIFY(readSourceFile(
        QStringLiteral("qml/features/workbench/MessageFilterPopover.qml"),
        filterSource));
    QVERIFY2(filterSource.contains(QStringLiteral("qsTr(\"Display filter\")")),
        "The message filter popover must label presentation-only filtering explicitly");
    QVERIFY2(filterSource.contains(QStringLiteral("qsTr(\"Capture filter\")")),
        "The message filter popover must label capture filtering explicitly");
    QVERIFY2(filterSource.contains(QStringLiteral("setCurrentMessageCapturePolicy")),
        "Capture policy changes must go through a WorkbenchViewModel intent command");
    QVERIFY2(!filterSource.contains(QStringLiteral("eventHistory.")),
        "QML must not mutate EventHistoryService capture policy directly");

    QString workbenchSource;
    QVERIFY(readSourceFile(
        QStringLiteral("qml/features/workbench/WorkbenchView.qml"),
        workbenchSource));
    QVERIFY2(workbenchSource.contains(QStringLiteral("id: pressureStatusButton")),
        "The workbench status bar must expose bounded-pipeline pressure state");
    QVERIFY2(workbenchSource.contains(QStringLiteral("qsTr(\"Raw only\")")),
        "Degraded capture must identify raw-only storage to the user");
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
        QStringLiteral("void messageStreamRowAppended("),
    };

    for (const QString &token : forbiddenSignals) {
        QVERIFY2(!source.contains(token),
            qPrintable(QStringLiteral("WorkbenchViewModel must not forward non-workbench signal %1").arg(token)));
    }
}

void ArchitectureBoundariesTest::featureViewModelsDoNotDependOnApplicationLayer()
{
    const QString viewModelRoot = QStringLiteral(MQTT_PLUS_SOURCE_DIR)
        + QStringLiteral("/src/viewmodels");
    QDirIterator sourceFiles(
        viewModelRoot,
        {QStringLiteral("*.h"), QStringLiteral("*.cpp")},
        QDir::Files,
        QDirIterator::Subdirectories);

    while (sourceFiles.hasNext()) {
        const QString path = sourceFiles.next();
        QFile file(path);
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text),
            qPrintable(QStringLiteral("Cannot read %1").arg(path)));
        const QString source = QString::fromUtf8(file.readAll());
        QVERIFY2(!source.contains(QStringLiteral("#include \"app/")),
            qPrintable(QStringLiteral("%1 must not depend on application-layer headers").arg(path)));
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
