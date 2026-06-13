pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

Item {
    id: root

    required property var appController
    required property var streamModel
    required property var loadOlderRows
    required property var clearRows
    required property var session
    required property var ui
    required property string fontFamily
    required property string title

    property bool showOutputControls: false
    property bool loadingOlderEvents: false
    property bool reachedHistoryStart: false

    signal publishDraftRequested(string topic, string payload, int format)

    Layout.fillWidth: true
    Layout.fillHeight: true

    function resetStreamPosition() {
        root.loadingOlderEvents = false
        root.reachedHistoryStart = false

        if (eventList) {
            eventList.unreadCount = 0
        }

        Qt.callLater(function() {
            if (eventList) {
                eventList.scrollToBottom()
            }
        })
    }

    function noteStreamRowAppended(row) {
        const shouldStickToBottom = !eventList || eventList.shouldFollowOutput

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

    function loadOlderEvents() {
        if (root.loadingOlderEvents || root.reachedHistoryStart || !eventList) {
            return
        }

        root.loadingOlderEvents = true
        const previousContentHeight = eventList.contentHeight
        const previousContentY = eventList.contentY
        const insertedRows = root.loadOlderRows()
        if (insertedRows === 0) {
            root.reachedHistoryStart = true
            root.loadingOlderEvents = false
            return
        }

        Qt.callLater(function() {
            eventList.contentY = previousContentY + eventList.contentHeight - previousContentHeight
            root.loadingOlderEvents = false
        })
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: root.ui.themePalette.windowBg

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: root.ui.themePalette.separator
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 8

                Label {
                    text: root.title
                    color: root.ui.textStrong
                    font.pixelSize: 22
                    font.bold: true
                }

                AppBadge {
                    ui: root.ui
                    label: `${eventList.count}`
                    badgeRadius: 11
                    horizontalPadding: 7
                    verticalPadding: 3
                    badgeBg: root.ui.themePalette.selectedBg
                    badgeBorder: "transparent"
                    badgeText: root.ui.themePalette.infoText
                }

                Item {
                    Layout.fillWidth: true
                }

                AppIconButton {
                    ui: root.ui
                    visible: root.showOutputControls
                    iconSource: root.ui.materialIcon(root.session.outputPaused ? "play" : "pause")
                    iconSize: 14
                    implicitWidth: 32
                    implicitHeight: 32
                    cornerRadius: 16
                    restBg: root.ui.themePalette.windowBg
                    outlineColor: root.ui.themePalette.innerPanelBorder
                    accessibleName: root.session.outputPaused ? qsTr("Resume output") : qsTr("Pause output")
                    onClicked: root.appController.setCurrentOutputPaused(!root.session.outputPaused)
                }

                AppIconButton {
                    ui: root.ui
                    iconSource: root.ui.materialIcon("delete")
                    iconSize: 14
                    implicitWidth: 32
                    implicitHeight: 32
                    cornerRadius: 16
                    restBg: root.ui.themePalette.windowBg
                    outlineColor: root.ui.themePalette.innerPanelBorder
                    accessibleName: qsTr("Clear history")
                    onClicked: root.clearRows()
                }
            }
        }

        Label {
            visible: root.showOutputControls && root.session.outputPaused
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.topMargin: 8
            text: qsTr("Output paused: incoming MQTT messages are still stored in history.")
            color: root.ui.themePalette.warningText
            font.pixelSize: 12
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: eventList
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                clip: true
                spacing: 4
                model: root.streamModel
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
                        root.loadOlderEvents()
                    }
                }

                delegate: Rectangle {
                    id: eventDelegate
                    required property string kind
                    required property string timestamp
                    required property string title
                    required property string topic
                    required property string payload
                    required property string payloadFormat
                    required property int payloadSize
                    required property string testPayload
                    required property int testFormat
                    readonly property string payloadSizeLabel: qsTr("%1 B").arg(eventDelegate.payloadSize)
                    width: ListView.view.width
                    radius: root.ui.innerRadius
                    color: eventDelegate.kind === "divider"
                           ? "transparent"
                           : root.ui.themePalette.itemBg
                    border.color: eventDelegate.kind === "event"
                                  ? root.ui.themePalette.eventBorder
                                  : (eventDelegate.kind === "divider"
                                     ? "transparent"
                                     : root.ui.themePalette.innerPanelBorder)
                    implicitHeight: eventDelegate.kind === "divider"
                                    ? dividerRow.implicitHeight + 6
                                    : rowBody.implicitHeight + 14

                    RowLayout {
                        id: dividerRow
                        visible: eventDelegate.kind === "divider"
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 0

                        Label {
                            text: eventDelegate.title
                            color: root.ui.textMuted
                            font.pixelSize: 11
                            font.bold: true
                            padding: 6

                            background: Rectangle {
                                radius: 9
                                color: root.ui.themePalette.dividerLabelBg
                                border.color: root.ui.themePalette.dividerLine
                            }
                        }
                    }

                    Column {
                        id: rowBody
                        visible: eventDelegate.kind !== "divider"
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 10
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 5

                        RowLayout {
                            width: parent.width
                            spacing: 6

                            Label {
                                text: eventDelegate.timestamp
                                color: root.ui.themePalette.timestampText
                                font.pixelSize: 11
                            }

                            Label {
                                Layout.fillWidth: true
                                text: eventDelegate.title
                                color: eventDelegate.kind === "event"
                                       ? root.ui.themePalette.eventTitle
                                       : root.ui.themePalette.messageTitle
                                font.pixelSize: 12
                                font.bold: true
                                elide: Label.ElideRight
                            }

                            AppBadge {
                                ui: root.ui
                                visible: eventDelegate.kind === "message"
                                label: eventDelegate.payloadSizeLabel
                                badgeRadius: 7
                                badgeBorder: root.ui.themePalette.eventBorder
                                horizontalPadding: 6
                                verticalPadding: 3
                                strong: false
                            }

                            AppBadge {
                                ui: root.ui
                                visible: eventDelegate.payloadFormat.length > 0
                                label: eventDelegate.payloadFormat
                                badgeRadius: 7
                                badgeBorder: root.ui.themePalette.eventBorder
                                horizontalPadding: 6
                                verticalPadding: 3
                            }

                            AppIconButton {
                                ui: root.ui
                                visible: eventDelegate.kind === "message"
                                symbol: "T"
                                symbolSize: 11
                                implicitWidth: 26
                                implicitHeight: 26
                                cornerRadius: 6
                                restBg: "transparent"
                                outlineColor: "transparent"
                                accessibleName: qsTr("Copy topic")
                                onClicked: root.appController.copyTextToClipboard(eventDelegate.topic)
                            }

                            AppIconButton {
                                ui: root.ui
                                visible: eventDelegate.kind === "message"
                                symbol: "P"
                                symbolSize: 11
                                implicitWidth: 26
                                implicitHeight: 26
                                cornerRadius: 6
                                restBg: "transparent"
                                outlineColor: "transparent"
                                accessibleName: qsTr("Copy payload")
                                onClicked: root.appController.copyTextToClipboard(
                                               eventDelegate.testPayload.length > 0
                                               ? eventDelegate.testPayload
                                               : eventDelegate.payload)
                            }

                            AppIconButton {
                                ui: root.ui
                                visible: eventDelegate.kind === "message"
                                iconSource: root.ui.materialIcon("send")
                                implicitWidth: 26
                                implicitHeight: 26
                                iconSize: 12
                                cornerRadius: 6
                                restBg: "transparent"
                                outlineColor: "transparent"
                                accessibleName: qsTr("Use this message in publisher")
                                onClicked: root.publishDraftRequested(
                                               eventDelegate.topic,
                                               eventDelegate.testPayload.length > 0
                                               ? eventDelegate.testPayload
                                               : eventDelegate.payload,
                                               eventDelegate.testFormat)
                            }
                        }

                        Text {
                            text: eventDelegate.payload
                            width: parent.width
                            color: root.ui.textStrong
                            font.family: eventDelegate.kind === "message" ? "Menlo" : root.fontFamily
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
                       ? root.ui.themePalette.actionHoverBg
                       : root.ui.themePalette.followBg
                border.color: root.ui.themePalette.followBorder
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
                        color: root.ui.themePalette.followText
                        font.pixelSize: 15
                        font.bold: true
                    }

                    Label {
                        visible: eventList.unreadCount > 0
                        text: `+${eventList.unreadCount}`
                        color: root.ui.themePalette.followBadgeText
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
    }
}
