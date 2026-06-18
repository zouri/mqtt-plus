pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import QtQuick.Shapes
import QtQuick.Templates as T

ToolTip {
    id: control

    enum Position {
        Top,
        Bottom,
        Left,
        Right
    }

    required property AppUi ui
    property int position: AppToolTip.Position.Right
    property bool showArrow: true
    property int gap: 9
    property int maxTextWidth: 280
    property bool active: false
    readonly property bool horizontalPosition: control.position === AppToolTip.Position.Left
                                               || control.position === AppToolTip.Position.Right
    readonly property int arrowDepth: control.showArrow ? 7 : 0
    readonly property int arrowSpan: control.showArrow ? 12 : 0
    readonly property int bubblePaddingX: 11
    readonly property int bubblePaddingY: 6
    readonly property int bubbleRadius: 7
    readonly property color bubbleColor: control.ui.isDarkTheme ? "#303136" : "#ffffff"
    readonly property color borderColor: control.ui.isDarkTheme
                                        ? Qt.rgba(1, 1, 1, 0.10)
                                        : Qt.rgba(0, 0, 0, 0.10)

    function clamp(value, minimum, maximum) {
        return Math.max(minimum, Math.min(maximum, value))
    }

    function bubblePath(x, y, width, height, radius) {
        const right = x + width
        const bottom = y + height
        const depth = control.arrowDepth
        const halfSpan = control.arrowSpan / 2
        const centerX = control.clamp(x + width / 2, x + radius + halfSpan, right - radius - halfSpan)
        const centerY = control.clamp(y + height / 2, y + radius + halfSpan, bottom - radius - halfSpan)
        const hasArrow = control.showArrow && depth > 0 && halfSpan > 0
        const topArrow = hasArrow && control.position === AppToolTip.Position.Bottom
        const rightArrow = hasArrow && control.position === AppToolTip.Position.Left
        const bottomArrow = hasArrow && control.position === AppToolTip.Position.Top
        const leftArrow = hasArrow && control.position === AppToolTip.Position.Right
        const path = [
            `M ${x + radius} ${y}`,
            topArrow
                ? `L ${centerX - halfSpan} ${y} L ${centerX} ${y - depth} L ${centerX + halfSpan} ${y}`
                : "",
            `L ${right - radius} ${y}`,
            `Q ${right} ${y} ${right} ${y + radius}`,
            rightArrow
                ? `L ${right} ${centerY - halfSpan} L ${right + depth} ${centerY} L ${right} ${centerY + halfSpan}`
                : "",
            `L ${right} ${bottom - radius}`,
            `Q ${right} ${bottom} ${right - radius} ${bottom}`,
            bottomArrow
                ? `L ${centerX + halfSpan} ${bottom} L ${centerX} ${bottom + depth} L ${centerX - halfSpan} ${bottom}`
                : "",
            `L ${x + radius} ${bottom}`,
            `Q ${x} ${bottom} ${x} ${bottom - radius}`,
            leftArrow
                ? `L ${x} ${centerY + halfSpan} L ${x - depth} ${centerY} L ${x} ${centerY - halfSpan}`
                : "",
            `L ${x} ${y + radius}`,
            `Q ${x} ${y} ${x + radius} ${y}`,
            "Z"
        ]

        return path.filter(part => part.length > 0).join(" ")
    }

    x: {
        if (!parent) {
            return 0
        }

        switch (control.position) {
        case AppToolTip.Position.Left:
            return -implicitWidth - control.gap
        case AppToolTip.Position.Right:
            return parent.width + control.gap
        case AppToolTip.Position.Top:
        case AppToolTip.Position.Bottom:
            return Math.round((parent.width - implicitWidth) / 2)
        }
        return 0
    }
    y: {
        if (!parent) {
            return 0
        }

        switch (control.position) {
        case AppToolTip.Position.Top:
            return -implicitHeight - control.gap
        case AppToolTip.Position.Bottom:
            return parent.height + control.gap
        case AppToolTip.Position.Left:
        case AppToolTip.Position.Right:
            return Math.round((parent.height - implicitHeight) / 2)
        }
        return 0
    }

    visible: control.active && control.text.length > 0
    delay: 450
    timeout: -1
    padding: 0
    margins: 10
    font.pixelSize: 12
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutsideParent | T.Popup.CloseOnReleaseOutsideParent

    enter: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: 120
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                property: "scale"
                from: 0.96
                to: 1
                duration: 140
                easing.type: Easing.OutCubic
            }
        }
    }

    exit: Transition {
        NumberAnimation {
            property: "opacity"
            to: 0
            duration: 90
            easing.type: Easing.OutCubic
        }
    }

    contentItem: Item {
        id: surface

        readonly property real bubbleWidth: body.width + control.bubblePaddingX * 2
        readonly property real bubbleHeight: body.implicitHeight + control.bubblePaddingY * 2
        readonly property real bubbleX: control.position === AppToolTip.Position.Right ? control.arrowDepth : 0
        readonly property real bubbleY: control.position === AppToolTip.Position.Bottom ? control.arrowDepth : 0

        implicitWidth: surface.bubbleWidth + (control.horizontalPosition ? control.arrowDepth : 0)
        implicitHeight: surface.bubbleHeight + (control.horizontalPosition ? 0 : control.arrowDepth)

        Shape {
            id: bubble

            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            layer.enabled: control.visible
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowBlur: 0.42
                shadowColor: control.ui.isDarkTheme ? "#82000000" : "#22000000"
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 5
            }

            ShapePath {
                fillColor: control.bubbleColor
                strokeColor: control.borderColor
                strokeWidth: 1

                PathSvg {
                    path: control.bubblePath(surface.bubbleX,
                                             surface.bubbleY,
                                             surface.bubbleWidth,
                                             surface.bubbleHeight,
                                             control.bubbleRadius)
                }
            }
        }

        Label {
            id: body

            x: surface.bubbleX + control.bubblePaddingX
            y: surface.bubbleY + Math.round((surface.bubbleHeight - implicitHeight) / 2)
            width: Math.min(implicitWidth, control.maxTextWidth)
            text: control.text
            color: control.ui.textStrong
            font: control.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap
        }
    }

    background: Item {
    }
}
