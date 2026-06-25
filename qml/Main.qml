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
        const availableWidth = Screen.desktopAvailableWidth > 0 ? Screen.desktopAvailableWidth : root.defaultWindowWidth;
        return Math.max(root.minimumWidth, Math.min(Math.round(value), availableWidth));
    }

    function clampWindowHeight(value) {
        const availableHeight = Screen.desktopAvailableHeight > 0 ? Screen.desktopAvailableHeight : root.defaultWindowHeight;
        return Math.max(root.minimumHeight, Math.min(Math.round(value), availableHeight));
    }

    function persistWindowGeometry() {
        if (!root.windowGeometryReady || root.visibility !== Window.Windowed) {
            return;
        }

        root.settings.saveWindowGeometry(root.width, root.height);
    }

    function restoreWindowGeometry() {
        root.width = root.clampWindowWidth(root.settings.windowWidth);
        root.height = root.clampWindowHeight(root.settings.windowHeight);
        root.windowGeometryReady = true;

        if (root.settings.windowMaximized) {
            Qt.callLater(function () {
                root.showMaximized();
            });
        }
    }

    Component.onCompleted: root.restoreWindowGeometry()
    onWidthChanged: windowGeometrySaveTimer.restart()
    onHeightChanged: windowGeometrySaveTimer.restart()
    onVisibilityChanged: {
        if (!root.windowGeometryReady) {
            return;
        }

        root.settings.windowMaximized = root.visibility === Window.Maximized;
        if (root.visibility === Window.Windowed) {
            windowGeometrySaveTimer.restart();
        }
    }
    onClosing: function () {
        windowGeometrySaveTimer.stop();
        root.persistWindowGeometry();
        root.settings.windowMaximized = root.visibility === Window.Maximized;
    }

    Timer {
        id: windowGeometrySaveTimer
        interval: 250
        repeat: false
        onTriggered: root.persistWindowGeometry()
    }

    AppUi {
        id: appUi
        isDarkTheme: root.settings.effectiveTheme === "dark"
    }

    Material.theme: appUi.materialTheme
    Material.accent: appUi.materialAccent
    Material.primary: appUi.materialPrimary
    Material.background: appUi.themePalette.windowBg

    background: Rectangle {
        color: appUi.themePalette.windowBg
    }

    readonly property var sessions: root.app.sessions
    readonly property var subscriptions: root.app.subscriptions
    readonly property var connection: root.app.connection
    readonly property var messages: root.app.messages
    readonly property var settings: root.app.settings
    readonly property var scripts: root.app.scripts
    readonly property var logs: root.app.logs
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
                color: appUi.themePalette.sidebarBg

                Rectangle {
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    width: 1
                    color: appUi.themePalette.sidebarBorder
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    anchors.topMargin: 16
                    anchors.bottomMargin: 16
                    spacing: 10

                    AppIconButton {
                        ui: appUi
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        cornerRadius: 13
                        iconSource: appUi.materialIcon("workbench")
                        iconSize: 21
                        restBg: "transparent"
                        hoverBg: appUi.themePalette.selectedBg
                        outlineColor: "transparent"
                        symbolColor: root.currentAppView === "workbench" ? appUi.themePalette.infoText : appUi.textMuted
                        forceActive: root.currentAppView === "workbench"
                        accessibleName: qsTr("Workbench")
                        toolTipText: qsTr("Workbench")
                        onClicked: root.currentAppView = "workbench"
                    }

                    AppIconButton {
                        ui: appUi
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        cornerRadius: 13
                        iconSource: appUi.materialIcon("logs")
                        iconSize: 20
                        restBg: "transparent"
                        hoverBg: appUi.themePalette.selectedBg
                        outlineColor: "transparent"
                        symbolColor: root.currentAppView === "logs" ? appUi.themePalette.infoText : appUi.textMuted
                        forceActive: root.currentAppView === "logs"
                        accessibleName: qsTr("Logs")
                        toolTipText: qsTr("Logs")
                        onClicked: root.currentAppView = "logs"
                    }

                    AppIconButton {
                        ui: appUi
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        cornerRadius: 13
                        iconSource: appUi.materialIcon("script-development")
                        iconSize: 20
                        restBg: "transparent"
                        hoverBg: appUi.themePalette.selectedBg
                        outlineColor: "transparent"
                        symbolColor: root.currentAppView === "scripts" ? appUi.themePalette.infoText : appUi.textMuted
                        forceActive: root.currentAppView === "scripts"
                        accessibleName: qsTr("Lua scripts")
                        toolTipText: qsTr("Lua scripts")
                        onClicked: root.currentAppView = "scripts"
                    }

                    Item {
                        Layout.fillHeight: true
                    }

                    AppIconButton {
                        ui: appUi
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        cornerRadius: 13
                        iconSource: appUi.materialIcon("settings")
                        iconSize: 20
                        restBg: "transparent"
                        hoverBg: appUi.themePalette.selectedBg
                        outlineColor: "transparent"
                        symbolColor: root.currentAppView === "settings" ? appUi.themePalette.infoText : appUi.textMuted
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
                currentIndex: root.currentAppView === "logs" ? 1 : (root.currentAppView === "scripts" ? 2 : (root.currentAppView === "settings" ? 3 : 0))

                WorkbenchView {
                    id: workbenchPage
                    ui: appUi
                    sessions: root.sessions
                    subscriptions: root.subscriptions
                    connection: root.connection
                    messages: root.messages
                    scripts: root.scripts
                    fontFamily: root.font.family
                }

                LogsView {
                    id: logsPage
                    ui: appUi
                    logs: root.logs
                }

                ScriptsView {
                    id: scriptsPage
                    ui: appUi
                    scripts: root.scripts
                }

                SettingsView {
                    id: settingsPage
                    ui: appUi
                    settings: root.settings
                }
            }
        }
    }
}
