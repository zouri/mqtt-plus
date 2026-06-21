pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Controls.Material
import QtQuick.Layouts
import "features/logs"
import "features/scripts"
import "features/settings"
import "features/workbench"

ApplicationWindow {
    id: root
    required property var app

    readonly property string appTitle: qsTr("MQTT Plus")
    readonly property int defaultWindowWidth: 1480
    readonly property int defaultWindowHeight: 820
    readonly property int minimumWindowWidth: 1100
    readonly property int minimumWindowHeight: 700
    property bool windowGeometryReady: false

    width: root.defaultWindowWidth
    height: root.defaultWindowHeight
    minimumWidth: root.minimumWindowWidth
    minimumHeight: root.minimumWindowHeight
    visible: true
    flags: Qt.Window
    title: root.appTitle
    topPadding: 0

    function clampWindowWidth(value) {
        const availableWidth = Screen.desktopAvailableWidth > 0
                             ? Screen.desktopAvailableWidth
                             : root.defaultWindowWidth
        return Math.max(root.minimumWidth, Math.min(Math.round(value), availableWidth))
    }

    function clampWindowHeight(value) {
        const availableHeight = Screen.desktopAvailableHeight > 0
                              ? Screen.desktopAvailableHeight
                              : root.defaultWindowHeight
        return Math.max(root.minimumHeight, Math.min(Math.round(value), availableHeight))
    }

    function persistWindowGeometry() {
        if (!root.windowGeometryReady || root.visibility !== Window.Windowed) {
            return
        }

        root.appController.saveWindowGeometry(root.width, root.height)
    }

    function restoreWindowGeometry() {
        root.width = root.clampWindowWidth(root.appController.windowWidth)
        root.height = root.clampWindowHeight(root.appController.windowHeight)
        root.windowGeometryReady = true

        if (root.appController.windowMaximized) {
            Qt.callLater(function() {
                root.showMaximized()
            })
        }
    }

    Component.onCompleted: root.restoreWindowGeometry()
    onWidthChanged: windowGeometrySaveTimer.restart()
    onHeightChanged: windowGeometrySaveTimer.restart()
    onVisibilityChanged: {
        if (!root.windowGeometryReady) {
            return
        }

        root.appController.windowMaximized = root.visibility === Window.Maximized
        if (root.visibility === Window.Windowed) {
            windowGeometrySaveTimer.restart()
        }
    }
    onClosing: function() {
        windowGeometrySaveTimer.stop()
        root.persistWindowGeometry()
        root.appController.windowMaximized = root.visibility === Window.Maximized
    }

    Timer {
        id: windowGeometrySaveTimer
        interval: 250
        repeat: false
        onTriggered: root.persistWindowGeometry()
    }

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
                Layout.preferredWidth: 56
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
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
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
                        toolTipText: qsTr("Workbench")
                        onClicked: root.currentAppView = "workbench"
                    }

                    AppIconButton {
                        ui: ui
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        cornerRadius: 13
                        iconSource: ui.materialIcon("logs")
                        iconSize: 20
                        restBg: "transparent"
                        hoverBg: ui.themePalette.selectedBg
                        outlineColor: "transparent"
                        symbolColor: root.currentAppView === "logs" ? ui.themePalette.infoText : ui.textMuted
                        forceActive: root.currentAppView === "logs"
                        accessibleName: qsTr("Logs")
                        toolTipText: qsTr("Logs")
                        onClicked: root.currentAppView = "logs"
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
                        toolTipText: qsTr("Lua scripts")
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
                        toolTipText: qsTr("Settings")
                        onClicked: root.currentAppView = "settings"
                    }

                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.currentAppView === "logs"
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

                LogsView {
                    id: logsPage
                    ui: ui
                    appController: root.appController
                }

                ScriptsView {
                    id: scriptsPage
                    ui: ui
                    appController: root.appController
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
