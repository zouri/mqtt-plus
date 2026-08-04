pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

Rectangle {
    id: root

    required property AppUi ui
    required property var viewModel
    required property string currentDraftId

    signal draftRequested(string draftId)

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
            placeholderText: qsTr("Search drafts")
            text: root.viewModel.filteredDrafts.filterText
            onTextEdited: root.viewModel.setFilterText(text)
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0

            Label {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.topMargin: 18
                visible: root.viewModel.loading
                text: qsTr("Loading draft library…")
                color: root.ui.textMuted
                font.pixelSize: root.ui.textSm
                horizontalAlignment: Text.AlignHCenter
            }

            Label {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.topMargin: 18
                visible: !root.viewModel.loading && root.viewModel.filteredDrafts.count === 0
                text: root.viewModel.filteredDrafts.filterText.length > 0
                      ? qsTr("No matching drafts")
                      : qsTr("No saved drafts yet")
                color: root.ui.textMuted
                font.pixelSize: root.ui.textSm
                horizontalAlignment: Text.AlignHCenter
            }

            ListView {
                id: draftList

                anchors.fill: parent
                visible: !root.viewModel.loading && root.viewModel.filteredDrafts.count > 0
                model: root.viewModel.filteredDrafts
                spacing: 7
                clip: true
                reuseItems: true

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                delegate: Item {
                    id: draftDelegate

                    required property int index
                    required property string id
                    required property string name
                    required property string description
                    required property string defaultTopic
                    required property string formatName
                    required property int qos
                    required property bool retain
                    required property string updatedAt

                    width: ListView.view.width
                    implicitHeight: 82
                    activeFocusOnTab: true
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Draft %1").arg(draftDelegate.name)

                    Rectangle {
                        anchors.fill: parent
                        radius: root.ui.radiusMd
                        color: draftHover.hovered
                               ? root.ui.themePalette.rowHover
                               : root.ui.themePalette.itemBg
                        border.color: draftDelegate.id === root.currentDraftId
                                      ? root.ui.themePalette.selectedBorder
                                      : root.ui.themePalette.itemBorder
                        border.width: 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 11
                            anchors.rightMargin: 11
                            anchors.topMargin: 9
                            anchors.bottomMargin: 9
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Label {
                                    Layout.fillWidth: true
                                    text: draftDelegate.name
                                    color: root.ui.textStrong
                                    font.pixelSize: root.ui.textMd
                                    font.bold: true
                                    elide: Label.ElideRight
                                }

                                Label {
                                    visible: draftDelegate.retain
                                    text: qsTr("RETAIN")
                                    color: root.ui.themePalette.warningText
                                    font.pixelSize: root.ui.textXs
                                    font.bold: true
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: draftDelegate.description.length > 0
                                      ? draftDelegate.description
                                      : (draftDelegate.defaultTopic.length > 0
                                         ? draftDelegate.defaultTopic
                                         : qsTr("Topic requested when sending"))
                                color: root.ui.textMuted
                                font.pixelSize: root.ui.textXs
                                elide: Label.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("%1 · QoS %2").arg(draftDelegate.formatName).arg(draftDelegate.qos)
                                color: root.ui.themePalette.textSubtle
                                font.pixelSize: root.ui.textXs
                                elide: Label.ElideRight
                            }
                        }
                    }

                    HoverHandler {
                        id: draftHover
                        cursorShape: Qt.PointingHandCursor
                    }

                    TapHandler {
                        onTapped: root.draftRequested(draftDelegate.id)
                    }

                    Keys.onPressed: event => {
                        if (event.key === Qt.Key_Return
                                || event.key === Qt.Key_Enter
                                || event.key === Qt.Key_Space) {
                            root.draftRequested(draftDelegate.id)
                            event.accepted = true
                        }
                    }
                }
            }
        }
    }
}
