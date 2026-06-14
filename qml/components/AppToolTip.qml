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
    readonly property color bubbleColor: control.ui.isDarkTheme ? "#303136" : "#ffffff"
    readonly property color borderColor: control.ui.isDarkTheme
                                        ? Qt.rgba(1, 1, 1, 0.10)
                                        : Qt.rgba(0, 0, 0, 0.10)

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
        implicitWidth: bubble.implicitWidth + (control.horizontalPosition ? control.arrowDepth : 0)
        implicitHeight: bubble.implicitHeight + (control.horizontalPosition ? 0 : control.arrowDepth)

        Rectangle {
            id: bubble

            x: control.position === AppToolTip.Position.Right ? control.arrowDepth : 0
            y: control.position === AppToolTip.Position.Bottom ? control.arrowDepth : 0
            implicitWidth: body.width + 22
            implicitHeight: body.implicitHeight + 12
            radius: 7
            color: control.bubbleColor
            border.color: control.borderColor
            border.width: 1
            layer.enabled: control.visible
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowBlur: 0.42
                shadowColor: control.ui.isDarkTheme ? "#82000000" : "#22000000"
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 5
            }

            Label {
                id: body

                anchors.centerIn: parent
                width: Math.min(implicitWidth, control.maxTextWidth)
                text: control.text
                color: control.ui.textStrong
                font: control.font
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.WordWrap
            }
        }

        Shape {
            id: arrow

            visible: control.showArrow
            x: {
                switch (control.position) {
                case AppToolTip.Position.Left:
                    return bubble.width
                case AppToolTip.Position.Right:
                    return 0
                case AppToolTip.Position.Top:
                case AppToolTip.Position.Bottom:
                    return Math.round((bubble.width - width) / 2)
                }
                return 0
            }
            y: {
                switch (control.position) {
                case AppToolTip.Position.Top:
                    return bubble.height
                case AppToolTip.Position.Bottom:
                    return 0
                case AppToolTip.Position.Left:
                case AppToolTip.Position.Right:
                    return Math.round((bubble.height - height) / 2)
                }
                return 0
            }
            width: control.horizontalPosition ? control.arrowDepth : control.arrowSpan
            height: control.horizontalPosition ? control.arrowSpan : control.arrowDepth
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                fillColor: control.bubbleColor
                strokeColor: "transparent"
                startX: control.position === AppToolTip.Position.Right ? arrow.width : 0
                startY: control.position === AppToolTip.Position.Bottom ? arrow.height : 0

                PathLine {
                    x: control.position === AppToolTip.Position.Right
                       ? arrow.width
                       : (control.position === AppToolTip.Position.Left ? 0 : arrow.width)
                    y: control.position === AppToolTip.Position.Bottom
                       ? arrow.height
                       : (control.position === AppToolTip.Position.Top ? 0 : arrow.height)
                }

                PathLine {
                    x: control.horizontalPosition
                       ? (control.position === AppToolTip.Position.Left ? arrow.width : 0)
                       : Math.round(arrow.width / 2)
                    y: control.horizontalPosition
                       ? Math.round(arrow.height / 2)
                       : (control.position === AppToolTip.Position.Top ? arrow.height : 0)
                }

                PathLine {
                    x: control.position === AppToolTip.Position.Right ? arrow.width : 0
                    y: control.position === AppToolTip.Position.Bottom ? arrow.height : 0
                }
            }
        }
    }

    background: Item {
    }
}
