pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

Rectangle {
    id: control

    required property AppUi ui
    property string sessionName: ""
    property string sessionState: ""

    radius: 11
    implicitHeight: 30
    implicitWidth: sessionPillRow.implicitWidth + 20
    color: control.ui.themePalette.accentPanelBg
    border.color: control.ui.themePalette.accentPanelBorder

    Row {
        id: sessionPillRow
        anchors.centerIn: parent
        spacing: 8

        Rectangle {
            width: 8
            height: 8
            radius: 4
            color: control.ui.stateColor(control.sessionState)
        }

        Label {
            text: control.sessionName.length > 0 ? control.sessionName : "No session"
            color: control.ui.textStrong
            font.pixelSize: 12
            font.bold: true
        }

        Label {
            text: control.sessionState || "idle"
            color: control.ui.textMuted
            font.pixelSize: 11
        }
    }
}
