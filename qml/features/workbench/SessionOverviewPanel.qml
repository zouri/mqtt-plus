pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

AppPanel {
    id: control

    required property var session
    required property var status
    required property var workbench
    required property var sessionEditor

    readonly property bool canDisconnect: control.status.state === "connected"
                                           || control.status.state === "connecting"
                                           || control.status.state === "disconnecting"
    readonly property bool isBusy: control.status.state === "connecting"
                                   || control.status.state === "disconnecting"
    readonly property bool hasError: Boolean(control.status.hasError)
    readonly property string endpointText: `${control.session.host || "-"}:${control.session.port || "-"}`
    readonly property string connectionActionText: control.status.state === "connected"
                                                   ? qsTr("Disconnect")
                                                   : (control.status.state === "connecting"
                                                      ? qsTr("Connecting...")
                                                      : (control.hasError ? qsTr("Retry") : qsTr("Connect")))
    readonly property url connectionActionIcon: control.status.state === "connecting"
                                                ? control.ui.materialIcon("xmark")
                                                : (control.canDisconnect
                                                   ? control.ui.materialIcon("plug-off")
                                                   : control.ui.materialIcon("plug"))

    showTopBorder: false
    showRightBorder: false
    showBottomBorder: false

    Layout.fillWidth: true
    Layout.minimumHeight: 70
    Layout.preferredHeight: 80

    ColumnLayout {
        id: currentSessionColumn
        anchors.fill: parent
        anchors.topMargin: 0
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.bottomMargin: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: control.session.name || qsTr("No session")
                        color: control.ui.textStrong
                        font.pixelSize: 24
                        font.bold: true
                        elide: Label.ElideRight
                        Layout.fillWidth: true
                    }

                }

                Label {
                    text: qsTr("%1  (%2)").arg(control.endpointText).arg(control.session.transportLabel || "TCP")
                    color: control.ui.textMuted
                    font.pixelSize: 12
                    elide: Label.ElideRight
                    Layout.fillWidth: true
                }
            }

            AppIconButton {
                id: editButton
                ui: control.ui
                enabled: control.status.state === "disconnected"
                iconSource: control.ui.materialIcon("edit")
                iconSize: 15
                implicitWidth: 32
                implicitHeight: 32
                cornerRadius: 16
                restBg: control.ui.themePalette.innerPanelBg
                outlineColor: control.ui.themePalette.innerPanelBorder
                symbolColor: control.ui.themePalette.infoText
                accessibleName: qsTr("Edit connection")
                onClicked: control.sessionEditor.openForEdit(control.workbench.currentSessionIndex)
            }

            AppIconButton {
                ui: control.ui
                iconSource: control.connectionActionIcon
                iconSize: 16
                implicitWidth: 32
                implicitHeight: 32
                cornerRadius: 16
                primary: !control.canDisconnect
                danger: control.canDisconnect
                accessibleName: control.connectionActionText
                toolTipText: control.connectionActionText

                onClicked: {
                    if (control.canDisconnect) {
                        control.workbench.disconnectCurrentSession()
                    } else {
                        control.workbench.connectCurrentSession()
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 9

            Label {
                text: qsTr("Protocol")
                color: control.ui.textMuted
                font.pixelSize: 11
            }

            Label {
                text: control.session.protocolVersionName || "MQTT 5"
                color: control.ui.textStrong
                font.pixelSize: 12
                font.bold: true
            }

            Label {
                text: qsTr("Client ID")
                color: control.ui.textMuted
                font.pixelSize: 11
            }

            Label {
                Layout.fillWidth: true
                text: control.session.clientId || "-"
                color: control.ui.textStrong
                font.pixelSize: 12
                font.bold: true
                elide: Label.ElideRight
            }
        }
    }
}
