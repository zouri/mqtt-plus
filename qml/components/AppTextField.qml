pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

TextField {
    id: control

    required property AppUi ui
    readonly property bool focusIndicatorVisible: control.ui.showFocusIndicators && control.activeFocus

    implicitHeight: control.ui.compactControlHeight + 4
    leftPadding: 12
    rightPadding: 12
    font.pixelSize: control.ui.compactFontSize
    color: control.ui.textStrong
    placeholderTextColor: control.ui.themePalette.fieldPlaceholder
    selectByMouse: true

    background: Rectangle {
        radius: 8
        color: control.ui.themePalette.fieldBg
        border.color: control.focusIndicatorVisible ? control.ui.themePalette.fieldFocusBorder : control.ui.themePalette.fieldBorder

        Behavior on border.color {
            ColorAnimation {
                duration: 120
                easing.type: Easing.OutCubic
            }
        }
    }
}
