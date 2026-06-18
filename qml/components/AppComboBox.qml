pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

ComboBox {
    id: control

    required property AppUi ui

    implicitHeight: control.ui.compactControlHeight + 4
    font.pixelSize: control.ui.compactFontSize
    leftPadding: 12
    rightPadding: 32

    HoverHandler {
        cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    delegate: ItemDelegate {
        id: comboDelegate
        required property var modelData
        required property int index
        width: ListView.view ? ListView.view.width : control.width
        implicitHeight: 34
        hoverEnabled: true
        leftPadding: 10
        rightPadding: 10
        topPadding: 0
        bottomPadding: 0
        contentItem: Label {
            text: comboDelegate.modelData
            color: comboDelegate.enabled ? control.ui.textStrong : control.ui.themePalette.fieldPlaceholder
            font.pixelSize: control.ui.compactFontSize
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        highlighted: control.highlightedIndex === index

        background: Rectangle {
            radius: 7
            color: comboDelegate.highlighted
                   ? control.ui.themePalette.selectedBg
                   : comboDelegate.hovered
                     ? control.ui.themePalette.actionHoverBg
                     : "transparent"
            border.color: comboDelegate.highlighted ? control.ui.themePalette.selectedBorder : "transparent"
        }
    }

    contentItem: Label {
        leftPadding: 0
        rightPadding: 0
        text: control.displayText
        color: control.ui.textStrong
        font.pixelSize: control.font.pixelSize
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Canvas {
        id: comboIndicator
        x: control.width - width - 12
        y: (control.height - height) / 2
        width: 10
        height: 6
        contextType: "2d"

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        Connections {
            target: control.ui

            function onTextMutedChanged() {
                comboIndicator.requestPaint()
            }
        }

        onPaint: {
            const ctx = getContext("2d")
            if (!ctx) {
                return
            }

            ctx.clearRect(0, 0, width, height)
            ctx.beginPath()
            ctx.moveTo(0, 0)
            ctx.lineTo(width, 0)
            ctx.lineTo(width / 2, height)
            ctx.closePath()
            ctx.fillStyle = control.ui.textMuted
            ctx.fill()
        }
    }

    background: Rectangle {
        radius: 10
        color: control.ui.themePalette.fieldBg
        border.color: control.activeFocus ? control.ui.themePalette.fieldFocusBorder : control.ui.themePalette.fieldBorder

        Behavior on border.color {
            ColorAnimation {
                duration: 120
                easing.type: Easing.OutCubic
            }
        }
    }

    popup: Popup {
        y: control.height + 6
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 220)
        padding: 4

        background: Rectangle {
            radius: 12
            color: control.ui.themePalette.dialogBg
            border.color: control.ui.themePalette.dialogBorder
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.delegateModel
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator { }
        }
    }
}
