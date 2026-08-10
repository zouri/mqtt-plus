pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import "../../components"

Rectangle {
    id: root

    required property AppUi ui
    required property var viewModel
    required property var preferences
    required property var eventHistory
    required property var configurationTransfer
    required property var updates

    readonly property var themeLabels: [qsTr("System"), qsTr("Light"), qsTr("Dark")]
    readonly property var themeColorOptions: [
        { "key": "mint", "label": qsTr("Mint"), "color": "#35d0aa" },
        { "key": "blue", "label": qsTr("Blue"), "color": "#6aa3ff" },
        { "key": "violet", "label": qsTr("Violet"), "color": "#ad8cff" },
        { "key": "amber", "label": qsTr("Amber"), "color": "#f1b86a" },
        { "key": "rose", "label": qsTr("Rose"), "color": "#ff879d" }
    ]
    readonly property var languageLabels: [qsTr("System"), qsTr("English"), qsTr("Simplified Chinese")]
    readonly property var messagePayloadDisplayLabels: [qsTr("Compact"), qsTr("Expand on hover"), qsTr("Always expanded")]
    readonly property var autoFollowFpsLabels: [qsTr("15 FPS"), qsTr("30 FPS"), qsTr("60 FPS")]
    readonly property var messageRetentionLabels: [qsTr("1,000 messages"), qsTr("5,000 messages"), qsTr("10,000 messages"), qsTr("Unlimited")]
    readonly property var logRetentionLabels: [qsTr("500 logs"), qsTr("2,000 logs"), qsTr("5,000 logs"), qsTr("Unlimited")]
    readonly property var pageSizeLabels: [qsTr("200 rows"), qsTr("500 rows"), qsTr("1,000 rows")]
    readonly property var payloadLimitLabels: [qsTr("256 KiB"), qsTr("1 MiB"), qsTr("5 MiB"), qsTr("16 MiB")]
    readonly property var cleanupLabels: [qsTr("Do not clear"), qsTr("Current session"), qsTr("All sessions")]

    color: root.ui.themePalette.windowBg

    component SettingsSection: Rectangle {
        id: section

        required property AppUi ui
        property string title: ""
        default property alias rows: sectionBody.data

        Layout.fillWidth: true
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredHeight: sectionColumn.implicitHeight + 24
        radius: 12
        color: section.ui.themePalette.itemBg
        border.color: section.ui.themePalette.itemBorder

        ColumnLayout {
            id: sectionColumn
            anchors.fill: parent
            spacing: 0

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                Layout.topMargin: 14
                Layout.bottomMargin: 14
                text: section.title
                color: section.ui.textStrong
                font.pixelSize: 15
                font.bold: true
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: section.ui.themePalette.innerPanelBorder
            }

            ColumnLayout {
                id: sectionBody
                Layout.fillWidth: true
                spacing: 0
            }
        }
    }

    component SettingRow: Rectangle {
        id: settingRow

        required property AppUi ui
        property string title: ""
        property string detail: ""
        property bool showDivider: true
        default property alias controls: controlRow.data

        Layout.fillWidth: true
        Layout.preferredHeight: Math.max(54, rowLayout.implicitHeight + 18)
        color: "transparent"

        RowLayout {
            id: rowLayout
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            spacing: 14

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: settingRow.title
                    color: settingRow.ui.textStrong
                    font.pixelSize: 13
                    font.bold: true
                    elide: Label.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    visible: settingRow.detail.length > 0
                    text: settingRow.detail
                    color: settingRow.ui.textMuted
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
            }

            RowLayout {
                id: controlRow
                Layout.alignment: Qt.AlignVCenter
                spacing: 8
            }
        }

        Rectangle {
            visible: settingRow.showDivider
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: settingRow.ui.themePalette.separator
        }
    }

    component SettingSwitch: Switch {
        id: settingSwitch

        required property AppUi ui
        property bool controlsGlobalMotion: false
        property bool presentationReady: false
        property real presentationProgress: 0.0

        implicitWidth: 40
        implicitHeight: 22
        text: ""

        function syncPresentation(animate) {
            const targetProgress = settingSwitch.checked ? 1.0 : 0.0;
            settingSwitchAnimation.stop();
            if (!animate
                    || !settingSwitch.ui.animationsEnabled
                    || (settingSwitch.controlsGlobalMotion && !settingSwitch.checked)
                    || settingSwitch.ui.motionMicroDuration <= 0) {
                settingSwitch.presentationProgress = targetProgress;
                return;
            }
            settingSwitchAnimation.to = targetProgress;
            settingSwitchAnimation.restart();
        }

        onCheckedChanged: settingSwitch.syncPresentation(settingSwitch.presentationReady)

        Component.onCompleted: {
            settingSwitch.presentationReady = true;
            settingSwitch.syncPresentation(false);
        }

        Connections {
            target: settingSwitch.ui

            function onAnimationsEnabledChanged() {
                if (!settingSwitch.ui.animationsEnabled) {
                    settingSwitch.syncPresentation(false);
                }
            }
        }

        NumberAnimation {
            id: settingSwitchAnimation

            target: settingSwitch
            property: "presentationProgress"
            duration: settingSwitch.ui.motionMicroDuration
            easing.type: settingSwitch.ui.motionEnterEasing
        }

        indicator: Rectangle {
            implicitWidth: 40
            implicitHeight: 22
            radius: 11
            color: settingSwitch.ui.themePalette.innerPanelBorder

            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: settingSwitch.ui.themePalette.buttonPrimaryBg
                opacity: settingSwitch.presentationProgress
            }

            Rectangle {
                x: 2 + 18 * settingSwitch.presentationProgress
                y: 2
                width: 18
                height: 18
                radius: 9
                color: "#ffffff"
            }
        }

        contentItem: Item {
            implicitWidth: 0
            implicitHeight: 0
        }
    }

    component ThemeColorButton: ToolButton {
        id: themeColorButton

        required property AppUi ui
        required property string colorKey
        required property color swatchColor
        required property string accessibleLabel
        required property bool selected

        signal colorSelected(string colorKey)

        implicitWidth: 30
        implicitHeight: 30
        padding: 0
        display: AbstractButton.IconOnly
        icon.source: selected ? ui.materialIcon("check") : ""
        icon.width: 14
        icon.height: 14
        icon.color: "#ffffff"
        Accessible.name: accessibleLabel
        onClicked: colorSelected(colorKey)

        HoverHandler {
            cursorShape: Qt.PointingHandCursor
        }

        background: Rectangle {
            radius: 7
            color: themeColorButton.swatchColor
            border.color: themeColorButton.selected
                          ? themeColorButton.ui.textStrong
                          : themeColorButton.ui.themePalette.innerPanelBorder
            border.width: themeColorButton.selected ? 2 : 1
        }

        AppToolTip {
            ui: themeColorButton.ui
            text: themeColorButton.accessibleLabel
            active: themeColorButton.hovered
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: root.ui.themePalette.headerBg

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 10

                Label {
                    text: qsTr("Settings")
                    color: root.ui.textStrong
                    font.pixelSize: 18
                    font.bold: true
                }

                Item {
                    Layout.fillWidth: true
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: root.ui.themePalette.separator
            }
        }

        Flickable {
            id: settingsFlickable

            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: settingsContent.implicitHeight + 28
            clip: true

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            ColumnLayout {
                id: settingsContent
                width: Math.min(settingsFlickable.width, 788)
                x: Math.round((settingsFlickable.width - width) / 2)
                spacing: 12

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 2
                }

                SettingsSection {
                    ui: root.ui
                    title: qsTr("Appearance")
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                    Layout.maximumWidth: 760

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Theme")
                        detail: qsTr("Choose how the interface follows system appearance.")

                        AppComboBox {
                            ui: root.ui
                            Layout.preferredWidth: 150
                            model: root.themeLabels
                            currentIndex: root.viewModel.themeModeIndex
                            onActivated: (index) => root.viewModel.setThemeModeIndex(index)
                        }
                    }

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Theme color")
                        detail: qsTr("Choose the accent used for actions and selections.")

                        Repeater {
                            model: root.themeColorOptions

                            delegate: ThemeColorButton {
                                required property var modelData

                                ui: root.ui
                                Layout.preferredWidth: 30
                                Layout.preferredHeight: 30
                                colorKey: modelData.key
                                swatchColor: modelData.color
                                accessibleLabel: modelData.label
                                selected: root.viewModel.themeColor === colorKey
                                onColorSelected: color => root.viewModel.setThemeColor(color)
                            }
                        }
                    }

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Animations")
                        detail: qsTr("Enable motion effects throughout the interface.")

                        SettingSwitch {
                            ui: root.ui
                            controlsGlobalMotion: true
                            checked: root.viewModel.animationsEnabled
                            onToggled: root.viewModel.setAnimationsEnabled(checked)
                        }
                    }

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Font")
                        detail: qsTr("Choose an installed monospace font. Missing characters use the system fallback.")

                        AppComboBox {
                            ui: root.ui
                            Layout.preferredWidth: 220
                            model: root.viewModel.availableFontFamilies
                            currentIndex: root.viewModel.fontFamilyIndex
                            onActivated: (index) => root.viewModel.setFontFamilyIndex(index)
                        }
                    }

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Language")
                        detail: qsTr("Switch the interface language.")
                        showDivider: false

                        AppComboBox {
                            ui: root.ui
                            Layout.preferredWidth: 170
                            model: root.languageLabels
                            currentIndex: root.viewModel.languageModeIndex
                            onActivated: (index) => root.viewModel.setLanguageModeIndex(index)
                        }
                    }
                }

                SettingsSection {
                    ui: root.ui
                    title: qsTr("Workbench")
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                    Layout.maximumWidth: 760

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Auto-collapse connections")
                        detail: qsTr("Collapse the connection list after a connection succeeds.")

                        SettingSwitch {
                            ui: root.ui
                            checked: root.preferences.autoCollapseConnectionListOnConnect
                            onToggled: root.preferences.autoCollapseConnectionListOnConnect = checked
                        }
                    }

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Message content")
                        detail: qsTr("Choose how much payload text is shown in the message list.")

                        AppComboBox {
                            ui: root.ui
                            Layout.preferredWidth: 170
                            model: root.messagePayloadDisplayLabels
                            currentIndex: root.viewModel.messagePayloadDisplayModeIndex
                            onActivated: (index) => root.viewModel.setMessagePayloadDisplayModeIndex(index)
                        }
                    }

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Auto-follow refresh rate")
                        detail: qsTr("Limit how often the message list follows new rows.")
                        showDivider: false

                        AppComboBox {
                            ui: root.ui
                            Layout.preferredWidth: 130
                            model: root.autoFollowFpsLabels
                            currentIndex: root.preferences.autoFollowFps === 15
                                          ? 0
                                          : (root.preferences.autoFollowFps === 60 ? 2 : 1)
                            onActivated: (index) => root.preferences.autoFollowFps = [15, 30, 60][index]
                        }
                    }
                }

                SettingsSection {
                    ui: root.ui
                    title: qsTr("Software update")
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                    Layout.maximumWidth: 760

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Version %1").arg(root.updates.currentVersion)
                        detail: root.updates.statusMessage

                        AppButton {
                            ui: root.ui
                            text: root.updates.busy ? qsTr("Checking...") : qsTr("Check now")
                            minimumWidth: 100
                            enabled: !root.updates.busy
                            onClicked: root.updates.checkForUpdates()
                        }

                        AppButton {
                            ui: root.ui
                            visible: root.updates.updateAvailable
                            text: root.updates.directDownloadAvailable
                                  ? qsTr("Download DMG")
                                  : qsTr("View release")
                            minimumWidth: 116
                            onClicked: root.updates.openDownloadPage()
                        }
                    }

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Automatic checks")
                        detail: qsTr("Check GitHub Releases at most once every 24 hours.")
                        showDivider: false

                        SettingSwitch {
                            ui: root.ui
                            checked: root.updates.automaticChecksEnabled
                            onToggled: root.updates.automaticChecksEnabled = checked
                        }
                    }
                }

                SettingsSection {
                    ui: root.ui
                    title: qsTr("History")
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                    Layout.maximumWidth: 760

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Saved messages")
                        detail: qsTr("Maximum MQTT messages kept per connection. Cleanup runs when the app starts or exits.")

                        AppComboBox {
                            ui: root.ui
                            Layout.preferredWidth: 170
                            model: root.messageRetentionLabels
                            currentIndex: root.viewModel.messageRetentionLimitIndex
                            onActivated: (index) => root.viewModel.setMessageRetentionLimitIndex(index)
                        }
                    }

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Saved logs")
                        detail: qsTr("Maximum event log entries retained per connection.")

                        AppComboBox {
                            ui: root.ui
                            Layout.preferredWidth: 150
                            model: root.logRetentionLabels
                            currentIndex: root.viewModel.logRetentionLimitIndex
                            onActivated: (index) => root.viewModel.setLogRetentionLimitIndex(index)
                        }
                    }

                    SettingRow {
                        ui: root.ui
                        title: qsTr("History page size")
                        detail: qsTr("Rows loaded when opening a connection or scrolling back.")

                        AppComboBox {
                            ui: root.ui
                            Layout.preferredWidth: 130
                            model: root.pageSizeLabels
                            currentIndex: root.viewModel.historyPageSizeIndex
                            onActivated: (index) => root.viewModel.setHistoryPageSizeIndex(index)
                        }
                    }

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Max payload size")
                        detail: qsTr("Largest incoming MQTT payload decoded, scripted, and fully stored.")

                        AppComboBox {
                            ui: root.ui
                            Layout.preferredWidth: 130
                            model: root.payloadLimitLabels
                            currentIndex: root.viewModel.maxIncomingPayloadBytesIndex
                            onActivated: (index) => root.viewModel.setMaxIncomingPayloadBytesIndex(index)
                        }
                    }

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Delete connection history")
                        detail: qsTr("Remove stored messages and logs when a connection is deleted.")
                        showDivider: false

                        SettingSwitch {
                            ui: root.ui
                            checked: root.preferences.deleteHistoryWithSession
                            onToggled: root.preferences.deleteHistoryWithSession = checked
                        }
                    }
                }

                SettingsSection {
                    ui: root.ui
                    title: qsTr("Output")
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                    Layout.maximumWidth: 760

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Save while paused")
                        detail: qsTr("Keep storing incoming messages when output is paused.")
                        showDivider: false

                        SettingSwitch {
                            ui: root.ui
                            checked: root.preferences.saveMessagesWhenOutputPaused
                            onToggled: root.preferences.saveMessagesWhenOutputPaused = checked
                        }
                    }
                }

                SettingsSection {
                    ui: root.ui
                    title: qsTr("Storage locations")
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                    Layout.maximumWidth: 760

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Scripts")
                        detail: root.viewModel.scriptStorageDirectory

                        AppIconButton {
                            ui: root.ui
                            iconSource: root.ui.materialIcon("folder-open")
                            accessibleName: qsTr("Open script storage folder")
                            toolTipText: accessibleName
                            onClicked: root.viewModel.openScriptStorageDirectory()
                        }
                    }

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Drafts")
                        detail: root.viewModel.draftStorageDirectory

                        AppIconButton {
                            ui: root.ui
                            iconSource: root.ui.materialIcon("folder-open")
                            accessibleName: qsTr("Open draft storage folder")
                            toolTipText: accessibleName
                            onClicked: root.viewModel.openDraftStorageDirectory()
                        }
                    }

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Database")
                        detail: root.viewModel.databaseStorageDirectory
                        showDivider: false

                        AppIconButton {
                            ui: root.ui
                            iconSource: root.ui.materialIcon("folder-open")
                            accessibleName: qsTr("Open database storage folder")
                            toolTipText: accessibleName
                            onClicked: root.viewModel.openDatabaseStorageDirectory()
                        }
                    }
                }

                SettingsSection {
                    ui: root.ui
                    title: qsTr("Data transfer")
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                    Layout.maximumWidth: 760

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Import configuration")
                        detail: qsTr("Import MQTT Plus backups or MQTTX connection exports. Existing data is kept and imported items are added as copies.")

                        AppButton {
                            ui: root.ui
                            text: qsTr("Choose file")
                            minimumWidth: 104
                            enabled: !root.configurationTransfer.busy
                            onClicked: importFileDialog.open()
                        }
                    }

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Export configuration")
                        detail: qsTr("Export connections, subscriptions, drafts, and portable settings. Scripts, message history, and logs are excluded.")
                        showDivider: false

                        AppButton {
                            ui: root.ui
                            text: qsTr("Export")
                            minimumWidth: 86
                            enabled: !root.configurationTransfer.busy
                            onClicked: {
                                exportSensitiveCheck.checked = false
                                exportOptionsDialog.open()
                            }
                        }
                    }
                }

                SettingsSection {
                    ui: root.ui
                    title: qsTr("Cleanup")
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                    Layout.maximumWidth: 760

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Messages on exit")
                        detail: qsTr("Choose whether MQTT messages are cleared when the app closes.")

                        AppComboBox {
                            ui: root.ui
                            Layout.preferredWidth: 150
                            model: root.cleanupLabels
                            currentIndex: root.viewModel.clearMessagesOnExitIndex
                            onActivated: (index) => root.viewModel.setClearMessagesOnExitIndex(index)
                        }
                    }

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Logs on exit")
                        detail: qsTr("Choose whether event logs are cleared when the app closes.")

                        AppComboBox {
                            ui: root.ui
                            Layout.preferredWidth: 150
                            model: root.cleanupLabels
                            currentIndex: root.viewModel.clearLogsOnExitIndex
                            onActivated: (index) => root.viewModel.setClearLogsOnExitIndex(index)
                        }
                    }

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Manual cleanup")
                        detail: qsTr("Clear stored data immediately.")
                        showDivider: false

                        AppButton {
                            ui: root.ui
                            text: qsTr("Messages")
                            minimumWidth: 96
                            onClicked: root.eventHistory.clearAllMessages()
                        }

                        AppButton {
                            ui: root.ui
                            text: qsTr("Logs")
                            minimumWidth: 74
                            onClicked: root.eventHistory.clearAllLogs()
                        }

                        AppButton {
                            ui: root.ui
                            text: qsTr("All")
                            danger: true
                            minimumWidth: 70
                            onClicked: root.eventHistory.clearAllHistory()
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 14
                }
            }
        }
    }

    FileDialog {
        id: importFileDialog

        title: qsTr("Import configuration")
        fileMode: FileDialog.OpenFile
        nameFilters: [
            qsTr("Configuration files (*.json)"),
            qsTr("All files (*)")
        ]
        onAccepted: root.configurationTransfer.inspectImportFile(selectedFile)
    }

    Connections {
        target: root.configurationTransfer

        function onImportPreviewReady() {
            importSensitiveCheck.checked = root.configurationTransfer.previewContainsSensitiveData
            importPreviewDialog.commitRequested = false
            importPreviewDialog.open()
        }
    }

    FileDialog {
        id: exportFileDialog

        title: qsTr("Export configuration")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "mqttplus.json"
        nameFilters: [qsTr("MQTT Plus configuration (*.mqttplus.json)")]
        onAccepted: root.configurationTransfer.exportConfiguration(
                        selectedFile,
                        exportSensitiveCheck.checked)
    }

    AppDialog {
        id: exportOptionsDialog

        ui: root.ui
        width: 520
        height: 248
        closePolicy: Popup.CloseOnEscape
        header: Item { implicitHeight: 0; visible: false }
        background: Rectangle {
            radius: root.ui.radiusLg
            color: root.ui.themePalette.dialogBg
            border.color: root.ui.themePalette.dialogBorder
        }
        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            Label {
                Layout.fillWidth: true
                text: qsTr("Export configuration")
                color: root.ui.textStrong
                font.pixelSize: root.ui.text2xl
                font.bold: true
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("The export can contain draft payloads. Store it as private data even when credentials are excluded.")
                color: root.ui.textMuted
                font.pixelSize: root.ui.textSm
                wrapMode: Text.WordWrap
            }

            AppCheckBox {
                id: exportSensitiveCheck

                ui: root.ui
                text: qsTr("Include passwords, authentication data, and private keys")
                checked: false
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Item { Layout.fillWidth: true }

                AppButton {
                    ui: root.ui
                    text: qsTr("Cancel")
                    minimumWidth: 78
                    onClicked: exportOptionsDialog.close()
                }

                AppButton {
                    ui: root.ui
                    text: qsTr("Continue")
                    primary: true
                    minimumWidth: 92
                    onClicked: {
                        exportOptionsDialog.close()
                        exportFileDialog.open()
                    }
                }
            }
        }
    }

    AppDialog {
        id: importPreviewDialog

        property bool commitRequested: false

        ui: root.ui
        width: 570
        height: 460
        closePolicy: Popup.CloseOnEscape
        onClosed: {
            if (!commitRequested) {
                root.configurationTransfer.clearPreview()
            }
            commitRequested = false
        }
        header: Item { implicitHeight: 0; visible: false }
        background: Rectangle {
            radius: root.ui.radiusLg
            color: root.ui.themePalette.dialogBg
            border.color: root.ui.themePalette.dialogBorder
        }
        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            Label {
                Layout.fillWidth: true
                text: qsTr("Import preview")
                color: root.ui.textStrong
                font.pixelSize: root.ui.text2xl
                font.bold: true
            }

            Label {
                Layout.fillWidth: true
                text: root.configurationTransfer.previewFormat === "mqttx"
                      ? qsTr("MQTTX connection export")
                      : qsTr("MQTT Plus configuration")
                color: root.ui.themePalette.infoText
                font.pixelSize: root.ui.textSm
                font.bold: true
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("%1 connections · %2 subscriptions · %3 drafts")
                    .arg(root.configurationTransfer.previewConnectionCount)
                    .arg(root.configurationTransfer.previewSubscriptionCount)
                    .arg(root.configurationTransfer.previewDraftCount)
                color: root.ui.textStrong
                font.pixelSize: root.ui.textMd
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: root.ui.radiusMd
                color: root.ui.themePalette.innerPanelBg
                border.color: root.ui.themePalette.innerPanelBorder
                visible: root.configurationTransfer.previewWarnings.length > 0

                Flickable {
                    anchors.fill: parent
                    anchors.margins: 12
                    contentWidth: width
                    contentHeight: warningColumn.implicitHeight
                    clip: true

                    ColumnLayout {
                        id: warningColumn

                        width: parent.width
                        spacing: 8

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Compatibility notes")
                            color: root.ui.textStrong
                            font.pixelSize: root.ui.textSm
                            font.bold: true
                        }

                        Repeater {
                            model: root.configurationTransfer.previewWarnings

                            delegate: Label {
                                required property string modelData

                                Layout.fillWidth: true
                                text: "• " + modelData
                                color: root.ui.textMuted
                                font.pixelSize: root.ui.textSm
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }

            Item {
                Layout.fillHeight: true
                visible: root.configurationTransfer.previewWarnings.length === 0
            }

            AppCheckBox {
                id: importSensitiveCheck

                ui: root.ui
                visible: root.configurationTransfer.previewContainsSensitiveData
                text: qsTr("Import passwords, authentication data, and private keys")
                checked: true
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Imported connections remain disconnected. Existing connections and drafts are not replaced. Scripts are not imported.")
                color: root.ui.textMuted
                font.pixelSize: root.ui.textXs
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Item { Layout.fillWidth: true }

                AppButton {
                    ui: root.ui
                    text: qsTr("Cancel")
                    minimumWidth: 78
                    onClicked: {
                        importPreviewDialog.close()
                    }
                }

                AppButton {
                    ui: root.ui
                    text: root.configurationTransfer.busy ? qsTr("Importing…") : qsTr("Import")
                    primary: true
                    minimumWidth: 92
                    enabled: !root.configurationTransfer.busy
                    onClicked: {
                        importPreviewDialog.commitRequested = true
                        root.configurationTransfer.importPreview(importSensitiveCheck.checked)
                        importPreviewDialog.close()
                    }
                }
            }
        }
    }
}
