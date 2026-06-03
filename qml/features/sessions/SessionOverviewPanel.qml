pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

AppPanel {
    id: control

    required property var session
    required property var status
    required property var appController
    required property SessionEditorDialog sessionEditor

    showTopBorder: false
    Layout.fillWidth: true
    Layout.preferredHeight: currentSessionColumn.implicitHeight + 28
    Layout.minimumHeight: currentSessionColumn.implicitHeight + 28

    ColumnLayout {
        id: currentSessionColumn
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3

                Label {
                    text: control.session.name || "No session"
                    color: control.ui.textStrong
                    font.pixelSize: 24
                    font.bold: true
                    elide: Label.ElideRight
                    Layout.fillWidth: true
                }

                Label {
                    text: `${control.session.host || "-"}:${control.session.port || "-"}  (${control.session.transportLabel || "TCP"})`
                    color: control.ui.textMuted
                    font.pixelSize: 13
                    elide: Label.ElideRight
                    Layout.fillWidth: true
                }
            }

            AppStatusBadge {
                ui: control.ui
                status: control.status.state
                badgeRadius: 11
                horizontalPadding: 9
                verticalPadding: 4
                Layout.alignment: Qt.AlignTop
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    text: "Protocol"
                    color: control.ui.textMuted
                    font.pixelSize: 11
                }

                Label {
                    text: control.session.protocolVersionName || "MQTT 5"
                    color: control.ui.textStrong
                    font.pixelSize: 13
                    font.bold: true
                }
            }

            Rectangle {
                implicitWidth: 1
                implicitHeight: 26
                Layout.preferredWidth: 1
                Layout.preferredHeight: 26
                radius: 1
                color: control.ui.themePalette.separator
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    text: "Client ID"
                    color: control.ui.textMuted
                    font.pixelSize: 11
                }

                Label {
                    text: control.session.clientId || "-"
                    color: control.ui.textStrong
                    font.pixelSize: 13
                    font.bold: true
                    elide: Label.ElideRight
                    Layout.fillWidth: true
                }
            }

            RowLayout {
                spacing: 8
                Layout.alignment: Qt.AlignVCenter

                AppIconButton {
                    ui: control.ui
                    readonly property bool canDisconnect: control.status.state === "connected"
                                                         || control.status.state === "connecting"
                    iconSource: control.ui.materialIcon(canDisconnect ? "power" : "link")
                    iconSize: 17
                    implicitWidth: 38
                    implicitHeight: 38
                    primary: !canDisconnect
                    danger: canDisconnect
                    toolTipText: canDisconnect ? "Disconnect" : "Connect"

                    onClicked: {
                        if (canDisconnect) {
                            control.appController.disconnectCurrentSession()
                        } else {
                            control.appController.connectCurrentSession()
                        }
                    }
                }

                AppIconButton {
                    ui: control.ui
                    iconSource: control.ui.materialIcon("edit")
                    iconSize: 17
                    implicitWidth: 38
                    implicitHeight: 38
                    enabled: control.status.state === "disconnected"
                    toolTipText: "Edit connection"
                    onClicked: control.sessionEditor.openForEdit(control.appController.currentSessionIndex)
                }
            }
        }
    }
}
