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
            placeholderText: qsTr("Search drafts")
            text: root.viewModel.filteredDrafts.filterText
            onTextEdited: root.viewModel.setFilterText(text)

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
                    readonly property bool selected: draftDelegate.id === root.currentDraftId

                    width: ListView.view.width
                    implicitHeight: 68
                    activeFocusOnTab: true
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Draft %1").arg(draftDelegate.name)

                    AppSelectableCard {
                        anchors.fill: parent
                        ui: root.ui
                        selected: draftDelegate.selected
                        hovered: draftHover.hovered

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
                                    text: draftDelegate.name
                                    color: root.ui.textStrong
                                    font.pixelSize: 12
                                    font.bold: true
                                    elide: Label.ElideRight
                                }

                                Rectangle {
                                    visible: draftDelegate.retain
                                    Layout.preferredWidth: retainLabel.implicitWidth + 12
                                    Layout.preferredHeight: 20
                                    radius: 5
                                    color: root.ui.themePalette.innerPanelBg

                                    Label {
                                        id: retainLabel

                                        anchors.centerIn: parent
                                        text: qsTr("RETAIN")
                                        color: root.ui.themePalette.warningText
                                        font.pixelSize: 9
                                        font.bold: true
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Label {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    text: qsTr("%1 · QoS %2").arg(draftDelegate.formatName).arg(draftDelegate.qos)
                                    color: root.ui.textMuted
                                    font.pixelSize: 10
                                    elide: Label.ElideRight
                                }

                                Label {
                                    Layout.preferredWidth: Math.min(120, implicitWidth)
                                    Layout.minimumWidth: 0
                                    Layout.maximumWidth: 120
                                    text: draftDelegate.updatedAt
                                    color: root.ui.themePalette.textSubtle
                                    font.pixelSize: 10
                                    horizontalAlignment: Text.AlignRight
                                    elide: Label.ElideRight
                                }
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
