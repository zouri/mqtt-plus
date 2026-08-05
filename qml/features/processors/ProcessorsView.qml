pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

Item {
    id: root

    required property AppUi ui
    required property var viewModel

    readonly property var editor: root.viewModel.editor

    Layout.fillWidth: true
    Layout.fillHeight: true

    Component.onCompleted: root.viewModel.ensureEditorSelection()

    Connections {
        target: root.viewModel

        function onProcessorLibraryChanged() {
            root.viewModel.ensureEditorSelection()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: root.ui.themePalette.headerBg

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 10

                Label {
                    text: qsTr("Processor Library")
                    color: root.ui.textStrong
                    font.pixelSize: 18
                    font.bold: true
                }

                AppBadge {
                    ui: root.ui
                    label: `${root.viewModel.filteredProcessors.count}`
                    badgeRadius: 11
                    horizontalPadding: 8
                    verticalPadding: 4
                    badgeBg: root.ui.themePalette.selectedBg
                    badgeBorder: "transparent"
                    badgeText: root.ui.themePalette.infoText
                }

                Item {
                    Layout.fillWidth: true
                }

                AppButton {
                    ui: root.ui
                    text: qsTr("New Lua")
                    minimumWidth: 88
                    onClicked: root.viewModel.newProcessor("lua")
                }

                AppButton {
                    ui: root.ui
                    text: qsTr("New JavaScript")
                    minimumWidth: 118
                    primary: true
                    onClicked: root.viewModel.newProcessor("javascript")
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: root.ui.themePalette.separator
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            ProcessorListPane {
                ui: root.ui
                viewModel: root.viewModel
                currentProcessorId: root.editor.currentProcessorId
                onProcessorRequested: (index) => root.viewModel.selectFilteredProcessorAt(index)
            }

            Rectangle {
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                color: root.ui.themePalette.separator
            }

            ProcessorEditor {
                ui: root.ui
                viewModel: root.viewModel
                editor: root.editor
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 22
                Layout.rightMargin: 22
                Layout.topMargin: 14
                Layout.bottomMargin: 14
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 62
            color: root.ui.themePalette.headerBg

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: 320
                height: 1
                color: root.ui.themePalette.separator
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 344
                anchors.rightMargin: 22
                spacing: 10

                Label {
                    Layout.fillWidth: true
                    text: root.editor.hasUnsavedChanges
                          ? qsTr("Unsaved revision")
                          : root.editor.validationStatus
                    color: root.editor.hasUnsavedChanges
                           ? root.ui.textMuted
                           : (root.editor.validationOk
                              ? root.ui.stateColor("completed")
                              : root.ui.textMuted)
                    font.pixelSize: 13
                    elide: Label.ElideRight
                }

                AppButton {
                    ui: root.ui
                    text: root.editor.archived ? qsTr("Restore") : qsTr("Archive")
                    minimumWidth: 86
                    danger: !root.editor.archived
                    enabled: root.editor.archived
                             ? root.editor.canRestore
                             : root.editor.canArchive
                    onClicked: {
                        if (root.editor.archived) {
                            root.viewModel.restoreCurrent()
                        } else {
                            root.viewModel.archiveCurrent()
                        }
                    }
                }

                AppButton {
                    ui: root.ui
                    text: qsTr("Validate")
                    minimumWidth: 86
                    enabled: !root.editor.archived
                    onClicked: root.viewModel.validateEditor()
                }

                AppButton {
                    ui: root.ui
                    text: qsTr("Save Revision")
                    primary: true
                    enabled: root.editor.canSave
                    minimumWidth: 110
                    onClicked: root.viewModel.saveEditor()
                }
            }
        }
    }
}
