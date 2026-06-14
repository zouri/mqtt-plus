pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import "../../components"
import "../streams"

AppPanel {
    id: root

    required property var appController
    required property string fontFamily

    showTopBorder: false
    Layout.fillWidth: true
    Layout.fillHeight: true

    function resetStreamPosition() {
        logStreamView.resetStreamPosition()
    }

    function noteStreamRowAppended(row) {
        logStreamView.noteStreamRowAppended(row)
    }

    Component.onCompleted: root.resetStreamPosition()

    Connections {
        target: root.appController

        function onMessageStreamChanged() {
            root.resetStreamPosition()
        }

        function onLogStreamChanged() {
            root.resetStreamPosition()
        }

        function onLogStreamRowAppended(row) {
            root.noteStreamRowAppended(row)
        }
    }

    EventStreamView {
        id: logStreamView

        anchors.fill: parent
        anchors.margins: 14
        ui: root.ui
        appController: root.appController
        streamModel: root.appController.logs
        loadOlderRows: function() { return root.appController.loadOlderCurrentSessionLogs() }
        clearRows: function() { root.appController.clearCurrentLogs() }
        session: root.appController.currentSession
        fontFamily: root.fontFamily
        title: qsTr("Log")
    }
}
