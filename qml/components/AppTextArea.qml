pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

Control {
    id: control

    required property AppUi ui

    property alias text: textArea.text
    property alias readOnly: textArea.readOnly
    property alias color: textArea.color
    property alias placeholderText: textArea.placeholderText
    property alias placeholderTextColor: textArea.placeholderTextColor
    property alias selectByMouse: textArea.selectByMouse
    property alias wrapMode: textArea.wrapMode
    property bool showLineNumbers: false
    property bool showFocusBorder: true
    property int backgroundRadius: 12
    property int backgroundBorderWidth: 1
    property color backgroundColor: control.ui.themePalette.fieldBg

    readonly property real contentY: contentRoot.editorContentY
    readonly property real contentHeight: contentRoot.editorContentHeight
    readonly property real viewportHeight: contentRoot.editorViewportHeight
    readonly property int lineCount: Math.max(1, textArea.text.split("\n").length)
    readonly property int lineNumberGutterWidth: control.showLineNumbers
                                               ? Math.max(42, 24 + String(control.lineCount).length * 8)
                                               : 0

    clip: true
    font.pixelSize: 13

    function setContentY(value) {
        const viewport = contentRoot.editorViewport
        if (!viewport) {
            return
        }
        const maxContentY = Math.max(0, viewport.contentHeight - viewport.height)
        viewport.contentY = Math.max(0, Math.min(value, maxContentY))
    }

    function scrollToBottom() {
        Qt.callLater(function() {
            control.setContentY(control.contentHeight - control.viewportHeight)
        })
    }

    FontMetrics {
        id: lineNumberFontMetrics

        font.family: control.font.family
        font.pixelSize: control.font.pixelSize
    }

    background: Rectangle {
        radius: control.backgroundRadius
        color: control.backgroundColor
        border.width: control.backgroundBorderWidth
        border.color: textArea.activeFocus && control.showFocusBorder ? control.ui.themePalette.fieldFocusBorder : control.ui.themePalette.fieldBorder

        Behavior on border.color {
            ColorAnimation {
                duration: 120
                easing.type: Easing.OutCubic
            }
        }
    }

    contentItem: Item {
        id: contentRoot

        readonly property var editorViewport: editorScrollView.contentItem
        readonly property real editorContentY: contentRoot.editorViewport
                                               ? Number(contentRoot.editorViewport.contentY || 0)
                                               : 0
        readonly property real editorContentHeight: contentRoot.editorViewport
                                                   ? Number(contentRoot.editorViewport.contentHeight || 0)
                                                   : 0
        readonly property real editorViewportHeight: contentRoot.editorViewport
                                                   ? Number(contentRoot.editorViewport.height || 0)
                                                   : 0

        ScrollView {
            id: editorScrollView

            anchors.fill: parent
            anchors.leftMargin: control.lineNumberGutterWidth
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AsNeeded
            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            background: null

            TextArea {
                id: textArea

                width: editorScrollView.availableWidth
                leftPadding: control.showLineNumbers ? 8 : 12
                rightPadding: 12
                topPadding: 10
                bottomPadding: 10
                font.family: control.font.family
                font.pixelSize: control.font.pixelSize
                color: control.ui.textStrong
                placeholderTextColor: control.ui.themePalette.fieldPlaceholder
                selectByMouse: true
                background: null
            }
        }

        Rectangle {
            id: lineNumberGutter

            visible: control.showLineNumbers
            x: 1
            y: 1
            width: control.lineNumberGutterWidth
            height: contentRoot.height - 2
            color: control.ui.themePalette.innerPanelBg
            clip: true

            ListView {
                id: lineNumberList

                anchors.fill: parent
                anchors.topMargin: textArea.topPadding
                anchors.bottomMargin: textArea.bottomPadding
                anchors.rightMargin: 10
                interactive: false
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                contentY: contentRoot.editorContentY
                model: control.lineCount
                reuseItems: true

                delegate: Label {
                    id: lineNumberDelegate

                    required property int index

                    width: lineNumberList.width
                    height: lineNumberFontMetrics.lineSpacing
                    horizontalAlignment: Text.AlignRight
                    verticalAlignment: Text.AlignTop
                    text: lineNumberDelegate.index + 1
                    color: control.ui.themePalette.fieldPlaceholder
                    font.family: textArea.font.family
                    font.pixelSize: textArea.font.pixelSize
                }
            }

            Rectangle {
                anchors.right: parent.right
                width: 1
                height: parent.height
                color: control.ui.themePalette.fieldBorder
            }
        }
    }
}
