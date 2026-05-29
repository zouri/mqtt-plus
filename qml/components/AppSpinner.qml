pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: control

    required property AppUi ui
    property color strokeColor: control.ui.textStrong
    property real strokeWidth: 2

    implicitWidth: 14
    implicitHeight: 14

    Canvas {
        id: spinnerCanvas
        anchors.fill: parent
        antialiasing: true
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        Connections {
            target: control

            function onStrokeColorChanged() {
                spinnerCanvas.requestPaint()
            }

            function onStrokeWidthChanged() {
                spinnerCanvas.requestPaint()
            }
        }

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.beginPath()
            ctx.lineWidth = control.strokeWidth
            ctx.lineCap = "round"
            ctx.strokeStyle = control.strokeColor
            const radius = Math.max(0, (Math.min(width, height) - control.strokeWidth) / 2)
            ctx.arc(width / 2,
                    height / 2,
                    radius,
                    -Math.PI * 0.15,
                    Math.PI * 1.2,
                    false)
            ctx.stroke()
        }
    }

    RotationAnimator on rotation {
        from: 0
        to: 360
        duration: 850
        loops: Animation.Infinite
        running: control.visible
    }
}
