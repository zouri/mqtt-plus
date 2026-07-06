pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

Item {
    id: root

    required property var publisher
    required property var publishStatus
    required property var status
    required property var ui

    property bool expanded: true
    property int composerHeight: 220
    readonly property int collapsedHeight: 50
    readonly property int minComposerHeight: 180
    readonly property int maxComposerHeight: 420
    readonly property string publishFeedback: root.publishStatus.state && root.publishStatus.state !== "idle"
                                              ? (root.publishStatus.reason && root.publishStatus.reason.length > 0
                                                 ? root.publishStatus.reason
                                                 : qsTr("Publish status: %1").arg(root.ui.statusLabel(root.publishStatus.state)))
                                              : ""

    Layout.fillWidth: true
    Layout.preferredHeight: root.expanded ? root.composerHeight : root.collapsedHeight
    Layout.minimumHeight: root.expanded ? root.minComposerHeight : root.collapsedHeight
    SplitView.fillWidth: true
    SplitView.preferredHeight: root.expanded ? root.composerHeight : root.collapsedHeight
    SplitView.minimumHeight: root.expanded ? root.minComposerHeight : root.collapsedHeight
    SplitView.maximumHeight: root.expanded ? root.maxComposerHeight : root.collapsedHeight

    function resizeComposer(height) {
        root.composerHeight = Math.max(
                    root.minComposerHeight,
                    Math.min(root.maxComposerHeight, Math.round(height)))
    }

    function revealDraftEditor() {
        root.expanded = true
        publishPayloadArea.forceActiveFocus()
    }

    onHeightChanged: {
        if (root.expanded && height > 0) {
            root.resizeComposer(height)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            Rectangle {
                anchors.fill: parent
                color: root.ui.themePalette.windowBg
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                anchors.topMargin: 10
                anchors.bottomMargin: 10
                spacing: 7

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    spacing: 7

                    Label {
                        text: qsTr("Publish Message")
                        color: root.ui.textStrong
                        font.pixelSize: 13
                        font.bold: true
                    }

                    Label {
                        visible: root.publishFeedback.length > 0
                        text: root.publishFeedback
                        color: root.publishStatus.state === "failed"
                               ? root.ui.themePalette.errorText
                               : root.ui.textMuted
                        font.pixelSize: 11
                        elide: Label.ElideRight
                        Layout.fillWidth: true
                    }

                    Item {
                        visible: root.publishFeedback.length === 0
                        Layout.fillWidth: true
                    }

                    AppButton {
                        ui: root.ui
                        text: root.expanded ? qsTr("Collapse") : qsTr("Expand")
                        minimumWidth: 74
                        onClicked: root.expanded = !root.expanded
                    }
                }

                RowLayout {
                    visible: root.expanded
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? 52 : 0
                    spacing: 7

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3

                        Label {
                            text: qsTr("Topic")
                            color: root.ui.textMuted
                            font.pixelSize: 10
                            font.bold: true
                        }

                        AppTextField {
                            ui: root.ui
                            id: publishTopicField
                            Layout.fillWidth: true
                            text: root.publisher.topic
                            placeholderText: qsTr("home/living-room/light/set")
                            onTextEdited: root.publisher.topic = text
                        }
                    }

                    ColumnLayout {
                        Layout.preferredWidth: 104
                        spacing: 3

                        Label {
                            text: qsTr("QoS")
                            color: root.ui.textMuted
                            font.pixelSize: 10
                            font.bold: true
                        }

                        AppComboBox {
                            ui: root.ui
                            id: publishQosBox
                            model: [qsTr("QoS 0"), qsTr("QoS 1")]
                            currentIndex: root.publisher.qos
                            Layout.fillWidth: true
                            onActivated: root.publisher.qos = currentIndex
                        }
                    }

                    ColumnLayout {
                        Layout.preferredWidth: 118
                        spacing: 3

                        Label {
                            text: qsTr("Payload format")
                            color: root.ui.textMuted
                            font.pixelSize: 10
                            font.bold: true
                        }

                        AppComboBox {
                            ui: root.ui
                            id: publishFormatBox
                            model: root.publisher.payloadFormats
                            currentIndex: root.publisher.format
                            Layout.fillWidth: true
                            onActivated: root.publisher.format = currentIndex
                        }
                    }

                    ColumnLayout {
                        Layout.preferredWidth: 78
                        spacing: 3

                        Label {
                            text: qsTr("Retain")
                            color: root.ui.textMuted
                            font.pixelSize: 10
                            font.bold: true
                        }

                        AppCheckBox {
                            ui: root.ui
                            id: retainCheck
                            text: qsTr("Retain")
                            checked: root.publisher.retain
                            onToggled: root.publisher.retain = checked
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: root.expanded

                    AppTextArea {
                        ui: root.ui
                        id: publishPayloadArea
                        anchors.fill: parent
                        placeholderText: publishFormatBox.currentText === "JSON"
                                         ? "{\"value\": 23.7}"
                                         : qsTr("Payload")
                        text: root.publisher.payload
                        wrapMode: TextEdit.Wrap
                        onTextChanged: {
                            if (root.publisher.payload !== text) {
                                root.publisher.payload = text
                            }
                        }
                    }

                    AppIconButton {
                        ui: root.ui
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.rightMargin: 10
                        anchors.bottomMargin: 10
                        implicitWidth: 34
                        implicitHeight: 34
                        cornerRadius: 17
                        iconSource: root.ui.materialIcon("send")
                        iconSize: 17
                        primary: true
                        enabled: root.publisher.canPublish
                        accessibleName: qsTr("Publish message")
                        onClicked: root.publisher.publishDraft()
                    }
                }
            }
        }
    }
}
