pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

Item {
    id: root

    required property var appController
    required property var session
    required property var ui
    required property string fontFamily

    property bool loadingOlderEvents: false
    property bool reachedHistoryStart: false

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

    function noteStreamRowAppended() {
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
        const insertedRows = root.appController.loadOlderCurrentSessionEvents()
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
        spacing: 12

        AppSectionHeader {
            ui: root.ui
            title: qsTr("Event Stream")
            titleSize: 17
            meta: `${eventList.count}`

            AppIconButton {
                ui: root.ui
                iconSource: root.ui.materialIcon(root.session.outputPaused ? "play" : "pause")
                iconSize: 14
                implicitWidth: 34
                implicitHeight: 34
                toolTipText: root.session.outputPaused ? qsTr("Resume output") : qsTr("Pause output")
                onClicked: root.appController.setCurrentOutputPaused(!root.session.outputPaused)
            }

            AppIconButton {
                ui: root.ui
                iconSource: root.ui.materialIcon("delete")
                iconSize: 14
                implicitWidth: 34
                implicitHeight: 34
                toolTipText: qsTr("Clear event stream")
                onClicked: root.appController.clearCurrentMessages()
            }
        }

        AppDivider {
            ui: root.ui
        }

        Label {
            visible: root.session.outputPaused
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
                clip: true
                spacing: 6
                model: root.appController.events
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
                    required property string payload
                    required property string payloadFormat
                    required property int payloadSize
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
                        anchors.margins: 12
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6

                        RowLayout {
                            width: parent.width
                            spacing: 8

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
