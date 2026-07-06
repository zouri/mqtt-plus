pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

Item {
    id: root

    required property var viewModel
    required property var publisher
    required property QtObject streamModel
    required property var session
    required property var ui
    required property string fontFamily
    required property string title

    property bool showOutputControls: false
    property bool loadingOlderEvents: false
    property bool reachedHistoryStart: false
    property string followMode: "smart"

    signal publishDraftRevealRequested()

    Layout.fillWidth: true
    Layout.fillHeight: true

    function resetStreamPosition() {
        root.loadingOlderEvents = false
        root.reachedHistoryStart = false

        if (eventList) {
            eventList.unreadCount = 0
            eventList.bottomAnchorActive = true
            eventList.shouldFollowOutput = true
        }

        root.requestFollowScroll()
    }

    function noteStreamRowsAppended(count) {
        if (!eventList) {
            return
        }

        if (root.shouldFollowNewRows()) {
            eventList.bottomAnchorActive = true
            eventList.shouldFollowOutput = true
            root.requestFollowScroll()
            return
        }

        eventList.bottomAnchorActive = false
        eventList.shouldFollowOutput = false
        eventList.unreadCount += Math.max(1, count)
    }

    function requestFollowScroll() {
        if (!eventList) {
            return
        }

        eventList.bottomAnchorActive = true
        if (eventList.followScrollQueued) {
            return
        }

        eventList.followScrollQueued = true
        Qt.callLater(function() {
            eventList.followScrollQueued = false
            if (eventList.bottomAnchorActive) {
                eventList.scrollToBottom()
            }
        })
    }

    function loadOlderEvents() {
        if (root.loadingOlderEvents || root.reachedHistoryStart || !eventList) {
            return
        }

        root.loadingOlderEvents = true
        eventList.bottomAnchorActive = false
        const previousContentHeight = eventList.contentHeight
        const previousContentY = eventList.contentY
        const insertedRows = root.viewModel.loadOlderMessages()
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

    function followModeLabel(mode) {
        if (mode === "always") {
            return qsTr("Always")
        }
        if (mode === "manual") {
            return qsTr("Manual")
        }
        return qsTr("Smart")
    }

    function followModeSymbol(mode) {
        if (mode === "always") {
            return "A"
        }
        if (mode === "manual") {
            return "M"
        }
        return "S"
    }

    function followModeToolTip() {
        return qsTr("Follow mode: %1").arg(root.followModeLabel(root.followMode))
    }

    function setFollowMode(mode) {
        if (mode !== "smart" && mode !== "always" && mode !== "manual") {
            return
        }
        if (root.followMode === mode) {
            return
        }

        root.followMode = mode
        if (!eventList) {
            return
        }

        if (mode === "manual") {
            eventList.bottomAnchorActive = false
            return
        }

        if (mode === "always") {
            root.requestFollowScroll()
            return
        }

        eventList.refreshFollowState()
    }

    function shouldFollowNewRows() {
        if (root.followMode === "always") {
            return true
        }
        if (root.followMode === "manual") {
            return false
        }
        return eventList.shouldFollowOutput
    }

    ListModel {
        id: followModeActions

        ListElement { actionId: "smart" }
        ListElement { actionId: "always" }
        ListElement { actionId: "manual" }
    }

    AppPlatformMenu {
        id: followModeMenu
        model: followModeActions
        actionText: actionId => root.followModeLabel(actionId)

        onTriggered: actionId => root.setFollowMode(actionId)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
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
                    symbol: root.followModeSymbol(root.followMode)
                    symbolSize: 12
                    implicitWidth: 32
                    implicitHeight: 32
                    cornerRadius: 16
                    restBg: root.ui.themePalette.windowBg
                    outlineColor: root.ui.themePalette.innerPanelBorder
                    accessibleName: root.followModeToolTip()
                    toolTipText: root.followModeToolTip()
                    onClicked: followModeMenu.open()
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
                    onClicked: root.viewModel.toggleCurrentOutputPaused(root.session.outputPaused)
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
                    onClicked: root.viewModel.clearMessages()
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
                anchors.leftMargin: 8
                anchors.rightMargin: 12
                clip: true
                spacing: 2
                model: root.streamModel
                reuseItems: true
                property bool shouldFollowOutput: true
                property bool programmaticScroll: false
                property bool bottomAnchorActive: false
                property bool followScrollQueued: false
                property int unreadCount: 0

                function scrollToBottom() {
                    programmaticScroll = true
                    bottomAnchorActive = true
                    unreadCount = 0
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
                    shouldFollowOutput = root.followMode === "always" || count === 0 || distanceFromBottom <= 24
                    if (!shouldFollowOutput) {
                        bottomAnchorActive = false
                    }
                    if (shouldFollowOutput) {
                        unreadCount = 0
                    }
                }

                function noteManualScrollStarted() {
                    if (programmaticScroll) {
                        return
                    }

                    bottomAnchorActive = false
                    refreshFollowState()
                }

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                onMovementStarted: noteManualScrollStarted()
                onMovementEnded: refreshFollowState()
                onFlickStarted: noteManualScrollStarted()
                onFlickEnded: refreshFollowState()
                onContentHeightChanged: {
                    if (bottomAnchorActive && !followScrollQueued && !root.loadingOlderEvents) {
                        root.requestFollowScroll()
                    }
                }
                onContentYChanged: {
                    refreshFollowState()
                    if (contentY <= originY + 48) {
                        root.loadOlderEvents()
                    }
                }

                delegate: Item {
                    id: eventDelegate
                    required property string kind
                    required property string timestamp
                    required property string title
                    required property string topic
                    required property string payload
                    required property string payloadFormat
                    required property int payloadSize
                    required property string topicColor
                    required property string testPayload
                    required property int testFormat
                    required property string historyId
                    readonly property bool isDivider: eventDelegate.kind === "divider"
                    readonly property bool isMessage: eventDelegate.kind === "message"
                    readonly property string payloadSizeLabel: qsTr("%1 B").arg(eventDelegate.payloadSize)
                    readonly property int timelineInset: 6
                    readonly property int timelineX: eventDelegate.timelineInset + 3
                    readonly property int timelineDotSize: 10
                    readonly property int bubbleLeft: eventDelegate.timelineX + 11
                    readonly property int timestampTop: 6
                    readonly property int timestampBubbleGap: 5
                    readonly property int bubbleMaxWidth: 760
                    readonly property int bubbleWidth: Math.max(220,
                                                                 Math.min(eventDelegate.width - eventDelegate.bubbleLeft - 8,
                                                                          eventDelegate.bubbleMaxWidth))
                    readonly property color timelineColor: eventDelegate.topicColor.length > 0
                                                          ? eventDelegate.topicColor
                                                          : (eventDelegate.isMessage
                                                             ? root.ui.themePalette.selectedBorder
                                                             : root.ui.themePalette.warningText)
                    width: ListView.view.width
                    implicitHeight: eventDelegate.isDivider
                                    ? dividerRow.implicitHeight + 14
                                    : timestampLabel.implicitHeight + eventDelegate.timestampBubbleGap
                                      + bubble.implicitHeight + 12

                    Rectangle {
                        visible: !eventDelegate.isDivider
                        x: eventDelegate.timelineX
                        y: 0
                        width: 1
                        height: parent.height
                        color: eventDelegate.timelineColor
                    }

                    Rectangle {
                        visible: !eventDelegate.isDivider
                        x: eventDelegate.timelineX - Math.round(width / 2)
                        y: timestampLabel.y + Math.round((timestampLabel.implicitHeight - height) / 2)
                        width: eventDelegate.timelineDotSize
                        height: eventDelegate.timelineDotSize
                        radius: width / 2
                        color: eventDelegate.timelineColor
                        border.width: 2
                        border.color: root.ui.themePalette.windowBg
                    }

                    RowLayout {
                        id: dividerRow
                        visible: eventDelegate.isDivider
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

                    Label {
                        id: timestampLabel
                        visible: !eventDelegate.isDivider
                        x: eventDelegate.bubbleLeft
                        y: eventDelegate.timestampTop
                        width: eventDelegate.bubbleWidth
                        text: eventDelegate.timestamp
                        color: root.ui.themePalette.timestampText
                        font.pixelSize: 11
                        elide: Label.ElideRight
                    }

                    Rectangle {
                        id: bubble
                        visible: !eventDelegate.isDivider
                        x: eventDelegate.bubbleLeft
                        y: timestampLabel.y + timestampLabel.implicitHeight
                           + eventDelegate.timestampBubbleGap
                        width: eventDelegate.bubbleWidth
                        implicitHeight: rowBody.implicitHeight + 12
                        radius: 8
                        color: eventDelegate.isMessage
                               ? root.ui.themePalette.itemBg
                               : root.ui.themePalette.innerPanelBg
                        border.color: eventDelegate.topicColor.length > 0
                                      ? eventDelegate.topicColor
                                      : (eventDelegate.isMessage
                                         ? root.ui.themePalette.eventBorder
                                         : root.ui.themePalette.innerPanelBorder)

                        Column {
                            id: rowBody
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.margins: 8
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 4

                            RowLayout {
                                width: parent.width
                                spacing: 5

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
                                    visible: eventDelegate.isMessage
                                    label: eventDelegate.payloadSizeLabel
                                    badgeRadius: 7
                                    badgeBorder: root.ui.themePalette.eventBorder
                                    horizontalPadding: 5
                                    verticalPadding: 2
                                    strong: false
                                }

                                AppBadge {
                                    ui: root.ui
                                    visible: eventDelegate.payloadFormat.length > 0
                                    label: eventDelegate.payloadFormat
                                    badgeRadius: 7
                                    badgeBorder: root.ui.themePalette.eventBorder
                                    horizontalPadding: 5
                                    verticalPadding: 2
                                    maximumLabelWidth: 160
                                }

                                AppIconButton {
                                    ui: root.ui
                                    visible: eventDelegate.isMessage
                                    symbol: "T"
                                    symbolSize: 11
                                    implicitWidth: 22
                                    implicitHeight: 22
                                    cornerRadius: 5
                                    restBg: "transparent"
                                    outlineColor: "transparent"
                                    accessibleName: qsTr("Copy topic")
                                    onClicked: root.viewModel.copyMessageTopic(eventDelegate.topic)
                                }

                                AppIconButton {
                                    ui: root.ui
                                    visible: eventDelegate.isMessage
                                    symbol: "P"
                                    symbolSize: 11
                                    implicitWidth: 22
                                    implicitHeight: 22
                                    cornerRadius: 5
                                    restBg: "transparent"
                                    outlineColor: "transparent"
                                    accessibleName: qsTr("Copy payload")
                                    onClicked: root.viewModel.copyMessagePayload(
                                                   eventDelegate.historyId,
                                                   eventDelegate.payload,
                                                   eventDelegate.testPayload,
                                                   eventDelegate.testFormat)
                                }

                                AppIconButton {
                                    ui: root.ui
                                    visible: eventDelegate.isMessage
                                    iconSource: root.ui.materialIcon("send")
                                    implicitWidth: 22
                                    implicitHeight: 22
                                    iconSize: 12
                                    cornerRadius: 5
                                    restBg: "transparent"
                                    outlineColor: "transparent"
                                    accessibleName: qsTr("Use this message in publisher")
                                    onClicked: {
                                        root.viewModel.useMessageAsDraft(
                                                    eventDelegate.historyId,
                                                    eventDelegate.topic,
                                                    eventDelegate.payload,
                                                    eventDelegate.testPayload,
                                                    eventDelegate.testFormat)
                                        root.publishDraftRevealRequested()
                                    }
                                }
                            }

                            Text {
                                id: payloadText
                                width: parent.width
                                text: eventDelegate.payload
                                color: root.ui.textStrong
                                font.family: eventDelegate.isMessage ? "Menlo" : root.fontFamily
                                font.pixelSize: 13
                                lineHeight: 1.12
                                textFormat: Text.PlainText
                                wrapMode: Text.WrapAnywhere
                            }
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
