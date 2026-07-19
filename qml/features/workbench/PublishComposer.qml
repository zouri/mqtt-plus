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
    property int composerHeight: 168
    readonly property int collapsedHeight: 40
    readonly property int minComposerHeight: 150
    readonly property int maxComposerHeight: 300
    readonly property color surfaceBg: root.ui.themePalette.panelBg
    readonly property string publishFeedback: root.publishStatus.state && root.publishStatus.state !== "idle"
                                              ? (root.publishStatus.reason && root.publishStatus.reason.length > 0
                                                 ? root.publishStatus.reason
                                                 : qsTr("Publish status: %1").arg(root.ui.statusLabel(root.publishStatus.state)))
                                              : ""
    readonly property string publishDisabledReason: root.status.state !== "connected"
                                                    ? qsTr("Connect before publishing")
                                                    : (root.publisher.topic.trim().length === 0
                                                       ? qsTr("Enter a topic before publishing")
                                                       : "")
    readonly property color publishFeedbackColor: root.publishStatus.state === "failed"
                                                  ? root.ui.themePalette.errorText
                                                  : (root.publishStatus.state === "sent"
                                                     || root.publishStatus.state === "acknowledged"
                                                     || root.publishStatus.state === "completed"
                                                     ? root.ui.themePalette.successText
                                                     : root.ui.textMuted)
    property bool publishPulseActive: false

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
    }

    function publishDraft() {
        if (root.publisher.canPublish) {
            root.publisher.publishDraft()
        }
    }

    onPublishStatusChanged: {
        const state = String(root.publishStatus.state || "");
        if (state === "failed" || state === "sent" || state === "acknowledged" || state === "completed") {
            root.publishPulseActive = true;
            publishPulseTimer.restart();
        }
    }

    Timer {
        id: publishPulseTimer
        interval: 300
        repeat: false
        onTriggered: root.publishPulseActive = false
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
                color: root.surfaceBg
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 7

                Button {
                    id: composerHeader

                    Layout.fillWidth: true
                    Layout.preferredHeight: root.collapsedHeight
                    leftPadding: 14
                    rightPadding: 12
                    Accessible.name: root.expanded ? qsTr("Collapse publish composer") : qsTr("Expand publish composer")

                    contentItem: RowLayout {
                        spacing: 7

                        Label {
                            text: qsTr("Publish Message")
                            color: root.ui.textStrong
                            font.pixelSize: 12
                            font.bold: true
                        }

                        Label {
                            visible: root.publishFeedback.length > 0
                            text: root.publishFeedback
                            color: root.publishFeedbackColor
                            font.pixelSize: 11
                            elide: Label.ElideRight
                            Layout.fillWidth: true
                        }

                        Item {
                            visible: root.publishFeedback.length === 0
                            Layout.fillWidth: true
                        }

                        Label {
                            text: root.expanded
                                  ? "⌄ " + qsTr("Collapse")
                                  : "› " + qsTr("Expand")
                            color: root.ui.textMuted
                            font.pixelSize: 11
                        }
                    }

                    background: Rectangle {
                        color: composerHeader.hovered ? root.ui.themePalette.rowHover : "transparent"
                    }

                    onClicked: root.expanded = !root.expanded
                }

                RowLayout {
                    visible: root.expanded
                    Layout.fillWidth: true
                    Layout.leftMargin: 14
                    Layout.rightMargin: 12
                    Layout.preferredHeight: visible ? 34 : 0
                    spacing: 6

                    ColumnLayout {
                        Layout.preferredWidth: 340
                        Layout.minimumWidth: 240
                        Layout.maximumWidth: 380
                        spacing: 0

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
                        Layout.preferredWidth: 88
                        spacing: 0

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
                        Layout.preferredWidth: 104
                        spacing: 0

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
                        Layout.preferredWidth: 68
                        spacing: 0

                        AppCheckBox {
                            ui: root.ui
                            id: retainCheck
                            text: qsTr("Retain")
                            checked: root.publisher.retain
                            onToggled: root.publisher.retain = checked
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.leftMargin: 14
                    Layout.rightMargin: 12
                    Layout.bottomMargin: 12
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
                        submitOnCtrlEnter: true
                        onTextChanged: {
                            if (root.publisher.payload !== text) {
                                root.publisher.payload = text
                            }
                        }
                        onSubmitRequested: root.publishDraft()
                    }

                    AppIconButton {
                        id: publishHistoryButton
                        ui: root.ui
                        anchors.right: publishButton.left
                        anchors.bottom: parent.bottom
                        anchors.rightMargin: 6
                        anchors.bottomMargin: 8
                        implicitWidth: 30
                        implicitHeight: 30
                        cornerRadius: 8
                        iconSource: root.ui.materialIcon("logs")
                        iconSize: 15
                        restBg: root.ui.themePalette.innerPanelBg
                        outlineColor: root.ui.themePalette.fieldBorder
                        accessibleName: qsTr("Recent publishes")
                        toolTipText: root.publisher.recentPublishes.length > 0
                                     ? qsTr("Recent publishes")
                                     : qsTr("No recent publishes")
                        onClicked: publishHistoryPopup.open()
                    }

                    AppIconButton {
                        id: publishButton
                        ui: root.ui
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.rightMargin: 8
                        anchors.bottomMargin: 8
                        implicitWidth: 30
                        implicitHeight: 30
                        cornerRadius: 8
                        iconSource: root.ui.materialIcon("send")
                        iconSize: 16
                        primary: true
                        forceActive: root.publishPulseActive
                        danger: root.publishPulseActive && root.publishStatus.state === "failed"
                        enabled: root.publisher.canPublish
                        accessibleName: qsTr("Publish message")
                        toolTipText: root.publisher.canPublish
                                     ? qsTr("Publish message (%1+Enter)").arg(Qt.platform.os === "osx" ? qsTr("Command") : qsTr("Ctrl"))
                                     : root.publishDisabledReason
                        onClicked: root.publishDraft()
                    }
                }
            }
        }
    }

    Popup {
        id: publishHistoryPopup

        width: Math.min(420, root.width - 24)
        height: Math.min(320, Math.max(96, historyContent.implicitHeight + 20))
        x: Math.max(12, root.width - width - 12)
        y: Math.max(12, root.height - height - 44)
        padding: 10
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: 8
            color: root.ui.themePalette.dialogBg
            border.color: root.ui.themePalette.dialogBorder
        }

        contentItem: ColumnLayout {
            id: historyContent
            spacing: 8

            RowLayout {
                Layout.fillWidth: true

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Recent publishes")
                    color: root.ui.textStrong
                    font.pixelSize: 12
                    font.bold: true
                }

                AppIconButton {
                    ui: root.ui
                    visible: root.publisher.recentPublishes.length > 0
                    implicitWidth: 26
                    implicitHeight: 26
                    cornerRadius: 6
                    iconSource: root.ui.materialIcon("delete")
                    iconSize: 13
                    restBg: "transparent"
                    outlineColor: "transparent"
                    accessibleName: qsTr("Clear recent publishes")
                    toolTipText: accessibleName
                    onClicked: root.publisher.clearRecentPublishes()
                }
            }

            Label {
                visible: root.publisher.recentPublishes.length === 0
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: qsTr("Published messages will appear here for quick reuse.")
                color: root.ui.textMuted
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.Wrap
            }

            ListView {
                id: recentPublishList
                visible: root.publisher.recentPublishes.length > 0
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, 250)
                clip: true
                spacing: 3
                model: root.publisher.recentPublishes
                reuseItems: true

                delegate: Rectangle {
                    id: recentPublishDelegate
                    required property int index
                    required property var modelData
                    width: ListView.view.width
                    height: 48
                    radius: 6
                    color: recentPublishHover.hovered
                           ? root.ui.themePalette.rowHover
                           : root.ui.themePalette.innerPanelBg

                    HoverHandler {
                        id: recentPublishHover
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 9
                        anchors.rightMargin: 6
                        spacing: 8

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 2

                            TapHandler {
                                gesturePolicy: TapHandler.ReleaseWithinBounds
                                onTapped: {
                                    root.publisher.useRecentPublish(recentPublishDelegate.index);
                                    publishHistoryPopup.close();
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: String(recentPublishDelegate.modelData.topic || "")
                                color: root.ui.textStrong
                                font.pixelSize: 11
                                font.bold: true
                                elide: Label.ElideMiddle
                            }

                            Label {
                                Layout.fillWidth: true
                                text: String(recentPublishDelegate.modelData.payload || qsTr("Empty payload"))
                                color: root.ui.textMuted
                                font.pixelSize: 10
                                elide: Label.ElideRight
                            }
                        }

                        Label {
                            text: qsTr("QoS %1").arg(Number(recentPublishDelegate.modelData.qos || 0))
                            color: root.ui.themePalette.textSubtle
                            font.pixelSize: 10
                        }

                        AppIconButton {
                            ui: root.ui
                            implicitWidth: 26
                            implicitHeight: 26
                            cornerRadius: 6
                            iconSource: root.ui.materialIcon("send")
                            iconSize: 13
                            primary: true
                            enabled: root.status.state === "connected"
                            accessibleName: qsTr("Publish again")
                            toolTipText: enabled ? accessibleName : qsTr("Connect before publishing")
                            onClicked: {
                                root.publisher.useRecentPublish(recentPublishDelegate.index);
                                root.publishDraft();
                                publishHistoryPopup.close();
                            }
                        }
                    }
                }
            }
        }
    }
}
