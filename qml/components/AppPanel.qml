pragma ComponentBehavior: Bound

import QtQuick

Rectangle {
    id: control

    required property AppUi ui
    property bool inner: false
    property bool showTopBorder: true
    property bool showRightBorder: true
    property bool showBottomBorder: true
    property bool showLeftBorder: true
    readonly property color panelBorderColor: inner ? control.ui.themePalette.innerPanelBorder : control.ui.panelBorder
    default property alias panelContent: panelContentItem.data

    radius: inner ? control.ui.innerRadius : control.ui.panelRadius
    color: inner ? control.ui.themePalette.innerPanelBg : control.ui.themePalette.windowBg
    border.width: 0

    Item {
        id: panelContentItem
        anchors.fill: parent
    }

    Rectangle {
        visible: control.showTopBorder
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: control.panelBorderColor
    }

    Rectangle {
        visible: control.showRightBorder
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: 1
        color: control.panelBorderColor
    }

    Rectangle {
        visible: control.showBottomBorder
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: control.panelBorderColor
    }

    Rectangle {
        visible: control.showLeftBorder
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: 1
        color: control.panelBorderColor
    }
}
