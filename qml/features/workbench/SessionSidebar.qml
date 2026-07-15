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
    property real visibleWidth: width
    property int sessionContextIndex: -1
    readonly property bool canDeleteSession: sessionList.count > 1
    readonly property color sidebarBg: control.ui.themePalette.sidebarBg
    readonly property color selectedSessionBg: control.ui.themePalette.selectedItemBg

    signal sessionCreateRequested
    signal sessionEditRequested(int index)
    signal collapseRequested
    signal expandRequested

    color: control.sidebarBg
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

        ListElement {
            actionId: "edit"
        }
        ListElement {
            actionId: "copy"
        }
        ListElement {
            actionId: "delete"
        }
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

            control.sessionContextIndex = -1;
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 0
        visible: !control.collapsed
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            Layout.leftMargin: 12
            Layout.rightMargin: 8
            spacing: 6

            Label {
                Layout.fillWidth: true
                text: qsTr("Connections")
                color: control.ui.textStrong
                elide: Label.ElideRight
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
                iconSource: control.ui.materialIcon("plus")
                iconSize: 16
                implicitWidth: 28
                implicitHeight: 28
                cornerRadius: 7
                restBg: control.ui.themePalette.itemBg
                hoverBg: control.ui.themePalette.rowHover
                pressedBg: control.ui.themePalette.actionPressedBg
                outlineColor: control.ui.themePalette.panelBorder
                accessibleName: qsTr("New connection")
                toolTipText: qsTr("New connection")
                onClicked: control.sessionCreateRequested()
            }

            AppIconButton {
                id: collapseButton
                ui: control.ui
                iconSource: control.ui.materialIcon("chevron-left")
                iconSize: 18
                implicitWidth: 24
                implicitHeight: 24
                cornerRadius: 7
                restBg: "transparent"
                hoverBg: control.sidebarBg
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
            Layout.margins: 8
            clip: true
            spacing: 4
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
                required property string host
                required property int port
                required property string connectionState
                required property string protocolVersionName
                required property string lastError
                readonly property bool selected: index === control.viewModel.currentSessionIndex
                readonly property bool highlighted: rowHover.hovered || activeFocus
                readonly property bool connected: sessionDelegate.connectionState === "connected"
                readonly property string endpointText: sessionDelegate.lastError.length > 0 ? sessionDelegate.lastError : qsTr("%1:%2").arg(sessionDelegate.host || "-").arg(sessionDelegate.port)
                width: ListView.view.width
                height: 54
                radius: 10
                opacity: sessionDelegate.connectionState === "disconnecting" ? 0.72 : 1.0
                color: sessionDelegate.selected ? control.selectedSessionBg : (sessionDelegate.highlighted ? control.ui.themePalette.rowHover : "transparent")
                border.color: "transparent"
                border.width: 0
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Connection %1").arg(sessionDelegate.name)

                HoverHandler {
                    id: rowHover

                    cursorShape: Qt.PointingHandCursor
                }

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
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 10
                    z: 1

                    Rectangle {
                        Layout.preferredWidth: 8
                        Layout.preferredHeight: 8
                        radius: 4
                        color: sessionDelegate.lastError.length > 0 ? control.ui.themePalette.errorText : (sessionDelegate.connected ? control.ui.themePalette.successText : "#9aa4b2")
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 5

                            Label {
                                Layout.fillWidth: true
                                text: sessionDelegate.name
                                color: sessionDelegate.selected ? control.ui.themePalette.infoText : control.ui.textStrong
                                elide: Label.ElideRight
                                verticalAlignment: Text.AlignVCenter
                                font.pixelSize: 13
                                font.bold: true
                            }

                            AppBadge {
                                ui: control.ui
                                visible: sessionDelegate.protocolVersionName !== "MQTT 5"
                                label: sessionDelegate.protocolVersionName.replace("MQTT ", "")
                                horizontalPadding: 5
                                verticalPadding: 1
                                badgeRadius: 5
                                strong: true
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: sessionDelegate.endpointText
                            color: control.ui.themePalette.textSubtle
                            elide: Label.ElideRight
                            font.pixelSize: 11
                        }
                    }

                    AppIconButton {
                        ui: control.ui
                        Layout.preferredWidth: 24
                        Layout.preferredHeight: 24
                        visible: sessionDelegate.selected || sessionDelegate.highlighted
                        iconSource: control.ui.materialIcon("more-horiz")
                        iconSize: 15
                        cornerRadius: 6
                        restBg: "transparent"
                        hoverBg: control.ui.themePalette.rowHover
                        outlineColor: "transparent"
                        symbolColor: control.ui.textMuted
                        accessibleName: qsTr("Connection actions")
                        onClicked: sessionDelegate.openSessionContextMenu()
                    }
                }

                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton

                    onClicked: mouse => {
                        if (mouse.button === Qt.LeftButton) {
                            sessionDelegate.forceActiveFocus();
                            control.viewModel.currentSessionIndex = sessionDelegate.index;
                        } else if (mouse.button === Qt.RightButton) {
                            sessionDelegate.openSessionContextMenu();
                        }
                    }
                }
            }

            footer: Item {
                width: sessionList.width
                height: 0

                Rectangle {
                    id: addSessionDelegate
                    visible: false
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 42
                    radius: control.ui.innerRadius
                    color: "transparent"
                    readonly property bool focusIndicatorVisible: control.ui.showFocusIndicators && addSessionDelegate.activeFocus
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
                            strokeColor: addRowMouse.containsMouse || addSessionDelegate.focusIndicatorVisible ? control.ui.themePalette.selectedBorder : control.ui.themePalette.itemBorder
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
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: control.visibleWidth
        color: collapsedBar.hot ? control.ui.themePalette.rowHover : control.sidebarBg
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
