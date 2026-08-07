pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

Button {
    id: control

    required property AppUi ui
    property bool primary: false
    property bool danger: false
    property bool outlined: false
    property int minimumWidth: 92
    property string toolTipText: ""
    property int toolTipPosition: AppToolTip.Position.Right
    readonly property color contentColor: !control.enabled
                                          ? control.ui.themePalette.disabledText
                                          : (control.outlined && control.danger
                                             ? control.ui.themePalette.errorText
                                             : (control.danger
                                                ? control.ui.themePalette.buttonDangerText
                                                : (control.primary
                                                   ? control.ui.themePalette.buttonPrimaryText
                                                   : control.ui.textStrong)))
    readonly property color restBackgroundColor: control.outlined
                                                  ? control.ui.themePalette.buttonBg
                                                  : (control.danger
                                                     ? control.ui.themePalette.buttonDangerBg
                                                     : (control.primary
                                                        ? control.ui.themePalette.buttonPrimaryBg
                                                        : control.ui.themePalette.buttonBg))
    readonly property color hoverBackgroundColor: control.outlined && control.danger
                                                   ? control.ui.themePalette.errorBg
                                                   : (control.danger
                                                      ? control.ui.themePalette.buttonDangerHoverBg
                                                      : (control.primary
                                                         ? control.ui.themePalette.buttonPrimaryHoverBg
                                                         : control.ui.themePalette.buttonHoverBg))
    readonly property color pressedBackgroundColor: control.danger
                                                     ? control.ui.themePalette.buttonDangerPressedBg
                                                     : (control.primary
                                                        ? control.ui.themePalette.buttonPrimaryPressedBg
                                                        : control.ui.themePalette.buttonPressedBg)
    implicitHeight: control.ui.compactControlHeight
    implicitWidth: Math.max(minimumWidth, contentItem.implicitWidth + leftPadding + rightPadding)
    leftPadding: 14
    rightPadding: 14
    font.pixelSize: control.ui.compactFontSize
    font.bold: true
    palette.buttonText: control.contentColor
    palette.text: control.contentColor
    display: control.icon.source.toString().length > 0
             ? AbstractButton.TextBesideIcon
             : AbstractButton.TextOnly

    HoverHandler {
        cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    background: Rectangle {
        radius: control.ui.radiusMd
        color: !control.enabled
               ? control.ui.themePalette.disabledButtonBg
               : (control.down
                  ? control.pressedBackgroundColor
                  : (control.hovered ? control.hoverBackgroundColor : control.restBackgroundColor))
        border.color: control.outlined
                      ? (control.enabled && control.danger
                         ? control.ui.themePalette.errorText
                         : control.ui.themePalette.buttonBorder)
                      : "transparent"
        border.width: control.outlined ? 1 : 0

        Behavior on color {
            enabled: control.ui.animationsEnabled

            ColorAnimation {
                duration: control.ui.motionMicroDuration
                easing.type: control.ui.motionEnterEasing
            }
        }
    }

    Loader {
        anchors.fill: parent
        active: control.hovered && control.toolTipText.length > 0

        sourceComponent: Component {
            AppToolTip {
                ui: control.ui
                text: control.toolTipText
                position: control.toolTipPosition
                active: true
            }
        }
    }
}
