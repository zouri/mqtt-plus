pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Shapes

Rectangle {
    id: root

    required property var appController
    required property string currentScriptId
    required property AppUi ui

    signal newScriptRequested()
    signal scriptRequested(var row)

    SplitView.preferredWidth: 260
    SplitView.minimumWidth: 210
    SplitView.maximumWidth: 340
    radius: root.ui.innerRadius
    color: root.ui.themePalette.innerPanelBg
    border.color: root.ui.themePalette.innerPanelBorder

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        ListView {
            id: scriptList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 7
            model: root.appController.scriptLibraryModel
            reuseItems: true

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: Rectangle {
                id: scriptDelegate
                required property var modelData
                width: ListView.view.width
                implicitHeight: 54
                radius: root.ui.innerRadius
                color: scriptDelegate.modelData.id === root.currentScriptId
                       ? root.ui.themePalette.selectedBg
                       : (scriptMouse.containsMouse || activeFocus
                          ? root.ui.rowHover
                          : root.ui.themePalette.itemBg)
                border.color: scriptDelegate.modelData.id === root.currentScriptId
                              ? root.ui.themePalette.selectedBorder
                              : root.ui.themePalette.itemBorder
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Lua script %1").arg(scriptDelegate.modelData.name)

                Column {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 4

                    Label {
                        text: scriptDelegate.modelData.name
                        width: parent.width
                        color: root.ui.textStrong
                        font.pixelSize: 12
                        font.bold: true
                        elide: Label.ElideRight
                    }

                    Label {
                        text: scriptDelegate.modelData.updatedAt || qsTr("Not saved")
                        width: parent.width
                        color: root.ui.textMuted
                        font.pixelSize: 10
                        elide: Label.ElideRight
                    }
                }

                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Return
                            || event.key === Qt.Key_Enter
                            || event.key === Qt.Key_Space) {
                        root.scriptRequested(scriptDelegate.modelData)
                        event.accepted = true
                    }
                }

                MouseArea {
                    id: scriptMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor

                    onClicked: {
                        scriptDelegate.forceActiveFocus()
                        root.scriptRequested(scriptDelegate.modelData)
                    }
                }
            }

            footer: Item {
                width: scriptList.width
                height: 58

                Rectangle {
                    id: addScriptDelegate
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 48
                    radius: root.ui.innerRadius
                    color: addScriptMouse.containsMouse || activeFocus
                           ? root.ui.rowHover
                           : root.ui.themePalette.itemBg
                    activeFocusOnTab: true
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("New script")

                    Keys.onPressed: (event) => {
                        if (event.key === Qt.Key_Return
                                || event.key === Qt.Key_Enter
                                || event.key === Qt.Key_Space) {
                            root.newScriptRequested()
                            event.accepted = true
                        }
                    }

                    Shape {
                        id: addScriptBorder
                        anchors.fill: parent
                        anchors.margins: 0.5
                        preferredRendererType: Shape.CurveRenderer
                        antialiasing: true

                        ShapePath {
                            fillColor: "transparent"
                            strokeColor: addScriptMouse.containsMouse || addScriptDelegate.activeFocus
                                         ? root.ui.themePalette.selectedBorder
                                         : root.ui.themePalette.itemBorder
                            strokeWidth: 1
                            strokeStyle: ShapePath.DashLine
                            dashPattern: [5, 4]
                            startX: root.ui.innerRadius
                            startY: 0

                            PathLine {
                                x: addScriptBorder.width - root.ui.innerRadius
                                y: 0
                            }
                            PathArc {
                                x: addScriptBorder.width
                                y: root.ui.innerRadius
                                radiusX: root.ui.innerRadius
                                radiusY: root.ui.innerRadius
                            }
                            PathLine {
                                x: addScriptBorder.width
                                y: addScriptBorder.height - root.ui.innerRadius
                            }
                            PathArc {
                                x: addScriptBorder.width - root.ui.innerRadius
                                y: addScriptBorder.height
                                radiusX: root.ui.innerRadius
                                radiusY: root.ui.innerRadius
                            }
                            PathLine {
                                x: root.ui.innerRadius
                                y: addScriptBorder.height
                            }
                            PathArc {
                                x: 0
                                y: addScriptBorder.height - root.ui.innerRadius
                                radiusX: root.ui.innerRadius
                                radiusY: root.ui.innerRadius
                            }
                            PathLine {
                                x: 0
                                y: root.ui.innerRadius
                            }
                            PathArc {
                                x: root.ui.innerRadius
                                y: 0
                                radiusX: root.ui.innerRadius
                                radiusY: root.ui.innerRadius
                            }
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 10
                        spacing: 8

                        ToolButton {
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 20
                            padding: 0
                            display: AbstractButton.IconOnly
                            icon.source: root.ui.materialIcon("plus")
                            icon.width: 16
                            icon.height: 16
                            icon.color: root.ui.textMuted
                            Accessible.ignored: true

                            background: Item {
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("New script")
                            color: root.ui.textMuted
                            elide: Label.ElideRight
                            font.pixelSize: 13
                            font.bold: true
                        }
                    }

                    MouseArea {
                        id: addScriptMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton
                        cursorShape: Qt.PointingHandCursor

                        onPressed: addScriptDelegate.forceActiveFocus()
                        onClicked: root.newScriptRequested()
                    }
                }
            }
        }
    }
}
