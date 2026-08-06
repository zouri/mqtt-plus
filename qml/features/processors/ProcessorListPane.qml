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

    Layout.preferredWidth: 270
    Layout.minimumWidth: 230
    Layout.maximumWidth: 270
    Layout.fillHeight: true
    color: root.ui.themePalette.sidebarBg

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        AppTextField {
            id: searchField

            ui: root.ui
            Layout.fillWidth: true
            leftPadding: 34
            placeholderText: qsTr("Search name or source code")
            text: root.viewModel.filteredProcessors.filterText
            onTextEdited: root.viewModel.setProcessorFilterText(text)

            AppIconButton {
                ui: root.ui
                anchors.left: parent.left
                anchors.leftMargin: 3
                anchors.verticalCenter: parent.verticalCenter
                implicitWidth: 28
                implicitHeight: 28
                iconSize: 16
                iconSource: root.ui.materialIcon("search")
                restBg: "transparent"
                hoverBg: "transparent"
                pressedBg: "transparent"
                outlineColor: "transparent"
                onClicked: searchField.forceActiveFocus()
                Accessible.ignored: true
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0

            ListView {
                id: processorList

                anchors.fill: parent
                clip: true
                spacing: 7
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
                    required property string readinessState
                    required property string updatedAt

                    width: ListView.view.width
                    implicitHeight: 68
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Message Processor %1").arg(processorDelegate.name)
                    activeFocusOnTab: true

                    Rectangle {
                        anchors.fill: parent
                        radius: 8
                        color: processorMouse.containsMouse
                               ? root.ui.rowHover
                               : root.ui.themePalette.itemBg
                        border.color: processorDelegate.id === root.currentProcessorId
                                      ? root.ui.textStrong
                                      : (processorMouse.containsMouse
                                         ? root.ui.themePalette.fieldBorder
                                         : root.ui.themePalette.itemBorder)
                        border.width: 1

                        Rectangle {
                            visible: processorDelegate.id === root.currentProcessorId
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 3
                            radius: 2
                            color: root.ui.textStrong
                            Accessible.ignored: true
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            anchors.topMargin: 8
                            anchors.bottomMargin: 8
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Label {
                                    Layout.fillWidth: true
                                    text: processorDelegate.name
                                    color: root.ui.textStrong
                                    font.pixelSize: 12
                                    font.bold: true
                                    elide: Label.ElideRight
                                }

                                Rectangle {
                                    Layout.preferredWidth: languageLabel.implicitWidth + 12
                                    Layout.preferredHeight: 20
                                    radius: 5
                                    color: root.ui.themePalette.innerPanelBg

                                    Label {
                                        id: languageLabel

                                        anchors.centerIn: parent
                                        text: processorDelegate.languageName
                                        color: root.ui.textMuted
                                        font.pixelSize: 9
                                        font.bold: true
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Label {
                                    text: processorDelegate.readinessState === "ready"
                                          ? qsTr("Ready")
                                          : qsTr("Unavailable")
                                    color: root.ui.textMuted
                                    font.pixelSize: 10
                                }

                                Item {
                                    Layout.fillWidth: true
                                }

                                Label {
                                    text: processorDelegate.updatedAt
                                    color: root.ui.themePalette.textSubtle
                                    font.pixelSize: 10
                                    elide: Label.ElideRight
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
