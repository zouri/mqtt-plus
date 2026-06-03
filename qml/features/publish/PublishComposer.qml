pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import "../../components"

Item {
    id: root

    required property var appController
    required property var publishStatus
    required property var status
    required property var ui

    property bool expanded: true
    property int composerHeight: 214
    readonly property int collapsedHeight: 36
    readonly property int minComposerHeight: 132
    readonly property int maxComposerHeight: 460

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

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            id: resizeHandle
            Layout.fillWidth: true
            Layout.preferredHeight: 12
            Layout.minimumHeight: 12
            property real pressY: 0
            property int pressHeight: root.composerHeight

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                height: 2
                color: resizeMouse.containsMouse || resizeMouse.pressed
                       ? root.ui.themePalette.selectedBorder
                       : root.ui.themePalette.separator
                opacity: resizeMouse.containsMouse || resizeMouse.pressed ? 1.0 : 0.65
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

            ColumnLayout {
                anchors.fill: parent
                spacing: 9

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    spacing: 8

                    AppTextField {
                        ui: root.ui
                        id: publishTopicField
                        Layout.fillWidth: true
                        placeholderText: qsTr("Topic")
                    }

                    AppComboBox {
                        ui: root.ui
                        id: publishQosBox
                        model: [qsTr("QoS 0"), qsTr("QoS 1")]
                        Layout.preferredWidth: 112
                    }

                    AppComboBox {
                        ui: root.ui
                        id: publishFormatBox
                        model: root.appController.payloadFormats
                        currentIndex: 0
                        Layout.preferredWidth: 126
                    }

                    AppCheckBox {
                        ui: root.ui
                        id: retainCheck
                        text: qsTr("Retain")
                    }

                    AppIconButton {
                        ui: root.ui
                        iconSource: root.ui.materialIcon(root.expanded ? "chevron-down" : "chevron-up")
                        iconSize: 18
                        implicitWidth: 34
                        implicitHeight: 34
                        toolTipText: root.expanded ? qsTr("Collapse publish") : qsTr("Expand publish")
                        onClicked: root.expanded = !root.expanded
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
                        placeholderText: "{\"value\": 23.7}"
                        wrapMode: TextEdit.Wrap
                    }

                    AppIconButton {
                        ui: root.ui
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.rightMargin: 12
                        anchors.bottomMargin: 12
                        iconSource: root.ui.materialIcon("send")
                        iconSize: 15
                        implicitWidth: 38
                        implicitHeight: 38
                        primary: true
                        toolTipText: qsTr("Publish message")
                        enabled: root.status.state === "connected"
                        onClicked: root.appController.publishCurrentSession(
                                       publishTopicField.text,
                                       publishPayloadArea.text,
                                       publishFormatBox.currentIndex,
                                       publishQosBox.currentIndex,
                                       retainCheck.checked)
                    }
                }
            }
        }
    }
}
