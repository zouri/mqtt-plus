pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Controls.Material
import QtQuick.Layouts
import "features/drafts"
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

    property string pendingNavigationView: ""
    property bool allowCloseWithUnsavedDraft: false

    function finishPendingNavigation() {
        const targetView = root.pendingNavigationView
        root.pendingNavigationView = ""
        if (targetView === "__close__") {
            root.allowCloseWithUnsavedDraft = true
            root.close()
        } else if (targetView.length > 0) {
            root.currentAppView = targetView
        }
    }

    function requestAppView(targetView) {
        if (targetView === root.currentAppView) {
            return
        }
        if (root.currentAppView === "drafts" && root.app.drafts.editor.hasUnsavedChanges) {
            root.pendingNavigationView = targetView
            unsavedDraftNavigationDialog.open()
            return
        }
        root.currentAppView = targetView
    }

    onClosing: function (close) {
        workbenchPage.persistLayout();
        if (!root.allowCloseWithUnsavedDraft
                && root.app.drafts.editor.hasUnsavedChanges) {
            close.accepted = false
            root.pendingNavigationView = "__close__"
            unsavedDraftNavigationDialog.open()
        }
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
        onActivated: root.requestAppView("settings")
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

        background: Item {
            Rectangle {
                anchors.fill: parent
                radius: 12
                color: railButton.active ? railButton.ui.themePalette.selectedBg : "transparent"
            }

            Rectangle {
                id: railHoverBackground

                anchors.fill: parent
                radius: 12
                color: railButton.ui.themePalette.selectedItemBg
                readonly property real targetOpacity: !railButton.active && railButton.hovered ? 1 : 0
                property real presentationOpacity: railHoverBackground.targetOpacity
                property bool presentationReady: false
                opacity: railHoverBackground.presentationOpacity

                function syncPresentation(animate) {
                    railHoverAnimation.stop()
                    if (!animate
                            || !railHoverBackground.presentationReady
                            || !railButton.ui.animationsEnabled
                            || railButton.ui.motionMicroDuration <= 0) {
                        railHoverBackground.presentationOpacity = railHoverBackground.targetOpacity
                        return
                    }
                    railHoverAnimation.to = railHoverBackground.targetOpacity
                    railHoverAnimation.restart()
                }

                onTargetOpacityChanged: railHoverBackground.syncPresentation(true)

                Component.onCompleted: {
                    railHoverBackground.presentationReady = true
                    railHoverBackground.syncPresentation(false)
                }

                Connections {
                    target: railButton.ui

                    function onAnimationsEnabledChanged() {
                        if (!railButton.ui.animationsEnabled) {
                            railHoverBackground.syncPresentation(false)
                        }
                    }
                }

                NumberAnimation {
                    id: railHoverAnimation

                    target: railHoverBackground
                    property: "presentationOpacity"
                    duration: railButton.ui.motionMicroDuration
                    easing.type: railButton.ui.motionEnterEasing
                }
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
                        onClicked: root.requestAppView("workbench")
                    }

                    RailButton {
                        ui: appUi
                        iconSource: appUi.materialIcon("script-development")
                        text: qsTr("Scripts")
                        active: root.currentAppView === "scripts"
                        accessibleLabel: qsTr("Lua scripts")
                        onClicked: root.requestAppView("scripts")
                    }

                    RailButton {
                        ui: appUi
                        iconSource: appUi.materialIcon("drafts")
                        text: qsTr("Drafts")
                        active: root.currentAppView === "drafts"
                        accessibleLabel: qsTr("Draft Library")
                        onClicked: root.requestAppView("drafts")
                    }

                    RailButton {
                        ui: appUi
                        iconSource: appUi.materialIcon("logs")
                        text: qsTr("Logs")
                        active: root.currentAppView === "logs"
                        accessibleLabel: qsTr("Logs")
                        onClicked: root.requestAppView("logs")
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
                        onClicked: root.requestAppView("settings")
                    }
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                z: 2
                currentIndex: root.currentAppView === "drafts" ? 1
                              : (root.currentAppView === "logs" ? 2
                                 : (root.currentAppView === "scripts" ? 3
                                    : (root.currentAppView === "settings" ? 4 : 0)))

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
                    onDraftsManageRequested: root.requestAppView("drafts")
                }

                Loader {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    active: root.currentAppView === "drafts"
                    asynchronous: true

                    sourceComponent: Component {
                        DraftsView {
                            ui: appUi
                            viewModel: root.app.drafts
                        }
                    }
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
                            configurationTransfer: root.app.configurationTransfer
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

    AppNotificationStack {
        ui: appUi
        notificationModel: root.app.notifications
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 16
        anchors.rightMargin: 16
        width: Math.min(368, root.width - 32)
        height: implicitHeight
        z: 100
    }

    Connections {
        target: root.app.notifications

        function onActionRequested(actionId) {
            if (actionId === "openLogs") {
                root.requestAppView("logs")
            }
        }
    }

    Connections {
        target: root.app.drafts

        function onEditorSaveSucceeded() {
            if (root.pendingNavigationView.length > 0) {
                root.finishPendingNavigation()
            }
        }

        function onLibraryStateChanged() {
            if (!root.app.drafts.busy
                    && root.app.drafts.storageError.length > 0
                    && root.pendingNavigationView.length > 0) {
                root.pendingNavigationView = ""
            }
        }
    }

    AppDialog {
        id: unsavedDraftNavigationDialog

        ui: appUi
        width: 470
        height: 210
        closePolicy: Popup.CloseOnEscape
        header: Item { implicitHeight: 0; visible: false }
        background: Rectangle {
            radius: appUi.radiusLg
            color: appUi.themePalette.dialogBg
            border.color: appUi.themePalette.dialogBorder
        }
        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            Label {
                Layout.fillWidth: true
                text: qsTr("Save draft changes before leaving?")
                color: appUi.textStrong
                font.pixelSize: appUi.text2xl
                font.bold: true
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("The Draft Library has unsaved editor changes.")
                color: appUi.textMuted
                font.pixelSize: appUi.textSm
                wrapMode: Text.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Item { Layout.fillWidth: true }

                AppButton {
                    ui: appUi
                    text: qsTr("Cancel")
                    minimumWidth: 76
                    onClicked: {
                        root.pendingNavigationView = ""
                        unsavedDraftNavigationDialog.close()
                    }
                }

                AppButton {
                    ui: appUi
                    text: qsTr("Discard")
                    danger: true
                    minimumWidth: 76
                    onClicked: {
                        unsavedDraftNavigationDialog.close()
                        root.app.drafts.discardEditorChanges()
                        root.finishPendingNavigation()
                    }
                }

                AppButton {
                    ui: appUi
                    text: qsTr("Save")
                    primary: true
                    minimumWidth: 76
                    enabled: root.app.drafts.editor.canSave && !root.app.drafts.busy
                    onClicked: {
                        if (root.app.drafts.saveEditor()) {
                            unsavedDraftNavigationDialog.close()
                        }
                    }
                }
            }
        }
    }
}
