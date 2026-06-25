pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import "../../components"

AppPanel {
    id: root

    required property var messages
    required property var connection
    required property var subscriptions
    required property var session
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
            messages: root.messages
            connection: root.connection
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
            connection: root.connection
            subscriptions: root.subscriptions
        }
    }
}
