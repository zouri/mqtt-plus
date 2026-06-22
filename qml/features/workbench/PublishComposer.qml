pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

Item {
    id: root

    required property var workbench
    required property var publishStatus
    required property var status
    required property var ui

    property bool expanded: true
    property int composerHeight: 220
    readonly property int collapsedHeight: 50
    readonly property int minComposerHeight: 180
    readonly property int maxComposerHeight: 420
    readonly property bool isConnected: root.status.state === "connected"
    readonly property bool hasTopic: publishTopicField.text.trim().length > 0
    readonly property bool canPublish: root.isConnected && root.hasTopic
    readonly property string publishFeedback: root.publishStatus.state && root.publishStatus.state !== "idle"
                                              ? (root.publishStatus.reason && root.publishStatus.reason.length > 0
                                                 ? root.publishStatus.reason
                                                 : qsTr("Publish status: %1").arg(root.ui.statusLabel(root.publishStatus.state)))
                                              : ""

    Layout.fillWidth: true
    Layout.preferredHeight: resizeHandle.Layout.preferredHeight
                            + (root.expanded ? root.composerHeight : root.collapsedHeight)
    Layout.minimumHeight: Layout.preferredHeight

    function resizeComposer(height) {
        root.composerHeight = Math.max(
                    root.minComposerHeight,
                    Math.min(root.maxComposerHeight, Math.round(height)))
    }

    function resizeComposerFromDrag(height) {
        if (height <= root.minComposerHeight) {
            root.expanded = false
            return
        }

        root.expanded = true
        root.resizeComposer(height)
    }

    function resizeY(mouse) {
        return resizeMouse.mapToItem(root, mouse.x, mouse.y).y
    }

    function setDraft(topic, payload, format) {
        publishTopicField.text = topic || ""
        publishPayloadArea.text = payload || ""
        if (format >= 0 && format < publishFormatBox.count) {
            publishFormatBox.currentIndex = format
        }
        root.expanded = true
        publishPayloadArea.forceActiveFocus()
    }

    function draftFromFields() {
        return {
            "topic": publishTopicField.text.trim(),
            "payload": publishPayloadArea.text,
            "format": publishFormatBox.currentIndex,
            "qos": publishQosBox.currentIndex,
            "retain": retainCheck.checked
        }
    }

    function publishCurrentDraft() {
        if (!root.canPublish) {
            return
        }

        const draft = root.draftFromFields()
        root.workbench.publishCurrentSession(
                    draft.topic,
                    draft.payload,
                    draft.format,
                    draft.qos,
                    draft.retain)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            id: resizeHandle
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            Layout.minimumHeight: 1
            property real pressY: 0
            property int pressHeight: root.composerHeight

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                height: 1
                color: root.ui.themePalette.separator
                opacity: 1.0
            }

            MouseArea {
                id: resizeMouse
                anchors.fill: parent
                cursorShape: Qt.SizeVerCursor
                hoverEnabled: true

                onPressed: (mouse) => {
                    resizeHandle.pressY = root.resizeY(mouse)
                    resizeHandle.pressHeight = root.expanded
                            ? root.composerHeight
                            : root.minComposerHeight
                }

                onPositionChanged: (mouse) => {
                    if (!pressed) {
                        return
                    }

                    const delta = root.resizeY(mouse) - resizeHandle.pressY
                    if (!root.expanded && Math.abs(delta) > 2) {
                        root.expanded = true
                    }
                    root.resizeComposerFromDrag(resizeHandle.pressHeight - delta)
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: root.expanded ? root.composerHeight : root.collapsedHeight
            Layout.minimumHeight: root.expanded ? root.minComposerHeight : root.collapsedHeight
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
                            placeholderText: qsTr("home/living-room/light/set")
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
                            Layout.fillWidth: true
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
                            model: root.workbench.payloadFormats
                            currentIndex: 1
                            Layout.fillWidth: true
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
                        wrapMode: TextEdit.Wrap
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
                        enabled: root.canPublish
                        accessibleName: qsTr("Publish message")
                        onClicked: root.publishCurrentDraft()
                    }
                }
            }
        }
    }
}
