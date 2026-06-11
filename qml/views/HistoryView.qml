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
        status: root.appController.sessionStatus
        fontFamily: root.fontFamily
    }

    function resetStreamPosition() {
        logPage.resetStreamPosition()
    }

    function noteStreamRowAppended(row) {
        logPage.noteStreamRowAppended(row)
    }
}
