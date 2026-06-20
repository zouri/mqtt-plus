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
    required property var sessionEditor
    property bool collapsed: false

    signal collapseRequested
    signal expandRequested

    color: ui.themePalette.panelBg

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        visible: !control.collapsed
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 7

            Label {
                text: qsTr("Connections")
                color: control.ui.textStrong
                font.pixelSize: 22
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
                id: collapseButton
                ui: control.ui
                iconSource: control.ui.materialIcon("chevron-left")
                iconSize: 18
                implicitWidth: 24
                implicitHeight: 24
                cornerRadius: 12
                restBg: "transparent"
                hoverBg: control.ui.themePalette.windowBg
                pressedBg: control.ui.themePalette.actionPressedBg
                outlineColor: collapseButton.hovered || collapseButton.down ? control.ui.themePalette.innerPanelBorder : "transparent"
                accessibleName: qsTr("Hide connection list")
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
                required property bool connected
                required property string host
                required property int port
                required property string transportLabel
                readonly property bool selected: index === control.appController.currentSessionIndex
                width: ListView.view.width
                height: 54
                radius: control.ui.innerRadius
                color: sessionDelegate.selected ? control.ui.themePalette.selectedBg : (rowMouse.containsMouse || activeFocus ? control.ui.rowHover : control.ui.themePalette.itemBg)
                border.color: sessionDelegate.selected ? Qt.rgba(control.ui.themePalette.selectedBorder.r, control.ui.themePalette.selectedBorder.g, control.ui.themePalette.selectedBorder.b, 0.36) : control.ui.themePalette.itemBorder
                border.width: 1
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Connection %1").arg(sessionDelegate.name)

                function showSessionContextMenu(globalPosition) {
                    const action = control.appController.showSessionContextMenu(sessionDelegate.index, globalPosition);
                    if (action === "edit") {
                        control.sessionEditor.openForEdit(sessionDelegate.index);
                    } else if (action === "copy") {
                        control.appController.duplicateSessionAt(sessionDelegate.index);
                    } else if (action === "delete") {
                        control.appController.removeSessionAt(sessionDelegate.index);
                    }
                }

                Keys.onPressed: event => {
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
                        control.appController.currentSessionIndex = sessionDelegate.index;
                        event.accepted = true;
                    } else if (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && event.modifiers & Qt.ShiftModifier)) {
                        control.appController.currentSessionIndex = sessionDelegate.index;
                        sessionDelegate.showSessionContextMenu(sessionDelegate.mapToGlobal(Qt.point(sessionDelegate.width - 8, Math.round(sessionDelegate.height / 2))));
                        event.accepted = true;
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 11
                    anchors.rightMargin: 9
                    spacing: 7

                    Rectangle {
                        Layout.preferredWidth: 3
                        Layout.preferredHeight: 30
                        radius: 2
                        color: sessionDelegate.selected ? control.ui.themePalette.selectedBorder : "transparent"
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

                    Rectangle {
                        Layout.preferredWidth: 7
                        Layout.preferredHeight: 7
                        radius: 3.5
                        color: sessionDelegate.connected ? control.ui.stateColor("connected") : control.ui.stateColor("disconnected")
                    }
                }

                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    cursorShape: Qt.PointingHandCursor

                    onPressed: mouse => {
                        sessionDelegate.forceActiveFocus();
                        if (mouse.button === Qt.RightButton) {
                            control.appController.currentSessionIndex = sessionDelegate.index;
                            sessionDelegate.showSessionContextMenu(sessionDelegate.mapToGlobal(Qt.point(mouse.x, mouse.y)));
                        }
                    }

                    onClicked: mouse => {
                        if (mouse.button === Qt.LeftButton) {
                            control.appController.currentSessionIndex = sessionDelegate.index;
                        }
                    }
                }
            }

            footer: Item {
                width: sessionList.width
                height: 50

                Rectangle {
                    id: addSessionDelegate
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 42
                    radius: control.ui.innerRadius
                    color: "transparent"
                    activeFocusOnTab: true
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("New connection")

                    Keys.onPressed: event => {
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
                            control.sessionEditor.openForCreate();
                            event.accepted = true;
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
                            strokeColor: addRowMouse.containsMouse || addSessionDelegate.activeFocus ? control.ui.themePalette.selectedBorder : control.ui.themePalette.itemBorder
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
        id: collapsedBar
        visible: control.collapsed
        anchors.fill: parent
        color: collapsedMouse.containsMouse || activeFocus ? control.ui.themePalette.rowHover : control.ui.themePalette.panelBg
        border.color: "transparent"
        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Show connection list")

        AppIconButton {
            ui: control.ui
            anchors.top: parent.top
            anchors.topMargin: 10
            anchors.horizontalCenter: parent.horizontalCenter
            implicitWidth: 24
            implicitHeight: 24
            cornerRadius: 12
            iconSource: control.ui.materialIcon("chevron-right")
            iconSize: 18
            restBg: "transparent"
            hoverBg: "transparent"
            pressedBg: "transparent"
            outlineColor: "transparent"
            accessibleName: qsTr("Show connection list")
            onClicked: control.expandRequested()
        }

        Label {
            anchors.centerIn: parent
            width: parent.width
            text: qsTr("Expand").split("").join("\n")
            color: control.ui.textMuted
            font.pixelSize: 10
            horizontalAlignment: Text.AlignHCenter
            lineHeight: 0.86
            lineHeightMode: Text.ProportionalHeight
        }

        MouseArea {
            id: collapsedMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onPressed: collapsedBar.forceActiveFocus()
            onClicked: control.expandRequested()
        }
    }
}
