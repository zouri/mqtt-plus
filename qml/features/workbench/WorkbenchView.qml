pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

Item {
    id: root

    required property AppUi ui
    required property bool active
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
    readonly property int effectiveExpandedConnectionPaneWidth: root.compactPaneWidths ? root.compactConnectionPaneWidth : root.expandedConnectionPaneWidth
    property real connectionPaneWidth: 0
    readonly property int subscriptionPaneMinWidth: 300
    readonly property int subscriptionPaneMaxWidth: 520
    property int subscriptionPaneWidth: root.preferences.subscriptionPaneWidth
    property bool layoutReady: false
    readonly property int compactSubscriptionPaneWidth: 286
    readonly property int effectiveSubscriptionPaneWidth: root.compactPaneWidths ? root.compactSubscriptionPaneWidth : root.subscriptionPaneWidth
    property bool collapseConnectionPaneOnConnect: false
    property int trackedConnectionSessionIndex: -1
    property string trackedConnectionState: ""
    property string pendingSessionEditorMode: ""
    property int pendingSessionEditorIndex: -1
    property string pendingSubscriptionDialogMode: ""
    property int pendingSubscriptionIndex: -1
    readonly property double incomingByteRate: root.viewModel.incomingByteRate
    readonly property double outgoingByteRate: root.viewModel.outgoingByteRate
    readonly property var messagePressure: root.viewModel.messagePressure
    readonly property string messagePressureState: String(root.messagePressure.state || "normal")
    readonly property bool messagePressureVisible: root.messagePressureState !== "normal"
                                                    || Boolean(root.messagePressure.storageDegraded)
    readonly property bool incomingTrafficActive: root.incomingByteRate > 0
    readonly property bool outgoingTrafficActive: root.outgoingByteRate > 0
    property double nowMs: Date.now()
    readonly property string connectionEndpointText: root.session.host
                                                        ? qsTr("%1:%2").arg(root.session.host).arg(root.session.port || "-")
                                                        : ""
    readonly property string liveConnectionStatusText: root.connectionStatusText()

    Layout.fillWidth: true
    Layout.fillHeight: true

    function connectionPaneTargetWidth(collapsed) {
        if (root.connectionPaneAutoHidden) {
            return 0;
        }
        return collapsed
                ? root.collapsedConnectionPaneWidth
                : root.effectiveExpandedConnectionPaneWidth;
    }

    function settleConnectionPaneWidth() {
        connectionPaneAnimation.stop();
        root.connectionPaneWidth = root.connectionPaneTargetWidth(root.connectionPaneCollapsed);
    }

    function updateConnectionPaneWidth(animate) {
        const targetWidth = root.connectionPaneTargetWidth(root.connectionPaneCollapsed);
        connectionPaneAnimation.stop();
        if (!animate
                || !root.layoutReady
                || !root.active
                || !root.visible
                || root.connectionPaneAutoHidden
                || !root.ui.animationsEnabled
                || root.ui.motionPanelDuration <= 0
                || Math.abs(root.connectionPaneWidth - targetWidth) < 0.5) {
            root.connectionPaneWidth = targetWidth;
            return;
        }
        connectionPaneAnimation.to = targetWidth;
        connectionPaneAnimation.restart();
    }

    NumberAnimation {
        id: connectionPaneAnimation

        target: root
        property: "connectionPaneWidth"
        duration: root.ui.motionPanelDuration
        easing.type: root.ui.motionEnterEasing
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

    function compactByteRate(bytesPerSecond) {
        const numericRate = Number(bytesPerSecond);
        const rate = Number.isFinite(numericRate) ? Math.max(0, numericRate) : 0;
        if (rate === 0) {
            return qsTr("/s");
        }
        if (rate < 1000) {
            return qsTr("%1 B/s").arg(rate.toFixed(0));
        }
        if (rate < 1000 * 1000) {
            const kilobytes = rate / 1000;
            return qsTr("%1 KB/s").arg(kilobytes.toFixed(kilobytes < 10 ? 1 : 0));
        }
        if (rate < 1000 * 1000 * 1000) {
            const megabytes = rate / 1000 / 1000;
            return qsTr("%1 MB/s").arg(megabytes.toFixed(megabytes < 10 ? 1 : 0));
        }
        const gigabytes = rate / 1000 / 1000 / 1000;
        return qsTr("%1 GB/s").arg(gigabytes.toFixed(gigabytes < 10 ? 1 : 0));
    }

    function compactBytes(bytes) {
        const numericBytes = Number(bytes);
        const value = Number.isFinite(numericBytes) ? Math.max(0, numericBytes) : 0;
        if (value < 1024) {
            return qsTr("%1 B").arg(value.toFixed(0));
        }
        if (value < 1024 * 1024) {
            return qsTr("%1 KiB").arg((value / 1024).toFixed(value < 10 * 1024 ? 1 : 0));
        }
        return qsTr("%1 MiB").arg((value / 1024 / 1024).toFixed(value < 10 * 1024 * 1024 ? 1 : 0));
    }

    function pressureStatusLabel() {
        if (root.messagePressureState === "dropping") {
            return qsTr("Dropping");
        }
        if (root.messagePressureState === "degraded") {
            return qsTr("Raw only");
        }
        if (root.messagePressureState === "elevated") {
            return qsTr("High load");
        }
        return qsTr("Storage error");
    }

    function pressureStatusDescription() {
        if (root.messagePressureState === "dropping") {
            return qsTr("The storage queue is full. Some messages are being dropped.");
        }
        if (String(root.messagePressure.captureMode || "full") === "raw_only") {
            return qsTr("Only raw messages are being saved until the queues recover.");
        }
        return qsTr("Message storage reported an error.");
    }

    function pressureTextColor() {
        return root.messagePressureState === "dropping"
                || Boolean(root.messagePressure.storageDegraded)
                ? root.ui.themePalette.errorText
                : root.ui.themePalette.warningText;
    }

    function pressureBackgroundColor() {
        return root.messagePressureState === "dropping"
                || Boolean(root.messagePressure.storageDegraded)
                ? root.ui.themePalette.errorBg
                : root.ui.themePalette.warningBg;
    }

    function connectionStatusText() {
        const state = root.status.state || "idle";
        if (state === "connected" && Number(root.status.connectedAtMs || 0) > 0) {
            return qsTr("Connected · %1").arg(root.compactDuration(root.nowMs - Number(root.status.connectedAtMs)));
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
        root.preferences.setWorkbenchLayout(root.subscriptionPaneWidth, sessionActivityPanel.composerHeight, root.connectionPaneCollapsed);
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
        root.settleConnectionPaneWidth();
        if (root.active) {
            root.resetStreamPosition();
        }
        root.layoutReady = true;
    }

    onActiveChanged: {
        root.settleConnectionPaneWidth();
        if (root.active) {
            root.trackedConnectionSessionIndex = root.viewModel.currentSessionIndex;
            root.trackedConnectionState = root.status.state || "";
        }
    }

    onVisibleChanged: root.settleConnectionPaneWidth()
    onConnectionPaneAutoHiddenChanged: root.settleConnectionPaneWidth()
    onEffectiveExpandedConnectionPaneWidthChanged: root.settleConnectionPaneWidth()
    onConnectionPaneCollapsedChanged: {
        root.scheduleLayoutSave();
        root.updateConnectionPaneWidth(true);
    }
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
        running: root.active && root.visible
        triggeredOnStart: true
        onTriggered: root.nowMs = Date.now()
    }

    onAutoCollapseConnectionListOnConnectChanged: {
        if (!root.autoCollapseConnectionListOnConnect) {
            root.collapseConnectionPaneOnConnect = false;
        }
    }

    Connections {
        target: root.viewModel

        function onMessageStreamChanged() {
            if (root.active) {
                root.resetStreamPosition();
            }
        }

        function onMessageStreamRowsAppended(count) {
            if (root.active) {
                root.noteStreamRowsAppended(count);
            }
        }

        function onSessionStatusChanged() {
            root.handleConnectionStateChanged();
        }

        function onCurrentSessionIndexChanged() {
            root.collapseConnectionPaneOnConnect = false;
        }

        function onMessagePressureChanged() {
            if (!root.messagePressureVisible) {
                pressurePopover.close();
            }
        }
    }

    Connections {
        target: root.ui

        function onAnimationsEnabledChanged() {
            if (!root.ui.animationsEnabled) {
                root.settleConnectionPaneWidth();
            }
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
                    active: root.active
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
            active: root.active
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
        height: 28
        color: root.ui.themePalette.innerPanelBg

        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: root.ui.themePalette.separator
            Accessible.ignored: true
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 8

            Rectangle {
                Layout.preferredWidth: 7
                Layout.preferredHeight: 7
                radius: 4
                color: root.ui.stateColor(root.status.hasError ? "error" : (root.status.state || "idle"))
                Accessible.ignored: true
            }

            Label {
                Layout.maximumWidth: 180
                text: root.session.name || qsTr("No session")
                color: root.ui.textMuted
                font.pixelSize: 10
                elide: Label.ElideRight
                visible: root.width >= 760
            }

            Label {
                Layout.maximumWidth: 220
                text: root.connectionEndpointText
                color: root.ui.textMuted
                font.pixelSize: 10
                elide: Label.ElideMiddle
                visible: root.connectionEndpointText.length > 0 && root.width >= 700
            }

            Label {
                Layout.maximumWidth: 190
                text: root.liveConnectionStatusText
                color: root.ui.stateColor(root.status.hasError ? "error" : (root.status.state || "idle"))
                font.pixelSize: 10
                font.bold: root.status.state === "connected" || root.status.state === "connecting"
                elide: Label.ElideRight
            }

            Rectangle {
                Layout.preferredWidth: 1
                Layout.preferredHeight: 12
                color: root.ui.themePalette.separator
            }

            Row {
                id: trafficStatusGroup

                Layout.alignment: Qt.AlignVCenter
                spacing: 8

                RowLayout {
                    id: incomingTrafficStatus

                    height: 20
                    spacing: 4

                    Rectangle {
                        Layout.preferredWidth: 5
                        Layout.preferredHeight: 5
                        radius: 3
                        color: root.incomingTrafficActive
                               ? root.ui.themePalette.messageTitle
                               : root.ui.themePalette.textSubtle
                        Accessible.ignored: true
                    }

                    Label {
                        text: qsTr("↓")
                        color: root.incomingTrafficActive
                               ? root.ui.themePalette.messageTitle
                               : root.ui.textMuted
                        font.pixelSize: 10
                        font.bold: root.incomingTrafficActive
                    }

                    Label {
                        text: root.compactByteRate(root.incomingByteRate)
                        color: root.incomingTrafficActive ? root.ui.textStrong : root.ui.themePalette.textSubtle
                        font.pixelSize: 10
                        font.bold: root.incomingTrafficActive
                    }
                }

                RowLayout {
                    id: outgoingTrafficStatus

                    height: 20
                    spacing: 4

                    Rectangle {
                        Layout.preferredWidth: 5
                        Layout.preferredHeight: 5
                        radius: 3
                        color: root.outgoingTrafficActive
                               ? root.ui.themePalette.eventTitle
                               : root.ui.themePalette.textSubtle
                        Accessible.ignored: true
                    }

                    Label {
                        text: qsTr("↑")
                        color: root.outgoingTrafficActive
                               ? root.ui.themePalette.eventTitle
                               : root.ui.textMuted
                        font.pixelSize: 10
                        font.bold: root.outgoingTrafficActive
                    }

                    Label {
                        text: root.compactByteRate(root.outgoingByteRate)
                        color: root.outgoingTrafficActive ? root.ui.textStrong : root.ui.themePalette.textSubtle
                        font.pixelSize: 10
                        font.bold: root.outgoingTrafficActive
                    }
                }
            }

            Item {
                Layout.fillWidth: true
            }

            Button {
                id: pressureStatusButton

                visible: root.messagePressureVisible
                Layout.preferredWidth: pressureStatusContent.implicitWidth + leftPadding + rightPadding
                Layout.preferredHeight: 20
                leftPadding: 7
                rightPadding: 7
                topPadding: 0
                bottomPadding: 0
                Accessible.name: qsTr("Message processing status: %1").arg(root.pressureStatusLabel())

                contentItem: RowLayout {
                    id: pressureStatusContent

                    spacing: 5

                    Rectangle {
                        Layout.preferredWidth: 5
                        Layout.preferredHeight: 5
                        Layout.alignment: Qt.AlignVCenter
                        radius: 3
                        color: root.pressureTextColor()
                        Accessible.ignored: true
                    }

                    Label {
                        text: root.pressureStatusLabel()
                        color: root.pressureTextColor()
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                background: Rectangle {
                    radius: 6
                    color: root.pressureBackgroundColor()
                    border.color: root.pressureTextColor()
                    border.width: 1
                }

                AppToolTip {
                    ui: root.ui
                    text: root.pressureStatusDescription()
                    position: AppToolTip.Position.Top
                    active: pressureStatusButton.hovered
                }

                onClicked: pressurePopover.visible ? pressurePopover.close() : pressurePopover.open()
            }

            Label {
                text: qsTr("%1 messages").arg(root.compactMessageCount(root.viewModel.displayTotalMessageCount))
                color: root.ui.textMuted
                font.pixelSize: 10
            }
        }

        AppPopover {
            id: pressurePopover

            ui: root.ui
            width: 330
            implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
            x: Math.max(8, workbenchStatusBar.width - width - 8)
            y: -implicitHeight + 1
            padding: 12
            modal: false
            focus: true
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

            background: Rectangle {
                radius: 8
                color: root.ui.themePalette.dialogBg
                border.color: root.ui.themePalette.dialogBorder
            }

            contentItem: ColumnLayout {
                spacing: 7

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Message processing")
                        color: root.ui.textStrong
                        font.pixelSize: 12
                        font.bold: true
                    }

                    AppBadge {
                        ui: root.ui
                        label: root.pressureStatusLabel()
                        badgeRadius: 6
                        horizontalPadding: 7
                        verticalPadding: 2
                        badgeBg: root.pressureBackgroundColor()
                        badgeBorder: root.pressureTextColor()
                        badgeText: root.pressureTextColor()
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: root.pressureStatusDescription()
                    color: root.pressureTextColor()
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: root.ui.themePalette.separator
                    Accessible.ignored: true
                }

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Writer queue")
                        color: root.ui.textMuted
                        font.pixelSize: 10
                    }

                    Label {
                        text: qsTr("%1 messages · %2")
                                  .arg(Number(root.messagePressure.backlog || 0))
                                  .arg(root.compactBytes(root.messagePressure.backlogBytes))
                        color: root.ui.textStrong
                        font.pixelSize: 10
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Parser queue")
                        color: root.ui.textMuted
                        font.pixelSize: 10
                    }

                    Label {
                        text: qsTr("%1 messages · %2")
                                  .arg(Number(root.messagePressure.parseBacklog || 0))
                                  .arg(root.compactBytes(root.messagePressure.parseBacklogBytes))
                        color: root.ui.textStrong
                        font.pixelSize: 10
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Dropped")
                        color: root.ui.textMuted
                        font.pixelSize: 10
                    }

                    Label {
                        text: qsTr("%1 raw · %2 parse · %3 results")
                                  .arg(Number(root.messagePressure.dropped || 0))
                                  .arg(Number(root.messagePressure.parseDropped || 0))
                                  .arg(Number(root.messagePressure.parseResultDropped || 0))
                        color: root.ui.textStrong
                        font.pixelSize: 10
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Work shed")
                        color: root.ui.textMuted
                        font.pixelSize: 10
                    }

                    Label {
                        text: qsTr("%1 capture · %2 parse")
                                  .arg(Number(root.messagePressure.captureFiltered || 0))
                                  .arg(Number(root.messagePressure.parseSkippedPressure || 0))
                        color: root.ui.textStrong
                        font.pixelSize: 10
                    }
                }

                Label {
                    visible: String(root.messagePressure.lastError || "").length > 0
                    Layout.fillWidth: true
                    text: String(root.messagePressure.lastError || "")
                    color: root.ui.themePalette.errorText
                    font.pixelSize: 10
                    wrapMode: Text.Wrap
                }
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
