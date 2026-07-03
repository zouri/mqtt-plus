pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Shapes
import "../../components"

Rectangle {
    id: control

    required property AppUi ui
    required property var viewModel
    property bool collapsed: false
    property int sessionContextIndex: -1
    readonly property bool canDeleteSession: sessionList.count > 1

    signal sessionCreateRequested
    signal sessionEditRequested(int index)
    signal collapseRequested
    signal expandRequested

    color: ui.themePalette.panelBg
    clip: true

    function sessionActionLabel(actionId) {
        if (actionId === "edit") {
            return qsTr("Edit");
        }
        if (actionId === "copy") {
            return qsTr("Copy");
        }
        if (actionId === "delete") {
            return qsTr("Delete");
        }
        return "";
    }

    function openSessionContextMenu(index) {
        control.sessionContextIndex = index;
        sessionContextMenu.open();
    }

    ListModel {
        id: sessionContextActions

        ListElement { actionId: "edit" }
        ListElement { actionId: "copy" }
        ListElement { actionId: "delete" }
    }

    AppPlatformMenu {
        id: sessionContextMenu

        model: sessionContextActions
        actionText: actionId => control.sessionActionLabel(actionId)
        actionEnabled: actionId => actionId !== "delete" || control.canDeleteSession

        onTriggered: actionId => {
            if (actionId === "edit") {
                control.sessionEditRequested(control.sessionContextIndex);
            } else if (actionId === "copy") {
                control.viewModel.requestSessionDuplicate(control.sessionContextIndex);
            } else if (actionId === "delete") {
                control.viewModel.requestSessionDelete(control.sessionContextIndex);
            }
        }

        onAboutToHide: Qt.callLater(function() {
            control.sessionContextIndex = -1;
        })
    }

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
                font.pixelSize: 18
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
            currentIndex: control.viewModel.currentSessionIndex
            model: control.viewModel.sessions
            reuseItems: true

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: Rectangle {
                id: sessionDelegate
                required property int index
                required property string name
                readonly property bool selected: index === control.viewModel.currentSessionIndex
                width: ListView.view.width
                height: 42
                radius: control.ui.innerRadius
                color: sessionDelegate.selected ? control.ui.themePalette.selectedBg : (rowMouse.containsMouse || activeFocus ? control.ui.rowHover : control.ui.themePalette.itemBg)
                border.color: sessionDelegate.selected ? Qt.rgba(control.ui.themePalette.selectedBorder.r, control.ui.themePalette.selectedBorder.g, control.ui.themePalette.selectedBorder.b, 0.36) : control.ui.themePalette.itemBorder
                border.width: 1
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Connection %1").arg(sessionDelegate.name)

                function openSessionContextMenu() {
                    control.openSessionContextMenu(sessionDelegate.index);
                }

                Keys.onPressed: event => {
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
                        control.viewModel.currentSessionIndex = sessionDelegate.index;
                        event.accepted = true;
                    } else if (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && event.modifiers & Qt.ShiftModifier)) {
                        sessionDelegate.openSessionContextMenu();
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

                    Label {
                        Layout.fillWidth: true
                        text: sessionDelegate.name
                        color: control.ui.textStrong
                        elide: Label.ElideRight
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 13
                        font.bold: true
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
                    }

                    onClicked: mouse => {
                        if (mouse.button === Qt.LeftButton) {
                            control.viewModel.currentSessionIndex = sessionDelegate.index;
                        } else if (mouse.button === Qt.RightButton) {
                            sessionDelegate.openSessionContextMenu();
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
                            control.sessionCreateRequested();
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
                        onClicked: control.sessionCreateRequested()
                    }
                }
            }
        }
    }

    Rectangle {
        id: collapsedBar

        readonly property bool hot: collapsedMouse.containsMouse

        visible: control.collapsed
        anchors.fill: parent
        color: collapsedBar.hot ? control.ui.themePalette.rowHover : control.ui.themePalette.panelBg
        border.color: "transparent"
        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Show connection list")

        Behavior on color {
            ColorAnimation {
                duration: 120
                easing.type: Easing.OutCubic
            }
        }

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
            symbolColor: collapsedBar.hot ? control.ui.themePalette.infoText : control.ui.textMuted
            accessibleName: qsTr("Show connection list")
            onClicked: control.expandRequested()
        }

        Label {
            anchors.centerIn: parent
            width: parent.width
            text: qsTr("Expand").split("").join("\n")
            color: collapsedBar.hot ? control.ui.themePalette.infoText : control.ui.textMuted
            font.pixelSize: 10
            horizontalAlignment: Text.AlignHCenter
            lineHeight: 0.86
            lineHeightMode: Text.ProportionalHeight

            Behavior on color {
                ColorAnimation {
                    duration: 120
                    easing.type: Easing.OutCubic
                }
            }
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
