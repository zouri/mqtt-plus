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
    void messageRowsUseHoverHandlerForNestedControls();
    void messageRowsUseButtonTapPolicy();
    void messageRowsDoNotNestPointerHandlingTextEdit();
    void messagePanelUsesSplitViewForComposerResize();
    void eventStreamFollowModeUsesSingleCycleButton();
    void qmlUsesApplicationViewModelRootOnly();
    void draftLibraryUsesDedicatedPageAndGlobalNotifications();
    void applicationUsesConfigurableMonospaceFont();
    void messageProfilerUsesIsolatedApplicationData();
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
    void qmlMotionPolicyUsesSharedTokens();
    void qmlUsesNativeFocusManagement();
    void qmlMenusAreApplicationRendered();
    void textEditorsUseNativeContextMenus();
    void workbenchViewsDoNotInterpretContextMenuActions();
    void workbenchViewsDoNotUseDialogBridgeObjects();
    void workbenchViewsUseIntentCommands();
    void messageWorkspaceSeparatesDisplayAndCaptureFilters();
    void eventStreamViewUsesLocalFollowScrollState();
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

void ArchitectureBoundariesTest::draftLibraryUsesDedicatedPageAndGlobalNotifications()
{
    QString mainSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/Main.qml"), mainSource));
    const qsizetype workbenchRail = mainSource.indexOf(QStringLiteral("accessibleLabel: qsTr(\"Workbench\")"));
    const qsizetype draftsRail = mainSource.indexOf(QStringLiteral("accessibleLabel: qsTr(\"Draft Library\")"));
    const qsizetype scriptsRail = mainSource.indexOf(QStringLiteral("accessibleLabel: qsTr(\"Lua scripts\")"));
    const qsizetype logsRail = mainSource.indexOf(QStringLiteral("accessibleLabel: qsTr(\"Logs\")"));
    QVERIFY2(workbenchRail >= 0
            && scriptsRail > workbenchRail
            && draftsRail > scriptsRail
            && logsRail > draftsRail,
        "The navigation rail must order Workbench, Scripts, Draft Library, then Logs");
    QVERIFY2(mainSource.contains(QStringLiteral("iconSource: appUi.materialIcon(\"drafts\")")),
        "Draft Library navigation must use its dedicated document icon");
    QVERIFY2(mainSource.contains(QStringLiteral("AppNotificationStack"))
            && mainSource.contains(QStringLiteral("notificationModel: root.app.notifications")),
        "Application-wide notifications must be owned above individual feature pages");
    QVERIFY2(mainSource.contains(QStringLiteral("hasUnsavedChanges"))
            && mainSource.contains(QStringLiteral("unsavedDraftNavigationDialog")),
        "Leaving or closing the Draft Library must protect unsaved editor changes");

    QString draftsSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/drafts/DraftsView.qml"), draftsSource));
    QVERIFY2(draftsSource.contains(QStringLiteral("text: qsTr(\"Save Draft\")"))
            && draftsSource.contains(QStringLiteral("currentNeedsTopic()"))
            && draftsSource.contains(QStringLiteral("sessionNames")),
        "The dedicated page must use explicit save, one-time Topic, and connection selection flows");

    QString composerSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/PublishComposer.qml"), composerSource));
    QVERIFY2(composerSource.contains(QStringLiteral("quickPublishDraft"))
            && composerSource.contains(QStringLiteral("wouldReplaceWithDraft"))
            && composerSource.contains(QStringLiteral("quickPublishRecent"))
            && composerSource.contains(QStringLiteral("manageDraftsRequested")),
        "The composer Send Library must separate load, quick publish, repeat publish, and management intents");
    QVERIFY2(composerSource.contains(QStringLiteral("iconSource: root.ui.materialIcon(\"drafts\")")),
        "The composer Send Library must reuse the dedicated Draft Library icon");

    QString draftStoreSource;
    QVERIFY(readSourceFile(QStringLiteral("src/services/storage/draftstore.cpp"), draftStoreSource));
    QVERIFY2(draftStoreSource.contains(QStringLiteral("QSaveFile"))
            && draftStoreSource.contains(QStringLiteral("drafts.json.bak"))
            && draftStoreSource.contains(QStringLiteral("drafts.json.corrupt-%1")),
        "Persistent drafts must keep atomic primary, backup, and recovery artifacts");
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

    QVERIFY2(!eventHistorySource.contains(QStringLiteral("m_luaRuntimeCache")),
        "Lua runtime ownership belongs to MessageParseWorker, not EventHistoryService");
    QString parserSource;
    QVERIFY(readSourceFile(QStringLiteral("src/services/parsing/messageparseworker.cpp"), parserSource));
    QVERIFY2(parserSource.contains(QStringLiteral("std::make_unique<LuaRunner::RuntimeCache>()")),
        "The parser worker must create its Lua cache on the parser thread");
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
    QVERIFY2(source.contains(QStringLiteral("root.eventHistory.setMessageStreamFrozen(true)")),
        "Manual mode should freeze live mutations of the rendered message model");
    QVERIFY2(source.contains(QStringLiteral("root.eventHistory.setMessageStreamFrozen(false)")),
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
    QVERIFY2(source.contains(QStringLiteral("root.viewModel.displayTotalMessageCount")),
        "The message badge should use the coalesced session total instead of the capped visible model count");
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

void ArchitectureBoundariesTest::applicationUsesConfigurableMonospaceFont()
{
    QString mainSource;
    QVERIFY(readSourceFile(QStringLiteral("src/app/main.cpp"), mainSource));
    QVERIFY2(mainSource.contains(
                 QStringLiteral("settingsViewModel->effectiveFontFamily()")),
        "The application must apply the persisted font before loading QML");
    QVERIFY2(mainSource.contains(QStringLiteral("&SettingsViewModel::fontFamilyChanged")),
        "The application must apply font changes immediately");
    QVERIFY2(!mainSource.contains(QStringLiteral("addApplicationFont")),
        "The application must not bundle a large font file");

    QString settingsSource;
    QVERIFY(readSourceFile(QStringLiteral("src/viewmodels/settingsviewmodel.cpp"), settingsSource));
    QVERIFY2(settingsSource.contains(QStringLiteral("preferredFixedFontFamily"))
            && settingsSource.contains(QStringLiteral("QFontDatabase::isFixedPitch")),
        "The configurable font must resolve to a concrete installed fixed-width family");
    QVERIFY2(settingsSource.contains(QStringLiteral("appearance/fontFamily")),
        "The configured font family must persist through QSettings");

    QString qmlMain;
    QVERIFY(readSourceFile(QStringLiteral("qml/Main.qml"), qmlMain));
    QVERIFY2(qmlMain.contains(
                 QStringLiteral("font.family: root.settingsViewModel.effectiveFontFamily")),
        "The existing QML window must update when the configured font changes");

    QString settingsQml;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/settings/SettingsView.qml"), settingsQml));
    QVERIFY2(!settingsQml.contains(QStringLiteral("System monospace"))
            && settingsQml.contains(
                QStringLiteral("model: root.viewModel.availableFontFamilies")),
        "The font selector must display concrete installed family names only");

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
            qPrintable(QStringLiteral("%1 must inherit the application fixed-width font")
                           .arg(file.fileName())));
    }
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

void ArchitectureBoundariesTest::translationsDoNotReferenceLegacyFacade()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("i18n/mqtt_plus_zh_CN.ts"), source));

    const QStringList forbiddenTokens {
        QStringLiteral("AppFacade"),
        QStringLiteral("appfacade"),
        QStringLiteral("type=\"vanished\""),
        QStringLiteral("type=\"obsolete\""),
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
    QVERIFY(workbenchSource.contains(QStringLiteral("subscriptionPaneWidth: root.preferences.subscriptionPaneWidth")));
    QVERIFY(workbenchSource.contains(QStringLiteral("setWorkbenchLayout")));
    QVERIFY(workbenchSource.contains(QStringLiteral("function persistLayout()")));
    QVERIFY2(!workbenchSource.contains(
                 QStringLiteral("target: root.viewModel\n        enabled: root.active")),
        "WorkbenchView must keep session status transitions active while another page is visible");
    QVERIFY2(workbenchSource.contains(QStringLiteral("function onSessionStatusChanged()")),
        "WorkbenchView must keep handling background connection status transitions");

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
    QVERIFY2(panelSource.contains(QStringLiteral("function onCurrentSessionChanged()"))
            && panelSource.contains(QStringLiteral("root.inspectorSessionId"))
            && panelSource.contains(QStringLiteral("currentSessionId !== root.inspectorSessionId")),
        "Changing session identity must close the previous inspector without reacting to ordinary session refreshes");

    QString streamSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/EventStreamView.qml"), streamSource));
    QVERIFY(streamSource.contains(QStringLiteral("MessageFilterPopover")));
    QVERIFY(streamSource.contains(QStringLiteral("filteredMessageCount")));
    QVERIFY(streamSource.contains(QStringLiteral("totalMessageCount")));
    QVERIFY2(streamSource.contains(QStringLiteral("root.viewModel.displayTotalMessageCount")),
        "The unfiltered message badge should use the coalesced UI-facing session total");
    QVERIFY2(streamSource.contains(
                 QStringLiteral(".arg(root.streamModel.totalMessageCount)")),
        "Filtered and total message counts must use the same loaded-model scope");
    QVERIFY(streamSource.contains(QStringLiteral("filterSummaryText")));
    QVERIFY(streamSource.contains(QStringLiteral("Accessible.role: Accessible.Button")));
    QVERIFY(streamSource.contains(QStringLiteral("Keys.onPressed")));
    QVERIFY(streamSource.contains(QStringLiteral("streamActionsMenu.openForItem(streamActionsButton)")));
    QVERIFY(streamSource.contains(QStringLiteral("accessibleName: qsTr(\"More message actions\")")));
    QVERIFY2(streamSource.contains(QStringLiteral("const searchPending = messageSearchDebounce.running"))
            && streamSource.contains(QStringLiteral("!root.active && searchPending"))
            && streamSource.contains(
                QStringLiteral("setMessageSearchText(root.pendingMessageSearchText)")),
        "Leaving the workbench must flush a pending debounced message search");
    QVERIFY2(streamSource.contains(QStringLiteral("AppEmptyState {")),
        "The message workspace must guide users when no rows are visible");
    QVERIFY2(streamSource.contains(QStringLiteral("id: clearMessagesDialog")),
        "Clearing message history must require confirmation");
    QVERIFY2(streamSource.contains(QStringLiteral("? 12"))
            && streamSource.contains(QStringLiteral(": 3")),
        "Large payloads must stay clamped until the row is selected or inspected");
    QVERIFY2(streamSource.contains(QStringLiteral("required property int payloadDisplayMode"))
            && streamSource.contains(QStringLiteral("? 64"))
            && !streamSource.contains(QStringLiteral("2147483647")),
        "The always-expanded list mode must remain bounded; full payloads belong in the inspector");
    QVERIFY2(streamSource.contains(QStringLiteral("property int streamRevision"))
            && streamSource.contains(QStringLiteral("indexOfHistoryId(anchorHistoryId)")),
        "Older-history paging must restore a stable row anchor across the bounded render window");
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
            && workbenchSource.contains(QStringLiteral("root.viewModel.incomingByteRate"))
            && workbenchSource.contains(QStringLiteral("root.viewModel.outgoingByteRate"))
            && !workbenchSource.contains(QStringLiteral("currentIncomingByteRate"))
            && !workbenchSource.contains(QStringLiteral("currentOutgoingByteRate"))
            && !workbenchSource.contains(QStringLiteral("currentIncomingMessageRate"))
            && !workbenchSource.contains(QStringLiteral("currentOutgoingMessageRate"))
            && workbenchSource.contains(QStringLiteral("id: trafficStatusGroup"))
            && workbenchSource.contains(QStringLiteral("id: incomingTrafficStatus"))
            && workbenchSource.contains(QStringLiteral("id: outgoingTrafficStatus"))
            && workbenchSource.contains(QStringLiteral("incomingTrafficActive"))
            && workbenchSource.contains(QStringLiteral("outgoingTrafficActive"))
            && workbenchSource.contains(QStringLiteral("themePalette.messageTitle"))
            && workbenchSource.contains(QStringLiteral("themePalette.eventTitle"))
            && workbenchSource.contains(QStringLiteral("text: qsTr(\"↓\")"))
            && workbenchSource.contains(QStringLiteral("text: qsTr(\"↑\")"))
            && !workbenchSource.contains(QStringLiteral("text: qsTr(\"RX\")"))
            && !workbenchSource.contains(QStringLiteral("text: qsTr(\"TX\")"))
            && !workbenchSource.contains(QStringLiteral("compactMessageRate"))
            && workbenchSource.contains(QStringLiteral("return qsTr(\"/s\")"))
            && !workbenchSource.contains(QStringLiteral("FPS"))
            && workbenchSource.count(QStringLiteral("height: 20")) == 2
            && workbenchSource.contains(QStringLiteral("spacing: 8"))
            && !workbenchSource.contains(QStringLiteral("Layout.preferredHeight: 10"))
            && !workbenchSource.contains(QStringLiteral("anchors.verticalCenter: parent.verticalCenter"))
            && !workbenchSource.contains(QStringLiteral("width: 126"))
            && !workbenchSource.contains(QStringLiteral("horizontalAlignment: Text.AlignRight"))
            && !workbenchSource.contains(QStringLiteral("rowSpacing:")),
        "The workbench must expose aggregate traffic and history state");
    QVERIFY2(workbenchSource.contains(QStringLiteral("text: root.liveConnectionStatusText"))
            && workbenchSource.contains(QStringLiteral("connectionEndpointText"))
            && workbenchSource.contains(QStringLiteral("text: root.connectionEndpointText"))
            && workbenchSource.contains(QStringLiteral("qsTr(\"%1:%2\")"))
            && workbenchSource.contains(QStringLiteral("root.session.host"))
            && workbenchSource.contains(QStringLiteral("root.session.port"))
            && workbenchSource.contains(QStringLiteral("connectedAtMs"))
            && workbenchSource.contains(QStringLiteral("connectionStartedAtMs")),
        "The bottom status bar must include the endpoint plus live connection duration or timeout context");

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
    QVERIFY2(source.contains(QStringLiteral("control.payloadTextMaximumHeight"))
            && source.contains(QStringLiteral("id: payloadScroll")),
        "The payload document must use a bounded internal scroll area");
    QVERIFY2(source.contains(QStringLiteral("control.parsedTextMaximumHeight"))
            && source.contains(QStringLiteral("id: parsedResultScroll")),
        "The parsed-result document must use a bounded internal scroll area");
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

    const int resetPosition = source.indexOf(QStringLiteral("function resetStreamPosition()"));
    const int nextResetFunction = source.indexOf(
        QStringLiteral("function requestFollowScroll()"),
        resetPosition);
    QVERIFY(resetPosition >= 0);
    QVERIFY(nextResetFunction > resetPosition);
    const QString resetPositionSource = source.mid(
        resetPosition,
        nextResetFunction - resetPosition);
    QVERIFY2(!resetPositionSource.contains(QStringLiteral("root.clearMessageSelection()")),
        "Generic stream-position resets must not desynchronize list selection from an open inspector");
}

void ArchitectureBoundariesTest::messageInspectorUsesLeftEdgeShadow()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/MessageInspector.qml"), source));

    QVERIFY2(!source.contains(QStringLiteral("import QtQuick.Effects"))
            && !source.contains(QStringLiteral("layer.effect:")),
        "The inspector must not allocate a full-panel offscreen effect merely to draw elevation");
    QVERIFY2(source.contains(QStringLiteral("id: inspectorEdgeShadow"))
            && source.contains(QStringLiteral("anchors.right: inspectorSurface.left"))
            && source.contains(QStringLiteral("orientation: Gradient.Horizontal")),
        "The inspector should render a narrow left-edge gradient toward the message list");
}

void ArchitectureBoundariesTest::qmlMotionPolicyUsesSharedTokens()
{
    QString uiSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/AppUi.qml"), uiSource));
    const QStringList durationTokens {
        QStringLiteral("motionMicroDuration"),
        QStringLiteral("motionPopoverEnterDuration"),
        QStringLiteral("motionPopoverExitDuration"),
        QStringLiteral("motionPanelDuration"),
        QStringLiteral("motionModalEnterDuration"),
        QStringLiteral("motionModalExitDuration"),
    };
    for (const QString &token : durationTokens) {
        QVERIFY2(uiSource.contains(token + QStringLiteral(": root.animationsEnabled ?")),
            qPrintable(QStringLiteral("%1 must snap to zero duration when animations are disabled").arg(token)));
    }

    QString dialogSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/components/AppDialog.qml"), dialogSource));
    QVERIFY2(dialogSource.contains(QStringLiteral("OpacityAnimator"))
            && dialogSource.contains(QStringLiteral("ScaleAnimator"))
            && dialogSource.contains(QStringLiteral("motionModalEnterDuration"))
            && dialogSource.contains(QStringLiteral("motionModalExitDuration")),
        "Application dialogs must share the lightweight modal transition");

    QString popoverSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/components/AppPopover.qml"), popoverSource));
    QVERIFY2(popoverSource.contains(QStringLiteral("OpacityAnimator"))
            && popoverSource.contains(QStringLiteral("ScaleAnimator"))
            && popoverSource.contains(QStringLiteral("motionPopoverEnterDuration"))
            && popoverSource.contains(QStringLiteral("motionPopoverExitDuration")),
        "Application popovers must share the lightweight transient-surface transition");

    QString overlaySource;
    QVERIFY(readSourceFile(QStringLiteral("qml/components/AppDialogOverlay.qml"), overlaySource));
    QVERIFY2(overlaySource.contains(QStringLiteral("opacity: 1"))
            && overlaySource.contains(QStringLiteral("target: control")),
        "Disabling animations must preserve the final modal overlay while its optional fade targets the overlay itself");

    QString settingsSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/settings/SettingsView.qml"), settingsSource));
    QVERIFY2(settingsSource.contains(QStringLiteral("property real presentationProgress"))
            && settingsSource.contains(QStringLiteral("settingSwitchAnimation.to = targetProgress"))
            && settingsSource.contains(QStringLiteral("controlsGlobalMotion: true"))
            && settingsSource.contains(QStringLiteral("duration: settingSwitch.ui.motionMicroDuration")),
        "Setting switches must use shared timing and explicitly snap the global motion control itself");

    QString mainSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/Main.qml"), mainSource));
    QVERIFY2(mainSource.contains(QStringLiteral("id: railHoverBackground"))
            && mainSource.contains(QStringLiteral("color: railButton.ui.themePalette.selectedItemBg"))
            && mainSource.contains(QStringLiteral("railHoverAnimation.stop()"))
            && mainSource.contains(QStringLiteral("railHoverAnimation.to = railHoverBackground.targetOpacity"))
            && mainSource.contains(QStringLiteral("property: \"presentationOpacity\""))
            && mainSource.contains(QStringLiteral("NumberAnimation"))
            && !mainSource.contains(QStringLiteral("ColorAnimation")),
        "The navigation rail hover must fade a visible overlay and snap it to the target when motion is disabled");

    QString composerSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/PublishComposer.qml"), composerSource));
    QVERIFY2(composerSource.contains(QStringLiteral("property real expansionProgress"))
            && composerSource.contains(QStringLiteral("SplitView.preferredHeight: root.animatedComposerHeight"))
            && composerSource.contains(QStringLiteral("root.updateExpansion(false)"))
            && composerSource.contains(QStringLiteral(
                "root.ui.materialIcon(root.expanded ? \"chevron-down\" : \"chevron-up\")")),
        "The publish composer is the bounded layout-animation exception and must still snap when motion is disabled");

    QString workbenchSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/WorkbenchView.qml"), workbenchSource));
    QVERIFY2(workbenchSource.contains(QStringLiteral("function connectionPaneTargetWidth(collapsed)"))
            && workbenchSource.contains(QStringLiteral(
                "const targetWidth = root.connectionPaneTargetWidth(root.connectionPaneCollapsed);"))
            && workbenchSource.contains(QStringLiteral("onConnectionPaneAutoHiddenChanged: root.settleConnectionPaneWidth()"))
            && workbenchSource.contains(QStringLiteral("onEffectiveExpandedConnectionPaneWidthChanged: root.settleConnectionPaneWidth()"))
            && !workbenchSource.contains(QStringLiteral("targetConnectionPaneWidth"))
            && !workbenchSource.contains(QStringLiteral("Behavior on connectionPaneWidth")),
        "Connection-pane commands must resolve the new collapsed state directly while responsive changes snap");

    QString streamSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/EventStreamView.qml"), streamSource));
    QVERIFY2(streamSource.contains(QStringLiteral("property bool presentationVisible"))
            && streamSource.contains(QStringLiteral("Accessible.ignored: !targetShown"))
            && !streamSource.contains(QStringLiteral("Behavior on anchors.bottomMargin")),
        "The follow affordance must finish its exit without animating layout or remaining interactive");
    QVERIFY2(streamSource.contains(QStringLiteral("AppDialog {\n        id: clearMessagesDialog")),
        "Message-history confirmation must use the shared dialog motion");

    QString inspectorSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/MessageInspector.qml"), inspectorSource));
    QVERIFY2(inspectorSource.contains(QStringLiteral("property real revealProgress"))
            && inspectorSource.contains(QStringLiteral("visible: control.opened || control.revealProgress > 0.0"))
            && inspectorSource.contains(QStringLiteral("enabled: control.opened"))
            && inspectorSource.contains(QStringLiteral("inspectorRevealAnimation.to = targetProgress")),
        "The inspector must remain rendered for exit motion while disabling interaction immediately");

    QString iconButtonSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/components/AppIconButton.qml"), iconButtonSource));
    QVERIFY2(iconButtonSource.contains(QStringLiteral("control.forceActive ? control.hoverBg : control.effectiveRestBg"))
            && iconButtonSource.contains(QStringLiteral("control.down || control.hovered) ? 1 : 0"))
            && !iconButtonSource.contains(QStringLiteral("control.down || control.hovered || control.forceActive")),
        "Reusable icon-button delegates must apply semantic active state immediately and animate pointer state only");

    QString emptyStateSource;
    QVERIFY(readSourceFile(QStringLiteral("qml/components/AppEmptyState.qml"), emptyStateSource));
    QVERIFY2(emptyStateSource.contains(QStringLiteral("visible: root.ui.animationsEnabled"))
            && emptyStateSource.contains(QStringLiteral("visible: !root.ui.animationsEnabled")),
        "Loading feedback must retain a visible static fallback when motion is disabled");
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
