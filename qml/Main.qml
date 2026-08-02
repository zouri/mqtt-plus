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
    width: root.defaultWindowWidth
    height: root.defaultWindowHeight
    minimumWidth: root.minimumWindowWidth
    minimumHeight: root.minimumWindowHeight
    visible: false
    flags: Qt.Window
    title: root.appTitle
    font.family: root.settingsViewModel.effectiveFontFamily
    topPadding: 0

    // C++ sizes and centers the window on the primary screen before showing it.

    onClosing: function () {
        workbenchPage.persistLayout();
    }

    AppUi {
        id: appUi
        isDarkTheme: root.settingsViewModel.effectiveTheme === "dark"
        themeColor: root.settingsViewModel.themeColor
        animationsEnabled: root.settingsViewModel.animationsEnabled
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
        onActivated: root.currentAppView = "settings"
    }

    Material.theme: appUi.materialTheme
    Material.accent: appUi.materialAccent
    Material.primary: appUi.materialPrimary
    Material.background: appUi.themePalette.windowBg

    background: Rectangle {
        color: appUi.themePalette.windowBg
    }

    readonly property var settingsViewModel: root.app.settings
    readonly property var preferences: root.app.preferences
    property string currentAppView: "workbench"
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

        HoverHandler {
            cursorShape: Qt.PointingHandCursor
        }

        AppToolTip {
            ui: railButton.ui
            text: railButton.accessibleLabel
            position: AppToolTip.Position.Right
            active: railButton.hovered
        }

        background: Rectangle {
            id: railBackground

            readonly property color targetColor: railButton.active
                                                  ? railButton.ui.themePalette.selectedBg
                                                  : (railButton.hovered
                                                     ? railButton.ui.themePalette.rowHover
                                                     : "transparent")
            property color presentationColor: railBackground.targetColor
            property bool presentationReady: false

            radius: 12
            color: railBackground.presentationColor
            border.color: "transparent"

            function syncPresentation(animate) {
                railBackgroundAnimation.stop();
                if (!animate
                        || !railBackground.presentationReady
                        || !railButton.ui.animationsEnabled
                        || railButton.ui.motionMicroDuration <= 0) {
                    railBackground.presentationColor = railBackground.targetColor;
                    return;
                }
                railBackgroundAnimation.to = railBackground.targetColor;
                railBackgroundAnimation.restart();
            }

            onTargetColorChanged: railBackground.syncPresentation(true)

            Component.onCompleted: {
                railBackground.presentationReady = true;
                railBackground.syncPresentation(false);
            }

            Connections {
                target: railButton.ui

                function onAnimationsEnabledChanged() {
                    if (!railButton.ui.animationsEnabled) {
                        railBackground.syncPresentation(false);
                    }
                }
            }

            ColorAnimation {
                id: railBackgroundAnimation

                target: railBackground
                property: "presentationColor"
                duration: railButton.ui.motionMicroDuration
                easing.type: railButton.ui.motionEnterEasing
            }
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

                    RailButton {
                        ui: appUi
                        iconSource: appUi.materialIcon("workbench")
                        text: qsTr("Workbench")
                        active: root.currentAppView === "workbench"
                        accessibleLabel: qsTr("Workbench")
                        onClicked: root.currentAppView = "workbench"
                    }

                    RailButton {
                        ui: appUi
                        iconSource: appUi.materialIcon("logs")
                        text: qsTr("Logs")
                        active: root.currentAppView === "logs"
                        accessibleLabel: qsTr("Logs")
                        onClicked: root.currentAppView = "logs"
                    }

                    RailButton {
                        ui: appUi
                        iconSource: appUi.materialIcon("script-development")
                        text: qsTr("Scripts")
                        active: root.currentAppView === "scripts"
                        accessibleLabel: qsTr("Lua scripts")
                        onClicked: root.currentAppView = "scripts"
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
                        onClicked: root.currentAppView = "settings"
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
                    active: root.currentAppView === "workbench"
                    viewModel: root.app.workbench
                    settingsViewModel: root.settingsViewModel
                    preferences: root.preferences
                    eventHistory: root.app.eventHistory
                    sessionService: root.app.sessionService
                    subscriptionService: root.app.subscriptionService
                    fontFamily: root.settingsViewModel.effectiveFontFamily
                    autoCollapseConnectionListOnConnect: root.preferences.autoCollapseConnectionListOnConnect
                }

                Loader {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    active: root.currentAppView === "logs"
                    asynchronous: true

                    sourceComponent: Component {
                        LogsView {
                            ui: appUi
                            viewModel: root.app.logs
                            eventHistory: root.app.eventHistory
                        }
                    }
                }

                Loader {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    active: root.currentAppView === "scripts"
                    asynchronous: true

                    sourceComponent: Component {
                        ScriptsView {
                            ui: appUi
                            viewModel: root.app.scripts
                        }
                    }
                }

                Loader {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    active: root.currentAppView === "settings"
                    asynchronous: true

                    sourceComponent: Component {
                        SettingsView {
                            ui: appUi
                            viewModel: root.app.settings
                            preferences: root.preferences
                            eventHistory: root.app.eventHistory
                        }
                    }
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
