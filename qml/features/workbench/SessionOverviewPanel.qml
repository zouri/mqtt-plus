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

    readonly property bool canDisconnect: control.status.state === "connected" || control.status.state === "connecting" || control.status.state === "disconnecting"
    readonly property bool isBusy: control.status.state === "connecting" || control.status.state === "disconnecting"
    readonly property bool hasError: Boolean(control.status.hasError)
    readonly property string endpointText: `${control.session.host || "-"}:${control.session.port || "-"}`
    readonly property string connectionActionText: control.status.state === "connected" ? qsTr("Disconnect") : (control.status.state === "connecting" ? qsTr("Connecting...") : (control.hasError ? qsTr("Retry") : qsTr("Connect")))
    readonly property url connectionActionIcon: control.status.state === "connecting" ? control.ui.materialIcon("xmark") : (control.canDisconnect ? control.ui.materialIcon("plug-off") : control.ui.materialIcon("plug"))

    showTopBorder: false
    showLeftBorder: false
    showRightBorder: false
    // showBottomBorder: false
    color: control.ui.themePalette.headerBg

    Layout.fillWidth: true
    Layout.minimumHeight: 82
    Layout.preferredHeight: 86

    ColumnLayout {
        id: currentSessionColumn
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 8
        anchors.bottomMargin: 8
        spacing: 6

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
                        font.pixelSize: 18
                        font.bold: true
                        elide: Label.ElideRight
                        Layout.fillWidth: true
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: qsTr("Host")
                        color: control.ui.textMuted
                        font.pixelSize: 10
                    }

                    Label {
                        Layout.fillWidth: true
                        text: control.endpointText
                        color: control.ui.textMuted
                        font.pixelSize: 11
                        elide: Label.ElideRight
                    }

                    AppBadge {
                        ui: control.ui
                        visible: (control.session.transportLabel || "TCP") !== "TCP"
                        label: control.session.transportLabel
                        badgeBg: control.ui.themePalette.successBg
                        badgeBorder: "transparent"
                        badgeText: control.ui.themePalette.successText
                        badgeRadius: 5
                        horizontalPadding: 6
                        verticalPadding: 1
                    }
                }
            }

            AppIconButton {
                id: editButton
                ui: control.ui
                enabled: control.status.state === "disconnected"
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
            spacing: 10

            Repeater {
                model: [
                    {
                        "label": qsTr("Protocol"),
                        "value": control.session.protocolVersionName || "MQTT 5",
                        "expand": false
                    },
                    {
                        "label": qsTr("MQTT ID"),
                        "value": control.session.clientId || "-",
                        "expand": true
                    },
                    {
                        "label": qsTr("Status"),
                        "value": control.ui.statusLabel(control.status.state || "idle"),
                        "expand": false
                    }
                ]

                delegate: RowLayout {
                    id: metricDelegate

                    required property var modelData

                    Layout.fillWidth: metricDelegate.modelData.expand
                    spacing: 4

                    Label {
                        text: metricDelegate.modelData.label
                        color: control.ui.themePalette.textSubtle
                        font.pixelSize: 10
                        elide: Label.ElideRight
                    }

                    Label {
                        Layout.fillWidth: metricDelegate.modelData.expand
                        text: metricDelegate.modelData.value
                        color: metricDelegate.modelData.label === qsTr("Status") ? control.ui.stateColor(control.status.state || "idle") : control.ui.textStrong
                        font.pixelSize: 11
                        font.bold: true
                        elide: Label.ElideRight
                    }
                }
            }
        }
    }
}
