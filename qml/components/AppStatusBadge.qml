pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

Rectangle {
    id: control

    required property AppUi ui
    property string status: ""
    property string label: status || "idle"
    property color badgeBg: control.ui.statusFill(status)
    property color badgeBorder: control.ui.stateColor(status)
    property color badgeText: control.ui.textStrong
    property int badgeRadius: 11
    property int horizontalPadding: 9
    property int verticalPadding: 4
    property bool strong: true
    readonly property bool showBusy: status === "connecting"

    radius: badgeRadius
    color: badgeBg
    border.color: badgeBorder
    implicitHeight: statusBadgeRow.implicitHeight + verticalPadding * 2
    implicitWidth: statusBadgeRow.implicitWidth + horizontalPadding * 2

    Row {
        id: statusBadgeRow
        anchors.centerIn: parent
        spacing: control.showBusy ? 6 : 0

        AppSpinner {
            ui: control.ui
            implicitWidth: 15
            implicitHeight: 15
            visible: control.showBusy
            strokeColor: control.badgeText
            strokeWidth: 2.2
        }

        Label {
            text: control.ui.statusLabel(control.label)
            color: control.badgeText
            font.pixelSize: 11
            font.bold: control.strong
        }
    }
}
