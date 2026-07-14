pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: root

    required property AppUi ui
    required property var viewModel
    required property string fontFamily
    required property bool autoCollapseConnectionListOnConnect
    property bool connectionPaneCollapsed: false
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
    property int subscriptionPaneWidth: 320
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

    Layout.fillWidth: true
    Layout.fillHeight: true

    Behavior on connectionPaneWidth {
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
        anchors.bottom: parent.bottom
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
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: root.connectionPaneWidth
        z: 2
        orientation: Qt.Horizontal

        handle: Item {
            implicitWidth: workbenchSplit.orientation === Qt.Horizontal ? 6 : workbenchSplit.width
            implicitHeight: workbenchSplit.orientation === Qt.Horizontal ? workbenchSplit.height : 6

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
            session: root.session
            status: root.status
            publishStatus: root.viewModel.publishStatus
            publisher: root.viewModel.publisher
            fontFamily: root.fontFamily
            SplitView.fillWidth: true
            SplitView.fillHeight: true
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
