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
    void domainDoesNotDependOnOuterProjectLayers();
    void usecasesDoNotDependOnApplicationLayer();
    void updateControllerDoesNotExposeQmlApi();
    void messageHistoryWritesUseDedicatedWorker();
    void messageAdmissionChecksMetadataBeforePayloadWork();
    void processorCallersUseEngineSeamOnly();
    void messagePipelineUsesResolvedProcessorSnapshots();
    void messageQmlUsesTypedObjectProperties();
    void alwaysExpandedMessagesDoNotTruncatePayloadText();
    void qmlUsesSingleApplicationRoot();
    void applicationViewModelExportsApprovedQmlInterfaces();
    void qmlElementTypesRemainOnApplicationTarget();
    void messageProfilerUsesIsolatedApplicationData();
    void addSubscriptionDialogDoesNotBuildScriptOptions();
    void processorLibraryUiUsesProcessorContracts();
    void subscriptionEditorUsesProcessorBindings();
    void subscriptionsPanelDoesNotReadModelRowsForEditing();
    void subscriptionsPanelDoesNotOwnBusinessState();
    void workbenchViewsDoNotInterpretContextMenuActions();
    void workbenchViewsDoNotUseDialogBridgeObjects();
    void workbenchViewsUseIntentCommands();
    void messageWorkspaceSeparatesDisplayAndCaptureFilters();
    void comboBoxDelegatesHonorTextRole();
    void topicTreeUsesQmlFacingModel();
    void workbenchViewModelDoesNotDuplicateWorkflowCommands();
    void workbenchViewModelDoesNotExposeUnusedRawModels();
    void workbenchViewModelDoesNotForwardNonWorkbenchSignals();
    void featureViewModelsDoNotDependOnApplicationLayer();
    void editorViewModelsDoNotExposeInternalWorkflowHelpers();
    void legacyScriptingSubsystemIsRemoved();
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

void ArchitectureBoundariesTest::domainDoesNotDependOnOuterProjectLayers()
{
    const QString domainRoot = QStringLiteral(MQTT_PLUS_SOURCE_DIR)
        + QStringLiteral("/src/domain");
    const QStringList outerLayers {
        QStringLiteral("app/"),
        QStringLiteral("models/"),
        QStringLiteral("presentation/"),
        QStringLiteral("services/"),
        QStringLiteral("usecases/"),
        QStringLiteral("viewmodels/"),
    };
    QDirIterator sourceFiles(
        domainRoot,
        {QStringLiteral("*.h"), QStringLiteral("*.cpp")},
        QDir::Files,
        QDirIterator::Subdirectories);

    while (sourceFiles.hasNext()) {
        const QString path = sourceFiles.next();
        QFile file(path);
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text),
            qPrintable(QStringLiteral("Cannot read %1").arg(path)));
        const QString source = QString::fromUtf8(file.readAll());
        for (const QString &layer : outerLayers) {
            const bool dependsOnOuterLayer = source.contains(
                QStringLiteral("#include \"") + layer)
                || source.contains(QStringLiteral("#include <") + layer);
            QVERIFY2(!dependsOnOuterLayer,
                qPrintable(QStringLiteral("%1 must not depend on %2")
                    .arg(path, layer)));
        }
    }
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

void ArchitectureBoundariesTest::updateControllerDoesNotExposeQmlApi()
{
    QString controllerHeader;
    QString viewModelHeader;
    QVERIFY(readSourceFile(
        QStringLiteral("src/usecases/updatecontroller.h"),
        controllerHeader));
    QVERIFY(readSourceFile(
        QStringLiteral("src/viewmodels/updateviewmodel.h"),
        viewModelHeader));

    QVERIFY2(!controllerHeader.contains(QStringLiteral("Q_PROPERTY")),
        "UpdateController must keep QML-facing state in UpdateViewModel");
    QVERIFY2(!controllerHeader.contains(QStringLiteral("Q_INVOKABLE")),
        "UpdateController must not expose QML commands directly");
    QVERIFY2(viewModelHeader.contains(QStringLiteral("Q_PROPERTY"))
            && viewModelHeader.contains(QStringLiteral("Q_INVOKABLE")),
        "UpdateViewModel must expose the update QML interface");
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
    QString admissionWorkerSource;
    QVERIFY(readSourceFile(
        QStringLiteral("src/services/messaging/messageadmissionworker.cpp"),
        admissionWorkerSource));
    const int prepareIndex = admissionWorkerSource.indexOf(
        QStringLiteral("PreparedIncomingMessage MessageAdmissionWorker::prepare"));
    const int approximateBytesIndex = admissionWorkerSource.indexOf(
        QStringLiteral("qint64 MessageAdmissionWorker::approximateBytes"),
        prepareIndex);
    QVERIFY(prepareIndex >= 0);
    QVERIFY(approximateBytesIndex > prepareIndex);
    const QString prepareSource = admissionWorkerSource.mid(
        prepareIndex,
        approximateBytesIndex - prepareIndex);
    const int capturePolicyIndex = prepareSource.indexOf(
        QStringLiteral("capturePolicy.accepts"));
    const int payloadPlanIndex = prepareSource.indexOf(
        QStringLiteral("MessagePayload::planStorage"));
    QVERIFY2(capturePolicyIndex >= 0 && capturePolicyIndex < payloadPlanIndex,
        "Capture policy must reject by topic and direction before payload preview, hashing, or DTO work");

    QString eventHistorySource;
    QVERIFY(readSourceFile(QStringLiteral("src/usecases/eventhistoryservice.cpp"), eventHistorySource));
    QVERIFY2(eventHistorySource.contains(QStringLiteral("MessagePayload::planStorage")),
        "Incoming and outgoing capture must share the payload planning module");
    QVERIFY2(!eventHistorySource.contains(QStringLiteral("QCryptographicHash"))
            && !admissionWorkerSource.contains(QStringLiteral("QCryptographicHash")),
        "Payload preview, hashing, and limit rules must stay inside MessagePayload");

    const int incomingIndex = eventHistorySource.indexOf(
        QStringLiteral("void EventHistoryService::applyPreparedIncomingMessages"));
    const int outgoingIndex = eventHistorySource.indexOf(
        QStringLiteral("void EventHistoryService::appendPublishedMessage"), incomingIndex);
    QVERIFY(incomingIndex >= 0);
    QVERIFY(outgoingIndex > incomingIndex);
    const QString incomingSource = eventHistorySource.mid(
        incomingIndex,
        outgoingIndex - incomingIndex);
    const int captureEnqueueIndex = incomingSource.indexOf(QStringLiteral("m_historyWriter.enqueueMessage"));
    const int parseEnqueueIndex = incomingSource.indexOf(QStringLiteral("enqueueMessageParsing"));
    QVERIFY2(captureEnqueueIndex >= 0 && captureEnqueueIndex < parseEnqueueIndex,
        "Raw capture admission must happen before optional structured parsing");

    QString subscriptionSource;
    QVERIFY(readSourceFile(QStringLiteral("src/usecases/subscriptionservice.cpp"), subscriptionSource));
    const int callbackIndex = subscriptionSource.indexOf(
        QStringLiteral("&QMqttSubscription::messageReceived"));
    QVERIFY(callbackIndex >= 0);
    const QString callbackSource = subscriptionSource.mid(callbackIndex, 900);
    QVERIFY2(callbackSource.contains(QStringLiteral("queueIncomingMessage")),
        "The MQTT receive callback must hand payload admission to the bounded worker queue");
    QVERIFY2(!callbackSource.contains(QStringLiteral("appendIncomingMessage")),
        "The MQTT receive callback must not perform payload planning on the GUI thread");
    QVERIFY2(!callbackSource.contains(QStringLiteral("BlockingQueuedConnection")),
        "The MQTT receive callback must not wait for storage or parsing workers");
}

void ArchitectureBoundariesTest::processorCallersUseEngineSeamOnly()
{
    QString engineHeader;
    QVERIFY(readSourceFile(
        QStringLiteral("src/services/processors/messageprocessorengine.h"),
        engineHeader));
    QVERIFY2(!engineHeader.contains(QStringLiteral("processorruntimeregistry.h")),
        "MessageProcessorEngine must keep the runtime registry behind its interface");
    QVERIFY2(!engineHeader.contains(QStringLiteral("processorruntimeadapter.h")),
        "MessageProcessorEngine must not expose the internal adapter header to callers");

    QString luaAdapterSource;
    QVERIFY(readSourceFile(
        QStringLiteral("src/services/processors/runtimes/luaruntimeadapter.cpp"),
        luaAdapterSource));
    QVERIFY2(!luaAdapterSource.contains(QStringLiteral("luarunner.h"))
            && !luaAdapterSource.contains(QStringLiteral("LuaRunner")),
        "The new Lua runtime adapter must not depend on the legacy LuaRunner contract");

    QString javascriptAdapterSource;
    QVERIFY(readSourceFile(
        QStringLiteral("src/services/processors/runtimes/javascriptruntimeadapter.cpp"),
        javascriptAdapterSource));
    QVERIFY2(javascriptAdapterSource.contains(QStringLiteral("QJSEngine"))
            && javascriptAdapterSource.contains(QStringLiteral("setInterrupted")),
        "The JavaScript adapter must use QJSEngine with watchdog interruption");
    const QStringList forbiddenJavaScriptHostBridges {
        QStringLiteral("newQObject"),
        QStringLiteral("newQMetaObject"),
        QStringLiteral("QQmlContext"),
        QStringLiteral("rootContext"),
        QStringLiteral("QCoreApplication::instance"),
        QStringLiteral("qApp"),
    };
    for (const QString &token : forbiddenJavaScriptHostBridges) {
        QVERIFY2(!javascriptAdapterSource.contains(token),
            qPrintable(QStringLiteral(
                "The JavaScript adapter must not expose host objects through %1")
                           .arg(token)));
    }

    const QString sourceRoot = QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QStringLiteral("/src");
    QDirIterator sourceFiles(
        sourceRoot,
        {QStringLiteral("*.h"), QStringLiteral("*.cpp")},
        QDir::Files,
        QDirIterator::Subdirectories);
    while (sourceFiles.hasNext()) {
        const QString path = sourceFiles.next();
        if (path.contains(QStringLiteral("/src/services/processors/"))) {
            continue;
        }
        QFile file(path);
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text),
            qPrintable(QStringLiteral("Cannot read %1").arg(path)));
        const QString source = QString::fromUtf8(file.readAll());
        QVERIFY2(!source.contains(QStringLiteral("processorruntimeregistry.h")),
            qPrintable(QStringLiteral("%1 must use MessageProcessorEngine instead of its registry")
                           .arg(path)));
        QVERIFY2(!source.contains(QStringLiteral("processorruntimeadapter.h")),
            qPrintable(QStringLiteral("%1 must not depend on the internal runtime adapter seam")
                           .arg(path)));
    }
}

void ArchitectureBoundariesTest::messagePipelineUsesResolvedProcessorSnapshots()
{
    QString parsingHeader;
    QVERIFY(readSourceFile(QStringLiteral("src/domain/messageparsing.h"), parsingHeader));
    QVERIFY2(parsingHeader.contains(
            QStringLiteral("QSharedPointer<const ProcessorRevisionSnapshot> processorRevision")),
        "Parser tasks must hold the immutable revision resolved at capture admission");
    QVERIFY2(!parsingHeader.contains(QStringLiteral("scriptCode"))
            && !parsingHeader.contains(QStringLiteral("scriptId")),
        "Parser tasks and results must not copy legacy script source or identity");

    QString eventHistoryHeader;
    QString eventHistorySource;
    QVERIFY(readSourceFile(
        QStringLiteral("src/usecases/eventhistoryservice.h"),
        eventHistoryHeader));
    QVERIFY(readSourceFile(
        QStringLiteral("src/usecases/eventhistoryservice.cpp"),
        eventHistorySource));
    QVERIFY2(eventHistoryHeader.contains(QStringLiteral("ProcessorLibrary &m_processorLibrary"))
            && eventHistorySource.contains(QStringLiteral("m_processorLibrary.resolve")),
        "EventHistoryService must resolve Processor References from the in-memory library before enqueueing");
    QVERIFY2(!eventHistoryHeader.contains(QStringLiteral("ScriptService"))
            && !eventHistorySource.contains(QStringLiteral("scriptById"))
            && !eventHistorySource.contains(QStringLiteral("clearRuntimeCache")),
        "The message pipeline must not depend on the legacy ScriptService or caller-driven runtime invalidation");

    QString subscriptionHeader;
    QVERIFY(readSourceFile(QStringLiteral("src/domain/subscription.h"), subscriptionHeader));
    QVERIFY2(subscriptionHeader.contains(QStringLiteral("ProcessorReference processor"))
            && !subscriptionHeader.contains(QStringLiteral("QString scriptId")),
        "Subscriptions must store ProcessorReference instead of legacy script IDs");

    QString sessionStoreSource;
    QVERIFY(readSourceFile(
        QStringLiteral("src/services/storage/sessionsettingsstore.cpp"),
        sessionStoreSource));
    QVERIFY2(sessionStoreSource.contains(QStringLiteral("parametersCborBase64"))
            && sessionStoreSource.contains(QStringLiteral("processorId"))
            && !sessionStoreSource.contains(QStringLiteral("revisionMode"))
            && !sessionStoreSource.contains(QStringLiteral("row.insert(QStringLiteral(\"scriptId\")")),
        "Subscription persistence must use the nested Processor Reference representation");
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

void ArchitectureBoundariesTest::alwaysExpandedMessagesDoNotTruncatePayloadText()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/EventStreamView.qml"), source));
    QVERIFY2(!source.contains(QStringLiteral("2147483647")),
        "Always-expanded messages must keep inline text layout bounded");
    QVERIFY2(source.contains(
                 QStringLiteral("root.payloadDisplayMode === 2 ? Text.ElideNone")),
        "Always-expanded messages must not elide payload text");
    QVERIFY2(source.contains(QStringLiteral("root.viewModel.requestExpandedMessage(")),
        "Always-expanded rows must request complete content asynchronously");
    QVERIFY2(!source.contains(QStringLiteral("root.viewModel.messagePayloadForDisplay(")),
        "Message delegates must not synchronously query stored payloads");
    QVERIFY2(source.contains(QStringLiteral("eventDelegate.expandedPayload")),
        "Always-expanded rows must consume cached expanded model content");
}

void ArchitectureBoundariesTest::qmlUsesSingleApplicationRoot()
{
    QString qmlMain;
    QVERIFY(readSourceFile(QStringLiteral("qml/Main.qml"), qmlMain));
    QVERIFY2(qmlMain.contains(QStringLiteral("required property var app")),
        "Main.qml must expose the ApplicationViewModel through the `app` root property");
    QVERIFY2(!qmlMain.contains(QStringLiteral("required property var appController")),
        "Main.qml must not expose the legacy application controller root");

    QString mainSource;
    QVERIFY(readSourceFile(QStringLiteral("src/app/main.cpp"), mainSource));
    const int initialPropertiesStart = mainSource.indexOf(
        QStringLiteral("engine.setInitialProperties({"));
    const int initialPropertiesEnd = mainSource.indexOf(
        QStringLiteral("});"),
        initialPropertiesStart);
    QVERIFY(initialPropertiesStart >= 0);
    QVERIFY(initialPropertiesEnd > initialPropertiesStart);
    const QString initialProperties = mainSource.mid(
        initialPropertiesStart,
        initialPropertiesEnd - initialPropertiesStart);
    QVERIFY2(initialProperties.contains(QStringLiteral("{QStringLiteral(\"app\")")),
        "main.cpp must inject ApplicationViewModel as the QML `app` root property");
    QCOMPARE(initialProperties.count(QStringLiteral("QStringLiteral(")), 1);
    QVERIFY2(!mainSource.contains(QStringLiteral("appController")),
        "main.cpp must not inject the legacy appController root property");
}

void ArchitectureBoundariesTest::applicationViewModelExportsApprovedQmlInterfaces()
{
    QString source;
    QVERIFY(readSourceFile(
        QStringLiteral("src/viewmodels/applicationviewmodel.h"),
        source));

    const QStringList approvedProperties {
        QStringLiteral("Q_PROPERTY(WorkbenchViewModel* workbench"),
        QStringLiteral("Q_PROPERTY(DraftsViewModel* drafts"),
        QStringLiteral("Q_PROPERTY(LogsViewModel* logs"),
        QStringLiteral("Q_PROPERTY(ProcessorsViewModel* processors"),
        QStringLiteral("Q_PROPERTY(SettingsViewModel* settings"),
        QStringLiteral("Q_PROPERTY(ConfigurationTransferService* configurationTransfer"),
        QStringLiteral("Q_PROPERTY(PreferencesController* preferences"),
        QStringLiteral("Q_PROPERTY(EventHistoryService* eventHistory"),
        QStringLiteral("Q_PROPERTY(SessionService* sessionService"),
        QStringLiteral("Q_PROPERTY(SubscriptionService* subscriptionService"),
        QStringLiteral("Q_PROPERTY(NotificationCenterModel* notifications"),
        QStringLiteral("Q_PROPERTY(UpdateViewModel* updates"),
    };
    for (const QString &property : approvedProperties) {
        QVERIFY2(source.contains(property),
            qPrintable(QStringLiteral("ApplicationViewModel is missing approved QML export %1")
                           .arg(property)));
    }
    QCOMPARE(source.count(QStringLiteral("Q_PROPERTY(")), approvedProperties.size());
    QVERIFY2(!source.contains(QStringLiteral("Q_PROPERTY(UpdateController*")),
        "UpdateController must stay behind UpdateViewModel");
}

void ArchitectureBoundariesTest::qmlElementTypesRemainOnApplicationTarget()
{
    QString cmakeSource;
    QVERIFY(readSourceFile(QStringLiteral("CMakeLists.txt"), cmakeSource));

    const int appOnlySourcesStart = cmakeSource.indexOf(
        QStringLiteral("set(MQTT_PLUS_APP_ONLY_SOURCES"));
    const int appOnlySourcesEnd = cmakeSource.indexOf(
        QLatin1Char(')'),
        appOnlySourcesStart);
    QVERIFY(appOnlySourcesStart >= 0);
    QVERIFY(appOnlySourcesEnd > appOnlySourcesStart);
    const QString appOnlySources = cmakeSource.mid(
        appOnlySourcesStart,
        appOnlySourcesEnd - appOnlySourcesStart);
    QVERIFY2(appOnlySources.contains(QStringLiteral("src/presentation/codesyntaxhighlighter.cpp"))
            && appOnlySources.contains(QStringLiteral("src/presentation/codesyntaxhighlighter.h")),
        "QML_ELEMENT types must remain on the target passed to qt_add_qml_module");
    QVERIFY2(cmakeSource.contains(QStringLiteral(
                 "qt_add_executable(mqtt_plus_app\n"
                 "    ${MQTT_PLUS_APP_ONLY_SOURCES}")),
        "Application-only QML types must be compiled into the executable target");
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
    const int appOnlySourcesStart = cmakeSource.indexOf(
        QStringLiteral("set(MQTT_PLUS_APP_ONLY_SOURCES"));
    const int appOnlySourcesEnd = cmakeSource.indexOf(
        QLatin1Char(')'),
        appOnlySourcesStart);
    QVERIFY(appOnlySourcesStart >= 0);
    QVERIFY(appOnlySourcesEnd > appOnlySourcesStart);
    const QString appOnlySources = cmakeSource.mid(
        appOnlySourcesStart,
        appOnlySourcesEnd - appOnlySourcesStart);
    QVERIFY2(appOnlySources.contains(QStringLiteral("src/app/messagestreamprofiledriver.cpp")),
        "The application-only profiling driver must be excluded from shared test sources");
    QVERIFY2(appOnlySources.contains(QStringLiteral("src/app/messagestreamprofiledriver.h")),
        "The profiling driver header must be excluded from shared test sources");
    QVERIFY2(cmakeSource.contains(QStringLiteral(
                 "list(REMOVE_ITEM MQTT_PLUS_APP_LIBRARY_SOURCES\n"
                 "    ${MQTT_PLUS_APP_ONLY_SOURCES}")),
        "Application-only sources must be removed before building the shared core library");
    QVERIFY2(cmakeSource.contains(QStringLiteral(
                 "qt_add_executable(mqtt_plus_app\n"
                 "    ${MQTT_PLUS_APP_ONLY_SOURCES}")),
        "Application-only sources must be compiled only into the executable target");
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

void ArchitectureBoundariesTest::processorLibraryUiUsesProcessorContracts()
{
    QString mainSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/Main.qml"), mainSource));
    QVERIFY2(mainSource.contains(QStringLiteral("ProcessorsView"))
            && mainSource.contains(QStringLiteral("root.app.processors")),
        "Main.qml must route the navigation rail to the Processor Library ViewModel");
    QVERIFY2(!mainSource.contains(QStringLiteral("ScriptsView"))
            && !mainSource.contains(QStringLiteral("root.app.scripts")),
        "The user-visible application route must not expose the legacy Script Manager");

    QString processorView;
    QVERIFY(readSourceFile(
        QStringLiteral("qml/features/processors/ProcessorsView.qml"),
        processorView));
    QVERIFY2(processorView.contains(QStringLiteral("text: qsTr(\"Save\")"))
            && !processorView.contains(QStringLiteral("Revision history"))
            && processorView.contains(QStringLiteral("newProcessor(\"lua\")"))
            && processorView.contains(QStringLiteral("newProcessor(\"javascript\")")),
        "Processor Library must expose simple saves and both initial runtime templates");

    QString applicationViewModel;
    QVERIFY(readSourceFile(
        QStringLiteral("src/viewmodels/applicationviewmodel.h"),
        applicationViewModel));
    QVERIFY2(applicationViewModel.contains(
            QStringLiteral("Q_PROPERTY(ProcessorsViewModel* processors"))
            && !applicationViewModel.contains(
                QStringLiteral("Q_PROPERTY(ScriptsViewModel* scripts")),
        "ApplicationViewModel must expose Processor Library rather than legacy Scripts");
}

void ArchitectureBoundariesTest::legacyScriptingSubsystemIsRemoved()
{
    const QString sourceRoot = QStringLiteral(MQTT_PLUS_SOURCE_DIR);
    const QStringList removedPaths {
        QStringLiteral("src/domain/script.h"),
        QStringLiteral("src/models/scriptfiltermodel.cpp"),
        QStringLiteral("src/models/scriptfiltermodel.h"),
        QStringLiteral("src/models/scriptlibrarymodel.cpp"),
        QStringLiteral("src/models/scriptlibrarymodel.h"),
        QStringLiteral("src/services/scripting/luarunner.cpp"),
        QStringLiteral("src/services/scripting/luarunner.h"),
        QStringLiteral("src/services/storage/scriptstore.cpp"),
        QStringLiteral("src/services/storage/scriptstore.h"),
        QStringLiteral("src/usecases/scriptservice.cpp"),
        QStringLiteral("src/usecases/scriptservice.h"),
        QStringLiteral("src/viewmodels/scripteditorviewmodel.cpp"),
        QStringLiteral("src/viewmodels/scripteditorviewmodel.h"),
        QStringLiteral("src/viewmodels/scriptsviewmodel.cpp"),
        QStringLiteral("src/viewmodels/scriptsviewmodel.h"),
        QStringLiteral("qml/features/scripts/ScriptListPane.qml"),
        QStringLiteral("qml/features/scripts/ScriptsView.qml"),
        QStringLiteral("tests/test_luarunner.cpp"),
        QStringLiteral("tests/test_scripteditorviewmodel.cpp"),
        QStringLiteral("tests/test_scriptlibrarymodel.cpp"),
        QStringLiteral("tests/test_scriptsviewmodel.cpp"),
    };
    for (const QString &relativePath : removedPaths) {
        QVERIFY2(!QFile::exists(sourceRoot + QLatin1Char('/') + relativePath),
            qPrintable(QStringLiteral("Legacy scripting file still exists: %1").arg(relativePath)));
    }

    const QStringList forbiddenTokens {
        QStringLiteral("ScriptEntry"),
        QStringLiteral("ScriptService"),
        QStringLiteral("ScriptStore"),
        QStringLiteral("LuaRunner"),
        QStringLiteral("scriptId"),
    };
    const QStringList productionRoots {
        sourceRoot + QStringLiteral("/src"),
        sourceRoot + QStringLiteral("/qml"),
    };
    for (const QString &root : productionRoots) {
        QDirIterator sourceFiles(
            root,
            {QStringLiteral("*.h"), QStringLiteral("*.cpp"), QStringLiteral("*.qml")},
            QDir::Files,
            QDirIterator::Subdirectories);
        while (sourceFiles.hasNext()) {
            const QString path = sourceFiles.next();
            QFile file(path);
            QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text),
                qPrintable(QStringLiteral("Cannot read %1").arg(path)));
            const QString source = QString::fromUtf8(file.readAll());
            for (const QString &token : forbiddenTokens) {
                QVERIFY2(!source.contains(token),
                    qPrintable(QStringLiteral("%1 still contains legacy scripting token %2")
                                   .arg(path, token)));
            }
        }
    }

    QString cmakeSource;
    QVERIFY(readSourceFile(QStringLiteral("CMakeLists.txt"), cmakeSource));
    const QStringList removedTargets {
        QStringLiteral("test_luarunner"),
        QStringLiteral("test_scripteditorviewmodel"),
        QStringLiteral("test_scriptlibrarymodel"),
        QStringLiteral("test_scriptsviewmodel"),
    };
    for (const QString &target : removedTargets) {
        QVERIFY2(!cmakeSource.contains(target),
            qPrintable(QStringLiteral("CMake still registers removed target %1").arg(target)));
    }
}

void ArchitectureBoundariesTest::subscriptionEditorUsesProcessorBindings()
{
    QString editorHeader;
    QVERIFY(readSourceFile(
        QStringLiteral("src/viewmodels/subscriptioneditorviewmodel.h"),
        editorHeader));
    QVERIFY2(!editorHeader.contains(QStringLiteral("processorRevisionMode"))
            && !editorHeader.contains(QStringLiteral("pinnedRevisionId"))
            && !editorHeader.contains(QStringLiteral("scriptId")),
        "SubscriptionEditorViewModel must expose only current Processor Bindings");

    QString dialogSource;
    QVERIFY(readSourceFile(
        QStringLiteral("qml/features/workbench/AddSubscriptionDialog.qml"),
        dialogSource));
    QVERIFY2(dialogSource.contains(QStringLiteral("processorOptionNames"))
            && !dialogSource.contains(QStringLiteral("pinnedRevisionOptionNames"))
            && !dialogSource.contains(QStringLiteral("scriptOptionNames")),
        "The subscription dialog must bind directly to the current Processor");
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
    QString captureSource;
    QVERIFY(readSourceFile(
        QStringLiteral("qml/features/workbench/MessageCaptureDialog.qml"),
        captureSource));
    QVERIFY2(captureSource.contains(QStringLiteral("qsTr(\"Capture settings\")")),
        "Capture filtering must use a separately labelled dialog");
    QVERIFY2(captureSource.contains(QStringLiteral("setCurrentMessageCapturePolicy")),
        "Capture policy changes must go through a WorkbenchViewModel intent command");
    QVERIFY2(!captureSource.contains(QStringLiteral("eventHistory.")),
        "QML must not mutate EventHistoryService capture policy directly");

    QString eventStreamSource;
    QVERIFY(readSourceFile(
        QStringLiteral("qml/features/workbench/EventStreamView.qml"),
        eventStreamSource));
    QVERIFY2(eventStreamSource.contains(QStringLiteral("actionId: \"capture-settings\"")),
        "Message actions must expose capture settings separately from display filters");
    QVERIFY2(!eventStreamSource.contains(QStringLiteral("MessageFilterPopover {")),
        "The message header must not retain a display filter popover");

    QString subscriptionsSource;
    QVERIFY(readSourceFile(
        QStringLiteral("qml/features/workbench/SubscriptionsPanel.qml"),
        subscriptionsSource));
    QVERIFY2(!subscriptionsSource.contains(QStringLiteral("AppTextField {")),
        "Subscriptions must use the shared context Topic filter field");

    QString topicTreeSource;
    QVERIFY(readSourceFile(
        QStringLiteral("qml/features/topics/TopicTreePanel.qml"),
        topicTreeSource));
    QVERIFY2(!topicTreeSource.contains(QStringLiteral("AppTextField {")),
        "The Topic tree must use the shared context Topic filter field");

    QString workbenchSource;
    QVERIFY(readSourceFile(
        QStringLiteral("qml/features/workbench/WorkbenchView.qml"),
        workbenchSource));
    QVERIFY2(workbenchSource.contains(QStringLiteral("id: contextTopicFilterField")),
        "Topics and subscriptions must share one Topic filter field");
    QVERIFY2(workbenchSource.contains(QStringLiteral("filteredMessages.selectedTopics")),
        "Typing in the shared field must directly update visible message Topic filters");
    QVERIFY2(workbenchSource.contains(QStringLiteral("topicTree.searchText"))
            && workbenchSource.contains(QStringLiteral("filteredSubscriptions.filterText")),
        "The shared Topic filter must narrow both context lists");
    QVERIFY2(workbenchSource.contains(QStringLiteral("id: pressureStatusButton")),
        "The workbench status bar must expose bounded-pipeline pressure state");
    QVERIFY2(workbenchSource.contains(QStringLiteral("qsTr(\"Raw only\")")),
        "Degraded capture must identify raw-only storage to the user");
}

void ArchitectureBoundariesTest::comboBoxDelegatesHonorTextRole()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("qml/components/AppComboBox.qml"), source));
    QVERIFY2(source.contains(QStringLiteral("control.textAt(comboDelegate.index)")),
        "AppComboBox delegates must resolve display text through ComboBox::textAt so textRole works with QAbstractItemModel models");
    QVERIFY2(!source.contains(QStringLiteral("text: comboDelegate.modelData")),
        "AppComboBox must not stringify QML model wrapper objects in its popup");
}

void ArchitectureBoundariesTest::topicTreeUsesQmlFacingModel()
{
    QString viewModelHeader;
    QVERIFY(readSourceFile(
        QStringLiteral("src/viewmodels/workbenchviewmodel.h"),
        viewModelHeader));
    QVERIFY2(viewModelHeader.contains(
                 QStringLiteral("Q_PROPERTY(TopicTreeModel* topicTree")),
        "WorkbenchViewModel must expose the topic tree through its QML-facing model");

    QString topicPanelSource;
    QVERIFY(readSourceFile(
        QStringLiteral("qml/features/topics/TopicTreePanel.qml"),
        topicPanelSource));
    QVERIFY2(topicPanelSource.contains(
                 QStringLiteral("control.viewModel.topicTree")),
        "TopicTreePanel must consume the QML-facing topic tree model");
    const QStringList forbiddenDependencies {
        QStringLiteral("eventHistory"),
        QStringLiteral("sessionService"),
        QStringLiteral("subscriptionService"),
    };
    for (const QString &dependency : forbiddenDependencies) {
        QVERIFY2(!topicPanelSource.contains(dependency),
            qPrintable(QStringLiteral("TopicTreePanel must not access %1 directly")
                .arg(dependency)));
    }

    QString workbenchSource;
    QVERIFY(readSourceFile(
        QStringLiteral("qml/features/workbench/WorkbenchView.qml"),
        workbenchSource));
    QVERIFY2(workbenchSource.contains(QStringLiteral("import \"../topics\"")),
        "The workbench must import the Topics feature");
    QVERIFY2(workbenchSource.contains(QStringLiteral("SubscriptionsPanel {")),
        "The workbench context pane must retain subscriptions");
    QVERIFY2(workbenchSource.contains(QStringLiteral("TopicTreePanel {")),
        "The workbench context pane must expose the topic tree");
    QVERIFY2(workbenchSource.contains(QStringLiteral("id: contextPaneTabs")),
        "The workbench must switch Topics and Subscriptions with tabs");

    QString mainSource;
    QVERIFY(readSourceFile(
        QStringLiteral("qml/Main.qml"),
        mainSource));
    QVERIFY2(!mainSource.contains(QStringLiteral("currentAppView === \"topics\"")),
        "Topics must not remain a separate application page");
    QVERIFY2(!mainSource.contains(QStringLiteral("TopicView")),
        "The application root must not host a duplicate Topics page");
}

void ArchitectureBoundariesTest::workbenchViewModelDoesNotDuplicateWorkflowCommands()
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
            qPrintable(QStringLiteral("WorkbenchViewModel must not duplicate workflow command %1").arg(token)));
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
                QStringLiteral("Q_INVOKABLE SessionConnectionConfig collectedConfig"),
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
