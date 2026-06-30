pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import "../../components"

AppPanel {
    id: root

    required property var viewModel
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

    function noteStreamRowAppended(row) {
        eventStreamView.noteStreamRowAppended(row);
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        EventStreamView {
            id: eventStreamView
            ui: root.ui
            viewModel: root.viewModel
            streamModel: root.viewModel.messages
            loadOlderRows: function () {
                return root.viewModel.loadOlderCurrentSessionMessages();
            }
            clearRows: function () {
                root.viewModel.clearCurrentMessages();
            }
            session: root.session
            fontFamily: root.fontFamily
            title: qsTr("Messages")
            showOutputControls: true
            onPublishDraftRequested: (topic, payload, format) => {
                publishComposer.setDraft(topic, payload, format);
            }
        }

        PublishComposer {
            id: publishComposer
            ui: root.ui
            viewModel: root.viewModel
            publishStatus: root.publishStatus
            status: root.status
        }
    }
}
