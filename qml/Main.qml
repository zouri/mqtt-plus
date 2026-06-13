pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Controls.Material
import QtQuick.Layouts
import "views"

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
    property string currentAppView: "workbench"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.preferredWidth: 64
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
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    anchors.topMargin: 16
                    anchors.bottomMargin: 16
                    spacing: 10

                    AppIconButton {
                        ui: ui
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        cornerRadius: 13
                        iconSource: ui.materialIcon("workbench")
                        iconSize: 21
                        restBg: "transparent"
                        hoverBg: ui.themePalette.selectedBg
                        outlineColor: "transparent"
                        symbolColor: root.currentAppView === "workbench" ? ui.themePalette.infoText : ui.textMuted
                        forceActive: root.currentAppView === "workbench"
                        accessibleName: qsTr("Workbench")
                        onClicked: root.currentAppView = "workbench"
                    }

                    AppIconButton {
                        ui: ui
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        cornerRadius: 13
                        iconSource: ui.materialIcon("history")
                        iconSize: 20
                        restBg: "transparent"
                        hoverBg: ui.themePalette.selectedBg
                        outlineColor: "transparent"
                        symbolColor: root.currentAppView === "history" ? ui.themePalette.infoText : ui.textMuted
                        forceActive: root.currentAppView === "history"
                        accessibleName: qsTr("History")
                        onClicked: root.currentAppView = "history"
                    }

                    AppIconButton {
                        ui: ui
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        cornerRadius: 13
                        iconSource: ui.materialIcon("script-development")
                        iconSize: 20
                        restBg: "transparent"
                        hoverBg: ui.themePalette.selectedBg
                        outlineColor: "transparent"
                        symbolColor: root.currentAppView === "scripts" ? ui.themePalette.infoText : ui.textMuted
                        forceActive: root.currentAppView === "scripts"
                        accessibleName: qsTr("Lua scripts")
                        onClicked: root.currentAppView = "scripts"
                    }

                    Item {
                        Layout.fillHeight: true
                    }

                    AppIconButton {
                        ui: ui
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        cornerRadius: 13
                        iconSource: ui.materialIcon("settings")
                        iconSize: 20
                        restBg: "transparent"
                        hoverBg: ui.themePalette.selectedBg
                        outlineColor: "transparent"
                        symbolColor: root.currentAppView === "settings" ? ui.themePalette.infoText : ui.textMuted
                        forceActive: root.currentAppView === "settings"
                        accessibleName: qsTr("Settings")
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

                WorkbenchView {
                    id: workbenchPage
                    ui: ui
                    appController: root.appController
                    fontFamily: root.font.family
                }

                HistoryView {
                    id: historyPage
                    ui: ui
                    appController: root.appController
                    fontFamily: root.font.family
                }

                ScriptsView {
                    id: scriptsPage
                    ui: ui
                    appController: root.appController
                    fontFamily: root.font.family
                }

                SettingsView {
                    id: settingsPage
                    ui: ui
                    appController: root.appController
                }
            }
        }
    }

}
