pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

CheckBox {
    id: control

    required property AppUi ui

    implicitHeight: control.ui.compactCheckHeight
    font.pixelSize: control.ui.compactFontSize
    spacing: 8

    HoverHandler {
        cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    indicator: Rectangle {
        implicitWidth: 18
        implicitHeight: 18
        x: control.leftPadding
        y: (control.height - height) / 2
        radius: 5
        color: control.checked ? control.ui.themePalette.buttonPrimaryBg : control.ui.themePalette.fieldBg
        border.color: control.checked ? "transparent" : control.ui.themePalette.fieldBorder

        Label {
            anchors.centerIn: parent
            visible: control.checked
            text: "✓"
            color: control.ui.themePalette.buttonPrimaryText
            font.pixelSize: 11
            font.bold: true
        }
    }

    contentItem: Label {
        text: control.text
        color: control.ui.textStrong
        font.pixelSize: control.font.pixelSize
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
    }
}
