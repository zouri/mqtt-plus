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
    readonly property int minimumWindowHeight: 600
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

        root.settingsViewModel.saveWindowGeometry(root.width, root.height);
    }

    function restoreWindowGeometry() {
        root.width = root.clampWindowWidth(root.settingsViewModel.windowWidth);
        root.height = root.clampWindowHeight(root.settingsViewModel.windowHeight);
        root.windowGeometryReady = true;

        if (root.settingsViewModel.windowMaximized) {
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

        root.settingsViewModel.windowMaximized = root.visibility === Window.Maximized;
        if (root.visibility === Window.Windowed) {
            windowGeometrySaveTimer.restart();
        }
    }
    onClosing: function () {
        windowGeometrySaveTimer.stop();
        root.persistWindowGeometry();
        workbenchPage.persistLayout();
        root.settingsViewModel.windowMaximized = root.visibility === Window.Maximized;
    }

    Timer {
        id: windowGeometrySaveTimer
        interval: 250
        repeat: false
        onTriggered: root.persistWindowGeometry()
    }

    AppUi {
        id: appUi
        isDarkTheme: root.settingsViewModel.effectiveTheme === "dark"
        themeColor: root.settingsViewModel.themeColor
    }

    Shortcut {
        sequences: ["Ctrl+K", "Meta+K"]
        enabled: root.currentAppView === "workbench"
        onActivated: workbenchPage.focusMessageSearch()
    }

    Shortcut {
        sequences: ["Ctrl+B", "Meta+B"]
        enabled: root.currentAppView === "workbench"
        onActivated: workbenchPage.toggleConnectionPane()
    }

    Shortcut {
        sequences: ["Ctrl+N", "Meta+N"]
        enabled: root.currentAppView === "workbench"
        onActivated: workbenchPage.createSession()
    }

    Shortcut {
        sequences: [StandardKey.Preferences]
        onActivated: root.navigationViewModel.currentView = "settings"
    }

    Material.theme: appUi.materialTheme
    Material.accent: appUi.materialAccent
    Material.primary: appUi.materialPrimary
    Material.background: appUi.themePalette.windowBg

    background: Rectangle {
        color: appUi.themePalette.windowBg
    }

    readonly property var navigationViewModel: root.app.navigation
    readonly property var settingsViewModel: root.app.settings
    readonly property string currentAppView: root.navigationViewModel.currentView
    readonly property int navigationRailWidth: 52

    component RailButton: ToolButton {
        id: railButton

        required property AppUi ui
        required property bool active
        property string accessibleLabel: railButton.text
        property url iconSource: ""

        Layout.preferredWidth: 40
        Layout.preferredHeight: 40
        display: AbstractButton.IconOnly
        icon.source: railButton.iconSource
        icon.width: 22
        icon.height: 22
        icon.color: railButton.active ? railButton.ui.themePalette.infoText : railButton.ui.themePalette.textSubtle
        font.pixelSize: 10
        font.bold: true
        palette.buttonText: railButton.active ? railButton.ui.themePalette.infoText : railButton.ui.themePalette.textSubtle
        padding: 0
        spacing: 0
        Accessible.name: railButton.accessibleLabel
        ToolTip.visible: railButton.hovered
        ToolTip.delay: 400
        ToolTip.text: railButton.accessibleLabel

        HoverHandler {
            cursorShape: Qt.PointingHandCursor
        }

        background: Rectangle {
            radius: 12
            color: railButton.active ? railButton.ui.themePalette.selectedBg : (railButton.hovered ? railButton.ui.themePalette.rowHover : "transparent")
            border.color: "transparent"
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
                Layout.preferredWidth: root.navigationRailWidth
                Layout.fillHeight: true
                z: 2
                color: appUi.themePalette.navigationBg

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    anchors.topMargin: 10
                    anchors.bottomMargin: 10
                    spacing: 6

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 38
                        Layout.preferredHeight: 38
                        Layout.bottomMargin: 8
                        radius: 11
                        color: appUi.themePalette.buttonPrimaryBg

                        Label {
                            anchors.centerIn: parent
                            text: qsTr("M")
                            color: appUi.themePalette.buttonPrimaryText
                            font.pixelSize: 18
                            font.bold: true
                        }
                    }

                    RailButton {
                        ui: appUi
                        iconSource: appUi.materialIcon("workbench")
                        text: qsTr("Workbench")
                        active: root.currentAppView === "workbench"
                        accessibleLabel: qsTr("Workbench")
                        onClicked: root.navigationViewModel.currentView = "workbench"
                    }

                    RailButton {
                        ui: appUi
                        iconSource: appUi.materialIcon("logs")
                        text: qsTr("Logs")
                        active: root.currentAppView === "logs"
                        accessibleLabel: qsTr("Logs")
                        onClicked: root.navigationViewModel.currentView = "logs"
                    }

                    RailButton {
                        ui: appUi
                        iconSource: appUi.materialIcon("script-development")
                        text: qsTr("Scripts")
                        active: root.currentAppView === "scripts"
                        accessibleLabel: qsTr("Lua scripts")
                        onClicked: root.navigationViewModel.currentView = "scripts"
                    }

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 34
                        Layout.preferredHeight: 1
                        Layout.topMargin: 6
                        color: appUi.themePalette.separator
                    }

                    Item {
                        Layout.fillHeight: true
                    }

                    RailButton {
                        ui: appUi
                        iconSource: appUi.materialIcon("settings")
                        text: qsTr("Settings")
                        active: root.currentAppView === "settings"
                        accessibleLabel: qsTr("Settings")
                        onClicked: root.navigationViewModel.currentView = "settings"
                    }
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                z: 2
                currentIndex: root.currentAppView === "logs" ? 1 : (root.currentAppView === "scripts" ? 2 : (root.currentAppView === "settings" ? 3 : 0))

                WorkbenchView {
                    id: workbenchPage
                    ui: appUi
                    viewModel: root.app.workbench
                    settingsViewModel: root.settingsViewModel
                    fontFamily: root.font.family
                    autoCollapseConnectionListOnConnect: root.settingsViewModel.autoCollapseConnectionListOnConnect
                }

                LogsView {
                    id: logsPage
                    ui: appUi
                    viewModel: root.app.logs
                }

                ScriptsView {
                    id: scriptsPage
                    ui: appUi
                    viewModel: root.app.scripts
                }

                SettingsView {
                    id: settingsPage
                    ui: appUi
                    viewModel: root.app.settings
                }
            }
        }
    }

    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        x: root.navigationRailWidth - 1
        width: 1
        z: 4
        color: appUi.themePalette.sidebarBorder
    }
}
