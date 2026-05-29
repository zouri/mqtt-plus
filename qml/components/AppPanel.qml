pragma ComponentBehavior: Bound

import QtQuick

Rectangle {
    id: control

    required property AppUi ui
    property bool inner: false
    default property alias panelContent: panelContentItem.data

    radius: inner ? control.ui.innerRadius : control.ui.panelRadius
    color: inner ? control.ui.themePalette.innerPanelBg : control.ui.panelBg
    border.color: inner ? control.ui.themePalette.innerPanelBorder : control.ui.panelBorder

    Item {
        id: panelContentItem
        anchors.fill: parent
    }
}
