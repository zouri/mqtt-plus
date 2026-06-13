pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import "../features/events"

Item {
    id: root

    required property AppUi ui
    required property var appController
    required property string fontFamily

    Layout.fillWidth: true
    Layout.fillHeight: true

    LogPage {
        id: logPage

        anchors.fill: parent
        ui: root.ui
        appController: root.appController
        session: root.appController.currentSession
        fontFamily: root.fontFamily
    }

    function resetStreamPosition() {
        logPage.resetStreamPosition()
    }

    function noteStreamRowAppended(row) {
        logPage.noteStreamRowAppended(row)
    }

    Component.onCompleted: {
        root.resetStreamPosition()
    }

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
}
