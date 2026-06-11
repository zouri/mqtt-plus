pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

Rectangle {
    id: root

    required property var appController
    required property string currentScriptId
    required property AppUi ui

    property string filterText: ""
    property int matchingScriptCount: 0

    signal scriptRequested(var row)

    Layout.preferredWidth: 280
    Layout.minimumWidth: 280
    Layout.maximumWidth: 280
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
        let visibleRows = 0
        const model = root.appController ? root.appController.scripts : null
        const rowCount = model ? model.count : 0
        for (let i = 0; i < rowCount; ++i) {
            const row = model.rowAt(i)
            if (root.rowMatches(row.name || "", row.description || "", row.code || "")) {
                visibleRows += 1
            }
        }
        root.matchingScriptCount = visibleRows
    }

    onFilterTextChanged: root.recomputeVisibleCount()
    Component.onCompleted: root.recomputeVisibleCount()

    Connections {
        target: root.appController ? root.appController.scripts : null

        function onCountChanged() {
            root.recomputeVisibleCount()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 22
        anchors.rightMargin: 12
        anchors.topMargin: 16
        anchors.bottomMargin: 16
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
            spacing: 0
            model: root.appController.scripts
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
                implicitHeight: matchesFilter ? 80 : 0
                visible: matchesFilter
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Lua script %1").arg(scriptDelegate.name)

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 8
                    radius: 8
                    color: scriptMouse.containsMouse || scriptDelegate.activeFocus
                           ? root.ui.rowHover
                           : root.ui.themePalette.windowBg
                    border.color: scriptDelegate.id === root.currentScriptId
                                  ? root.ui.themePalette.selectedBorder
                                  : "transparent"
                    border.width: scriptDelegate.id === root.currentScriptId ? 2 : 1

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
                            font.pixelSize: 14
                            font.bold: true
                            elide: Label.ElideRight
                        }

                        Label {
                            width: parent.width
                            text: qsTr("Lua decoder · %1").arg(scriptDelegate.updatedAt || qsTr("Not saved"))
                            color: root.ui.textMuted
                            font.pixelSize: 11
                            elide: Label.ElideRight
                        }
                    }
                }

                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Return
                            || event.key === Qt.Key_Enter
                            || event.key === Qt.Key_Space) {
                        root.scriptRequested(root.appController.scripts.rowAt(scriptDelegate.index))
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
                        root.scriptRequested(root.appController.scripts.rowAt(scriptDelegate.index))
                    }
                }
            }
        }
    }
}
