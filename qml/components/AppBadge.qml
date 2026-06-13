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
    property int maximumLabelWidth: 0
    readonly property real effectiveLabelWidth: control.maximumLabelWidth > 0
                                                ? Math.min(badgeLabel.implicitWidth, control.maximumLabelWidth)
                                                : badgeLabel.implicitWidth

    radius: badgeRadius
    color: badgeBg
    border.color: badgeBorder
    implicitHeight: badgeLabel.implicitHeight + verticalPadding * 2
    implicitWidth: control.effectiveLabelWidth + horizontalPadding * 2

    Label {
        id: badgeLabel
        anchors.centerIn: parent
        width: control.effectiveLabelWidth
        text: control.label
        color: control.badgeText
        elide: Label.ElideRight
        font.pixelSize: 11
        font.bold: control.strong
    }
}
