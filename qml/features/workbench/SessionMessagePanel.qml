pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

AppPanel {
    id: root

    required property var viewModel
    required property bool active
    required property var publisher
    required property var eventHistory
    required property var sessionService
    required property var session
    required property var status
    required property var publishStatus
    required property string fontFamily
    required property int messagePayloadDisplayMode
    required property int autoFollowFps
    property bool topicExplorerMode: false
    property string selectedTopic: ""
    property string selectedTopicHistoryId: ""
    property string selectedMessageHistoryId: ""
    property string inspectorSessionId: ""
    property bool inspectorOpened: false
    property alias composerHeight: publishComposer.composerHeight

    signal subscriptionCreateRequested
    signal draftsManageRequested

    showTopBorder: false
    showLeftBorder: false
    showRightBorder: false
    showBottomBorder: false
    color: root.ui.themePalette.panelBg
    Layout.fillWidth: true
    Layout.fillHeight: true

    function resetStreamPosition() {
        eventStreamView.resetStreamPosition();
    }

    function noteStreamRowsAppended(count) {
        eventStreamView.noteStreamRowsAppended(count);
    }

    function focusMessageSearch() {
        eventStreamView.focusSearch();
    }

    function closeInspector() {
        root.inspectorOpened = false;
        root.selectedMessageHistoryId = "";
        root.inspectorSessionId = "";
        eventStreamView.clearMessageSelection();
    }

    onInspectorOpenedChanged: {
        if (root.inspectorOpened) {
            messageInspectorPopup.open();
        } else {
            messageInspectorPopup.close();
        }
    }

    onTopicExplorerModeChanged: {
        if (root.topicExplorerMode) {
            root.closeInspector();
        }
    }

    Connections {
        target: root.viewModel

        function onCurrentSessionChanged() {
            const currentSession = root.viewModel.currentSession || ({});
            const currentSessionId = String(currentSession.id || "");
            if (root.inspectorOpened && currentSessionId !== root.inspectorSessionId) {
                root.closeInspector();
            }
        }
    }

    SplitView {
        id: messageSplit

        anchors.fill: parent
        orientation: Qt.Vertical

        handle: Item {
            implicitWidth: messageSplit.width
            implicitHeight: 6
            z: 2

            Rectangle {
                anchors.fill: parent
                Accessible.ignored: true
                gradient: Gradient {
                    orientation: Gradient.Vertical

                    GradientStop {
                        position: 0.0
                        color: "transparent"
                    }

                    GradientStop {
                        position: 1.0
                        color: splitHandleHover.hovered
                               ? (root.ui.isDarkTheme ? "#47000000" : "#1c000000")
                               : (root.ui.isDarkTheme ? "#33000000" : "#12000000")
                    }
                }
            }

            HoverHandler {
                id: splitHandleHover
                cursorShape: Qt.SplitVCursor
            }
        }

        StackLayout {
            id: primaryContent

            currentIndex: root.topicExplorerMode ? 1 : 0
            SplitView.fillWidth: true
            SplitView.fillHeight: true

            EventStreamView {
                id: eventStreamView
                ui: root.ui
                active: root.active && !root.topicExplorerMode
                viewModel: root.viewModel
                publisher: root.publisher
                eventHistory: root.eventHistory
                sessionService: root.sessionService
                streamModel: root.viewModel.filteredMessages
                session: root.session
                status: root.status
                fontFamily: root.fontFamily
                payloadDisplayMode: root.messagePayloadDisplayMode
                autoFollowFps: root.autoFollowFps
                title: qsTr("Messages")
                showOutputControls: true
                bottomVisualOverflow: 6
                Layout.fillWidth: true
                Layout.fillHeight: true
                onPublishDraftRevealRequested: {
                    publishComposer.revealDraftEditor();
                }
                onMessageSelected: historyId => {
                    root.selectedMessageHistoryId = historyId;
                    root.inspectorSessionId = String(root.session.id || "");
                    root.inspectorOpened = true;
                }
                onMessagesCleared: root.closeInspector()
                onSubscriptionCreateRequested: root.subscriptionCreateRequested()
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                AppEmptyState {
                    anchors.centerIn: parent
                    visible: root.selectedTopicHistoryId.length === 0
                    ui: root.ui
                    iconSource: root.ui.materialIcon("topic")
                    title: root.selectedTopic.length > 0
                           ? qsTr("No message for this topic")
                           : qsTr("Select a topic")
                    description: root.selectedTopic.length > 0
                                 ? qsTr("A message will appear here when the topic receives a value.")
                                 : qsTr("Choose a topic in the tree to inspect its latest message.")
                }

                MessageInspector {
                    anchors.fill: parent
                    visible: root.selectedTopicHistoryId.length > 0
                    ui: root.ui
                    viewModel: root.viewModel
                    historyId: root.selectedTopicHistoryId
                    payloadFormatContext: root.selectedTopic
                    opened: root.selectedTopicHistoryId.length > 0
                    embedded: true
                    onDraftUsed: publishComposer.revealDraftEditor()
                }
            }
        }

        PublishComposer {
            id: publishComposer
            z: 1
            ui: root.ui
            publisher: root.publisher
            publishStatus: root.publishStatus
            status: root.status
            fontFamily: root.fontFamily
            onManageDraftsRequested: root.draftsManageRequested()
        }
    }

    Popup {
        id: messageInspectorPopup

        parent: root
        x: root.width - width
        y: 0
        width: Math.min(400, root.width * 0.88)
        height: root.height
        padding: 0
        modal: false
        dim: false
        closePolicy: Popup.CloseOnPressOutside
        background: Item {}
        exit: Transition {
            PauseAnimation {
                duration: root.ui.animationsEnabled
                          ? root.ui.motionPanelDuration
                          : 0
            }
        }
        onAboutToHide: {
            if (root.inspectorOpened) {
                root.closeInspector();
            }
        }

        contentItem: MessageInspector {
            id: messageInspector

            ui: root.ui
            viewModel: root.viewModel
            historyId: root.selectedMessageHistoryId
            opened: root.inspectorOpened
            width: messageInspectorPopup.availableWidth
            height: messageInspectorPopup.availableHeight
            onCloseRequested: root.closeInspector()
            onDraftUsed: {
                publishComposer.revealDraftEditor();
                root.closeInspector();
            }
        }
    }
}
