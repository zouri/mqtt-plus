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
    property int cornerRadius: 10
    property bool primary: false
    property bool danger: false
    property string toolTipText: ""
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
    readonly property color effectiveRestBg: control.restBg.a === 0
                                           ? Qt.rgba(control.hoverBg.r,
                                                     control.hoverBg.g,
                                                     control.hoverBg.b,
                                                     0)
                                           : control.restBg

    implicitWidth: 30
    implicitHeight: 30
    padding: 0
    scale: control.down ? 0.95 : 1.0
    text: control.symbol
    display: control.iconSource.toString().length > 0 ? AbstractButton.IconOnly : AbstractButton.TextOnly
    icon.source: control.iconSource
    icon.width: control.iconSize
    icon.height: control.iconSize
    icon.color: control.symbolColor
    font.pixelSize: control.symbolSize
    font.bold: true
    Accessible.name: control.toolTipText.length > 0 ? control.toolTipText : control.symbol

    onHoveredChanged: {
        if (control.hovered && control.enabled && control.toolTipText.length > 0) {
            tooltipShowTimer.restart()
        } else {
            tooltipShowTimer.stop()
            tooltipPopup.close()
        }
    }

    onEnabledChanged: {
        if (!control.enabled) {
            tooltipShowTimer.stop()
            tooltipPopup.close()
        }
    }

    onToolTipTextChanged: {
        if (control.toolTipText.length === 0) {
            tooltipShowTimer.stop()
            tooltipPopup.close()
        }
    }

    Timer {
        id: tooltipShowTimer
        interval: 320
        repeat: false
        onTriggered: {
            if (control.hovered && control.enabled && control.toolTipText.length > 0) {
                tooltipPopup.open()
            }
        }
    }

    Popup {
        id: tooltipPopup

        readonly property int maxTextWidth: 220

        parent: control
        x: Math.round((control.width - tooltipPopup.width) / 2)
        y: -tooltipPopup.height - 8
        z: 1000
        modal: false
        dim: false
        focus: false
        closePolicy: Popup.NoAutoClose
        leftPadding: 10
        rightPadding: 10
        topPadding: 7
        bottomPadding: 12
        contentWidth: Math.min(tooltipLabel.implicitWidth, tooltipPopup.maxTextWidth)
        transformOrigin: Popup.Bottom

        enter: Transition {
            NumberAnimation {
                property: "scale"
                from: 0.96
                to: 1.0
                duration: 110
                easing.type: Easing.OutCubic
            }
        }

        exit: Transition {
            NumberAnimation {
                property: "scale"
                from: 1.0
                to: 0.98
                duration: 80
                easing.type: Easing.OutCubic
            }
        }

        background: Item {
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 5
                radius: 8
                color: control.ui.themePalette.tooltipBg
                border.color: control.ui.themePalette.tooltipBorder
            }

            Rectangle {
                width: 9
                height: 9
                x: Math.round((parent.width - width) / 2)
                y: parent.height - 11
                rotation: 45
                radius: 1
                color: control.ui.themePalette.tooltipBg
                border.color: control.ui.themePalette.tooltipBorder
            }
        }

        contentItem: Label {
            id: tooltipLabel

            text: control.toolTipText
            color: control.ui.themePalette.tooltipText
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 12
            font.weight: Font.Medium
            lineHeight: 1.12
        }
    }

    HoverHandler {
        cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    Behavior on scale {
        NumberAnimation {
            duration: 110
            easing.type: Easing.OutCubic
        }
    }

    background: Rectangle {
        radius: control.cornerRadius
        color: control.down
               ? control.pressedBg
               : ((control.hovered || control.forceActive) ? control.hoverBg : control.effectiveRestBg)
        border.color: control.outlineColor

        Behavior on color {
            ColorAnimation {
                duration: 120
                easing.type: Easing.OutCubic
            }
        }
    }
}
