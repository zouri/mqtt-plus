pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Controls.Material
import QtQuick.Layouts
import "features/events"
import "features/scripts"
import "features/sessions"
import "features/subscriptions"

ApplicationWindow {
    id: root
    required property var app

    readonly property string appTitle: qsTr("MQTT Plus")

    width: 1380
    height: 880
    visible: true
    flags: Qt.Window
    title: root.appTitle
    topPadding: 0

    AppUi {
        id: ui
        isDarkTheme: root.appController.effectiveTheme === "dark"
    }

    Material.theme: ui.materialTheme
    Material.accent: ui.materialAccent
    Material.primary: ui.materialPrimary
    Material.background: ui.themePalette.windowBg

    background: Rectangle {
        color: ui.themePalette.windowBg
    }

    readonly property var appController: root.app
    readonly property var session: root.appController.currentSession
    readonly property var status: root.appController.sessionStatus
    readonly property var publishStatus: root.appController.publishStatus
    property string currentWorkspacePage: "messages"

    function resetVisibleStreams() {
        sessionActivityPanel.resetStreamPosition()
        logPage.resetStreamPosition()
    }

    function noteVisibleStreamRowAppended(row) {
        if (root.currentWorkspacePage === "log") {
            logPage.noteStreamRowAppended(row)
        } else {
            sessionActivityPanel.noteStreamRowAppended(row)
        }
    }

    Component.onCompleted: {
        root.resetVisibleStreams()
    }

    Connections {
        target: root.appController

        function onEventStreamChanged() {
            root.resetVisibleStreams()
        }

        function onEventStreamRowAppended(row) {
            root.noteVisibleStreamRowAppended(row)
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        SessionSidebar {
            ui: ui
            appController: root.appController
            sessionEditor: sessionEditor
            currentPage: root.currentWorkspacePage
            Layout.preferredWidth: 238
            Layout.fillHeight: true
            onPageRequested: (page) => root.currentWorkspacePage = page
            onScriptWorkspaceRequested: scriptLibraryWindow.openLibrary()
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: ui.themePalette.windowBg

            RowLayout {
                anchors.fill: parent
                anchors.margins: 0
                spacing: 0

                Rectangle {
                    Layout.preferredWidth: 396
                    Layout.fillHeight: true
                    color: "transparent"

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        SessionOverviewPanel {
                            ui: ui
                            session: root.session
                            status: root.status
                            appController: root.appController
                            sessionEditor: sessionEditor
                        }

                        SubscriptionsPanel {
                            id: subscriptionsPanel
                            ui: ui
                            appController: root.appController
                            addSubscriptionDialog: addSubscriptionDialog
                        }
                    }
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: root.currentWorkspacePage === "log" ? 1 : 0

                    SessionActivityPanel {
                        id: sessionActivityPanel
                        ui: ui
                        appController: root.appController
                        session: root.session
                        status: root.status
                        publishStatus: root.publishStatus
                        fontFamily: root.font.family
                    }

                    LogPage {
                        id: logPage
                        ui: ui
                        appController: root.appController
                        session: root.session
                        status: root.status
                        fontFamily: root.font.family
                    }
                }
            }
        }
    }

    SessionEditorDialog {
        id: sessionEditor
        ui: ui
        appController: root.appController
    }

    AddSubscriptionDialog {
        id: addSubscriptionDialog
        ui: ui
        appController: root.appController
    }

    ScriptLibraryWindow {
        id: scriptLibraryWindow
        ui: ui
        appController: root.appController
        fontFamily: root.font.family
        ownerWindow: root
    }

}
