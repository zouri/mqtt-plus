pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import "../../components"

AppPanel {
    id: root

    required property var workbench
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
            workbench: root.workbench
            streamModel: root.workbench.messages
            loadOlderRows: function () {
                return root.workbench.loadOlderCurrentSessionMessages();
            }
            clearRows: function () {
                root.workbench.clearCurrentMessages();
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
            workbench: root.workbench
            publishStatus: root.publishStatus
            status: root.status
        }
    }
}
