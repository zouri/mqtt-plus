pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

Item {
    id: root

    required property var appController
    required property var publishStatus
    required property var status
    required property var ui

    property bool expanded: true
    property int composerHeight: 220
    readonly property int collapsedHeight: 50
    readonly property int minComposerHeight: 180
    readonly property int maxComposerHeight: 420
    property var recentDrafts: []
    property bool repeatEnabled: false
    property int repeatIntervalMs: 5000
    readonly property bool isConnected: root.status.state === "connected"
    readonly property bool hasTopic: publishTopicField.text.trim().length > 0
    readonly property bool canPublish: root.isConnected && root.hasTopic
    readonly property var recentDraftLabels: root.recentDrafts.map(function(draft) {
        return qsTr("%1 · QoS %2 · %3").arg(draft.topic).arg(draft.qos).arg(draft.formatName)
    })
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
            "formatName": publishFormatBox.currentText,
            "qos": publishQosBox.currentIndex,
            "retain": retainCheck.checked
        }
    }

    function rememberDraft(draft) {
        const key = `${draft.topic}\n${draft.payload}\n${draft.format}\n${draft.qos}\n${draft.retain}`
        const nextDrafts = [draft]
        for (let i = 0; i < root.recentDrafts.length && nextDrafts.length < 6; ++i) {
            const current = root.recentDrafts[i]
            const currentKey = `${current.topic}\n${current.payload}\n${current.format}\n${current.qos}\n${current.retain}`
            if (currentKey !== key) {
                nextDrafts.push(current)
            }
        }
        root.recentDrafts = nextDrafts
    }

    function applyDraft(draft) {
        if (!draft) {
            return
        }

        publishTopicField.text = draft.topic || ""
        publishPayloadArea.text = draft.payload || ""
        publishFormatBox.currentIndex = Math.max(0, Math.min(publishFormatBox.count - 1, Number(draft.format || 0)))
        publishQosBox.currentIndex = Math.max(0, Math.min(publishQosBox.count - 1, Number(draft.qos || 0)))
        retainCheck.checked = Boolean(draft.retain)
        root.expanded = true
    }

    function publishCurrentDraft() {
        if (!root.canPublish) {
            return
        }

        const draft = root.draftFromFields()
        root.rememberDraft(draft)
        root.appController.publishCurrentSession(
                    draft.topic,
                    draft.payload,
                    draft.format,
                    draft.qos,
                    draft.retain)
    }

    Timer {
        interval: root.repeatIntervalMs
        running: root.repeatEnabled && root.canPublish
        repeat: true
        onTriggered: root.publishCurrentDraft()
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
                            model: root.appController.payloadFormats
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

                RowLayout {
                    visible: false
                    Layout.fillWidth: true
                    Layout.preferredHeight: 0
                    spacing: 8

                    AppComboBox {
                        ui: root.ui
                        visible: root.recentDrafts.length > 0
                        enabled: root.recentDrafts.length > 0
                        Layout.preferredWidth: visible ? 220 : 0
                        model: [qsTr("Recent publishes")].concat(root.recentDraftLabels)
                        currentIndex: 0
                        onActivated: (index) => {
                            if (index > 0) {
                                root.applyDraft(root.recentDrafts[index - 1])
                                currentIndex = 0
                            }
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    AppCheckBox {
                        ui: root.ui
                        text: qsTr("Repeat")
                        checked: root.repeatEnabled
                        enabled: root.canPublish
                        onToggled: root.repeatEnabled = checked
                    }

                    AppComboBox {
                        ui: root.ui
                        enabled: root.repeatEnabled
                        Layout.preferredWidth: 82
                        model: [qsTr("1s"), qsTr("5s"), qsTr("10s")]
                        currentIndex: 1
                        onCurrentIndexChanged: {
                            root.repeatIntervalMs = currentIndex === 0 ? 1000 : (currentIndex === 1 ? 5000 : 10000)
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
