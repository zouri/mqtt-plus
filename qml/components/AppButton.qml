pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

Button {
    id: control

    required property AppUi ui
    property bool primary: false
    property bool danger: false
    property int minimumWidth: 92
    property string toolTipText: ""
    property int toolTipPosition: AppToolTip.Position.Right
    readonly property bool focusIndicatorVisible: control.ui.showFocusIndicators && control.activeFocus

    implicitHeight: control.ui.compactControlHeight
    implicitWidth: Math.max(minimumWidth, contentItem.implicitWidth + leftPadding + rightPadding)
    leftPadding: 14
    rightPadding: 14
    font.pixelSize: control.ui.compactFontSize

    HoverHandler {
        cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    background: Rectangle {
        radius: control.ui.radiusMd
        color: !control.enabled
               ? control.ui.themePalette.disabledButtonBg
               : (control.down
                  ? (control.danger
                     ? control.ui.themePalette.buttonDangerPressedBg
                     : (control.primary ? control.ui.themePalette.buttonPrimaryPressedBg : control.ui.themePalette.buttonPressedBg))
                  : (control.hovered
                     ? (control.danger
                        ? control.ui.themePalette.buttonDangerHoverBg
                        : (control.primary ? control.ui.themePalette.buttonPrimaryHoverBg : control.ui.themePalette.buttonHoverBg))
                     : (control.danger
                        ? control.ui.themePalette.buttonDangerBg
                        : (control.primary ? control.ui.themePalette.buttonPrimaryBg : control.ui.themePalette.buttonBg))))
        border.color: control.focusIndicatorVisible ? control.ui.focusRingColor : (control.primary || control.danger ? "transparent" : control.ui.themePalette.buttonBorder)
        border.width: control.focusIndicatorVisible ? control.ui.focusRingWidth : 0

        Behavior on color {
            ColorAnimation {
                duration: 120
                easing.type: Easing.OutCubic
            }
        }
    }

    contentItem: Label {
        text: control.text
        color: !control.enabled
               ? control.ui.themePalette.disabledText
               : (control.danger
                  ? control.ui.themePalette.buttonDangerText
                  : (control.primary ? control.ui.themePalette.buttonPrimaryText : control.ui.textStrong))
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: control.font.pixelSize
        font.bold: true
    }

    AppToolTip {
        ui: control.ui
        text: control.toolTipText
        position: control.toolTipPosition
        active: control.hovered
    }
}
