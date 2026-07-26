pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

Item {
    id: root

    required property var viewModel
    required property var publisher
    required property var eventHistory
    required property var sessionService
    required property QtObject streamModel
    required property var session
    required property var status
    required property var ui
    required property string fontFamily
    required property string title
    required property int payloadDisplayMode

    property bool showOutputControls: false
    property bool loadingOlderEvents: false
    property bool reachedHistoryStart: false
    property string followMode: "smart"
    property string selectedHistoryId: ""
    readonly property bool connected: root.status.state === "connected"
    readonly property bool connecting: root.status.state === "connecting"
    readonly property color surfaceBg: root.ui.themePalette.panelBg
    readonly property bool compactHeader: root.width <= 520
    readonly property var messageTopicFilterState: root.viewModel.messageTopicFilterState
    readonly property int selectedTopicCount: Number(root.messageTopicFilterState.selectedCount || 0)
    readonly property int selectedTopicsPausedCount: Number(root.messageTopicFilterState.pausedCount || 0)
    readonly property string filterSummaryText: root.messageFilterSummary()
    readonly property string receiveStateText: root.selectedTopicsPausedCount <= 0
                                               ? ""
                                               : (root.selectedTopicCount > 1
                                                  ? qsTr("%1 selected Topics are paused").arg(root.selectedTopicsPausedCount)
                                                  : qsTr("Receiving is paused"))
    readonly property int subscriptionCount: root.viewModel.messageFilterSubscriptions
                                             ? root.viewModel.messageFilterSubscriptions.count
                                             : 0
    // qmllint disable missing-property
    readonly property bool streamEmpty: root.streamModel.count === 0
    readonly property bool filtersActive: root.streamModel.filterActive
    // qmllint enable missing-property
    readonly property string emptyStateTitle: root.filtersActive
                                               ? qsTr("No matching messages")
                                               : (root.connecting
                                                  ? qsTr("Connecting to broker")
                                                  : (!root.connected
                                                     ? qsTr("Connect to receive messages")
                                                     : (root.subscriptionCount === 0
                                                        ? qsTr("No subscriptions yet")
                                                        : qsTr("Listening for messages"))))
    readonly property string emptyStateDescription: root.filtersActive
                                                     ? qsTr("Adjust the search or filters to see messages again.")
                                                     : (root.connecting
                                                        ? qsTr("The message stream will start when the connection is ready.")
                                                        : (!root.connected
                                                           ? qsTr("Start this connection, then subscribe to the topics you want to inspect.")
                                                           : (root.subscriptionCount === 0
                                                              ? qsTr("Add a topic to start listening on this connection.")
                                                              : qsTr("Listening to %1 topic(s). New messages will appear here.").arg(root.subscriptionCount))))
    readonly property string emptyStateAction: root.filtersActive
                                                ? qsTr("Clear filters")
                                                : (!root.connected && !root.connecting
                                                   ? qsTr("Connect")
                                                   : (root.connected && root.subscriptionCount === 0
                                                      ? qsTr("Add subscription")
                                                      : ""))

    signal publishDraftRevealRequested()
    signal messageSelected(string historyId)
    signal messagesCleared()
    signal subscriptionCreateRequested()

    Layout.fillWidth: true
    Layout.fillHeight: true

    function resetStreamPosition() {
        root.loadingOlderEvents = false
        root.reachedHistoryStart = false
        root.followMode = "smart"

        if (eventList) {
            eventList.unreadCount = 0
            eventList.bottomAnchorActive = true
            eventList.shouldFollowOutput = true
            eventList.userScrollActive = false
            eventList.programmaticScroll = true
        }

        root.eventHistory.setMessageStreamFrozen(false)
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

    // qmllint disable missing-property
    function messageFilterSummary() {
        const parts = [];
        if (root.selectedTopicCount === 1) {
            parts.push(String(root.messageTopicFilterState.singleTopicLabel || ""));
        } else if (root.selectedTopicCount > 1) {
            parts.push(qsTr("%1 Topics").arg(root.selectedTopicCount));
        }
        if (root.streamModel.direction === "incoming") {
            parts.push(qsTr("Received"));
        } else if (root.streamModel.direction === "outgoing") {
            parts.push(qsTr("Sent"));
        }
        return parts.length > 0 ? parts.join(" · ") : qsTr("Filter");
    }
    // qmllint enable missing-property

    function clearMessageSelection() {
        root.selectedHistoryId = "";
    }

    function selectAdjacentMessage(fromIndex, direction) {
        for (let index = fromIndex + direction;
             index >= 0 && index < root.streamModel.count;
             index += direction) {
            const row = root.streamModel.rowAt(index);
            if (String(row.kind || "") !== "message") {
                continue;
            }
            root.setFollowMode("manual");
            eventList.currentIndex = index;
            root.selectedHistoryId = String(row.historyId || "");
            root.messageSelected(root.selectedHistoryId);
            eventList.positionViewAtIndex(index, ListView.Contain);
            Qt.callLater(function() {
                const delegateItem = eventList.itemAtIndex(index);
                if (delegateItem) {
                    delegateItem.focusMessageRow();
                }
            });
            return;
        }
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

    function focusSearch() {
        messageSearchField.forceActiveFocus()
        messageSearchField.selectAll()
    }

    function triggerEmptyStateAction() {
        if (root.filtersActive) {
            root.viewModel.clearMessageFilters()
        } else if (!root.connected && !root.connecting) {
            root.viewModel.toggleCurrentSessionConnection()
        } else if (root.connected && root.subscriptionCount === 0) {
            root.subscriptionCreateRequested()
        }
    }

    function loadOlderEvents() {
        if (root.loadingOlderEvents || root.reachedHistoryStart || !eventList) {
            return
        }

        root.loadingOlderEvents = true
        eventList.bottomAnchorActive = false
        const previousContentHeight = eventList.contentHeight
        const previousContentY = eventList.contentY
        const insertedRows = root.eventHistory.loadOlderCurrentSessionMessages()
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
            root.eventHistory.setMessageStreamFrozen(true)
            return
        }

        eventList.programmaticScroll = true
        eventList.shouldFollowOutput = true
        root.eventHistory.setMessageStreamFrozen(false)
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
                    // qmllint disable missing-property
                    label: root.streamModel.filterActive
                           ? qsTr("%1/%2").arg(root.streamModel.filteredMessageCount)
                                           .arg(root.viewModel.totalMessageCount)
                           : String(root.viewModel.totalMessageCount)
                    // qmllint enable missing-property
                    badgeRadius: 11
                    horizontalPadding: 7
                    verticalPadding: 3
                    badgeBg: root.ui.themePalette.dividerLabelBg
                    badgeBorder: "transparent"
                    badgeText: root.ui.textMuted
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
                    onTextChanged: root.viewModel.setMessageSearchText(text)
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
                        enabled: false
                        Accessible.ignored: true
                    }
                }

                Button {
                    id: messageFilterButton

                    visible: root.showOutputControls
                    // qmllint disable missing-property
                    readonly property bool scopedFilterActive: root.selectedTopicCount > 0
                                                                || root.streamModel.direction !== "all"
                    // qmllint enable missing-property
                    Layout.preferredWidth: root.compactHeader
                                           ? 32
                                           : Math.min(156, Math.max(68, implicitContentWidth + 18))
                    Layout.preferredHeight: 30
                    leftPadding: root.compactHeader ? 0 : 8
                    rightPadding: root.compactHeader ? 0 : 8
                    spacing: root.compactHeader ? 0 : 6
                    text: root.filterSummaryText
                    display: root.compactHeader ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon
                    icon.source: root.ui.materialIcon("filter")
                    icon.width: 14
                    icon.height: 14
                    icon.color: messageFilterButton.scopedFilterActive
                                ? root.ui.themePalette.infoText
                                : root.ui.textMuted
                    font.pixelSize: 11
                    palette.buttonText: messageFilterButton.scopedFilterActive
                                        ? root.ui.themePalette.infoText
                                        : root.ui.textStrong
                    Accessible.name: qsTr("Message filters: %1").arg(root.filterSummaryText)

                    background: Rectangle {
                        radius: 8
                        color: messageFilterButton.scopedFilterActive
                               ? root.ui.themePalette.selectedBg
                               : (messageFilterButton.hovered
                                  ? root.ui.themePalette.rowHover
                                  : root.ui.themePalette.itemBg)
                        border.color: messageFilterButton.scopedFilterActive
                                      ? root.ui.themePalette.selectedBorder
                                      : root.ui.themePalette.panelBorder
                        border.width: 1
                    }

                    AppToolTip {
                        ui: root.ui
                        text: qsTr("Message filters")
                        position: AppToolTip.Position.Bottom
                        active: messageFilterButton.hovered
                    }

                    onClicked: messageFilterPopover.open()
                }

                AppIconButton {
                    id: followModeButton

                    ui: root.ui
                    visible: root.showOutputControls
                    iconSource: root.ui.materialIcon("follow-mode")
                    iconSize: 15
                    implicitWidth: 32
                    implicitHeight: 32
                    cornerRadius: 7
                    restBg: root.ui.themePalette.itemBg
                    hoverBg: root.ui.themePalette.selectedBg
                    outlineColor: root.ui.themePalette.panelBorder
                    symbolColor: eventList.shouldFollowOutput ? root.ui.themePalette.infoText : root.ui.textMuted
                    forceActive: eventList.shouldFollowOutput
                    accessibleName: root.followModeToolTip()
                    toolTipText: root.followModeToolTip()
                    onClicked: root.setFollowMode("smart")
                }

                AppIconButton {
                    id: streamMoreButton

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
                    onClicked: root.sessionService.setCurrentOutputPaused(!root.session.outputPaused)
                }

                AppIconButton {
                    id: streamActionsButton

                    ui: root.ui
                    iconSource: root.ui.materialIcon("more-horiz")
                    iconSize: 14
                    implicitWidth: 32
                    implicitHeight: 32
                    cornerRadius: 7
                    restBg: root.ui.themePalette.itemBg
                    outlineColor: root.ui.themePalette.panelBorder
                    forceActive: streamActionsMenu.visible
                    accessibleName: qsTr("More message actions")
                    toolTipText: qsTr("More message actions")
                    onClicked: streamActionsMenu.openForItem(streamActionsButton)
                }
            }

            MessageFilterPopover {
                id: messageFilterPopover

                ui: root.ui
                filterModel: root.streamModel
                subscriptionsModel: root.viewModel.messageFilterSubscriptions
                receiveStateText: root.receiveStateText
                x: Math.max(8, parent.width - width - 110)
                y: parent.height - 2
            }

            ListModel {
                id: streamActions

                ListElement { actionId: "clear-messages" }
            }

            AppMenu {
                id: streamActionsMenu

                ui: root.ui
                accessibleName: qsTr("More message actions")
                model: streamActions
                actionText: actionId => actionId === "clear-messages" ? qsTr("Clear message history") : ""
                actionIcon: actionId => actionId === "clear-messages" ? root.ui.materialIcon("delete") : ""
                actionDanger: actionId => actionId === "clear-messages"
                onTriggered: actionId => {
                    if (actionId !== "clear-messages") {
                        return;
                    }
                    clearMessagesDialog.open();
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
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                anchors.topMargin: 8
                anchors.bottomMargin: 8
                clip: true
                spacing: 2
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

                function refreshFollowState(allowResume) {
                    if (programmaticScroll) {
                        return
                    }

                    const atBottom = count === 0 || atYEnd
                    if (root.followMode === "manual") {
                        shouldFollowOutput = false
                        if (atBottom && allowResume && eventList.userScrollActive) {
                            root.setFollowMode("smart")
                        }
                        return
                    }

                    shouldFollowOutput = atBottom
                    if (!shouldFollowOutput) {
                        root.setFollowMode("manual")
                        return
                    }
                    unreadCount = 0
                }

                function noteManualScrollStarted() {
                    if (programmaticScroll) {
                        return
                    }

                    userScrollActive = true
                    bottomAnchorActive = false
                }

                function noteManualScrollEnded() {
                    if (programmaticScroll) {
                        return
                    }

                    refreshFollowState(true)
                    userScrollActive = false
                }

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded

                    onPressedChanged: {
                        if (pressed) {
                            eventList.noteManualScrollStarted()
                        } else {
                            eventList.noteManualScrollEnded()
                        }
                    }
                }

                WheelHandler {
                    target: null
                    blocking: false
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad

                    onActiveChanged: {
                        if (active) {
                            eventList.noteManualScrollStarted()
                        } else {
                            eventList.noteManualScrollEnded()
                        }
                    }
                }

                onDraggingChanged: {
                    if (dragging) {
                        noteManualScrollStarted()
                    } else if (!flicking) {
                        noteManualScrollEnded()
                    }
                }
                onFlickingChanged: {
                    if (flicking) {
                        noteManualScrollStarted()
                    } else if (!dragging) {
                        noteManualScrollEnded()
                    }
                }
                onContentHeightChanged: {
                    if (bottomAnchorActive && !followScrollQueued && !root.loadingOlderEvents) {
                        root.requestFollowScroll()
                    }
                }
                onContentYChanged: {
                    if (userScrollActive || !shouldFollowOutput) {
                        refreshFollowState(userScrollActive)
                    }
                    if (contentY <= originY + 48) {
                        root.loadOlderEvents()
                    }
                }

                delegate: Item {
                    id: eventDelegate
                    required property int index
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
                                    : messageRow.implicitHeight + 2

                    function selectMessage() {
                        if (!eventDelegate.isMessage) {
                            return;
                        }
                        root.setFollowMode("manual");
                        eventList.currentIndex = eventDelegate.index;
                        root.selectedHistoryId = eventDelegate.historyId;
                        root.messageSelected(eventDelegate.historyId);
                    }

                    function focusMessageRow() {
                        messageRow.forceActiveFocus();
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

                    Rectangle {
                        id: messageRow
                        visible: !eventDelegate.isDivider
                        width: parent.width
                        implicitHeight: Math.max(64, rowBody.implicitHeight + 16)
                        radius: 7
                        color: eventDelegate.historyId === root.selectedHistoryId
                               ? root.ui.themePalette.selectedBg
                               : (rowHover.hovered ? root.ui.themePalette.rowHover : "transparent")
                        border.color: eventDelegate.historyId === root.selectedHistoryId
                                      ? root.ui.themePalette.selectedBorder
                                      : "transparent"
                        border.width: 1
                        Accessible.ignored: !eventDelegate.isMessage
                        Accessible.role: Accessible.Button
                        Accessible.name: eventDelegate.isMessage
                                         ? qsTr("%1 message, %2, %3")
                                             .arg(eventDelegate.direction === "outgoing" ? qsTr("Sent") : qsTr("Received"))
                                             .arg(eventDelegate.topic)
                                             .arg(root.compactTimestamp(eventDelegate.timestamp))
                                         : ""
                        activeFocusOnTab: eventDelegate.isMessage

                        Keys.onPressed: event => {
                            if (event.key === Qt.Key_Return
                                    || event.key === Qt.Key_Enter
                                    || event.key === Qt.Key_Space) {
                                eventDelegate.selectMessage();
                                event.accepted = true;
                            } else if (event.key === Qt.Key_Up) {
                                root.selectAdjacentMessage(eventDelegate.index, -1);
                                event.accepted = true;
                            } else if (event.key === Qt.Key_Down) {
                                root.selectAdjacentMessage(eventDelegate.index, 1);
                                event.accepted = true;
                            }
                        }

                        HoverHandler {
                            id: rowHover
                        }

                        AppToolTip {
                            ui: root.ui
                            text: eventDelegate.topic
                            position: AppToolTip.Position.Right
                            active: rowHover.hovered && messageTopicLabel.truncated
                        }

                        TapHandler {
                            enabled: eventDelegate.isMessage
                            gesturePolicy: TapHandler.ReleaseWithinBounds
                            onTapped: {
                                messageRow.forceActiveFocus()
                                eventDelegate.selectMessage()
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

                            Rectangle {
                                Layout.preferredWidth: 18
                                Layout.preferredHeight: 18
                                radius: 5
                                color: eventDelegate.isMessage
                                       ? Qt.rgba(eventDelegate.topicSwatchColor.r,
                                                 eventDelegate.topicSwatchColor.g,
                                                 eventDelegate.topicSwatchColor.b,
                                                 0.14)
                                       : "transparent"

                                Label {
                                    anchors.centerIn: parent
                                    text: eventDelegate.isMessage
                                          ? (eventDelegate.direction === "outgoing" ? "↑" : "↓")
                                          : "•"
                                    color: eventDelegate.isMessage
                                           ? eventDelegate.topicSwatchColor
                                           : root.ui.themePalette.warningText
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                            }

                            Label {
                                Layout.preferredWidth: 82
                                text: root.compactTimestamp(eventDelegate.timestamp)
                                color: root.ui.themePalette.timestampText
                                font.pixelSize: 11
                                elide: Label.ElideRight
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 3

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    Label {
                                        visible: eventDelegate.alias.length > 0
                                        text: eventDelegate.alias
                                        color: eventDelegate.isEvent
                                               ? root.ui.themePalette.eventTitle
                                               : root.ui.textStrong
                                        font.pixelSize: 12
                                        font.bold: true
                                        elide: Label.ElideRight
                                    }

                                    Label {
                                        visible: eventDelegate.alias.length > 0
                                        text: "·"
                                        color: root.ui.textMuted
                                        font.pixelSize: 11
                                    }

                                    Label {
                                        id: messageTopicLabel
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        text: eventDelegate.alias.length > 0
                                              ? eventDelegate.topic
                                              : eventDelegate.title
                                        color: eventDelegate.alias.length > 0
                                               ? root.ui.textMuted
                                               : (eventDelegate.isEvent
                                                  ? root.ui.themePalette.eventTitle
                                                  : root.ui.textStrong)
                                        font.pixelSize: 11
                                        font.bold: eventDelegate.alias.length === 0
                                        elide: Label.ElideRight
                                    }
                                }

                                Label {
                                    id: payloadText

                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    text: eventDelegate.payload
                                    color: eventDelegate.isEvent ? root.ui.textMuted : root.ui.textStrong
                                    font.family: root.fontFamily
                                    font.pixelSize: 12
                                    textFormat: Text.PlainText
                                    wrapMode: Text.WrapAnywhere
                                    maximumLineCount: root.payloadDisplayMode === 2
                                                      ? 2147483647
                                                      : (root.payloadDisplayMode === 1
                                                         && (eventDelegate.historyId === root.selectedHistoryId
                                                             || rowHover.hovered)
                                                         ? 12
                                                         : 3)
                                    elide: Label.ElideRight
                                }
                            }

                            ColumnLayout {
                                id: messageActions
                                visible: eventDelegate.isMessage || eventDelegate.payloadFormat.length > 0
                                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                                Layout.preferredWidth: Math.max(metadataRow.implicitWidth,
                                                                quickActionsRow.implicitWidth)
                                Layout.minimumWidth: Layout.preferredWidth
                                Layout.maximumWidth: Layout.preferredWidth
                                spacing: 4

                                RowLayout {
                                    id: metadataRow
                                    Layout.alignment: Qt.AlignRight
                                    spacing: 6

                                    Label {
                                        visible: eventDelegate.payloadFormat.length > 0
                                        text: eventDelegate.payloadFormat.replace(": ", " · ")
                                        color: root.ui.themePalette.textSubtle
                                        font.pixelSize: 10
                                        elide: Label.ElideRight
                                        Layout.maximumWidth: 160
                                    }

                                    Label {
                                        visible: eventDelegate.isMessage
                                        text: eventDelegate.payloadSizeLabel
                                        color: root.ui.themePalette.textSubtle
                                        font.pixelSize: 10
                                        horizontalAlignment: Text.AlignRight
                                    }
                                }

                                RowLayout {
                                    id: quickActionsRow

                                    visible: eventDelegate.isMessage
                                             && (rowHover.hovered || messageRow.activeFocus
                                                 || eventDelegate.historyId === root.selectedHistoryId)
                                    Layout.alignment: Qt.AlignRight
                                    spacing: 2

                                    AppIconButton {
                                        ui: root.ui
                                        implicitWidth: 24
                                        implicitHeight: 24
                                        cornerRadius: 6
                                        iconSource: root.ui.materialIcon("content-copy")
                                        iconSize: 13
                                        restBg: root.ui.themePalette.itemBg
                                        outlineColor: root.ui.themePalette.panelBorder
                                        accessibleName: qsTr("Copy payload")
                                        toolTipText: qsTr("Copy payload")
                                        onClicked: root.viewModel.copyMessagePayload(
                                            eventDelegate.historyId,
                                            eventDelegate.payload,
                                            eventDelegate.testPayload,
                                            eventDelegate.testFormat)
                                    }

                                    AppIconButton {
                                        ui: root.ui
                                        implicitWidth: 24
                                        implicitHeight: 24
                                        cornerRadius: 6
                                        iconSource: root.ui.materialIcon("topic")
                                        iconSize: 13
                                        restBg: root.ui.themePalette.itemBg
                                        outlineColor: root.ui.themePalette.panelBorder
                                        accessibleName: qsTr("Copy topic")
                                        toolTipText: qsTr("Copy topic")
                                        onClicked: root.viewModel.copyMessageTopic(eventDelegate.topic)
                                    }

                                    AppIconButton {
                                        ui: root.ui
                                        implicitWidth: 24
                                        implicitHeight: 24
                                        cornerRadius: 6
                                        iconSource: root.ui.materialIcon("edit")
                                        iconSize: 13
                                        restBg: root.ui.themePalette.itemBg
                                        outlineColor: root.ui.themePalette.panelBorder
                                        accessibleName: qsTr("Use as publish draft")
                                        toolTipText: qsTr("Use as publish draft")
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


            AppEmptyState {
                anchors.centerIn: parent
                width: Math.min(360, parent.width - 48)
                visible: root.streamEmpty
                ui: root.ui
                mode: root.connecting ? "loading" : "empty"
                title: root.emptyStateTitle
                description: root.emptyStateDescription
                actionLabel: root.emptyStateAction
                onActionTriggered: root.triggerEmptyStateAction()
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
                color: followMouse.containsMouse
                       ? root.ui.themePalette.buttonPrimaryHoverBg
                       : root.ui.themePalette.followBg
                border.color: root.ui.themePalette.followBorder
                opacity: visible ? 0.97 : 0
                z: 2
                scale: visible ? 1.0 : 0.96
                Accessible.role: Accessible.Button
                Accessible.name: eventList.unreadCount > 0
                                 ? qsTr("Scroll to latest, %1 unread").arg(eventList.unreadCount)
                                 : qsTr("Scroll to latest")

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
                        root.setFollowMode("smart")
                    }
                }
            }
        }
    }


    Popup {
        id: clearMessagesDialog

        anchors.centerIn: Overlay.overlay
        width: Math.min(390, root.width - 32)
        height: clearMessagesDialogContent.implicitHeight + 36
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        padding: 0

        Overlay.modal: AppDialogOverlay {
            ui: root.ui
        }

        background: Rectangle {
            radius: 10
            color: root.ui.themePalette.dialogBg
            border.color: root.ui.themePalette.dialogBorder
        }

        contentItem: ColumnLayout {
            id: clearMessagesDialogContent

            anchors.fill: parent
            anchors.margins: 18
            spacing: 14

            Label {
                Layout.fillWidth: true
                text: qsTr("Clear message history?")
                color: root.ui.textStrong
                font.pixelSize: 15
                font.bold: true
                wrapMode: Text.Wrap
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("This permanently removes all saved messages for the current connection.")
                color: root.ui.textMuted
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Item {
                    Layout.fillWidth: true
                }

                AppButton {
                    ui: root.ui
                    text: qsTr("Cancel")
                    minimumWidth: 78
                    onClicked: clearMessagesDialog.close()
                }

                AppButton {
                    ui: root.ui
                    text: qsTr("Clear")
                    minimumWidth: 78
                    danger: true
                    onClicked: {
                        root.eventHistory.clearCurrentMessages()
                        root.clearMessageSelection()
                        root.messagesCleared()
                        clearMessagesDialog.close()
                    }
                }
            }
        }
    }
}
