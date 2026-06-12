pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../components"

Rectangle {
    id: root

    required property AppUi ui
    required property var appController

    color: root.ui.themePalette.windowBg

    AppPanel {
        ui: root.ui
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
                    color: root.ui.textStrong
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
                radius: root.ui.innerRadius
                color: root.ui.themePalette.itemBg
                border.color: root.ui.themePalette.itemBorder

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
                            color: root.ui.textStrong
                            font.pixelSize: 15
                            font.bold: true
                        }

                        Label {
                            text: qsTr("Current theme: %1").arg(root.ui.themeModeMeta(root.appController.themeMode).label)
                            color: root.ui.textMuted
                            font.pixelSize: 13
                        }
                    }

                    AppButton {
                        ui: root.ui
                        text: qsTr("Switch Theme")
                        minimumWidth: 116
                        onClicked: {
                            const meta = root.ui.themeModeMeta(root.appController.themeMode)
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
