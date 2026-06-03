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

    signal scriptWorkspaceRequested()

    anchors.topMargin: 50
    color: ui.themePalette.windowBg
    // border.color: ui.themePalette.sidebarBorder

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        AppSectionHeader {
            ui: control.ui
            title: qsTr("Connections")
            titleSize: 15
        }

        ListView {
            id: sessionList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
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
                width: ListView.view.width
                height: 48
                radius: control.ui.innerRadius
                color: index === control.appController.currentSessionIndex
                       ? control.ui.themePalette.selectedBg
                       : (rowMouse.containsMouse || activeFocus ? control.ui.rowHover : control.ui.themePalette.itemBg)
                border.color: index === control.appController.currentSessionIndex
                              ? control.ui.themePalette.selectedBorder
                              : control.ui.themePalette.itemBorder
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Connection %1").arg(sessionDelegate.name)

                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Return
                            || event.key === Qt.Key_Enter
                            || event.key === Qt.Key_Space) {
                        control.appController.currentSessionIndex = sessionDelegate.index
                        event.accepted = true
                    } else if (event.key === Qt.Key_Menu
                               || (event.key === Qt.Key_F10 && event.modifiers & Qt.ShiftModifier)) {
                        control.appController.currentSessionIndex = sessionDelegate.index
                        sessionMenu.popup()
                        event.accepted = true
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 10
                    spacing: 8

                    Rectangle {
                        implicitWidth: 8
                        implicitHeight: 8
                        Layout.preferredWidth: 8
                        Layout.preferredHeight: 8
                        radius: 4
                        color: control.ui.stateColor(sessionDelegate.connectionState)
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        Label {
                            Layout.fillWidth: true
                            text: sessionDelegate.name
                            color: control.ui.textStrong
                            elide: Label.ElideRight
                            font.pixelSize: 13
                            font.bold: true
                        }
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
                            sessionMenu.popup()
                        }
                    }

                    onClicked: (mouse) => {
                        if (mouse.button === Qt.LeftButton) {
                            control.appController.currentSessionIndex = sessionDelegate.index
                        }
                    }
                }

                Menu {
                    id: sessionMenu

                    MenuItem {
                        text: qsTr("Edit")
                        onTriggered: control.sessionEditor.openForEdit(sessionDelegate.index)
                    }

                    MenuItem {
                        text: qsTr("Copy")
                        onTriggered: control.appController.duplicateSessionAt(sessionDelegate.index)
                    }

                    MenuItem {
                        text: qsTr("Delete")
                        enabled: control.appController.sessions.count > 1
                        onTriggered: control.appController.removeSessionAt(sessionDelegate.index)
                    }
                }
            }

            footer: Item {
                width: sessionList.width
                height: 52

                Rectangle {
                    id: addSessionDelegate
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 48
                    radius: control.ui.innerRadius
                    color: addRowMouse.containsMouse || activeFocus
                           ? control.ui.rowHover
                           : control.ui.themePalette.itemBg
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
                        anchors.leftMargin: 12
                        anchors.rightMargin: 10
                        spacing: 8

                        ToolButton {
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 20
                            padding: 0
                            display: AbstractButton.IconOnly
                            icon.source: control.ui.materialIcon("plus")
                            icon.width: 16
                            icon.height: 16
                            icon.color: control.ui.textMuted
                            Accessible.ignored: true

                            background: Item {
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("New connection")
                            color: control.ui.textMuted
                            elide: Label.ElideRight
                            font.pixelSize: 13
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

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            AppIconButton {
                ui: control.ui
                readonly property string currentMode: control.appController.themeMode
                readonly property var currentMeta: control.ui.themeModeMeta(currentMode)
                implicitWidth: 34
                implicitHeight: 34
                cornerRadius: 12
                iconSource: control.ui.materialIcon(currentMeta.icon)
                iconSize: 18
                toolTipText: qsTr("Theme: %1\nClick to switch to %2")
                             .arg(currentMeta.label)
                             .arg(control.ui.themeModeMeta(currentMeta.next).label)
                onClicked: control.appController.themeMode = currentMeta.next
            }

            AppIconButton {
                ui: control.ui
                implicitWidth: 34
                implicitHeight: 34
                cornerRadius: 12
                iconSource: control.ui.materialIcon("script-development")
                iconSize: 18
                toolTipText: qsTr("Lua scripts")
                onClicked: control.scriptWorkspaceRequested()
            }

            Item {
                Layout.fillWidth: true
            }
        }
    }
}
