pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

Item {
    id: root

    required property AppUi ui
    required property var viewModel
    required property var settingsViewModel
    required property var preferences
    required property var eventHistory
    required property var sessionService
    required property var subscriptionService
    required property string fontFamily
    required property bool autoCollapseConnectionListOnConnect
    property bool connectionPaneCollapsed: root.preferences.connectionPaneCollapsed
    readonly property var session: root.viewModel.currentSession
    readonly property var status: root.viewModel.sessionStatus
    readonly property int collapsedConnectionPaneWidth: 34
    readonly property int connectionPaneVisualCollapseWidth: 84
    readonly property int expandedConnectionPaneWidth: 208
    readonly property int compactConnectionPaneWidth: 188
    readonly property color connectionPaneEdgeColor: root.ui.themePalette.panelBorder
    readonly property bool compactPaneWidths: root.width <= 1208
    readonly property bool connectionPaneAutoHidden: root.width <= 988
    readonly property bool subscriptionPaneAutoHidden: root.width <= 708
    readonly property int effectiveExpandedConnectionPaneWidth: root.compactPaneWidths
                                                                ? root.compactConnectionPaneWidth
                                                                : root.expandedConnectionPaneWidth
    property real connectionPaneWidth: root.connectionPaneAutoHidden
                                       ? 0
                                       : (root.connectionPaneCollapsed
                                          ? root.collapsedConnectionPaneWidth
                                          : root.effectiveExpandedConnectionPaneWidth)
    readonly property int subscriptionPaneMinWidth: 300
    readonly property int subscriptionPaneMaxWidth: 520
    property int subscriptionPaneWidth: root.preferences.subscriptionPaneWidth
    property bool layoutReady: false
    readonly property int compactSubscriptionPaneWidth: 286
    readonly property int effectiveSubscriptionPaneWidth: root.compactPaneWidths
                                                          ? root.compactSubscriptionPaneWidth
                                                          : root.subscriptionPaneWidth
    property bool collapseConnectionPaneOnConnect: false
    property int trackedConnectionSessionIndex: -1
    property string trackedConnectionState: ""
    property string pendingSessionEditorMode: ""
    property int pendingSessionEditorIndex: -1
    property string pendingSubscriptionDialogMode: ""
    property int pendingSubscriptionIndex: -1
    property real incomingMessageRate: 0
    property real outgoingMessageRate: 0
    property double nowMs: Date.now()
    readonly property string liveConnectionStatusText: root.connectionStatusText()

    Layout.fillWidth: true
    Layout.fillHeight: true

    Behavior on connectionPaneWidth {
        enabled: root.ui.animationsEnabled

        NumberAnimation {
            duration: 180
            easing.type: Easing.OutCubic
        }
    }

    function resetStreamPosition() {
        sessionActivityPanel.resetStreamPosition();
    }

    function noteStreamRowsAppended(count) {
        sessionActivityPanel.noteStreamRowsAppended(count);
    }

    function focusMessageSearch() {
        sessionActivityPanel.focusMessageSearch();
    }

    function toggleConnectionPane() {
        if (!root.connectionPaneAutoHidden) {
            root.connectionPaneCollapsed = !root.connectionPaneCollapsed;
        }
    }

    function createSession() {
        root.openSessionEditorForCreate();
    }

    function refreshMessageRates() {
        root.incomingMessageRate = root.viewModel.currentIncomingMessageRate();
        root.outgoingMessageRate = root.viewModel.currentOutgoingMessageRate();
    }

    function compactMessageCount(count) {
        const numericCount = Number(count || 0);
        if (numericCount < 1000) {
            return String(numericCount);
        }
        if (numericCount < 1000000) {
            return qsTr("%1K").arg((numericCount / 1000).toFixed(numericCount < 10000 ? 1 : 0));
        }
        return qsTr("%1M").arg((numericCount / 1000000).toFixed(numericCount < 10000000 ? 1 : 0));
    }

    function compactDuration(milliseconds) {
        const totalSeconds = Math.max(0, Math.floor(milliseconds / 1000));
        if (totalSeconds < 60) {
            return qsTr("%1s").arg(totalSeconds);
        }
        const totalMinutes = Math.floor(totalSeconds / 60);
        if (totalMinutes < 60) {
            return qsTr("%1m").arg(totalMinutes);
        }
        const hours = Math.floor(totalMinutes / 60);
        return qsTr("%1h %2m").arg(hours).arg(totalMinutes % 60);
    }

    function connectionStatusText() {
        const state = root.status.state || "idle";
        if (state === "connected" && Number(root.status.connectedAtMs || 0) > 0) {
            return qsTr("Connected · %1").arg(
                        root.compactDuration(root.nowMs - Number(root.status.connectedAtMs)));
        }
        if (state === "connecting" && Number(root.status.connectionStartedAtMs || 0) > 0) {
            const timeoutMs = Number(root.status.connectTimeoutSeconds || 10) * 1000;
            const remaining = Math.max(0, timeoutMs - (root.nowMs - Number(root.status.connectionStartedAtMs)));
            return qsTr("Connecting · %1 left").arg(root.compactDuration(remaining));
        }
        return root.ui.statusLabel(root.status.hasError ? "error" : state);
    }

    function scheduleLayoutSave() {
        if (root.layoutReady) {
            layoutSaveTimer.restart();
        }
    }

    function persistLayout() {
        if (!root.layoutReady) {
            return;
        }
        layoutSaveTimer.stop();
        root.preferences.setWorkbenchLayout(
            root.subscriptionPaneWidth,
            sessionActivityPanel.composerHeight,
            root.connectionPaneCollapsed);
    }

    function handleConnectionStateChanged() {
        const state = root.status.state || "";
        const sessionIndex = root.viewModel.currentSessionIndex;
        if (root.trackedConnectionSessionIndex !== sessionIndex) {
            root.trackedConnectionSessionIndex = sessionIndex;
            root.trackedConnectionState = state;
            root.collapseConnectionPaneOnConnect = false;
            return;
        }

        if (root.autoCollapseConnectionListOnConnect && root.collapseConnectionPaneOnConnect && state === "connected") {
            root.connectionPaneCollapsed = true;
            root.collapseConnectionPaneOnConnect = false;
        } else if (root.autoCollapseConnectionListOnConnect && (state === "disconnecting" || state === "disconnected") && (root.trackedConnectionState === "connected" || root.trackedConnectionState === "disconnecting")) {
            root.connectionPaneCollapsed = false;
            root.collapseConnectionPaneOnConnect = false;
        } else if (state === "disconnected" || state === "disconnecting") {
            root.collapseConnectionPaneOnConnect = false;
        }
        root.trackedConnectionState = state;
    }

    function openPendingSessionEditor() {
        if (sessionEditorLoader.status !== Loader.Ready) {
            return;
        }

        if (root.pendingSessionEditorMode === "create") {
            sessionEditorLoader.openForCreate();
        } else if (root.pendingSessionEditorMode === "edit") {
            sessionEditorLoader.openForEdit(root.pendingSessionEditorIndex);
        }

        root.pendingSessionEditorMode = "";
        root.pendingSessionEditorIndex = -1;
    }

    function openSessionEditorForCreate() {
        root.pendingSessionEditorMode = "create";
        root.pendingSessionEditorIndex = -1;
        sessionEditorLoader.active = true;
        root.openPendingSessionEditor();
    }

    function openSessionEditorForEdit(index) {
        root.pendingSessionEditorMode = "edit";
        root.pendingSessionEditorIndex = index;
        sessionEditorLoader.active = true;
        root.openPendingSessionEditor();
    }

    function openPendingSubscriptionDialog() {
        if (addSubscriptionDialogLoader.status !== Loader.Ready) {
            return;
        }

        if (root.pendingSubscriptionDialogMode === "create") {
            addSubscriptionDialogLoader.openForCreate();
        } else if (root.pendingSubscriptionDialogMode === "edit") {
            addSubscriptionDialogLoader.openForEdit(root.pendingSubscriptionIndex);
        }

        root.pendingSubscriptionDialogMode = "";
        root.pendingSubscriptionIndex = -1;
    }

    function openSubscriptionDialogForCreate() {
        root.pendingSubscriptionDialogMode = "create";
        root.pendingSubscriptionIndex = -1;
        addSubscriptionDialogLoader.active = true;
        root.openPendingSubscriptionDialog();
    }

    function openSubscriptionDialogForEdit(index) {
        root.pendingSubscriptionDialogMode = "edit";
        root.pendingSubscriptionIndex = index;
        addSubscriptionDialogLoader.active = true;
        root.openPendingSubscriptionDialog();
    }

    Component.onCompleted: {
        root.trackedConnectionSessionIndex = root.viewModel.currentSessionIndex;
        root.trackedConnectionState = root.status.state || "";
        root.resetStreamPosition();
        root.layoutReady = true;
    }

    onConnectionPaneCollapsedChanged: root.scheduleLayoutSave()
    onSubscriptionPaneWidthChanged: root.scheduleLayoutSave()

    Timer {
        id: layoutSaveTimer

        interval: 250
        repeat: false
        onTriggered: root.persistLayout()
    }

    Timer {
        interval: 1000
        repeat: true
        running: root.visible
        triggeredOnStart: true
        onTriggered: {
            root.nowMs = Date.now();
            root.refreshMessageRates();
        }
    }

    onAutoCollapseConnectionListOnConnectChanged: {
        if (!root.autoCollapseConnectionListOnConnect) {
            root.collapseConnectionPaneOnConnect = false;
        }
    }

    Connections {
        target: root.viewModel

        function onMessageStreamChanged() {
            root.resetStreamPosition();
        }

        function onMessageStreamRowsAppended(count) {
            root.noteStreamRowsAppended(count);
        }

        function onSessionEditRequested(index) {
            root.openSessionEditorForEdit(index);
        }

        function onSessionStatusChanged() {
            root.handleConnectionStateChanged();
        }

        function onCurrentSessionIndexChanged() {
            root.collapseConnectionPaneOnConnect = false;
        }
    }

    SessionSidebar {
        anchors.top: parent.top
        anchors.bottom: workbenchStatusBar.top
        anchors.left: parent.left
        width: root.effectiveExpandedConnectionPaneWidth
        z: 0
        ui: root.ui
        viewModel: root.viewModel
        collapsed: root.connectionPaneWidth <= root.connectionPaneVisualCollapseWidth
        visible: !root.connectionPaneAutoHidden
        visibleWidth: root.connectionPaneWidth
        onSessionCreateRequested: root.openSessionEditorForCreate()
        onSessionEditRequested: index => root.openSessionEditorForEdit(index)
        onCollapseRequested: root.connectionPaneCollapsed = true
        onExpandRequested: root.connectionPaneCollapsed = false
    }

    SplitView {
        id: workbenchSplit

        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: workbenchStatusBar.top
        anchors.left: parent.left
        anchors.leftMargin: root.connectionPaneWidth
        z: 2
        orientation: Qt.Horizontal

        handle: Rectangle {
            implicitWidth: workbenchSplit.orientation === Qt.Horizontal ? 6 : workbenchSplit.width
            implicitHeight: workbenchSplit.orientation === Qt.Horizontal ? workbenchSplit.height : 6
            color: root.ui.themePalette.panelBg

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 1
                color: root.ui.themePalette.separator
            }

            HoverHandler {
                id: splitHandleHover
                cursorShape: Qt.SplitHCursor
            }
        }

        Rectangle {
            visible: !root.subscriptionPaneAutoHidden
            SplitView.preferredWidth: visible ? root.effectiveSubscriptionPaneWidth : 0
            SplitView.minimumWidth: visible ? (root.compactPaneWidths ? 276 : root.subscriptionPaneMinWidth) : 0
            SplitView.maximumWidth: visible ? root.subscriptionPaneMaxWidth : 0
            SplitView.fillHeight: true
            color: root.ui.themePalette.panelBg
            onWidthChanged: {
                if (root.layoutReady && visible && width >= root.subscriptionPaneMinWidth) {
                    root.subscriptionPaneWidth = Math.round(width);
                }
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                SessionOverviewPanel {
                    ui: root.ui
                    session: root.session
                    status: root.status
                    viewModel: root.viewModel
                    onSessionEditRequested: index => root.openSessionEditorForEdit(index)
                    onConnectionConnectRequested: root.collapseConnectionPaneOnConnect = root.autoCollapseConnectionListOnConnect
                }

                SubscriptionsPanel {
                    id: subscriptionsPanel
                    ui: root.ui
                    viewModel: root.viewModel
                    subscriptionService: root.subscriptionService
                    onSubscriptionCreateRequested: root.openSubscriptionDialogForCreate()
                    onSubscriptionEditRequested: index => root.openSubscriptionDialogForEdit(index)
                    onReplaceMessageTopicFilter: topic => root.viewModel.setMessageTopicFilter(topic)
                    onAddMessageTopicFilter: topic => root.viewModel.addMessageTopicFilter(topic)
                }
            }
        }

        SessionMessagePanel {
            id: sessionActivityPanel
            ui: root.ui
            viewModel: root.viewModel
            eventHistory: root.eventHistory
            sessionService: root.sessionService
            session: root.session
            status: root.status
            publishStatus: root.viewModel.publishStatus
            publisher: root.viewModel.publisher
            fontFamily: root.fontFamily
            messagePayloadDisplayMode: root.settingsViewModel.messagePayloadDisplayModeIndex
            composerHeight: root.preferences.publishComposerHeight
            onSubscriptionCreateRequested: root.openSubscriptionDialogForCreate()
            onComposerHeightChanged: root.scheduleLayoutSave()
            SplitView.fillWidth: true
            SplitView.fillHeight: true
        }
    }

    Rectangle {
        id: workbenchStatusBar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 24
        color: root.ui.themePalette.innerPanelBg
        border.color: root.ui.themePalette.separator

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 10

            Rectangle {
                Layout.preferredWidth: 7
                Layout.preferredHeight: 7
                radius: 4
                color: root.ui.stateColor(root.status.hasError ? "error" : (root.status.state || "idle"))
                Accessible.ignored: true
            }

            Label {
                Layout.maximumWidth: 240
                text: root.session.name || qsTr("No session")
                color: root.ui.textMuted
                font.pixelSize: 10
                elide: Label.ElideRight
            }

            Label {
                text: root.liveConnectionStatusText
                color: root.ui.stateColor(root.status.hasError ? "error" : (root.status.state || "idle"))
                font.pixelSize: 10
                font.bold: root.status.state === "connected" || root.status.state === "connecting"
            }

            Rectangle {
                Layout.preferredWidth: 1
                Layout.preferredHeight: 12
                color: root.ui.themePalette.separator
            }

            Label {
                text: qsTr("↓ %1/s").arg(Number(root.incomingMessageRate).toFixed(0))
                color: root.incomingMessageRate > 0 ? root.ui.textStrong : root.ui.textMuted
                font.pixelSize: 10
                font.bold: root.incomingMessageRate > 0
            }

            Label {
                text: qsTr("↑ %1/s").arg(Number(root.outgoingMessageRate).toFixed(0))
                color: root.outgoingMessageRate > 0 ? root.ui.textStrong : root.ui.textMuted
                font.pixelSize: 10
                font.bold: root.outgoingMessageRate > 0
            }

            Item {
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("%1 messages").arg(root.compactMessageCount(root.viewModel.totalMessageCount))
                color: root.ui.textMuted
                font.pixelSize: 10
            }
        }
    }

    Loader {
        id: sessionEditorLoader

        active: false
        asynchronous: true

        sourceComponent: Component {
            SessionEditorDialog {
                ui: root.ui
                viewModel: root.viewModel
            }
        }

        function openForCreate() {
            if (status === Loader.Ready) {
                // qmllint disable missing-property
                item.openForCreate();
                // qmllint enable missing-property
            }
        }

        function openForEdit(index) {
            if (status === Loader.Ready) {
                // qmllint disable missing-property
                item.openForEdit(index);
                // qmllint enable missing-property
            }
        }

        onLoaded: root.openPendingSessionEditor()
    }

    Loader {
        id: addSubscriptionDialogLoader

        active: false
        asynchronous: true

        sourceComponent: Component {
            AddSubscriptionDialog {
                ui: root.ui
                viewModel: root.viewModel
            }
        }

        function openForCreate() {
            if (status === Loader.Ready) {
                // qmllint disable missing-property
                item.openForCreate();
                // qmllint enable missing-property
            }
        }

        function openForEdit(index) {
            if (status === Loader.Ready) {
                // qmllint disable missing-property
                item.openForEdit(index);
                // qmllint enable missing-property
            }
        }

        onLoaded: root.openPendingSubscriptionDialog()
    }
}
