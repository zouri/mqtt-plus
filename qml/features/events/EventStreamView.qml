pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

Item {
    id: root

    required property var appController
    required property var session
    required property var status
    required property var ui
    required property string fontFamily

    property string streamKind: "message"
    property bool loadingOlderEvents: false
    property bool reachedHistoryStart: false
    property string filterText: ""
    readonly property bool showingMessages: root.streamKind === "message"
    readonly property string activeTitle: root.showingMessages ? qsTr("Messages") : qsTr("Log")
    readonly property string searchPlaceholder: root.showingMessages
                                                ? qsTr("Search topic or content")
                                                : qsTr("Search log channel or detail")
    property int matchingEventCount: 0
    property int activeKindCount: 0
    readonly property bool hasFilter: root.filterText.trim().length > 0
    readonly property bool connected: root.status.state === "connected"
    readonly property int subscriptionCount: Number(root.session.subscriptionCount || 0)
    readonly property string emptyTitle: root.hasFilter
                                        ? (root.showingMessages ? qsTr("No matching messages") : qsTr("No matching log entries"))
                                        : (!root.showingMessages
                                           ? qsTr("No log entries")
                                           : (!root.connected
                                              ? qsTr("Connect to start receiving messages")
                                              : (root.subscriptionCount === 0
                                                 ? qsTr("Add a subscription to start listening")
                                                 : qsTr("Waiting for messages"))))
    readonly property string emptyDetail: root.hasFilter
                                         ? qsTr("Adjust the search query or clear it to return to the live stream.")
                                         : (!root.showingMessages
                                            ? qsTr("Connection, subscription, publish, and storage events appear here.")
                                            : (!root.connected
                                               ? qsTr("Message history will appear here after this session is connected.")
                                               : (root.subscriptionCount === 0
                                                  ? qsTr("Subscriptions can be prepared offline and become active after connection.")
                                                  : qsTr("The stream follows new messages automatically unless you scroll up."))))

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
        const rowKind = row && row.kind ? row.kind : ""
        const rowMatchesCurrentView = rowKind === root.streamKind
        if (!rowMatchesCurrentView) {
            root.recomputeVisibleCount()
            return
        }

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

    function rowMatches(kind, timestamp, title, topic, payload, payloadFormat) {
        if (kind === "divider") {
            return root.activeKindCount > 0 && root.filterText.trim().length === 0
        }

        if (kind !== root.streamKind) {
            return false
        }

        const needle = root.filterText.trim().toLowerCase()
        if (needle.length === 0) {
            return true
        }

        return `${timestamp} ${title} ${topic} ${payload} ${payloadFormat}`.toLowerCase().indexOf(needle) >= 0
    }

    function recomputeVisibleCount() {
        let visibleRows = 0
        let activeRows = 0
        const model = root.appController ? root.appController.events : null
        const rowCount = model ? model.count : 0
        for (let i = 0; i < rowCount; ++i) {
            const row = model.rowAt(i)
            if ((row.kind || "") === root.streamKind) {
                activeRows += 1
            }
        }
        root.activeKindCount = activeRows

        for (let i = 0; i < rowCount; ++i) {
            const row = model.rowAt(i)
            if (root.rowMatches(row.kind || "",
                                row.timestamp || "",
                                row.title || "",
                                row.topic || "",
                                row.payload || "",
                                row.payloadFormat || "")) {
                visibleRows += 1
            }
        }
        root.matchingEventCount = visibleRows
    }

    onFilterTextChanged: root.recomputeVisibleCount()
    onStreamKindChanged: {
        root.recomputeVisibleCount()
        if (eventList) {
            eventList.unreadCount = 0
            Qt.callLater(function() {
                if (eventList) {
                    eventList.scrollToBottom()
                }
            })
        }
    }

    Component.onCompleted: root.recomputeVisibleCount()

    Connections {
        target: root.appController ? root.appController.events : null

        function onCountChanged() {
            root.recomputeVisibleCount()
        }
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
                    text: root.activeTitle
                    color: root.ui.textStrong
                    font.pixelSize: 22
                    font.bold: true
                }

                AppBadge {
                    ui: root.ui
                    label: root.hasFilter
                           ? qsTr("%1/%2").arg(root.matchingEventCount).arg(root.activeKindCount)
                           : `${root.activeKindCount}`
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

                AppTextField {
                    ui: root.ui
                    Layout.preferredWidth: Math.min(280, Math.max(200, root.width * 0.25))
                    placeholderText: root.searchPlaceholder
                    text: root.filterText
                    onTextChanged: root.filterText = text
                }

                AppComboBox {
                    ui: root.ui
                    visible: root.showingMessages
                    Layout.preferredWidth: visible ? 104 : 0
                    model: [qsTr("All directions"), qsTr("Received"), qsTr("Published")]
                }

                AppIconButton {
                    ui: root.ui
                    iconSource: root.ui.materialIcon(root.session.outputPaused ? "play" : "pause")
                    iconSize: 14
                    implicitWidth: 32
                    implicitHeight: 32
                    cornerRadius: 16
                    restBg: root.ui.themePalette.windowBg
                    outlineColor: root.ui.themePalette.innerPanelBorder
                    toolTipText: root.session.outputPaused ? qsTr("Resume output") : qsTr("Pause output")
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
                    toolTipText: qsTr("Clear history")
                    onClicked: root.appController.clearCurrentMessages()
                }
            }
        }

        Label {
            visible: root.session.outputPaused
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
                clip: true
                spacing: 4
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
                    required property string topic
                    required property string payload
                    required property string payloadFormat
                    required property int payloadSize
                    required property string testPayload
                    required property int testFormat
                    readonly property string payloadSizeLabel: qsTr("%1 B").arg(eventDelegate.payloadSize)
                    readonly property bool matchesFilter: root.rowMatches(
                                                              eventDelegate.kind,
                                                              eventDelegate.timestamp,
                                                              eventDelegate.title,
                                                              eventDelegate.topic,
                                                              eventDelegate.payload,
                                                              eventDelegate.payloadFormat)
                    width: ListView.view.width
                    visible: eventDelegate.matchesFilter
                    radius: root.ui.innerRadius
                    color: eventDelegate.kind === "divider"
                           ? "transparent"
                           : root.ui.themePalette.itemBg
                    border.color: eventDelegate.kind === "event"
                                  ? root.ui.themePalette.eventBorder
                                  : (eventDelegate.kind === "divider"
                                     ? "transparent"
                                     : root.ui.themePalette.innerPanelBorder)
                    implicitHeight: !eventDelegate.matchesFilter
                                    ? 0
                                    : (eventDelegate.kind === "divider"
                                    ? dividerRow.implicitHeight + 6
                                    : rowBody.implicitHeight + 14)

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
                                toolTipText: qsTr("Copy topic")
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
                                toolTipText: qsTr("Copy payload")
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
                                toolTipText: qsTr("Use this message in publisher")
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
                visible: root.matchingEventCount === 0
                anchors.centerIn: parent
                width: Math.min(parent.width - 48, 420)
                height: emptyStateColumn.implicitHeight + 20
                radius: root.ui.innerRadius
                color: "transparent"
                border.color: "transparent"

                ColumnLayout {
                    id: emptyStateColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 5

                    Label {
                        Layout.fillWidth: true
                        text: root.emptyTitle
                        color: root.ui.textStrong
                        font.pixelSize: 14
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.emptyDetail
                        color: root.ui.textMuted
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
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
