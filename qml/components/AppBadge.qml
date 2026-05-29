pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

Rectangle {
    id: control

    required property AppUi ui
    property string label: ""
    property color badgeBg: control.ui.themePalette.chipBg
    property color badgeBorder: control.ui.themePalette.innerPanelBorder
    property color badgeText: control.ui.themePalette.chipText
    property int badgeRadius: 10
    property int horizontalPadding: 14
    property int verticalPadding: 6
    property bool strong: true

    radius: badgeRadius
    color: badgeBg
    border.color: badgeBorder
    implicitHeight: badgeLabel.implicitHeight + verticalPadding * 2
    implicitWidth: badgeLabel.implicitWidth + horizontalPadding * 2

    Label {
        id: badgeLabel
        anchors.centerIn: parent
        text: control.label
        color: control.badgeText
        font.pixelSize: 11
        font.bold: control.strong
    }
}
