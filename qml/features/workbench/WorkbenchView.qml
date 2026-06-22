pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

Item {
    id: root

    required property AppUi ui
    required property var workbench
    required property var scriptLibrary
    required property string fontFamily
    property bool connectionPaneCollapsed: false
    readonly property var session: root.workbench.currentSession
    readonly property var status: root.workbench.sessionStatus
    readonly property int expandedConnectionPaneWidth: 248
    readonly property int subscriptionPaneMinWidth: 280
    readonly property int subscriptionPaneMaxWidth: 520
    property int subscriptionPaneWidth: 360
    property real subscriptionPaneDragBaseWidth: subscriptionPaneWidth
    property string pendingSessionEditorMode: ""
    property int pendingSessionEditorIndex: -1
    property string pendingSubscriptionDialogMode: ""
    property var pendingSubscription: ({})

    Layout.fillWidth: true
    Layout.fillHeight: true

    function resetStreamPosition() {
        sessionActivityPanel.resetStreamPosition();
    }

    function noteStreamRowAppended(row) {
        sessionActivityPanel.noteStreamRowAppended(row);
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
            addSubscriptionDialogLoader.openForEdit(root.pendingSubscription);
        }

        root.pendingSubscriptionDialogMode = "";
        root.pendingSubscription = {};
    }

    function openSubscriptionDialogForCreate() {
        root.pendingSubscriptionDialogMode = "create";
        root.pendingSubscription = {};
        addSubscriptionDialogLoader.active = true;
        root.openPendingSubscriptionDialog();
    }

    function openSubscriptionDialogForEdit(subscription) {
        root.pendingSubscriptionDialogMode = "edit";
        root.pendingSubscription = subscription;
        addSubscriptionDialogLoader.active = true;
        root.openPendingSubscriptionDialog();
    }

    Component.onCompleted: {
        root.resetStreamPosition();
    }

    QtObject {
        id: sessionEditorBridge

        function openForCreate() {
            root.openSessionEditorForCreate();
        }

        function openForEdit(index) {
            root.openSessionEditorForEdit(index);
        }
    }

    QtObject {
        id: addSubscriptionDialogBridge

        function openForCreate() {
            root.openSubscriptionDialogForCreate();
        }

        function openForEdit(subscription) {
            root.openSubscriptionDialogForEdit(subscription);
        }
    }

    Connections {
        target: root.workbench

        function onMessageStreamChanged() {
            root.resetStreamPosition();
        }

        function onMessageStreamRowAppended(row) {
            root.noteStreamRowAppended(row);
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        SessionSidebar {
            ui: root.ui
            workbench: root.workbench
            sessionEditor: sessionEditorBridge
            collapsed: root.connectionPaneCollapsed
            Layout.preferredWidth: root.connectionPaneCollapsed ? 32 : root.expandedConnectionPaneWidth
            Layout.fillHeight: true
            onCollapseRequested: root.connectionPaneCollapsed = true
            onExpandRequested: root.connectionPaneCollapsed = false
        }

        Rectangle {
            Layout.preferredWidth: root.subscriptionPaneWidth
            Layout.maximumWidth: root.subscriptionPaneWidth
            Layout.minimumWidth: root.subscriptionPaneWidth
            Layout.fillHeight: true
            color: root.ui.themePalette.windowBg

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                SessionOverviewPanel {
                    ui: root.ui
                    session: root.session
                    status: root.status
                    workbench: root.workbench
                    sessionEditor: sessionEditorBridge
                }

                SubscriptionsPanel {
                    id: subscriptionsPanel
                    ui: root.ui
                    workbench: root.workbench
                    addSubscriptionDialog: addSubscriptionDialogBridge
                }
            }
        }

        Item {
            id: subscriptionResizeHandle
            Layout.preferredWidth: 10
            Layout.minimumWidth: 10
            Layout.maximumWidth: 10
            Layout.fillHeight: true

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 2
                radius: 1
                color: resizeMouse.containsMouse || subscriptionResizeDrag.active
                       ? root.ui.themePalette.selectedBorder
                       : root.ui.themePalette.separator
            }

            MouseArea {
                id: resizeMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.SplitHCursor
            }

            DragHandler {
                id: subscriptionResizeDrag
                target: null
                yAxis.enabled: false

                onActiveChanged: {
                    if (active) {
                        root.subscriptionPaneDragBaseWidth = root.subscriptionPaneWidth
                    }
                }

                onTranslationChanged: {
                    root.subscriptionPaneWidth = Math.max(
                        root.subscriptionPaneMinWidth,
                        Math.min(
                            root.subscriptionPaneMaxWidth,
                            Math.round(root.subscriptionPaneDragBaseWidth + translation.x)))
                }
            }
        }

        SessionActivityPanel {
            id: sessionActivityPanel
            ui: root.ui
            workbench: root.workbench
            session: root.session
            status: root.status
            publishStatus: root.workbench.publishStatus
            fontFamily: root.fontFamily
        }
    }

    Loader {
        id: sessionEditorLoader

        active: false
        asynchronous: true

        sourceComponent: Component {
            SessionEditorDialog {
                ui: root.ui
                workbench: root.workbench
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
                workbench: root.workbench
                scriptLibrary: root.scriptLibrary
            }
        }

        function openForCreate() {
            if (status === Loader.Ready) {
                // qmllint disable missing-property
                item.openForCreate();
                // qmllint enable missing-property
            }
        }

        function openForEdit(subscription) {
            if (status === Loader.Ready) {
                // qmllint disable missing-property
                item.openForEdit(subscription);
                // qmllint enable missing-property
            }
        }

        onLoaded: root.openPendingSubscriptionDialog()
    }
}
