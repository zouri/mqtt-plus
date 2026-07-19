pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

Rectangle {
    id: root

    required property AppUi ui
    required property var viewModel

    readonly property var themeLabels: [qsTr("System"), qsTr("Light"), qsTr("Dark")]
    readonly property var themeColorOptions: [
        { "key": "mint", "label": qsTr("Mint"), "color": "#35d0aa" },
        { "key": "blue", "label": qsTr("Blue"), "color": "#6aa3ff" },
        { "key": "violet", "label": qsTr("Violet"), "color": "#ad8cff" },
        { "key": "amber", "label": qsTr("Amber"), "color": "#f1b86a" },
        { "key": "rose", "label": qsTr("Rose"), "color": "#ff879d" }
    ]
    readonly property var languageLabels: [qsTr("System"), qsTr("English"), qsTr("Simplified Chinese")]
    readonly property var messagePayloadDisplayLabels: [qsTr("Compact"), qsTr("Expand on hover"), qsTr("Always show full")]
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

        implicitWidth: 40
        implicitHeight: 22
        text: ""

        indicator: Rectangle {
            implicitWidth: 40
            implicitHeight: 22
            radius: 11
            color: settingSwitch.checked
                   ? settingSwitch.ui.themePalette.buttonPrimaryBg
                   : settingSwitch.ui.themePalette.innerPanelBorder

            Rectangle {
                x: settingSwitch.checked ? 20 : 2
                y: 2
                width: 18
                height: 18
                radius: 9
                color: "#ffffff"

                Behavior on x {
                    NumberAnimation {
                        duration: 150
                        easing.type: Easing.OutCubic
                    }
                }
            }

            Behavior on color {
                ColorAnimation {
                    duration: 150
                    easing.type: Easing.OutCubic
                }
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
                            checked: root.viewModel.autoCollapseConnectionListOnConnect
                            onToggled: root.viewModel.autoCollapseConnectionListOnConnect = checked
                        }
                    }

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Message content")
                        detail: qsTr("Choose how much payload text is shown in the message list.")
                        showDivider: false

                        AppComboBox {
                            ui: root.ui
                            Layout.preferredWidth: 170
                            model: root.messagePayloadDisplayLabels
                            currentIndex: root.viewModel.messagePayloadDisplayModeIndex
                            onActivated: (index) => root.viewModel.setMessagePayloadDisplayModeIndex(index)
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
                            checked: root.viewModel.deleteHistoryWithSession
                            onToggled: root.viewModel.deleteHistoryWithSession = checked
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
                            checked: root.viewModel.saveMessagesWhenOutputPaused
                            onToggled: root.viewModel.saveMessagesWhenOutputPaused = checked
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
                            onClicked: root.viewModel.clearAllMessages()
                        }

                        AppButton {
                            ui: root.ui
                            text: qsTr("Logs")
                            minimumWidth: 74
                            onClicked: root.viewModel.clearAllLogs()
                        }

                        AppButton {
                            ui: root.ui
                            text: qsTr("All")
                            danger: true
                            minimumWidth: 70
                            onClicked: root.viewModel.clearAllHistory()
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
}
