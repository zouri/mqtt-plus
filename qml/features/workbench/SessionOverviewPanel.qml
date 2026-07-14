pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

AppPanel {
    id: control

    required property var session
    required property var status
    required property var viewModel

    signal sessionEditRequested(int index)
    signal connectionConnectRequested

    readonly property string statusState: control.status.state || "idle"
    readonly property bool canDisconnect: control.statusState === "connected" || control.statusState === "connecting" || control.statusState === "disconnecting"
    readonly property bool hasError: Boolean(control.status.hasError)
    readonly property string effectiveState: control.hasError ? "error" : control.statusState
    readonly property color statusDotColor: control.ui.stateColor(control.effectiveState)
    readonly property string statusToolTipText: control.hasError && control.status.lastError
                                                    ? qsTr("%1: %2").arg(control.ui.statusLabel(control.statusState)).arg(control.status.lastError)
                                                    : control.ui.statusLabel(control.statusState)
    readonly property string endpointText: `${control.session.host || "-"}:${control.session.port || "-"}`
    readonly property string clientIdText: qsTr("Client ID %1").arg(control.session.clientId || "-")
    readonly property string connectionActionText: control.statusState === "connected" ? qsTr("Disconnect") : (control.statusState === "connecting" ? qsTr("Connecting...") : (control.hasError ? qsTr("Retry") : qsTr("Connect")))
    readonly property url connectionActionIcon: control.statusState === "connecting" ? control.ui.materialIcon("xmark") : (control.canDisconnect ? control.ui.materialIcon("plug-off") : control.ui.materialIcon("plug"))

    showTopBorder: false
    showLeftBorder: false
    showRightBorder: false
    color: control.ui.themePalette.headerBg

    Layout.fillWidth: true
    Layout.minimumHeight: 96
    Layout.preferredHeight: 96
    Layout.maximumHeight: 96

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 8
        anchors.bottomMargin: 8
        spacing: 5

        RowLayout {
            Layout.fillWidth: true
            spacing: 9

            Rectangle {
                id: statusDot

                Layout.preferredWidth: 9
                Layout.preferredHeight: 9
                radius: 5
                color: control.statusDotColor
                Accessible.role: Accessible.Indicator
                Accessible.name: control.statusToolTipText

                HoverHandler {
                    id: statusDotHover
                }

                AppToolTip {
                    ui: control.ui
                    text: control.statusToolTipText
                    position: AppToolTip.Position.Bottom
                    active: statusDotHover.hovered
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: control.session.name || qsTr("No session")
                color: control.ui.textStrong
                font.pixelSize: 18
                font.bold: true
                elide: Label.ElideRight
            }

            AppIconButton {
                ui: control.ui
                enabled: control.statusState === "disconnected"
                iconSource: control.ui.materialIcon("edit")
                iconSize: 16
                implicitWidth: 30
                implicitHeight: 30
                cornerRadius: 15
                restBg: control.ui.themePalette.innerPanelBg
                outlineColor: control.ui.themePalette.innerPanelBorder
                symbolColor: control.ui.themePalette.infoText
                accessibleName: qsTr("Edit connection")
                onClicked: control.sessionEditRequested(control.viewModel.currentSessionIndex)
            }

            AppIconButton {
                ui: control.ui
                iconSource: control.connectionActionIcon
                iconSize: 16
                implicitWidth: 30
                implicitHeight: 30
                cornerRadius: 15
                primary: !control.canDisconnect
                danger: control.canDisconnect
                accessibleName: control.connectionActionText
                toolTipText: control.connectionActionText

                onClicked: {
                    if (!control.canDisconnect) {
                        control.connectionConnectRequested();
                    }
                    control.viewModel.toggleCurrentSessionConnection();
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: control.endpointText
                color: control.ui.textStrong
                font.family: "Menlo"
                font.pixelSize: 12
                font.bold: true
                elide: Label.ElideRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: qsTr("%1 · %2 · Keep Alive %3s")
                      .arg(control.session.protocolVersionName || "MQTT 5")
                      .arg(control.session.transportLabel || "TCP")
                      .arg(control.session.keepAliveSeconds || 30)
                color: control.ui.themePalette.textSubtle
                font.pixelSize: 10
                elide: Label.ElideRight
            }
        }
    }
}
