pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

Item {
    id: control

    required property AppUi ui
    required property var viewModel

    readonly property var editor: control.viewModel.editor

    Layout.fillWidth: true
    Layout.fillHeight: true

    function newScript() {
        control.viewModel.newScript()
        nameField.selectAll()
    }

    Component.onCompleted: control.viewModel.ensureEditorSelection()

    Connections {
        target: control.viewModel

        function onScriptLibraryChanged() {
            control.viewModel.ensureEditorSelection()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: control.ui.themePalette.headerBg

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 10

                Label {
                    text: qsTr("Script Manager")
                    color: control.ui.textStrong
                    font.pixelSize: 18
                    font.bold: true
                }

                AppBadge {
                    ui: control.ui
                    label: `${control.viewModel.scripts.count}`
                    badgeRadius: 11
                    horizontalPadding: 8
                    verticalPadding: 4
                    badgeBg: control.ui.themePalette.selectedBg
                    badgeBorder: "transparent"
                    badgeText: control.ui.themePalette.infoText
                }

                Item {
                    Layout.fillWidth: true
                }

                AppButton {
                    ui: control.ui
                    text: qsTr("New Lua Script")
                    minimumWidth: 116
                    primary: true
                    onClicked: control.newScript()
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: control.ui.themePalette.separator
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            ScriptListPane {
                ui: control.ui
                viewModel: control.viewModel
                currentScriptId: control.editor.currentScriptId
                onScriptRequested: (index) => control.viewModel.selectScriptAt(index)
            }

            Rectangle {
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                color: control.ui.themePalette.separator
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.topMargin: 16
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    ColumnLayout {
                        Layout.preferredWidth: 360
                        spacing: 6

                        Label {
                            text: qsTr("Script name")
                            color: control.ui.textMuted
                            font.pixelSize: 12
                        }

                        AppTextField {
                            id: nameField
                            ui: control.ui
                            Layout.fillWidth: true
                            text: control.editor.name
                            placeholderText: qsTr("Script name")
                            onTextEdited: control.editor.name = text
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Label {
                            text: qsTr("Description")
                            color: control.ui.textMuted
                            font.pixelSize: 12
                        }

                        AppTextField {
                            id: descriptionField
                            ui: control.ui
                            Layout.fillWidth: true
                            text: control.editor.description
                            placeholderText: qsTr("Device protocol or payload structure")
                            onTextEdited: control.editor.description = text
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 8

                    Label {
                        text: qsTr("Lua decoder code")
                        color: control.ui.textMuted
                        font.pixelSize: 12
                    }

                    AppTextArea {
                        id: codeField
                        ui: control.ui
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 360
                        text: control.editor.code
                        font.family: "Menlo"
                        clip: true
                        showLineNumbers: false
                        wrapMode: TextEdit.NoWrap
                        placeholderText: qsTr("function parse(ctx)")
                        onTextChanged: {
                            if (text !== control.editor.code) {
                                control.editor.code = text
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 62
            color: control.ui.themePalette.headerBg

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: 300
                height: 1
                color: control.ui.themePalette.separator
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 324
                anchors.rightMargin: 24
                spacing: 10

                Label {
                    Layout.fillWidth: true
                    text: control.editor.hasUnsavedChanges ? qsTr("Unsaved") : control.editor.validationStatus
                    color: control.editor.hasUnsavedChanges
                           ? control.ui.textMuted
                           : (control.editor.validationOk
                              ? control.ui.stateColor("completed")
                              : control.ui.textMuted)
                    font.pixelSize: 13
                    elide: Label.ElideRight
                }

                AppButton {
                    ui: control.ui
                    text: qsTr("Validate structure")
                    minimumWidth: 98
                    onClicked: control.viewModel.validateEditorStructure()
                }

                AppButton {
                    ui: control.ui
                    text: qsTr("Save Script")
                    primary: true
                    enabled: control.editor.canSave
                    minimumWidth: 92
                    onClicked: control.viewModel.saveEditor()
                }
            }
        }
    }
}
