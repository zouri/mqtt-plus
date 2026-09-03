pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

AppPanel {
    id: control

    required property var viewModel
    required property bool active

    readonly property var topicModel: control.viewModel ? control.viewModel.topicTree : null
    property double nowMs: Date.now()
    property string contextTopic: ""
    property bool contextIsTopic: false
    property bool contextHasChildren: false
    property string selectedTopic: ""
    property string selectedHistoryId: ""

    signal subscriptionCreateRequested(string topic)
    signal topicSelected(string topic, string historyId)
    signal topicSelectionCleared

    showTopBorder: false
    showRightBorder: false
    showBottomBorder: false
    showLeftBorder: false
    color: control.ui.themePalette.panelBg
    Accessible.name: qsTr("Topics")

    Layout.fillWidth: true
    Layout.fillHeight: true

    function subtreeFilter(topic) {
        return topic.length > 0 ? `${topic}/#` : "/#";
    }

    function normalizedHistoryId(historyId) {
        const value = String(historyId || "");
        return value === "0" ? "" : value;
    }

    function selectTopic(topic, historyId) {
        control.selectedTopic = topic;
        control.selectedHistoryId = control.normalizedHistoryId(historyId);
        control.topicSelected(control.selectedTopic, control.selectedHistoryId);
    }

    function clearSelection() {
        control.selectedTopic = "";
        control.selectedHistoryId = "";
        topicList.currentIndex = -1;
        control.topicSelectionCleared();
    }

    function prepareContextMenu(topic, isTopic, hasChildren) {
        control.contextTopic = topic;
        control.contextIsTopic = isTopic;
        control.contextHasChildren = hasChildren;
    }

    function openContextMenuAt(topic, isTopic, hasChildren, sourceItem, localX, localY) {
        control.prepareContextMenu(topic, isTopic, hasChildren);
        topicContextMenu.openAtPoint(sourceItem, localX, localY);
    }

    function openContextMenuForItem(topic, isTopic, hasChildren, anchorItem) {
        control.prepareContextMenu(topic, isTopic, hasChildren);
        topicContextMenu.openForItem(anchorItem);
    }

    Timer {
        interval: 1000
        repeat: true
        running: control.active && control.visible
        onTriggered: {
            control.nowMs = Date.now();
            if (control.selectedTopic.length === 0) {
                return;
            }
            const latestHistoryId = control.topicModel.latestHistoryIdForTopic(
                                        control.selectedTopic);
            if (latestHistoryId !== control.selectedHistoryId) {
                control.selectTopic(control.selectedTopic, latestHistoryId);
            }
        }
    }

    ListModel {
        id: topicContextActions

        ListElement { actionId: "subscribe" }
        ListElement { actionId: "subscribe-subtree" }
        ListElement { actionId: "copy" }
    }

    AppMenu {
        id: topicContextMenu

        ui: control.ui
        accessibleName: qsTr("Topic actions")
        model: topicContextActions
        actionText: actionId => {
            if (actionId === "subscribe") {
                return qsTr("Subscribe to topic");
            }
            if (actionId === "subscribe-subtree") {
                return qsTr("Subscribe to subtree");
            }
            return qsTr("Copy topic");
        }
        actionIcon: actionId => {
            if (actionId === "subscribe" || actionId === "subscribe-subtree") {
                return control.ui.materialIcon("plus");
            }
            return control.ui.materialIcon("content-copy");
        }
        actionEnabled: actionId => {
            if (actionId === "subscribe") {
                return control.contextIsTopic;
            }
            if (actionId === "subscribe-subtree") {
                return control.contextHasChildren;
            }
            return control.contextTopic.length > 0;
        }
        actionSeparatorBefore: actionId => actionId === "copy"

        onTriggered: actionId => {
            if (actionId === "subscribe") {
                control.subscriptionCreateRequested(control.contextTopic);
            } else if (actionId === "subscribe-subtree") {
                control.subscriptionCreateRequested(control.subtreeFilter(control.contextTopic));
            } else if (actionId === "copy") {
                control.viewModel.copyMessageTopic(control.contextTopic);
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Label {
            visible: control.topicModel && control.topicModel.truncated
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.bottomMargin: 6
            text: qsTr("Topic discovery is limited to 10,000 topics.")
            color: control.ui.themePalette.warningText
            font.pixelSize: 10
            wrapMode: Text.Wrap
        }

        Item {
            visible: !control.topicModel || control.topicModel.count === 0
            Layout.fillWidth: true
            Layout.fillHeight: true

            AppEmptyState {
                id: topicEmptyState

                anchors.centerIn: parent
                ui: control.ui
                iconSource: control.ui.materialIcon("topic")
                title: qsTr("No observed topics")
                description: qsTr("Subscribe to # or another wildcard to discover topics from incoming messages.")
                actionLabel: qsTr("Subscribe to #")
                actionButton.icon.source: control.ui.materialIcon("plus")
                actionButton.icon.color: topicEmptyState.actionButton.contentColor
                onActionTriggered: control.subscriptionCreateRequested("#")
            }
        }

        ListView {
            id: topicList

            visible: control.topicModel && control.topicModel.count > 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 8
            clip: true
            spacing: 3
            model: control.active ? control.topicModel : null
            reuseItems: true

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: Rectangle {
                id: topicDelegate

                required property int index
                required property string segment
                required property string fullTopic
                required property int depth
                required property bool isTopic
                required property bool hasChildren
                required property bool expanded
                required property double lastSeenMs
                required property double subtreeLastSeenMs
                required property string latestPayloadPreview
                required property string latestHistoryId
                required property string subtreeLatestHistoryId

                readonly property string displaySegment: topicDelegate.segment.length > 0
                                                         ? topicDelegate.segment
                                                         : qsTr("(empty level)")
                readonly property string displayPayloadPreview: topicDelegate.latestPayloadPreview
                                                                   .replace(/[\r\n\t]+/g, " ")
                                                                   .replace(/ {2,}/g, " ")
                                                                   .trim()
                readonly property string displayedHistoryId: control.normalizedHistoryId(
                                                                  topicDelegate.isTopic
                                                                  ? topicDelegate.latestHistoryId
                                                                  : topicDelegate.subtreeLatestHistoryId)
                readonly property bool selected: control.selectedTopic === topicDelegate.fullTopic
                readonly property bool recentlyActive: topicDelegate.subtreeLastSeenMs > 0
                                                       && control.nowMs >= topicDelegate.subtreeLastSeenMs
                                                       && control.nowMs - topicDelegate.subtreeLastSeenMs < 2000
                width: ListView.view.width
                implicitHeight: 32
                radius: control.ui.radiusSm
                color: topicDelegate.selected
                       ? control.ui.themePalette.selectedBg
                       : (topicRowHover.hovered
                          ? control.ui.themePalette.rowHover
                          : "transparent")
                border.color: topicDelegate.selected
                              ? control.ui.themePalette.selectedBorder
                              : "transparent"
                border.width: topicDelegate.selected ? 1 : 0
                activeFocusOnTab: true
                Accessible.role: Accessible.TreeItem
                Accessible.name: topicDelegate.fullTopic
                Accessible.description: topicDelegate.displayPayloadPreview

                onDisplayedHistoryIdChanged: {
                    if (topicDelegate.selected) {
                        control.selectTopic(topicDelegate.fullTopic,
                                            topicDelegate.displayedHistoryId);
                    }
                }

                function primaryAction() {
                    control.selectTopic(topicDelegate.fullTopic,
                                        topicDelegate.displayedHistoryId);
                    return true;
                }

                Keys.onPressed: event => {
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
                        event.accepted = topicDelegate.primaryAction();
                    } else if (event.key === Qt.Key_Right && topicDelegate.hasChildren && !topicDelegate.expanded) {
                        control.topicModel.toggleExpanded(topicDelegate.index);
                        event.accepted = true;
                    } else if (event.key === Qt.Key_Left && topicDelegate.hasChildren && topicDelegate.expanded) {
                        control.topicModel.toggleExpanded(topicDelegate.index);
                        event.accepted = true;
                    } else if (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && event.modifiers & Qt.ShiftModifier)) {
                        control.openContextMenuForItem(topicDelegate.fullTopic,
                                                       topicDelegate.isTopic,
                                                       topicDelegate.hasChildren,
                                                       topicDelegate);
                        event.accepted = true;
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: mouse => {
                        topicList.currentIndex = topicDelegate.index;
                        topicDelegate.forceActiveFocus();
                        if (mouse.button === Qt.LeftButton) {
                            topicDelegate.primaryAction();
                        } else if (mouse.button === Qt.RightButton) {
                            control.openContextMenuAt(topicDelegate.fullTopic,
                                                      topicDelegate.isTopic,
                                                      topicDelegate.hasChildren,
                                                      topicDelegate,
                                                      mouse.x,
                                                      mouse.y);
                        }
                    }
                }

                HoverHandler {
                    id: topicRowHover
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 6 + Math.min(topicDelegate.depth, 12) * 14
                    anchors.rightMargin: 6
                    spacing: 4

                    AppIconButton {
                        id: topicExpandButton

                        ui: control.ui
                        visible: topicDelegate.hasChildren
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                        iconSource: control.ui.materialIcon(topicDelegate.expanded ? "chevron-down" : "chevron-right")
                        iconSize: 12
                        cornerRadius: 5
                        restBg: "transparent"
                        hoverBg: control.ui.themePalette.rowHover
                        pressedBg: control.ui.themePalette.actionPressedBg
                        outlineColor: "transparent"
                        symbolColor: control.ui.textMuted
                        accessibleName: topicDelegate.expanded ? qsTr("Collapse topic") : qsTr("Expand topic")
                        onClicked: control.topicModel.toggleExpanded(topicDelegate.index)
                    }

                    Item {
                        visible: !topicDelegate.hasChildren
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                        Accessible.ignored: true
                    }

                    Rectangle {
                        Layout.preferredWidth: 6
                        Layout.preferredHeight: 6
                        radius: 3
                        color: topicDelegate.recentlyActive
                               ? control.ui.stateColor("connected")
                               : control.ui.withAlpha(control.ui.textMuted, 0.35)
                        Accessible.ignored: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 0

                        Label {
                            Layout.maximumWidth: Math.max(72, parent.width * 0.55)
                            text: topicDelegate.displaySegment
                            color: control.ui.textStrong
                            font.pixelSize: 12
                            font.bold: topicDelegate.depth === 0
                            maximumLineCount: 1
                            wrapMode: Text.NoWrap
                            elide: Label.ElideRight
                        }

                        Label {
                            visible: topicDelegate.displayPayloadPreview.length > 0
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            text: qsTr(" = %1").arg(topicDelegate.displayPayloadPreview)
                            color: control.ui.themePalette.textSubtle
                            font.pixelSize: 11
                            maximumLineCount: 1
                            wrapMode: Text.NoWrap
                            elide: Label.ElideRight
                        }

                        Item {
                            visible: topicDelegate.displayPayloadPreview.length === 0
                            Layout.fillWidth: true
                        }
                    }

                    AppIconButton {
                        id: topicMenuButton

                        ui: control.ui
                        Layout.preferredWidth: 24
                        Layout.preferredHeight: 24
                        iconSource: control.ui.materialIcon("more-horiz")
                        iconSize: 16
                        cornerRadius: 12
                        restBg: "transparent"
                        hoverBg: "transparent"
                        pressedBg: "transparent"
                        outlineColor: "transparent"
                        symbolColor: topicMenuButton.hovered
                                     ? control.ui.themePalette.infoText
                                     : control.ui.textMuted
                        accessibleName: qsTr("Topic actions")
                        toolTipText: accessibleName
                        onClicked: control.openContextMenuForItem(topicDelegate.fullTopic,
                                                                  topicDelegate.isTopic,
                                                                  topicDelegate.hasChildren,
                                                                  this)
                    }
                }

                AppToolTip {
                    ui: control.ui
                    text: topicDelegate.fullTopic
                    position: AppToolTip.Position.Top
                    active: topicRowHover.hovered
                }
            }
        }
    }
}
