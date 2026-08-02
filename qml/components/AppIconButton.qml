pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

ToolButton {
    id: control

    required property AppUi ui
    property string symbol: ""
    property url iconSource: ""
    property int symbolSize: 16
    property int iconSize: symbolSize
    property int cornerRadius: ui.radiusMd
    property bool primary: false
    property bool danger: false
    property string accessibleName: ""
    property color restBg: control.danger
                           ? control.ui.themePalette.buttonDangerBg
                           : (control.primary ? control.ui.themePalette.buttonPrimaryBg : control.ui.themePalette.innerPanelBg)
    property color hoverBg: control.danger
                            ? control.ui.themePalette.buttonDangerHoverBg
                            : (control.primary ? control.ui.themePalette.buttonPrimaryHoverBg : control.ui.themePalette.actionHoverBg)
    property color pressedBg: control.danger
                              ? control.ui.themePalette.buttonDangerPressedBg
                              : (control.primary ? control.ui.themePalette.buttonPrimaryPressedBg : control.ui.themePalette.actionPressedBg)
    property color outlineColor: control.primary || control.danger ? "transparent" : control.ui.themePalette.innerPanelBorder
    property color symbolColor: control.danger
                                ? control.ui.themePalette.buttonDangerText
                                : (control.primary ? control.ui.themePalette.buttonPrimaryText : control.ui.textStrong)
    property bool forceActive: false
    property string toolTipText: ""
    property int toolTipPosition: AppToolTip.Position.Right
    readonly property color disabledBg: control.ui.themePalette.disabledButtonBg
    readonly property color disabledSymbolColor: control.ui.themePalette.disabledText
    readonly property color effectiveSymbolColor: control.enabled
                                                  ? control.symbolColor
                                                  : control.disabledSymbolColor
    readonly property color effectiveRestBg: control.restBg.a === 0
                                           ? Qt.rgba(control.hoverBg.r,
                                                     control.hoverBg.g,
                                                     control.hoverBg.b,
                                                     0)
                                           : control.restBg
    implicitWidth: 30
    implicitHeight: 30
    padding: 0
    scale: control.down ? 0.97 : 1.0
    text: control.symbol
    display: control.iconSource.toString().length > 0 ? AbstractButton.IconOnly : AbstractButton.TextOnly
    icon.source: control.iconSource
    icon.width: control.iconSize
    icon.height: control.iconSize
    icon.color: control.effectiveSymbolColor
    font.pixelSize: control.symbolSize
    font.bold: true
    Accessible.name: control.accessibleName.length > 0 ? control.accessibleName : control.symbol

    HoverHandler {
        cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    Behavior on scale {
        enabled: control.ui.animationsEnabled

        NumberAnimation {
            duration: 110
            easing.type: Easing.OutCubic
        }
    }

    background: Rectangle {
        radius: control.cornerRadius
        color: !control.enabled
               ? control.disabledBg
               : (control.down
               ? control.pressedBg
               : ((control.hovered || control.forceActive) ? control.hoverBg : control.effectiveRestBg))
        border.color: control.outlineColor
        border.width: 0

        Behavior on color {
            enabled: control.ui.animationsEnabled

            ColorAnimation {
                duration: 120
                easing.type: Easing.OutCubic
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
