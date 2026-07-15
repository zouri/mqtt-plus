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

    property string filterText: ""
    property int matchingScriptCount: 0

    signal scriptRequested(int index)

    Layout.preferredWidth: 300
    Layout.minimumWidth: 300
    Layout.maximumWidth: 300
    Layout.fillHeight: true
    color: root.ui.themePalette.windowBg

    function rowMatches(name, description, code) {
        const needle = root.filterText.trim().toLowerCase()
        if (needle.length === 0) {
            return true
        }
        return `${name} ${description} ${code}`.toLowerCase().indexOf(needle) >= 0
    }

    function recomputeVisibleCount() {
        root.matchingScriptCount = root.viewModel ? root.viewModel.visibleScriptCount(root.filterText) : 0
    }

    onFilterTextChanged: root.recomputeVisibleCount()
    Component.onCompleted: root.recomputeVisibleCount()

    Connections {
        target: root.viewModel ? root.viewModel.scripts : null

        function onCountChanged() {
            root.recomputeVisibleCount()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        AppTextField {
            ui: root.ui
            Layout.fillWidth: true
            placeholderText: qsTr("Search script name or description")
            text: root.filterText
            onTextChanged: root.filterText = text
        }

        ListView {
            id: scriptList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            model: root.viewModel.scripts
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
                required property string code
                required property string updatedAt

                readonly property bool matchesFilter: root.rowMatches(
                                                          scriptDelegate.name,
                                                          scriptDelegate.description,
                                                          scriptDelegate.code)

                width: ListView.view.width
                implicitHeight: matchesFilter ? 82 : 0
                visible: matchesFilter
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Lua script %1").arg(scriptDelegate.name)

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
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
