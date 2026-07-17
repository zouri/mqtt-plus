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
    property point anchorScenePosition: Qt.point(0, 0)
    readonly property var hostWindow: control.parent ? control.parent.Window.window : null
    readonly property int effectivePosition: control.resolvePosition()
    readonly property bool horizontalPosition: control.effectivePosition === AppToolTip.Position.Left
                                               || control.effectivePosition === AppToolTip.Position.Right
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
        if (maximum < minimum) {
            return minimum
        }
        return Math.max(minimum, Math.min(maximum, value))
    }

    function normalizedPosition(candidate) {
        switch (candidate) {
        case AppToolTip.Position.Top:
        case AppToolTip.Position.Bottom:
        case AppToolTip.Position.Left:
        case AppToolTip.Position.Right:
            return candidate
        }
        return AppToolTip.Position.Right
    }

    function isHorizontal(candidate) {
        return candidate === AppToolTip.Position.Left || candidate === AppToolTip.Position.Right
    }

    function oppositePosition(candidate) {
        switch (candidate) {
        case AppToolTip.Position.Top:
            return AppToolTip.Position.Bottom
        case AppToolTip.Position.Bottom:
            return AppToolTip.Position.Top
        case AppToolTip.Position.Left:
            return AppToolTip.Position.Right
        case AppToolTip.Position.Right:
            return AppToolTip.Position.Left
        }
        return AppToolTip.Position.Right
    }

    function candidateWidth(candidate) {
        return surface.bubbleWidth + (control.isHorizontal(candidate) ? control.arrowDepth : 0)
    }

    function candidateHeight(candidate) {
        return surface.bubbleHeight + (control.isHorizontal(candidate) ? 0 : control.arrowDepth)
    }

    function availableSpace(candidate) {
        if (!control.parent || !control.hostWindow) {
            return 0
        }

        switch (candidate) {
        case AppToolTip.Position.Top:
            return control.anchorScenePosition.y - control.margins
        case AppToolTip.Position.Bottom:
            return control.hostWindow.height - control.margins
                    - control.anchorScenePosition.y - control.parent.height
        case AppToolTip.Position.Left:
            return control.anchorScenePosition.x - control.margins
        case AppToolTip.Position.Right:
            return control.hostWindow.width - control.margins
                    - control.anchorScenePosition.x - control.parent.width
        }
        return 0
    }

    function requiredSpace(candidate) {
        const extent = control.isHorizontal(candidate)
                       ? control.candidateWidth(candidate)
                       : control.candidateHeight(candidate)
        return extent + control.gap
    }

    function positionFits(candidate) {
        return control.availableSpace(candidate) >= control.requiredSpace(candidate)
    }

    // Resolve edge collisions before Popup applies its own constraint and separates the arrow from its anchor.
    function resolvePosition() {
        const preferred = control.normalizedPosition(control.position)
        if (!control.parent || !control.hostWindow
                || control.hostWindow.width <= 0 || control.hostWindow.height <= 0
                || surface.bubbleWidth <= 0 || surface.bubbleHeight <= 0) {
            return preferred
        }

        if (control.positionFits(preferred)) {
            return preferred
        }

        const opposite = control.oppositePosition(preferred)
        const candidates = control.isHorizontal(preferred)
                           ? [AppToolTip.Position.Bottom, AppToolTip.Position.Top, opposite]
                           : [opposite, AppToolTip.Position.Right, AppToolTip.Position.Left]
        for (const candidate of candidates) {
            if (control.positionFits(candidate)) {
                return candidate
            }
        }

        let bestPosition = preferred
        let bestClearance = control.availableSpace(preferred) - control.requiredSpace(preferred)

        for (const candidate of candidates) {
            const clearance = control.availableSpace(candidate) - control.requiredSpace(candidate)
            if (clearance > bestClearance) {
                bestPosition = candidate
                bestClearance = clearance
            }
        }
        return bestPosition
    }

    function resolveX() {
        if (!control.parent) {
            return 0
        }

        switch (control.effectivePosition) {
        case AppToolTip.Position.Left:
            return -control.implicitWidth - control.gap
        case AppToolTip.Position.Right:
            return control.parent.width + control.gap
        case AppToolTip.Position.Top:
        case AppToolTip.Position.Bottom:
            if (!control.hostWindow) {
                return Math.round((control.parent.width - control.implicitWidth) / 2)
            }

            const desiredSceneX = control.anchorScenePosition.x
                                  + (control.parent.width - control.implicitWidth) / 2
            const maximumSceneX = control.hostWindow.width - control.margins - control.implicitWidth
            return Math.round(control.clamp(desiredSceneX, control.margins, maximumSceneX)
                              - control.anchorScenePosition.x)
        }
        return 0
    }

    function resolveY() {
        if (!control.parent) {
            return 0
        }

        switch (control.effectivePosition) {
        case AppToolTip.Position.Top:
            return -control.implicitHeight - control.gap
        case AppToolTip.Position.Bottom:
            return control.parent.height + control.gap
        case AppToolTip.Position.Left:
        case AppToolTip.Position.Right:
            if (!control.hostWindow) {
                return Math.round((control.parent.height - control.implicitHeight) / 2)
            }

            const desiredSceneY = control.anchorScenePosition.y
                                  + (control.parent.height - control.implicitHeight) / 2
            const maximumSceneY = control.hostWindow.height - control.margins - control.implicitHeight
            return Math.round(control.clamp(desiredSceneY, control.margins, maximumSceneY)
                              - control.anchorScenePosition.y)
        }
        return 0
    }

    function refreshAnchorPosition() {
        if (!control.parent) {
            control.anchorScenePosition = Qt.point(0, 0)
            return
        }
        control.anchorScenePosition = control.parent.mapToItem(null, 0, 0)
    }

    function bubblePath(x, y, width, height, radius, arrowCenterX, arrowCenterY) {
        const right = x + width
        const bottom = y + height
        const depth = control.arrowDepth
        const halfSpan = control.arrowSpan / 2
        const centerX = control.clamp(arrowCenterX,
                                      x + radius + halfSpan,
                                      right - radius - halfSpan)
        const centerY = control.clamp(arrowCenterY,
                                      y + radius + halfSpan,
                                      bottom - radius - halfSpan)
        const hasArrow = control.showArrow && depth > 0 && halfSpan > 0
        const topArrow = hasArrow && control.effectivePosition === AppToolTip.Position.Bottom
        const rightArrow = hasArrow && control.effectivePosition === AppToolTip.Position.Left
        const bottomArrow = hasArrow && control.effectivePosition === AppToolTip.Position.Top
        const leftArrow = hasArrow && control.effectivePosition === AppToolTip.Position.Right
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

    x: control.resolveX()
    y: control.resolveY()

    visible: control.active && control.text.length > 0
    delay: 450
    timeout: -1
    padding: 0
    margins: 10
    font.pixelSize: 12
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutsideParent | T.Popup.CloseOnReleaseOutsideParent

    Component.onCompleted: control.refreshAnchorPosition()
    onAboutToShow: control.refreshAnchorPosition()
    onPositionChanged: control.refreshAnchorPosition()

    Connections {
        target: control.parent

        function onXChanged() {
            control.refreshAnchorPosition()
        }

        function onYChanged() {
            control.refreshAnchorPosition()
        }

        function onWidthChanged() {
            control.refreshAnchorPosition()
        }

        function onHeightChanged() {
            control.refreshAnchorPosition()
        }
    }

    Connections {
        target: control.hostWindow

        function onWidthChanged() {
            control.refreshAnchorPosition()
        }

        function onHeightChanged() {
            control.refreshAnchorPosition()
        }
    }

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
        readonly property real bubbleX: control.effectivePosition === AppToolTip.Position.Right
                                        ? control.arrowDepth : 0
        readonly property real bubbleY: control.effectivePosition === AppToolTip.Position.Bottom
                                        ? control.arrowDepth : 0
        readonly property real arrowCenterX: control.parent
                                             ? control.parent.width / 2 - control.x
                                             : surface.bubbleWidth / 2
        readonly property real arrowCenterY: control.parent
                                             ? control.parent.height / 2 - control.y
                                             : surface.bubbleHeight / 2

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
                                             control.bubbleRadius,
                                             surface.arrowCenterX,
                                             surface.arrowCenterY)
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
