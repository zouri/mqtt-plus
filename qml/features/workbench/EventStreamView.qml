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
    required property var status
    required property var ui
    required property string fontFamily
    required property string title

    property bool showOutputControls: false
    property bool loadingOlderEvents: false
    property bool reachedHistoryStart: false
    property string followMode: "smart"
    property string selectedHistoryId: ""
    readonly property bool connected: root.status.state === "connected"
    readonly property color surfaceBg: root.ui.themePalette.panelBg

    signal publishDraftRevealRequested()
    signal messageSelected(string historyId)

    Layout.fillWidth: true
    Layout.fillHeight: true

    function resetStreamPosition() {
        root.loadingOlderEvents = false
        root.reachedHistoryStart = false

        if (eventList) {
            eventList.unreadCount = 0
            eventList.bottomAnchorActive = true
            eventList.shouldFollowOutput = true
            eventList.userScrollActive = false
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
        if (mode === "manual") {
            return qsTr("Manual")
        }
        return qsTr("Smart")
    }

    function nextFollowMode(mode) {
        if (mode === "smart") {
            return "manual"
        }
        return "smart"
    }

    function followModeToolTip() {
        const activeMode = eventList && eventList.shouldFollowOutput ? "smart" : "manual"
        return qsTr("Follow mode: %1").arg(root.followModeLabel(activeMode))
    }

    function compactTimestamp(value) {
        const text = String(value || "")
        const spaceIndex = text.lastIndexOf(" ")
        if (spaceIndex >= 0 && spaceIndex + 1 < text.length) {
            return text.slice(spaceIndex + 1)
        }
        return text
    }

    function setFollowMode(mode) {
        if (mode !== "smart" && mode !== "manual") {
            return
        }

        root.followMode = mode
        if (!eventList) {
            return
        }

        if (mode === "manual") {
            eventList.bottomAnchorActive = false
            eventList.shouldFollowOutput = false
            return
        }

        eventList.shouldFollowOutput = true
        root.requestFollowScroll()
    }

    function shouldFollowNewRows() {
        if (root.followMode === "manual") {
            return false
        }
        return eventList.shouldFollowOutput
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: root.surfaceBg

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 8

                Label {
                    text: root.title
                    color: root.ui.textStrong
                    font.pixelSize: 16
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

                AppTextField {
                    id: messageSearchField

                    ui: root.ui
                    Layout.fillWidth: true
                    Layout.maximumWidth: 220
                    Layout.minimumWidth: 100
                    Layout.preferredHeight: 30
                    leftPadding: 32
                    placeholderText: qsTr("Search messages")
                    // qmllint disable missing-property
                    text: root.streamModel.filterText
                    onTextEdited: root.streamModel.filterText = text
                    // qmllint enable missing-property

                    AppIconButton {
                        ui: root.ui
                        anchors.left: parent.left
                        anchors.leftMargin: 3
                        anchors.verticalCenter: parent.verticalCenter
                        implicitWidth: 24
                        implicitHeight: 24
                        iconSource: root.ui.materialIcon("search")
                        iconSize: 14
                        restBg: "transparent"
                        hoverBg: "transparent"
                        pressedBg: "transparent"
                        outlineColor: "transparent"
                        symbolColor: root.ui.textMuted
                        activeFocusOnTab: false
                        Accessible.ignored: true
                        onClicked: messageSearchField.forceActiveFocus()
                    }
                }

                AppIconButton {
                    id: messageFilterButton

                    ui: root.ui
                    visible: root.showOutputControls
                    iconSource: root.ui.materialIcon("filter")
                    iconSize: 15
                    implicitWidth: 32
                    implicitHeight: 32
                    cornerRadius: 7
                    restBg: root.ui.themePalette.itemBg
                    hoverBg: root.ui.themePalette.selectedBg
                    outlineColor: root.ui.themePalette.panelBorder
                    // qmllint disable missing-property
                    symbolColor: root.streamModel.filterActive ? root.ui.themePalette.infoText : root.ui.textMuted
                    forceActive: root.streamModel.filterActive
                    // qmllint enable missing-property
                    accessibleName: qsTr("Message filters")
                    toolTipText: qsTr("Message filters")
                    onClicked: messageFilterPopover.open()
                }

                AppIconButton {
                    id: followModeButton

                    ui: root.ui
                    visible: root.showOutputControls
                    checkable: true
                    checked: eventList.shouldFollowOutput
                    iconSource: root.ui.materialIcon("follow-mode")
                    iconSize: 15
                    implicitWidth: 32
                    implicitHeight: 32
                    cornerRadius: 7
                    restBg: root.ui.themePalette.itemBg
                    hoverBg: root.ui.themePalette.selectedBg
                    outlineColor: root.ui.themePalette.panelBorder
                    symbolColor: followModeButton.checked ? root.ui.themePalette.infoText : root.ui.textMuted
                    forceActive: followModeButton.checked
                    accessibleName: root.followModeToolTip()
                    toolTipText: root.followModeToolTip()
                    onClicked: root.setFollowMode(eventList.shouldFollowOutput ? "manual" : "smart")
                }

                AppIconButton {
                    ui: root.ui
                    visible: root.showOutputControls
                    iconSource: root.ui.materialIcon(root.session.outputPaused ? "play" : "pause")
                    iconSize: 14
                    implicitWidth: 32
                    implicitHeight: 32
                    cornerRadius: 7
                    restBg: root.ui.themePalette.itemBg
                    outlineColor: root.ui.themePalette.panelBorder
                    accessibleName: root.session.outputPaused ? qsTr("Resume output") : qsTr("Pause output")
                    onClicked: root.viewModel.toggleCurrentOutputPaused(root.session.outputPaused)
                }

                AppIconButton {
                    ui: root.ui
                    iconSource: root.ui.materialIcon("more-horiz")
                    iconSize: 14
                    implicitWidth: 32
                    implicitHeight: 32
                    cornerRadius: 7
                    restBg: root.ui.themePalette.itemBg
                    outlineColor: root.ui.themePalette.panelBorder
                    accessibleName: qsTr("Clear message history")
                    toolTipText: qsTr("Clear message history")
                    onClicked: root.viewModel.clearMessages()
                }
            }

            MessageFilterPopover {
                id: messageFilterPopover

                ui: root.ui
                filterModel: root.streamModel
                subscriptionsModel: root.viewModel.filteredSubscriptions
                x: Math.max(8, parent.width - width - 110)
                y: parent.height - 2
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
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                anchors.topMargin: 8
                anchors.bottomMargin: 8
                clip: true
                spacing: 4
                model: root.streamModel
                reuseItems: true
                property bool shouldFollowOutput: true
                property bool programmaticScroll: false
                property bool userScrollActive: false
                property bool bottomAnchorActive: false
                property bool followScrollQueued: false
                property int unreadCount: 0

                function scrollToBottom() {
                    programmaticScroll = true
                    userScrollActive = false
                    bottomAnchorActive = true
                    shouldFollowOutput = true
                    unreadCount = 0
                    root.followMode = "smart"
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

                    userScrollActive = true
                    bottomAnchorActive = false
                    refreshFollowState()
                }

                function noteManualScrollEnded() {
                    if (programmaticScroll) {
                        return
                    }

                    userScrollActive = false
                    refreshFollowState()
                }

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                onMovementStarted: noteManualScrollStarted()
                onMovementEnded: noteManualScrollEnded()
                onFlickStarted: noteManualScrollStarted()
                onFlickEnded: noteManualScrollEnded()
                onContentHeightChanged: {
                    if (bottomAnchorActive && !followScrollQueued && !root.loadingOlderEvents) {
                        root.requestFollowScroll()
                    }
                }
                onContentYChanged: {
                    if (userScrollActive || !shouldFollowOutput) {
                        refreshFollowState()
                    }
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
                    required property string direction
                    required property string alias
                    required property int qos
                    required property bool retain
                    required property bool retainKnown
                    required property string parsedPayload
                    required property string payloadState
                    required property string payloadHash
                    readonly property bool isDivider: eventDelegate.kind === "divider"
                    readonly property bool isMessage: eventDelegate.kind === "message"
                    readonly property bool isEvent: eventDelegate.kind === "event"
                    readonly property string payloadSizeLabel: qsTr("%1 B").arg(eventDelegate.payloadSize)
                    readonly property color topicSwatchColor: eventDelegate.topicColor.length > 0
                                                             ? eventDelegate.topicColor
                                                             : (eventDelegate.isMessage
                                                                ? root.ui.themePalette.selectedBorder
                                                                : root.ui.themePalette.warningText)
                    width: ListView.view.width
                    implicitHeight: eventDelegate.isDivider
                                    ? dividerRow.implicitHeight + 14
                                    : messageRow.implicitHeight + 8

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

                    Rectangle {
                        id: messageRow
                        visible: !eventDelegate.isDivider
                        width: parent.width
                        implicitHeight: rowBody.implicitHeight + 16
                        radius: 10
                        color: eventDelegate.historyId === root.selectedHistoryId
                               ? root.ui.themePalette.selectedBg
                               : (rowHover.hovered ? root.ui.themePalette.rowHover : "transparent")
                        border.color: eventDelegate.historyId === root.selectedHistoryId
                                      ? root.ui.themePalette.selectedBorder
                                      : "transparent"

                        HoverHandler {
                            id: rowHover
                        }

                        TapHandler {
                            enabled: eventDelegate.isMessage
                            onTapped: {
                                root.selectedHistoryId = eventDelegate.historyId;
                                root.messageSelected(eventDelegate.historyId);
                            }
                        }

                        RowLayout {
                            id: rowBody
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 10

                            Label {
                                Layout.preferredWidth: 82
                                text: root.compactTimestamp(eventDelegate.timestamp)
                                color: root.ui.themePalette.timestampText
                                font.family: "Menlo"
                                font.pixelSize: 11
                                elide: Label.ElideRight
                            }

                            Label {
                                Layout.preferredWidth: 14
                                text: eventDelegate.isMessage
                                      ? (eventDelegate.direction === "outgoing" ? "↑" : "↓")
                                      : "•"
                                color: eventDelegate.isMessage
                                       ? (eventDelegate.direction === "outgoing"
                                          ? root.ui.textMuted
                                          : root.ui.themePalette.successText)
                                       : root.ui.themePalette.warningText
                                horizontalAlignment: Text.AlignHCenter
                                font.pixelSize: 14
                                font.bold: true
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 3

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    Rectangle {
                                        Layout.preferredWidth: 8
                                        Layout.preferredHeight: 8
                                        radius: 2
                                        color: eventDelegate.topicSwatchColor
                                    }

                                    TextInput {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        text: eventDelegate.alias.length > 0
                                              ? `${eventDelegate.alias} · ${eventDelegate.topic}`
                                              : eventDelegate.title
                                        color: eventDelegate.isEvent
                                               ? root.ui.themePalette.eventTitle
                                               : root.ui.textStrong
                                        font.pixelSize: 12
                                        font.bold: true
                                        readOnly: true
                                        selectByMouse: true
                                        clip: true
                                        selectedTextColor: root.ui.themePalette.buttonPrimaryText
                                        selectionColor: root.ui.themePalette.buttonPrimaryBg
                                    }
                                }

                                TextEdit {
                                    id: payloadText

                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    Layout.preferredHeight: Math.min(implicitHeight,
                                                                     payloadLineMetrics.lineSpacing * 2)
                                    text: eventDelegate.payload
                                    color: eventDelegate.isEvent ? root.ui.textMuted : root.ui.textStrong
                                    font.family: eventDelegate.isMessage ? "Menlo" : root.fontFamily
                                    font.pixelSize: 12
                                    textFormat: Text.PlainText
                                    wrapMode: TextEdit.WrapAnywhere
                                    readOnly: true
                                    selectByMouse: true
                                    clip: true
                                    selectedTextColor: root.ui.themePalette.buttonPrimaryText
                                    selectionColor: root.ui.themePalette.buttonPrimaryBg

                                    HoverHandler {
                                        cursorShape: Qt.ArrowCursor
                                    }
                                }

                                FontMetrics {
                                    id: payloadLineMetrics

                                    font.family: payloadText.font.family
                                    font.pixelSize: payloadText.font.pixelSize
                                }
                            }

                            ColumnLayout {
                                id: messageActions
                                visible: eventDelegate.isMessage || eventDelegate.payloadFormat.length > 0
                                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                                Layout.preferredWidth: Math.max(metadataRow.implicitWidth,
                                                                actionButtonRow.implicitWidth)
                                Layout.minimumWidth: Layout.preferredWidth
                                Layout.maximumWidth: Layout.preferredWidth
                                spacing: 4

                                RowLayout {
                                    id: metadataRow
                                    Layout.alignment: Qt.AlignRight
                                    spacing: 6

                                    AppBadge {
                                        ui: root.ui
                                        visible: eventDelegate.payloadFormat.length > 0
                                        Layout.preferredWidth: implicitWidth
                                        Layout.preferredHeight: implicitHeight
                                        label: eventDelegate.payloadFormat
                                        badgeRadius: 6
                                        badgeBorder: root.ui.themePalette.eventBorder
                                        horizontalPadding: 6
                                        verticalPadding: 1
                                        maximumLabelWidth: 160
                                    }

                                    Label {
                                        visible: eventDelegate.isMessage
                                        text: eventDelegate.payloadSizeLabel
                                        color: root.ui.themePalette.textSubtle
                                        font.family: "Menlo"
                                        font.pixelSize: 10
                                        horizontalAlignment: Text.AlignRight
                                    }
                                }

                                RowLayout {
                                    id: actionButtonRow
                                    visible: eventDelegate.isMessage
                                    Layout.alignment: Qt.AlignRight
                                    spacing: 2

                                    AppIconButton {
                                        ui: root.ui
                                        iconSource: root.ui.materialIcon("topic")
                                        Layout.preferredWidth: 22
                                        Layout.preferredHeight: 22
                                        iconSize: 12
                                        cornerRadius: 5
                                        restBg: "transparent"
                                        outlineColor: "transparent"
                                        symbolColor: root.ui.themePalette.textSubtle
                                        accessibleName: qsTr("Copy topic")
                                        toolTipText: qsTr("Copy topic")
                                        toolTipPosition: AppToolTip.Position.Top
                                        onClicked: root.viewModel.copyMessageTopic(eventDelegate.topic)
                                    }

                                    AppIconButton {
                                        ui: root.ui
                                        iconSource: root.ui.materialIcon("content-copy")
                                        Layout.preferredWidth: 22
                                        Layout.preferredHeight: 22
                                        iconSize: 12
                                        cornerRadius: 5
                                        restBg: "transparent"
                                        outlineColor: "transparent"
                                        symbolColor: root.ui.themePalette.textSubtle
                                        accessibleName: qsTr("Copy payload")
                                        toolTipText: qsTr("Copy payload")
                                        toolTipPosition: AppToolTip.Position.Top
                                        onClicked: root.viewModel.copyMessagePayload(
                                                       eventDelegate.historyId,
                                                       eventDelegate.payload,
                                                       eventDelegate.testPayload,
                                                       eventDelegate.testFormat)
                                    }

                                    AppIconButton {
                                        ui: root.ui
                                        iconSource: root.ui.materialIcon("edit")
                                        Layout.preferredWidth: 22
                                        Layout.preferredHeight: 22
                                        iconSize: 12
                                        cornerRadius: 5
                                        restBg: "transparent"
                                        outlineColor: "transparent"
                                        symbolColor: root.ui.themePalette.textSubtle
                                        accessibleName: qsTr("Use as publish draft")
                                        toolTipText: qsTr("Use as publish draft")
                                        toolTipPosition: AppToolTip.Position.Top
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
                            }
                        }
                    }
                }
            }

            Rectangle {
                id: followButton
                visible: !eventList.shouldFollowOutput
                         && (root.connected || eventList.unreadCount > 0)
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: visible ? 14 : 8
                radius: 19
                height: 38
                width: followButtonRow.implicitWidth + 26
                color: followMouse.containsMouse || activeFocus
                       ? root.ui.themePalette.buttonPrimaryHoverBg
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
                        text: qsTr("Scroll to latest")
                        color: root.ui.themePalette.followText
                        font.pixelSize: 12
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
                        root.setFollowMode("smart")
                    }
                }
            }
        }
    }
}
