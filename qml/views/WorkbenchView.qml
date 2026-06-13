pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import "../features/events"
import "../features/sessions"
import "../features/subscriptions"

Item {
    id: root

    required property AppUi ui
    required property var appController
    required property string fontFamily
    property bool connectionPaneCollapsed: false
    readonly property var session: root.appController.currentSession
    readonly property var status: root.appController.sessionStatus
    readonly property int expandedConnectionPaneWidth: 248
    readonly property int subscriptionPaneWidth: 360

    Layout.fillWidth: true
    Layout.fillHeight: true

    function resetStreamPosition() {
        sessionActivityPanel.resetStreamPosition()
    }

    function noteStreamRowAppended(row) {
        sessionActivityPanel.noteStreamRowAppended(row)
    }

    Component.onCompleted: {
        root.resetStreamPosition()
    }

    Connections {
        target: root.appController

        function onMessageStreamChanged() {
            root.resetStreamPosition()
        }

        function onLogStreamChanged() {
            root.resetStreamPosition()
        }

        function onMessageStreamRowAppended(row) {
            root.noteStreamRowAppended(row)
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        SessionSidebar {
            ui: root.ui
            appController: root.appController
            sessionEditor: sessionEditorDialog
            collapsed: root.connectionPaneCollapsed
            Layout.preferredWidth: root.connectionPaneCollapsed ? 48 : root.expandedConnectionPaneWidth
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
                    appController: root.appController
                    sessionEditor: sessionEditorDialog
                }

                SubscriptionsPanel {
                    id: subscriptionsPanel
                    ui: root.ui
                    appController: root.appController
                    addSubscriptionDialog: addSubscriptionDialogItem
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
        }
    }

    SessionEditorDialog {
        id: sessionEditorDialog
        ui: root.ui
        appController: root.appController
    }

    AddSubscriptionDialog {
        id: addSubscriptionDialogItem
        ui: root.ui
        appController: root.appController
    }
}
