pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

TextField {
    id: control

    required property AppUi ui
    readonly property color hoverBorderColor: control.ui.isDarkTheme
                                                 ? Qt.lighter(control.ui.themePalette.fieldBorder, 1.25)
                                                 : Qt.darker(control.ui.themePalette.fieldBorder, 1.12)
    implicitHeight: control.ui.compactControlHeight + 4
    leftPadding: 12
    rightPadding: 12
    font.pixelSize: control.ui.compactFontSize
    color: control.ui.textStrong
    placeholderTextColor: control.ui.themePalette.fieldPlaceholder
    selectByMouse: true
    hoverEnabled: true

    ContextMenu.menu: AppNativeTextMenu {
        editor: control
    }

    background: Rectangle {
        radius: 8
        color: control.ui.themePalette.fieldBg
        border.color: !control.enabled
                      ? control.ui.themePalette.fieldBorder
                      : (control.activeFocus
                         ? control.ui.themePalette.selectedBorder
                         : (control.hovered
                            ? control.hoverBorderColor
                            : control.ui.themePalette.fieldBorder))

        Behavior on border.color {
            enabled: control.ui.animationsEnabled

            ColorAnimation {
                duration: control.ui.motionMicroDuration
                easing.type: control.ui.motionEnterEasing
            }
        }
    }
}
