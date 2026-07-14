pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

Rectangle {
    id: control

    required property AppUi ui
    required property var viewModel
    property string historyId: ""
    property var details: ({})
    property bool opened: false
    property real slideOffset: opened ? 0 : width

    signal closeRequested
    signal draftUsed

    width: Math.min(400, parent ? parent.width * 0.88 : 400)
    color: control.ui.themePalette.panelBg
    border.color: control.ui.themePalette.panelBorder
    transform: Translate { x: control.slideOffset }
    visible: control.opened || control.slideOffset < control.width
    activeFocusOnTab: control.opened
    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Message inspector")

    onHistoryIdChanged: {
        control.details = historyId.length > 0 ? control.viewModel.messageDetails(historyId) : ({});
    }

    onOpenedChanged: {
        if (opened) {
            control.forceActiveFocus();
        }
    }

    Keys.onEscapePressed: control.closeRequested()

    Behavior on slideOffset {
        NumberAnimation {
            duration: 200
            easing.type: Easing.OutCubic
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            Layout.leftMargin: 14
            Layout.rightMargin: 10
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: qsTr("Message Inspector")
                color: control.ui.textStrong
                font.pixelSize: 13
                font.bold: true
            }

            AppIconButton {
                ui: control.ui
                iconSource: control.ui.materialIcon("xmark")
                iconSize: 15
                implicitWidth: 28
                implicitHeight: 28
                cornerRadius: 7
                restBg: "transparent"
                outlineColor: "transparent"
                accessibleName: qsTr("Close message inspector")
                onClicked: control.closeRequested()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: control.ui.themePalette.separator
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: 14

                Item { Layout.preferredHeight: 1 }

                ColumnLayout {
                    visible: String(control.details.parsedPayload || "").length > 0
                    Layout.fillWidth: true
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                    spacing: 6

                    Label {
                        text: qsTr("Parsed result")
                        color: control.ui.textMuted
                        font.pixelSize: 11
                    }

                    TextEdit {
                        Layout.fillWidth: true
                        text: String(control.details.parsedPayload || "")
                        color: control.ui.textStrong
                        font.family: "Menlo"
                        font.pixelSize: 11
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextEdit.WrapAnywhere
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                    spacing: 6

                    Label {
                        text: control.details.fullPayloadAvailable === false
                              ? qsTr("Payload preview")
                              : qsTr("Payload")
                        color: control.ui.textMuted
                        font.pixelSize: 11
                    }

                    TextEdit {
                        Layout.fillWidth: true
                        text: String(control.details.fullPayload || qsTr("Select a message to inspect"))
                        color: control.ui.textStrong
                        font.family: "Menlo"
                        font.pixelSize: 11
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextEdit.WrapAnywhere
                    }

                    Label {
                        visible: control.details.fullPayloadAvailable === false
                        Layout.fillWidth: true
                        text: qsTr("The full payload was not stored. Hash: %1")
                                  .arg(String(control.details.payloadHash || qsTr("Unavailable")))
                        color: control.ui.themePalette.warningText
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                    columns: 2
                    columnSpacing: 10
                    rowSpacing: 8

                    Label { text: qsTr("Alias"); color: control.ui.textMuted; font.pixelSize: 11 }
                    Label { Layout.fillWidth: true; text: String(control.details.alias || qsTr("Unavailable")); color: control.ui.textStrong; elide: Label.ElideRight; font.pixelSize: 11 }
                    Label { text: qsTr("Topic"); color: control.ui.textMuted; font.pixelSize: 11 }
                    Label { Layout.fillWidth: true; text: String(control.details.topic || qsTr("Unavailable")); color: control.ui.textStrong; elide: Label.ElideRight; font.family: "Menlo"; font.pixelSize: 11 }
                    Label { text: qsTr("Direction"); color: control.ui.textMuted; font.pixelSize: 11 }
                    Label { text: control.details.direction === "outgoing" ? qsTr("Sent") : qsTr("Received"); color: control.ui.textStrong; font.pixelSize: 11 }
                    Label { text: qsTr("Time"); color: control.ui.textMuted; font.pixelSize: 11 }
                    Label { text: String(control.details.timestamp || qsTr("Unavailable")); color: control.ui.textStrong; font.family: "Menlo"; font.pixelSize: 11 }
                    Label { text: qsTr("QoS"); color: control.ui.textMuted; font.pixelSize: 11 }
                    Label { text: Number(control.details.qos) >= 0 ? String(control.details.qos) : qsTr("Unavailable"); color: control.ui.textStrong; font.pixelSize: 11 }
                    Label { text: qsTr("Format"); color: control.ui.textMuted; font.pixelSize: 11 }
                    Label { text: String(control.details.payloadFormat || qsTr("Unavailable")); color: control.ui.textStrong; font.pixelSize: 11 }
                    Label { text: qsTr("Size"); color: control.ui.textMuted; font.pixelSize: 11 }
                    Label { text: qsTr("%1 B").arg(Number(control.details.payloadSize || 0)); color: control.ui.textStrong; font.pixelSize: 11 }
                    Label { text: qsTr("Retain"); color: control.ui.textMuted; font.pixelSize: 11 }
                    Label { text: control.details.retainKnown ? (control.details.retain ? qsTr("Yes") : qsTr("No")) : qsTr("Unavailable"); color: control.ui.textStrong; font.pixelSize: 11 }
                }

                Flow {
                    Layout.fillWidth: true
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                    spacing: 7

                    AppButton {
                        ui: control.ui
                        visible: String(control.details.parsedPayload || "").length > 0
                        text: qsTr("Copy parsed result")
                        onClicked: control.viewModel.copyMessagePayload("0", String(control.details.parsedPayload), "", Number(control.details.testFormat || 0))
                    }

                    AppButton {
                        ui: control.ui
                        text: qsTr("Copy Payload")
                        onClicked: control.viewModel.copyMessagePayload(control.historyId, String(control.details.fullPayload || ""), String(control.details.testPayload || ""), Number(control.details.testFormat || 0))
                    }

                    AppButton {
                        ui: control.ui
                        text: qsTr("Copy Topic")
                        onClicked: control.viewModel.copyMessageTopic(String(control.details.topic || ""))
                    }

                    AppButton {
                        ui: control.ui
                        text: qsTr("Use as draft")
                        primary: true
                        onClicked: {
                            control.viewModel.useMessageAsDraft(
                                control.historyId,
                                String(control.details.topic || ""),
                                String(control.details.fullPayload || ""),
                                String(control.details.testPayload || ""),
                                Number(control.details.testFormat || 0));
                            control.draftUsed();
                        }
                    }
                }

                Item { Layout.preferredHeight: 8 }
            }
        }
    }
}
