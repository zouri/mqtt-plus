pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Shapes
import "../../components"

Rectangle {
    id: control

    required property AppUi ui
    required property var appController
    required property SessionEditorDialog sessionEditor
    property bool collapsed: false

    signal collapseRequested()
    signal expandRequested()

    color: ui.themePalette.panelBg

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        visible: !control.collapsed
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: qsTr("Connections")
                color: control.ui.textStrong
                font.pixelSize: 16
                font.bold: true
            }

            AppBadge {
                ui: control.ui
                label: `${sessionList.count}`
                horizontalPadding: 7
                verticalPadding: 4
            }

            Item {
                Layout.fillWidth: true
            }

            AppIconButton {
                ui: control.ui
                iconSource: control.ui.materialIcon("chevron-left")
                iconSize: 18
                implicitWidth: 30
                implicitHeight: 30
                cornerRadius: 15
                restBg: control.ui.themePalette.windowBg
                outlineColor: control.ui.themePalette.innerPanelBorder
                toolTipText: qsTr("Hide connection list")
                onClicked: control.collapseRequested()
            }
        }

        ListView {
            id: sessionList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            currentIndex: control.appController.currentSessionIndex
            model: control.appController.sessions
            reuseItems: true

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: Rectangle {
                id: sessionDelegate
                required property int index
                required property string name
                required property string connectionState
                required property string host
                required property int port
                required property string transportLabel
                readonly property bool selected: index === control.appController.currentSessionIndex
                width: ListView.view.width
                height: 56
                radius: control.ui.innerRadius
                color: sessionDelegate.selected
                       ? control.ui.themePalette.selectedBg
                       : (rowMouse.containsMouse || activeFocus
                          ? control.ui.rowHover
                          : control.ui.themePalette.itemBg)
                border.color: sessionDelegate.selected
                              ? Qt.rgba(control.ui.themePalette.selectedBorder.r,
                                        control.ui.themePalette.selectedBorder.g,
                                        control.ui.themePalette.selectedBorder.b,
                                        0.36)
                              : control.ui.themePalette.itemBorder
                border.width: 1
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Connection %1").arg(sessionDelegate.name)

                function showSessionContextMenu(globalPosition) {
                    const action = control.appController.showSessionContextMenu(sessionDelegate.index, globalPosition)
                    if (action === "edit") {
                        control.sessionEditor.openForEdit(sessionDelegate.index)
                    } else if (action === "copy") {
                        control.appController.duplicateSessionAt(sessionDelegate.index)
                    } else if (action === "delete") {
                        control.appController.removeSessionAt(sessionDelegate.index)
                    }
                }

                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Return
                            || event.key === Qt.Key_Enter
                            || event.key === Qt.Key_Space) {
                        control.appController.currentSessionIndex = sessionDelegate.index
                        event.accepted = true
                    } else if (event.key === Qt.Key_Menu
                               || (event.key === Qt.Key_F10 && event.modifiers & Qt.ShiftModifier)) {
                        control.appController.currentSessionIndex = sessionDelegate.index
                        sessionDelegate.showSessionContextMenu(
                                    sessionDelegate.mapToGlobal(Qt.point(sessionDelegate.width - 8,
                                                                         Math.round(sessionDelegate.height / 2))))
                        event.accepted = true
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 11
                    anchors.rightMargin: 10
                    spacing: 8

                    Rectangle {
                        Layout.preferredWidth: 3
                        Layout.preferredHeight: 30
                        radius: 2
                        color: sessionDelegate.selected
                               ? control.ui.themePalette.selectedBorder
                               : "transparent"
                    }

                    Rectangle {
                        Layout.preferredWidth: 7
                        Layout.preferredHeight: 7
                        radius: 3.5
                        color: control.ui.stateColor(sessionDelegate.connectionState)
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: sessionDelegate.name
                            color: control.ui.textStrong
                            elide: Label.ElideRight
                            font.pixelSize: 12
                            font.bold: true
                        }

                        Label {
                            Layout.fillWidth: true
                            text: sessionDelegate.transportLabel || "TCP"
                            color: control.ui.textMuted
                            elide: Label.ElideRight
                            font.pixelSize: 10
                        }
                    }

                    AppBadge {
                        ui: control.ui
                        label: control.ui.statusLabel(sessionDelegate.connectionState)
                        badgeRadius: 10
                        horizontalPadding: 7
                        verticalPadding: 3
                        badgeBg: control.ui.themePalette.chipBg
                        badgeBorder: "transparent"
                        badgeText: control.ui.textMuted
                    }
                }

                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton

                    onPressed: (mouse) => {
                        sessionDelegate.forceActiveFocus()
                        if (mouse.button === Qt.RightButton) {
                            control.appController.currentSessionIndex = sessionDelegate.index
                            sessionDelegate.showSessionContextMenu(
                                        sessionDelegate.mapToGlobal(Qt.point(mouse.x, mouse.y)))
                        }
                    }

                    onClicked: (mouse) => {
                        if (mouse.button === Qt.LeftButton) {
                            control.appController.currentSessionIndex = sessionDelegate.index
                        }
                    }
                }
            }

            footer: Item {
                width: sessionList.width
                height: 54

                Rectangle {
                    id: addSessionDelegate
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 44
                    radius: 16
                    color: "transparent"
                    activeFocusOnTab: true
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("New connection")

                    Keys.onPressed: (event) => {
                        if (event.key === Qt.Key_Return
                                || event.key === Qt.Key_Enter
                                || event.key === Qt.Key_Space) {
                            control.sessionEditor.openForCreate()
                            event.accepted = true
                        }
                    }

                    Shape {
                        id: addSessionBorder
                        anchors.fill: parent
                        anchors.margins: 0.5
                        preferredRendererType: Shape.CurveRenderer
                        antialiasing: true

                        ShapePath {
                            fillColor: "transparent"
                            strokeColor: addRowMouse.containsMouse || addSessionDelegate.activeFocus
                                         ? control.ui.themePalette.selectedBorder
                                         : control.ui.themePalette.itemBorder
                            strokeWidth: 1
                            strokeStyle: ShapePath.DashLine
                            dashPattern: [5, 4]
                            startX: control.ui.innerRadius
                            startY: 0

                            PathLine {
                                x: addSessionBorder.width - control.ui.innerRadius
                                y: 0
                            }
                            PathArc {
                                x: addSessionBorder.width
                                y: control.ui.innerRadius
                                radiusX: control.ui.innerRadius
                                radiusY: control.ui.innerRadius
                            }
                            PathLine {
                                x: addSessionBorder.width
                                y: addSessionBorder.height - control.ui.innerRadius
                            }
                            PathArc {
                                x: addSessionBorder.width - control.ui.innerRadius
                                y: addSessionBorder.height
                                radiusX: control.ui.innerRadius
                                radiusY: control.ui.innerRadius
                            }
                            PathLine {
                                x: control.ui.innerRadius
                                y: addSessionBorder.height
                            }
                            PathArc {
                                x: 0
                                y: addSessionBorder.height - control.ui.innerRadius
                                radiusX: control.ui.innerRadius
                                radiusY: control.ui.innerRadius
                            }
                            PathLine {
                                x: 0
                                y: control.ui.innerRadius
                            }
                            PathArc {
                                x: control.ui.innerRadius
                                y: 0
                                radiusX: control.ui.innerRadius
                                radiusY: control.ui.innerRadius
                            }
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 6

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("+ New connection")
                            color: control.ui.textMuted
                            elide: Label.ElideRight
                            horizontalAlignment: Text.AlignHCenter
                            font.pixelSize: 14
                            font.bold: true
                        }
                    }

                    MouseArea {
                        id: addRowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton
                        cursorShape: Qt.PointingHandCursor

                        onPressed: addSessionDelegate.forceActiveFocus()
                        onClicked: control.sessionEditor.openForCreate()
                    }
                }
            }
        }

    }

    Rectangle {
        visible: control.collapsed
        anchors.fill: parent
        color: control.ui.themePalette.panelBg
        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Show connection list")

        ColumnLayout {
            anchors.fill: parent
            anchors.topMargin: 16
            anchors.bottomMargin: 16
            spacing: 10

            AppIconButton {
                ui: control.ui
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: 34
                implicitHeight: 34
                cornerRadius: 17
                iconSource: control.ui.materialIcon("chevron-right")
                iconSize: 18
                restBg: control.ui.themePalette.windowBg
                toolTipText: qsTr("Show connection list")
                onClicked: control.expandRequested()
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Connections")
                color: control.ui.textMuted
                font.pixelSize: 12
                font.bold: true
                rotation: 90
            }

            Item {
                Layout.fillHeight: true
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: control.expandRequested()
        }
    }
}
