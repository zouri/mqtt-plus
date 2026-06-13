pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

Button {
    id: control

    required property AppUi ui
    property bool primary: false
    property bool danger: false
    property int minimumWidth: 92

    implicitHeight: control.ui.compactControlHeight
    implicitWidth: Math.max(minimumWidth, contentItem.implicitWidth + leftPadding + rightPadding)
    leftPadding: 14
    rightPadding: 14
    font.pixelSize: control.ui.compactFontSize

    HoverHandler {
        cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    background: Rectangle {
        radius: Math.round(height / 2)
        color: !control.enabled
               ? Qt.rgba((control.primary || control.danger) ? 0.55 : 0.5,
                         (control.primary || control.danger) ? 0.6 : 0.55,
                         (control.primary || control.danger) ? 0.7 : 0.6,
                         control.ui.isDarkTheme ? 0.18 : 0.28)
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
        border.color: control.primary || control.danger ? "transparent" : control.ui.themePalette.buttonBorder

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
               ? Qt.rgba(control.ui.isDarkTheme ? 0.82 : 0.28,
                         control.ui.isDarkTheme ? 0.86 : 0.34,
                         control.ui.isDarkTheme ? 0.92 : 0.42,
                         control.ui.isDarkTheme ? 0.42 : 0.52)
               : (control.danger
                  ? control.ui.themePalette.buttonDangerText
                  : (control.primary ? control.ui.themePalette.buttonPrimaryText : control.ui.textStrong))
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: control.font.pixelSize
        font.bold: control.primary || control.danger
    }
}
