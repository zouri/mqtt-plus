pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

Rectangle {
    id: root

    required property var viewModel
    required property string currentScriptId
    required property AppUi ui

    signal scriptRequested(int index)

    Layout.preferredWidth: 300
    Layout.minimumWidth: 300
    Layout.maximumWidth: 300
    Layout.fillHeight: true
    color: root.ui.themePalette.windowBg

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        AppTextField {
            ui: root.ui
            Layout.fillWidth: true
            placeholderText: qsTr("Search script name or description")
            text: root.viewModel.filteredScripts.filterText
            onTextEdited: root.viewModel.setScriptFilterText(text)
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0

            ListView {
                id: scriptList
                anchors.fill: parent
                clip: true
                spacing: 8
                model: root.viewModel.filteredScripts
                reuseItems: true

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                delegate: Item {
                    id: scriptDelegate

                    required property int index
                    required property string id
                    required property string name
                    required property string description
                    required property string updatedAt

                    width: ListView.view.width
                    implicitHeight: 82
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Lua script %1").arg(scriptDelegate.name)

                    Rectangle {
                        anchors.fill: parent
                        radius: 10
                        color: scriptMouse.containsMouse
                               ? root.ui.rowHover
                               : root.ui.themePalette.itemBg
                        border.color: scriptDelegate.id === root.currentScriptId
                                      ? root.ui.themePalette.selectedBorder
                                      : root.ui.themePalette.itemBorder
                        border.width: 1

                        Column {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            anchors.topMargin: 10
                            anchors.bottomMargin: 10
                            spacing: 5

                            Label {
                                width: parent.width
                                text: scriptDelegate.name
                                color: root.ui.textStrong
                                font.pixelSize: 13
                                font.bold: true
                                elide: Label.ElideRight
                            }

                            Label {
                                width: parent.width
                                text: scriptDelegate.description.length > 0
                                      ? scriptDelegate.description
                                      : qsTr("Lua decoder · %1").arg(scriptDelegate.updatedAt || qsTr("Not saved"))
                                color: root.ui.themePalette.textSubtle
                                font.pixelSize: 11
                                elide: Label.ElideRight
                            }
                        }
                    }

                    Keys.onPressed: (event) => {
                        if (event.key === Qt.Key_Return
                                || event.key === Qt.Key_Enter
                                || event.key === Qt.Key_Space) {
                            root.scriptRequested(scriptDelegate.index)
                            event.accepted = true
                        }
                    }

                    MouseArea {
                        id: scriptMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor

                        onClicked: {
                            root.scriptRequested(scriptDelegate.index)
                        }
                    }
                }
            }
        }
    }
}
