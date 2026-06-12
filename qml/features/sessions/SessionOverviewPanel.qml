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

    showTopBorder: false
    Layout.fillWidth: true
    Layout.preferredHeight: 126
    Layout.minimumHeight: 126

    ColumnLayout {
        id: currentSessionColumn
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.topMargin: 12
        anchors.bottomMargin: 12
        spacing: 9

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 5

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

            Button {
                id: editButton
                enabled: control.status.state === "disconnected"
                text: qsTr("Edit connection")
                font.pixelSize: 12
                font.bold: true
                leftPadding: 8
                rightPadding: 8
                topPadding: 4
                bottomPadding: 4
                icon.source: control.ui.materialIcon("edit")
                icon.width: 15
                icon.height: 15
                icon.color: enabled ? control.ui.themePalette.infoText : control.ui.textMuted
                contentItem: RowLayout {
                    spacing: 5

                    Image {
                        source: editButton.icon.source
                        sourceSize.width: 15
                        sourceSize.height: 15
                        Layout.preferredWidth: 15
                        Layout.preferredHeight: 15
                        opacity: editButton.enabled ? 1.0 : 0.45
                    }

                    Label {
                        text: editButton.text
                        color: editButton.enabled ? control.ui.themePalette.infoText : control.ui.textMuted
                        font.pixelSize: editButton.font.pixelSize
                        font.bold: true
                    }
                }
                background: Rectangle {
                    radius: 10
                    color: editButton.hovered ? control.ui.themePalette.actionHoverBg : "transparent"
                }
                onClicked: control.sessionEditor.openForEdit(control.appController.currentSessionIndex)
            }

            AppButton {
                ui: control.ui
                text: control.connectionActionText
                minimumWidth: 74
                primary: !control.canDisconnect

                onClicked: {
                    if (control.canDisconnect) {
                        control.appController.disconnectCurrentSession()
                    } else {
                        control.appController.connectCurrentSession()
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
