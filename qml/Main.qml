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

    width: 1480
    height: 820
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
    property string currentAppView: "workbench"
    property string currentWorkspacePage: "messages"
    property bool connectionPaneCollapsed: false

    function resetVisibleStreams() {
        sessionActivityPanel.resetStreamPosition()
        logPage.resetStreamPosition()
    }

    function noteVisibleStreamRowAppended(row) {
        if (root.currentAppView === "history" || root.currentWorkspacePage === "log") {
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

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.preferredWidth: 74
                Layout.fillHeight: true
                color: ui.themePalette.sidebarBg

                Rectangle {
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    width: 1
                    color: ui.themePalette.sidebarBorder
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    anchors.topMargin: 20
                    anchors.bottomMargin: 20
                    spacing: 12

                    AppIconButton {
                        ui: ui
                        Layout.preferredWidth: 46
                        Layout.preferredHeight: 46
                        cornerRadius: 15
                        iconSource: ui.materialIcon("workbench")
                        iconSize: 21
                        restBg: "transparent"
                        hoverBg: ui.themePalette.selectedBg
                        outlineColor: "transparent"
                        symbolColor: root.currentAppView === "workbench" ? ui.themePalette.infoText : ui.textMuted
                        forceActive: root.currentAppView === "workbench"
                        toolTipText: qsTr("Workbench")
                        onClicked: {
                            root.currentAppView = "workbench"
                            root.currentWorkspacePage = "messages"
                        }
                    }

                    AppIconButton {
                        ui: ui
                        Layout.preferredWidth: 46
                        Layout.preferredHeight: 46
                        cornerRadius: 15
                        iconSource: ui.materialIcon("history")
                        iconSize: 20
                        restBg: "transparent"
                        hoverBg: ui.themePalette.selectedBg
                        outlineColor: "transparent"
                        symbolColor: root.currentAppView === "history" ? ui.themePalette.infoText : ui.textMuted
                        forceActive: root.currentAppView === "history"
                        toolTipText: qsTr("History")
                        onClicked: {
                            root.currentAppView = "history"
                            root.currentWorkspacePage = "log"
                        }
                    }

                    AppIconButton {
                        ui: ui
                        Layout.preferredWidth: 46
                        Layout.preferredHeight: 46
                        cornerRadius: 15
                        iconSource: ui.materialIcon("script-development")
                        iconSize: 20
                        restBg: "transparent"
                        hoverBg: ui.themePalette.selectedBg
                        outlineColor: "transparent"
                        symbolColor: root.currentAppView === "scripts" ? ui.themePalette.infoText : ui.textMuted
                        forceActive: root.currentAppView === "scripts"
                        toolTipText: qsTr("Lua scripts")
                        onClicked: root.currentAppView = "scripts"
                    }

                    Item {
                        Layout.fillHeight: true
                    }

                    AppIconButton {
                        ui: ui
                        Layout.preferredWidth: 46
                        Layout.preferredHeight: 46
                        cornerRadius: 15
                        iconSource: ui.materialIcon("settings")
                        iconSize: 20
                        restBg: "transparent"
                        hoverBg: ui.themePalette.selectedBg
                        outlineColor: "transparent"
                        symbolColor: root.currentAppView === "settings" ? ui.themePalette.infoText : ui.textMuted
                        forceActive: root.currentAppView === "settings"
                        toolTipText: qsTr("Settings")
                        onClicked: root.currentAppView = "settings"
                    }

                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.currentAppView === "history"
                              ? 1
                              : (root.currentAppView === "scripts"
                                 ? 2
                                 : (root.currentAppView === "settings" ? 3 : 0))

                RowLayout {
                    spacing: 0

                    SessionSidebar {
                        ui: ui
                        appController: root.appController
                        sessionEditor: sessionEditor
                        currentPage: root.currentWorkspacePage
                        collapsed: root.connectionPaneCollapsed
                        Layout.preferredWidth: root.connectionPaneCollapsed ? 52 : 304
                        Layout.fillHeight: true
                        onCollapseRequested: root.connectionPaneCollapsed = true
                        onExpandRequested: root.connectionPaneCollapsed = false
                    }

                    Rectangle {
                        Layout.preferredWidth: 450
                        Layout.maximumWidth: 450
                        Layout.minimumWidth: 450
                        Layout.fillHeight: true
                        color: ui.themePalette.windowBg

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

                    SessionActivityPanel {
                        id: sessionActivityPanel
                        ui: ui
                        appController: root.appController
                        session: root.session
                        status: root.status
                        publishStatus: root.publishStatus
                        fontFamily: root.font.family
                    }
                }

                LogPage {
                    id: logPage
                    ui: ui
                    appController: root.appController
                    session: root.session
                    status: root.status
                    fontFamily: root.font.family
                }

                ScriptWorkspacePanel {
                    id: scriptWorkspacePage
                    ui: ui
                    appController: root.appController
                    fontFamily: root.font.family
                }

                Rectangle {
                    color: ui.themePalette.windowBg

                    AppPanel {
                        ui: ui
                        anchors.fill: parent
                        anchors.margins: 32
                        showTopBorder: false
                        showRightBorder: false
                        showBottomBorder: false
                        showLeftBorder: false

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 18

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                Label {
                                    text: qsTr("Settings")
                                    color: ui.textStrong
                                    font.pixelSize: 28
                                    font.bold: true
                                }

                                Item {
                                    Layout.fillWidth: true
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 72
                                radius: ui.innerRadius
                                color: ui.themePalette.itemBg
                                border.color: ui.themePalette.itemBorder

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 20
                                    anchors.rightMargin: 20
                                    spacing: 18

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 3

                                        Label {
                                            text: qsTr("Appearance")
                                            color: ui.textStrong
                                            font.pixelSize: 15
                                            font.bold: true
                                        }

                                        Label {
                                            text: qsTr("Current theme: %1").arg(ui.themeModeMeta(root.appController.themeMode).label)
                                            color: ui.textMuted
                                            font.pixelSize: 13
                                        }
                                    }

                                    AppButton {
                                        ui: ui
                                        text: qsTr("Switch Theme")
                                        minimumWidth: 116
                                        onClicked: {
                                            const meta = ui.themeModeMeta(root.appController.themeMode)
                                            root.appController.themeMode = meta.next
                                        }
                                    }
                                }
                            }

                            Item {
                                Layout.fillHeight: true
                            }
                        }
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
