pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

AppPanel {
    id: root

    required property var viewModel
    required property var publisher
    required property var session
    required property var status
    required property var publishStatus
    required property string fontFamily
    property string selectedMessageHistoryId: ""
    property bool inspectorOpened: false
    property alias composerHeight: publishComposer.composerHeight

    signal subscriptionCreateRequested

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
        eventStreamView.clearMessageSelection();
    }

    SplitView {
        id: messageSplit

        anchors.fill: parent
        orientation: Qt.Vertical

        handle: Item {
            implicitWidth: messageSplit.width
            implicitHeight: 6

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                height: 1
                color: root.ui.themePalette.separator
            }

            HoverHandler {
                id: splitHandleHover
                cursorShape: Qt.SplitVCursor
            }
        }

        EventStreamView {
            id: eventStreamView
            ui: root.ui
            viewModel: root.viewModel
            publisher: root.publisher
            streamModel: root.viewModel.filteredMessages
            session: root.session
            status: root.status
            fontFamily: root.fontFamily
            title: qsTr("Messages")
            showOutputControls: true
            SplitView.fillWidth: true
            SplitView.fillHeight: true
            onPublishDraftRevealRequested: {
                publishComposer.revealDraftEditor();
            }
            onMessageSelected: historyId => {
                root.selectedMessageHistoryId = historyId;
                root.inspectorOpened = true;
            }
            onMessagesCleared: root.closeInspector()
            onSubscriptionCreateRequested: root.subscriptionCreateRequested()
        }

        PublishComposer {
            id: publishComposer
            ui: root.ui
            publisher: root.publisher
            publishStatus: root.publishStatus
            status: root.status
        }
    }

    MessageInspector {
        id: messageInspector

        ui: root.ui
        viewModel: root.viewModel
        historyId: root.selectedMessageHistoryId
        opened: root.inspectorOpened
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        z: 10
        onCloseRequested: root.closeInspector()
        onDraftUsed: {
            publishComposer.revealDraftEditor();
            root.closeInspector();
        }
    }
}
