pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

Rectangle {
    id: root

    required property AppUi ui
    required property var settings

    readonly property var themeValues: ["system", "light", "dark"]
    readonly property var themeLabels: [qsTr("System"), qsTr("Light"), qsTr("Dark")]
    readonly property var languageValues: ["system", "en", "zh_CN"]
    readonly property var languageLabels: [qsTr("System"), qsTr("English"), qsTr("Simplified Chinese")]
    readonly property var retentionValues: [1000, 5000, 10000, 0]
    readonly property var messageRetentionLabels: [qsTr("1,000 messages"), qsTr("5,000 messages"), qsTr("10,000 messages"), qsTr("Unlimited")]
    readonly property var logRetentionLabels: [qsTr("500 logs"), qsTr("2,000 logs"), qsTr("5,000 logs"), qsTr("Unlimited")]
    readonly property var logRetentionValues: [500, 2000, 5000, 0]
    readonly property var pageSizeValues: [200, 500, 1000]
    readonly property var pageSizeLabels: [qsTr("200 rows"), qsTr("500 rows"), qsTr("1,000 rows")]
    readonly property var payloadLimitValues: [262144, 1048576, 5242880, 16777216]
    readonly property var payloadLimitLabels: [qsTr("256 KiB"), qsTr("1 MiB"), qsTr("5 MiB"), qsTr("16 MiB")]
    readonly property var cleanupValues: ["never", "current", "all"]
    readonly property var cleanupLabels: [qsTr("Do not clear"), qsTr("Current session"), qsTr("All sessions")]

    color: root.ui.themePalette.windowBg

    function optionIndex(values, value) {
        for (let i = 0; i < values.length; ++i) {
            if (values[i] === value) {
                return i
            }
        }
        return 0
    }

    function optionValue(values, index) {
        return values[Math.max(0, Math.min(index, values.length - 1))]
    }

    component SettingsSection: Rectangle {
        id: section

        required property AppUi ui
        property string title: ""
        default property alias rows: sectionBody.data

        Layout.fillWidth: true
        Layout.preferredHeight: sectionColumn.implicitHeight + 24
        radius: section.ui.innerRadius
        color: section.ui.themePalette.itemBg
        border.color: section.ui.themePalette.itemBorder

        ColumnLayout {
            id: sectionColumn
            anchors.fill: parent
            anchors.margins: 12
            spacing: 6

            Label {
                Layout.fillWidth: true
                text: section.title
                color: section.ui.textStrong
                font.pixelSize: 14
                font.bold: true
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
        Layout.preferredHeight: Math.max(54, rowLayout.implicitHeight + 14)
        color: "transparent"

        RowLayout {
            id: rowLayout
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
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

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: root.ui.themePalette.windowBg

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 32
                anchors.rightMargin: 32
                spacing: 10

                Label {
                    text: qsTr("Settings")
                    color: root.ui.textStrong
                    font.pixelSize: 22
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
                anchors.leftMargin: 32
                anchors.rightMargin: 32
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
                width: settingsFlickable.width
                spacing: 12

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 2
                }

                SettingsSection {
                    ui: root.ui
                    title: qsTr("Appearance")
                    Layout.leftMargin: 20
                    Layout.rightMargin: 24

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Theme")
                        detail: qsTr("Choose how the interface follows system appearance.")

                        AppComboBox {
                            ui: root.ui
                            Layout.preferredWidth: 150
                            model: root.themeLabels
                            currentIndex: root.optionIndex(root.themeValues, root.settings.themeMode)
                            onActivated: (index) => root.settings.themeMode = root.optionValue(root.themeValues, index)
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
                            currentIndex: root.optionIndex(root.languageValues, root.settings.languageMode)
                            onActivated: (index) => root.settings.languageMode = root.optionValue(root.languageValues, index)
                        }
                    }
                }

                SettingsSection {
                    ui: root.ui
                    title: qsTr("History")
                    Layout.leftMargin: 20
                    Layout.rightMargin: 24

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Saved messages")
                        detail: qsTr("Maximum MQTT messages retained per connection.")

                        AppComboBox {
                            ui: root.ui
                            Layout.preferredWidth: 170
                            model: root.messageRetentionLabels
                            currentIndex: root.optionIndex(root.retentionValues, root.settings.messageRetentionLimit)
                            onActivated: (index) => root.settings.messageRetentionLimit = root.optionValue(root.retentionValues, index)
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
                            currentIndex: root.optionIndex(root.logRetentionValues, root.settings.logRetentionLimit)
                            onActivated: (index) => root.settings.logRetentionLimit = root.optionValue(root.logRetentionValues, index)
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
                            currentIndex: root.optionIndex(root.pageSizeValues, root.settings.historyPageSize)
                            onActivated: (index) => root.settings.historyPageSize = root.optionValue(root.pageSizeValues, index)
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
                            currentIndex: root.optionIndex(root.payloadLimitValues, root.settings.maxIncomingPayloadBytes)
                            onActivated: (index) => root.settings.maxIncomingPayloadBytes = root.optionValue(root.payloadLimitValues, index)
                        }
                    }

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Delete connection history")
                        detail: qsTr("Remove stored messages and logs when a connection is deleted.")
                        showDivider: false

                        AppCheckBox {
                            ui: root.ui
                            text: qsTr("Enabled")
                            checked: root.settings.deleteHistoryWithSession
                            onToggled: root.settings.deleteHistoryWithSession = checked
                        }
                    }
                }

                SettingsSection {
                    ui: root.ui
                    title: qsTr("Output")
                    Layout.leftMargin: 20
                    Layout.rightMargin: 24

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Save while paused")
                        detail: qsTr("Keep storing incoming messages when output is paused.")
                        showDivider: false

                        AppCheckBox {
                            ui: root.ui
                            text: qsTr("Enabled")
                            checked: root.settings.saveMessagesWhenOutputPaused
                            onToggled: root.settings.saveMessagesWhenOutputPaused = checked
                        }
                    }
                }

                SettingsSection {
                    ui: root.ui
                    title: qsTr("Cleanup")
                    Layout.leftMargin: 20
                    Layout.rightMargin: 24

                    SettingRow {
                        ui: root.ui
                        title: qsTr("Messages on exit")
                        detail: qsTr("Choose whether MQTT messages are cleared when the app closes.")

                        AppComboBox {
                            ui: root.ui
                            Layout.preferredWidth: 150
                            model: root.cleanupLabels
                            currentIndex: root.optionIndex(root.cleanupValues, root.settings.clearMessagesOnExit)
                            onActivated: (index) => root.settings.clearMessagesOnExit = root.optionValue(root.cleanupValues, index)
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
                            currentIndex: root.optionIndex(root.cleanupValues, root.settings.clearLogsOnExit)
                            onActivated: (index) => root.settings.clearLogsOnExit = root.optionValue(root.cleanupValues, index)
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
                            onClicked: root.settings.clearAllMessages()
                        }

                        AppButton {
                            ui: root.ui
                            text: qsTr("Logs")
                            minimumWidth: 74
                            onClicked: root.settings.clearAllLogs()
                        }

                        AppButton {
                            ui: root.ui
                            text: qsTr("All")
                            danger: true
                            minimumWidth: 70
                            onClicked: root.settings.clearAllHistory()
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
