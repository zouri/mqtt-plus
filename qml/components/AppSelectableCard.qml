pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: control

    required property AppUi ui
    property bool selected: false
    property bool hovered: false
    property int selectedOffset: 6
    property int cornerRadius: 8
    default property alias contentData: contentContainer.data

    Rectangle {
        anchors.fill: parent
        radius: control.cornerRadius
        color: control.selected
               ? control.ui.themePalette.selectedBorder
               : control.ui.themePalette.itemBg
        border.color: control.selected
                      ? control.ui.themePalette.selectedBorder
                      : (control.hovered
                         ? control.ui.themePalette.fieldBorder
                         : control.ui.themePalette.itemBorder)
        border.width: 1
    }

    Rectangle {
        id: foreground

        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: control.selected ? control.selectedOffset : 0
        radius: control.cornerRadius
        color: control.hovered
               ? control.ui.themePalette.rowHover
               : control.ui.themePalette.itemBg
        border.color: control.selected
                      ? control.ui.themePalette.selectedBorder
                      : (control.hovered
                         ? control.ui.themePalette.fieldBorder
                         : control.ui.themePalette.itemBorder)
        border.width: 1

        Item {
            id: contentContainer

            anchors.fill: parent
        }
    }
}
