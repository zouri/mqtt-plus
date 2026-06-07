pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import "../../components"
import "../publish"

AppPanel {
    id: root

    required property var appController
    required property var session
    required property var status
    required property var publishStatus
    required property string fontFamily

    showTopBorder: false
    Layout.fillWidth: true
    Layout.fillHeight: true

    function resetStreamPosition() {
        eventStreamView.resetStreamPosition()
    }

    function noteStreamRowAppended(row) {
        eventStreamView.noteStreamRowAppended(row)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        EventStreamView {
            id: eventStreamView
            ui: root.ui
            appController: root.appController
            session: root.session
            status: root.status
            fontFamily: root.fontFamily
            streamKind: "message"
            onPublishDraftRequested: (topic, payload, format) => {
                publishComposer.setDraft(topic, payload, format)
            }
        }

        PublishComposer {
            id: publishComposer
            ui: root.ui
            appController: root.appController
            publishStatus: root.publishStatus
            status: root.status
        }
    }
}
