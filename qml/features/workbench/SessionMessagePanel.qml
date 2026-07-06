pragma ComponentBehavior: Bound

import QtQuick
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
    Layout.fillWidth: true
    Layout.fillHeight: true

    function resetStreamPosition() {
        eventStreamView.resetStreamPosition();
    }

    function noteStreamRowsAppended(count) {
        eventStreamView.noteStreamRowsAppended(count);
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        EventStreamView {
            id: eventStreamView
            ui: root.ui
            viewModel: root.viewModel
            publisher: root.publisher
            streamModel: root.viewModel.messages
            session: root.session
            fontFamily: root.fontFamily
            title: qsTr("Messages")
            showOutputControls: true
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
