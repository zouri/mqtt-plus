pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: root

    required property AppUi ui
    required property var appController
    required property string fontFamily
    property bool connectionPaneCollapsed: false
    readonly property var session: root.appController.currentSession
    readonly property var status: root.appController.sessionStatus
    readonly property int expandedConnectionPaneWidth: 248
    readonly property int subscriptionPaneMinWidth: 280
    readonly property int subscriptionPaneMaxWidth: 520
    property int subscriptionPaneWidth: 360
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
        target: root.appController

        function onMessageStreamChanged() {
            root.resetStreamPosition();
        }

        function onLogStreamChanged() {
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
            appController: root.appController
            sessionEditor: sessionEditorBridge
            collapsed: root.connectionPaneCollapsed
            Layout.preferredWidth: root.connectionPaneCollapsed ? 32 : root.expandedConnectionPaneWidth
            Layout.fillHeight: true
            onCollapseRequested: root.connectionPaneCollapsed = true
            onExpandRequested: root.connectionPaneCollapsed = false
        }

        SplitView {
            id: workbenchSplit

            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            handle: Item {
                implicitWidth: workbenchSplit.orientation === Qt.Horizontal ? 1 : workbenchSplit.width
                implicitHeight: workbenchSplit.orientation === Qt.Horizontal ? workbenchSplit.height : 1

                Rectangle {
                    anchors.fill: parent
                    color: splitHandleHover.hovered || SplitHandle.hovered || SplitHandle.pressed
                           ? root.ui.themePalette.selectedBorder
                           : root.ui.panelBorder
                }

                HoverHandler {
                    id: splitHandleHover
                    margin: 5
                    cursorShape: Qt.SplitHCursor
                }
            }

            Rectangle {
                SplitView.preferredWidth: root.subscriptionPaneWidth
                SplitView.minimumWidth: root.subscriptionPaneMinWidth
                SplitView.maximumWidth: root.subscriptionPaneMaxWidth
                SplitView.fillHeight: true
                color: root.ui.themePalette.windowBg

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    SessionOverviewPanel {
                        ui: root.ui
                        session: root.session
                        status: root.status
                        appController: root.appController
                        sessionEditor: sessionEditorBridge
                    }

                    SubscriptionsPanel {
                        id: subscriptionsPanel
                        ui: root.ui
                        appController: root.appController
                        addSubscriptionDialog: addSubscriptionDialogBridge
                    }
                }
            }

            SessionActivityPanel {
                id: sessionActivityPanel
                ui: root.ui
                appController: root.appController
                session: root.session
                status: root.status
                publishStatus: root.appController.publishStatus
                fontFamily: root.fontFamily
                SplitView.fillWidth: true
                SplitView.fillHeight: true
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
                appController: root.appController
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
                appController: root.appController
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
