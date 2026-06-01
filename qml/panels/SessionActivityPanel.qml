pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../components"

AppPanel {
    id: control

    required property var appController
    required property var session
    required property var status
    required property var publishStatus
    required property string fontFamily

    property bool loadingOlderEvents: false
    property bool reachedHistoryStart: false
    property bool publishComposerExpanded: true
    property int publishComposerHeight: 214
    readonly property int publishComposerCollapsedHeight: 36
    readonly property int publishComposerMinHeight: 132
    readonly property int publishComposerMaxHeight: 460
    readonly property int maxEventRows: 1200

    showTopBorder: false
    Layout.fillWidth: true
    Layout.fillHeight: true

    function reloadStream(rows) {
        eventStreamListModel.clear()
        control.loadingOlderEvents = false
        control.reachedHistoryStart = false
        const start = Math.max(0, rows.length - control.maxEventRows)
        for (let i = start; i < rows.length; ++i) {
            eventStreamListModel.append(rows[i])
        }

        if (eventList) {
            eventList.unreadCount = 0
        }

        Qt.callLater(function() {
            if (eventList) {
                eventList.scrollToBottom()
            }
        })
    }

    function appendStreamRow(row) {
        const shouldStickToBottom = !eventList || eventList.shouldFollowOutput
        eventStreamListModel.append(row)
        control.trimStreamRows()

        if (shouldStickToBottom && eventList) {
            Qt.callLater(function() {
                if (eventList) {
                    eventList.scrollToBottom()
                }
            })
        } else if (eventList) {
            eventList.unreadCount += 1
        }
    }

    function trimStreamRows() {
        const overflow = eventStreamListModel.count - control.maxEventRows
        if (overflow > 0) {
            eventStreamListModel.remove(0, overflow)
        }
    }

    function loadOlderEvents() {
        if (control.loadingOlderEvents || control.reachedHistoryStart || !eventList) {
            return
        }

        control.loadingOlderEvents = true
        const previousContentHeight = eventList.contentHeight
        const previousContentY = eventList.contentY
        const rows = control.appController.loadOlderCurrentEventRows()
        if (rows.length === 0) {
            control.reachedHistoryStart = true
            control.loadingOlderEvents = false
            return
        }

        for (let i = rows.length - 1; i >= 0; --i) {
            eventStreamListModel.insert(0, rows[i])
        }

        Qt.callLater(function() {
            eventList.contentY = previousContentY + eventList.contentHeight - previousContentHeight
            control.loadingOlderEvents = false
        })
    }

    function resizePublishComposer(height) {
        control.publishComposerHeight = Math.max(
                    control.publishComposerMinHeight,
                    Math.min(control.publishComposerMaxHeight, Math.round(height)))
    }

    function resizePublishComposerFromDrag(height) {
        if (height <= control.publishComposerMinHeight) {
            control.publishComposerExpanded = false
            return
        }

        control.publishComposerExpanded = true
        control.resizePublishComposer(height)
    }

    function publishResizeY(mouse) {
        return publishResizeMouse.mapToItem(control, mouse.x, mouse.y).y
    }

    ListModel {
        id: eventStreamListModel
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        AppSectionHeader {
            ui: control.ui
            title: "Event Stream"
            titleSize: 17
            meta: `${eventList.count}`

            AppIconButton {
                ui: control.ui
                iconSource: control.ui.materialIcon(control.session.outputPaused ? "play" : "pause")
                iconSize: 14
                implicitWidth: 34
                implicitHeight: 34
                toolTipText: control.session.outputPaused ? "Resume output" : "Pause output"
                onClicked: control.appController.setCurrentOutputPaused(!control.session.outputPaused)
            }

            AppIconButton {
                ui: control.ui
                iconSource: control.ui.materialIcon("delete")
                iconSize: 14
                implicitWidth: 34
                implicitHeight: 34
                toolTipText: "Clear event stream"
                onClicked: control.appController.clearCurrentMessages()
            }
        }

        AppDivider {
            ui: control.ui
        }

        Label {
            visible: control.session.outputPaused
            text: "Output paused: incoming MQTT messages are still stored in history."
            color: control.ui.themePalette.warningText
            font.pixelSize: 12
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: eventList
                anchors.fill: parent
                clip: true
                spacing: 6
                model: eventStreamListModel
                property bool shouldFollowOutput: true
                property bool programmaticScroll: false
                property int unreadCount: 0

                function scrollToBottom() {
                    programmaticScroll = true
                    unreadCount = 0
                    forceLayout()
                    if (count > 0) {
                        positionViewAtEnd()
                    } else {
                        contentY = originY
                    }
                    Qt.callLater(function() {
                        eventList.programmaticScroll = false
                        eventList.shouldFollowOutput = true
                    })
                }

                function refreshFollowState() {
                    if (programmaticScroll) {
                        return
                    }

                    const maxContentY = Math.max(originY, contentHeight - height)
                    const distanceFromBottom = Math.max(0, maxContentY - contentY)
                    shouldFollowOutput = count === 0 || distanceFromBottom <= 24
                    if (shouldFollowOutput) {
                        unreadCount = 0
                    }
                }

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                onMovementStarted: refreshFollowState()
                onMovementEnded: refreshFollowState()
                onFlickStarted: refreshFollowState()
                onFlickEnded: refreshFollowState()
                onContentYChanged: {
                    refreshFollowState()
                    if (contentY <= originY + 48) {
                        control.loadOlderEvents()
                    }
                }

                delegate: Rectangle {
                    id: eventDelegate
                    required property string kind
                    required property string timestamp
                    required property string title
                    required property string payload
                    required property string payloadFormat
                    required property int payloadSize
                    readonly property string payloadSizeLabel: qsTr("%1 B").arg(eventDelegate.payloadSize)
                    width: ListView.view.width
                    radius: control.ui.innerRadius
                    color: eventDelegate.kind === "divider"
                           ? "transparent"
                           : control.ui.themePalette.itemBg
                    border.color: eventDelegate.kind === "event"
                                  ? control.ui.themePalette.eventBorder
                                  : (eventDelegate.kind === "divider"
                                     ? "transparent"
                                     : control.ui.themePalette.innerPanelBorder)
                    implicitHeight: eventDelegate.kind === "divider"
                                    ? dividerRow.implicitHeight + 8
                                    : rowBody.implicitHeight + 18

                    RowLayout {
                        id: dividerRow
                        visible: eventDelegate.kind === "divider"
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 0

                        Label {
                            text: eventDelegate.title
                            color: control.ui.textMuted
                            font.pixelSize: 11
                            font.bold: true
                            padding: 6

                            background: Rectangle {
                                radius: 9
                                color: control.ui.themePalette.dividerLabelBg
                                border.color: control.ui.themePalette.dividerLine
                            }
                        }
                    }

                    Column {
                        id: rowBody
                        visible: eventDelegate.kind !== "divider"
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 12
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6

                        RowLayout {
                            width: parent.width
                            spacing: 8

                            Label {
                                text: eventDelegate.timestamp
                                color: control.ui.themePalette.timestampText
                                font.pixelSize: 11
                            }

                            Label {
                                Layout.fillWidth: true
                                text: eventDelegate.title
                                color: eventDelegate.kind === "event"
                                       ? control.ui.themePalette.eventTitle
                                       : control.ui.themePalette.messageTitle
                                font.pixelSize: 12
                                font.bold: true
                                elide: Label.ElideRight
                            }

                            AppBadge {
                                ui: control.ui
                                visible: eventDelegate.kind === "message"
                                label: eventDelegate.payloadSizeLabel
                                badgeRadius: 7
                                badgeBorder: control.ui.themePalette.eventBorder
                                horizontalPadding: 6
                                verticalPadding: 3
                                strong: false
                            }

                            AppBadge {
                                ui: control.ui
                                visible: eventDelegate.payloadFormat.length > 0
                                label: eventDelegate.payloadFormat
                                badgeRadius: 7
                                badgeBorder: control.ui.themePalette.eventBorder
                                horizontalPadding: 6
                                verticalPadding: 3
                            }
                        }

                        Text {
                            text: eventDelegate.payload
                            width: parent.width
                            color: control.ui.textStrong
                            font.family: eventDelegate.kind === "message" ? "Menlo" : control.fontFamily
                            font.pixelSize: 13
                            textFormat: Text.PlainText
                            wrapMode: Text.WrapAnywhere
                        }
                    }
                }
            }

            Rectangle {
                id: followButton
                visible: !eventList.shouldFollowOutput
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: visible ? 14 : 8
                radius: 19
                height: 38
                width: followButtonRow.implicitWidth + 26
                color: followMouse.containsMouse || activeFocus
                       ? control.ui.themePalette.actionHoverBg
                       : control.ui.themePalette.followBg
                border.color: control.ui.themePalette.followBorder
                opacity: visible ? 0.97 : 0
                z: 2
                scale: visible ? 1.0 : 0.96
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: eventList.unreadCount > 0
                                 ? qsTr("Scroll to latest, %1 unread").arg(eventList.unreadCount)
                                 : qsTr("Scroll to latest")

                onVisibleChanged: {
                    if (!visible && activeFocus) {
                        eventList.forceActiveFocus()
                    }
                }

                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Return
                            || event.key === Qt.Key_Enter
                            || event.key === Qt.Key_Space) {
                        eventList.scrollToBottom()
                        event.accepted = true
                    }
                }

                Behavior on opacity {
                    NumberAnimation {
                        duration: 140
                        easing.type: Easing.OutCubic
                    }
                }

                Behavior on scale {
                    NumberAnimation {
                        duration: 160
                        easing.type: Easing.OutBack
                    }
                }

                Behavior on anchors.bottomMargin {
                    NumberAnimation {
                        duration: 160
                        easing.type: Easing.OutCubic
                    }
                }

                Row {
                    id: followButtonRow
                    anchors.centerIn: parent
                    spacing: 7

                    Label {
                        text: "↓"
                        color: control.ui.themePalette.followText
                        font.pixelSize: 15
                        font.bold: true
                    }

                    Label {
                        visible: eventList.unreadCount > 0
                        text: `+${eventList.unreadCount}`
                        color: control.ui.themePalette.followBadgeText
                        font.pixelSize: 11
                        font.bold: true
                    }
                }

                MouseArea {
                    id: followMouse
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: {
                        followButton.forceActiveFocus()
                        eventList.scrollToBottom()
                    }
                }
            }
        }

        Item {
            id: publishResizeHandle
            Layout.fillWidth: true
            Layout.preferredHeight: 12
            Layout.minimumHeight: 12
            property real pressY: 0
            property int pressHeight: control.publishComposerHeight

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                height: 2
                color: publishResizeMouse.containsMouse || publishResizeMouse.pressed
                       ? control.ui.themePalette.selectedBorder
                       : control.ui.themePalette.separator
                opacity: publishResizeMouse.containsMouse || publishResizeMouse.pressed ? 1.0 : 0.65
            }

            MouseArea {
                id: publishResizeMouse
                anchors.fill: parent
                cursorShape: Qt.SizeVerCursor
                hoverEnabled: true

                onPressed: (mouse) => {
                    publishResizeHandle.pressY = control.publishResizeY(mouse)
                    publishResizeHandle.pressHeight = control.publishComposerExpanded
                            ? control.publishComposerHeight
                            : control.publishComposerMinHeight
                }

                onPositionChanged: (mouse) => {
                    if (!pressed) {
                        return
                    }

                    const delta = control.publishResizeY(mouse) - publishResizeHandle.pressY
                    if (!control.publishComposerExpanded && Math.abs(delta) > 2) {
                        control.publishComposerExpanded = true
                    }
                    control.resizePublishComposerFromDrag(publishResizeHandle.pressHeight - delta)
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: control.publishComposerExpanded
                                    ? control.publishComposerHeight
                                    : control.publishComposerCollapsedHeight
            Layout.minimumHeight: control.publishComposerExpanded
                                  ? control.publishComposerMinHeight
                                  : control.publishComposerCollapsedHeight
            clip: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 9

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    spacing: 8

                    AppTextField {
                        ui: control.ui
                        id: publishTopicField
                        Layout.fillWidth: true
                        placeholderText: "Topic"
                    }

                    AppComboBox {
                        ui: control.ui
                        id: publishQosBox
                        model: ["QoS 0", "QoS 1"]
                        Layout.preferredWidth: 112
                    }

                    AppComboBox {
                        ui: control.ui
                        id: publishFormatBox
                        model: control.appController.payloadFormats
                        currentIndex: 0
                        Layout.preferredWidth: 126
                    }

                    AppCheckBox {
                        ui: control.ui
                        id: retainCheck
                        text: "Retain"
                    }

                    AppIconButton {
                        ui: control.ui
                        iconSource: control.ui.materialIcon(control.publishComposerExpanded ? "chevron-down" : "chevron-up")
                        iconSize: 18
                        implicitWidth: 34
                        implicitHeight: 34
                        toolTipText: control.publishComposerExpanded ? "Collapse publish" : "Expand publish"
                        onClicked: control.publishComposerExpanded = !control.publishComposerExpanded
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: control.publishComposerExpanded

                    AppTextArea {
                        ui: control.ui
                        id: publishPayloadArea
                        anchors.fill: parent
                        placeholderText: "{\"value\": 23.7}"
                        wrapMode: TextEdit.Wrap
                    }

                    AppIconButton {
                        ui: control.ui
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.rightMargin: 12
                        anchors.bottomMargin: 12
                        iconSource: control.ui.materialIcon("send")
                        iconSize: 15
                        implicitWidth: 38
                        implicitHeight: 38
                        primary: true
                        toolTipText: "Publish message"
                        enabled: control.status.state === "connected"
                        onClicked: control.appController.publishCurrentSession(
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
