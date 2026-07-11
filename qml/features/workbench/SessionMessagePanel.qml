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
            streamModel: root.viewModel.messages
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
        }

        PublishComposer {
            id: publishComposer
            ui: root.ui
            publisher: root.publisher
            publishStatus: root.publishStatus
            status: root.status
        }
    }
}
