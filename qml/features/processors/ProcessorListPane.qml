pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

Rectangle {
    id: root

    required property var viewModel
    required property string currentProcessorId
    required property AppUi ui

    signal processorRequested(int index)

    Layout.preferredWidth: 320
    Layout.minimumWidth: 320
    Layout.maximumWidth: 320
    Layout.fillHeight: true
    color: root.ui.themePalette.windowBg

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        AppTextField {
            ui: root.ui
            Layout.fillWidth: true
            placeholderText: qsTr("Search processors, languages, or source")
            text: root.viewModel.filteredProcessors.filterText
            onTextEdited: root.viewModel.setProcessorFilterText(text)
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0

            ListView {
                id: processorList

                anchors.fill: parent
                clip: true
                spacing: 8
                model: root.viewModel.filteredProcessors
                reuseItems: true

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                delegate: Item {
                    id: processorDelegate

                    required property int index
                    required property string id
                    required property string name
                    required property string description
                    required property string languageName
                    required property int currentRevisionNumber
                    required property string readinessState
                    required property bool archived
                    required property string updatedAt

                    width: ListView.view.width
                    implicitHeight: 98
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Message Processor %1").arg(processorDelegate.name)
                    activeFocusOnTab: true

                    Rectangle {
                        anchors.fill: parent
                        radius: 10
                        color: processorMouse.containsMouse
                               ? root.ui.rowHover
                               : root.ui.themePalette.itemBg
                        border.color: processorDelegate.id === root.currentProcessorId
                                      ? root.ui.themePalette.selectedBorder
                                      : root.ui.themePalette.itemBorder
                        border.width: 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            anchors.topMargin: 9
                            anchors.bottomMargin: 9
                            spacing: 5

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Label {
                                    Layout.fillWidth: true
                                    text: processorDelegate.name
                                    color: root.ui.textStrong
                                    font.pixelSize: 13
                                    font.bold: true
                                    elide: Label.ElideRight
                                }

                                AppBadge {
                                    ui: root.ui
                                    label: processorDelegate.languageName
                                    horizontalPadding: 7
                                    verticalPadding: 3
                                    badgeRadius: 8
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: processorDelegate.description.length > 0
                                      ? processorDelegate.description
                                      : qsTr("No description")
                                color: root.ui.themePalette.textSubtle
                                font.pixelSize: 11
                                elide: Label.ElideRight
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Label {
                                    text: qsTr("Revision %1").arg(processorDelegate.currentRevisionNumber)
                                    color: root.ui.textMuted
                                    font.pixelSize: 10
                                }

                                Label {
                                    text: processorDelegate.archived
                                          ? qsTr("Archived")
                                          : (processorDelegate.readinessState === "ready"
                                             ? qsTr("Ready")
                                             : qsTr("Unavailable"))
                                    color: processorDelegate.archived
                                           ? root.ui.textMuted
                                           : (processorDelegate.readinessState === "ready"
                                              ? root.ui.stateColor("completed")
                                              : root.ui.stateColor("error"))
                                    font.pixelSize: 10
                                    font.bold: true
                                }

                                Item {
                                    Layout.fillWidth: true
                                }

                                Label {
                                    text: processorDelegate.updatedAt
                                    color: root.ui.themePalette.textSubtle
                                    font.pixelSize: 10
                                }
                            }
                        }
                    }

                    Keys.onPressed: (event) => {
                        if (event.key === Qt.Key_Return
                                || event.key === Qt.Key_Enter
                                || event.key === Qt.Key_Space) {
                            root.processorRequested(processorDelegate.index)
                            event.accepted = true
                        }
                    }

                    MouseArea {
                        id: processorMouse

                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.processorRequested(processorDelegate.index)
                    }
                }
            }
        }
    }
}
