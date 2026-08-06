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
                    badgeBg: root.ui.themePalette.innerPanelBg
                    badgeBorder: "transparent"
                    badgeText: root.ui.textMuted
                }

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    id: newProcessorButton

                    Layout.preferredWidth: 164
                    Layout.preferredHeight: 34
                    leftPadding: 12
                    rightPadding: 34
                    spacing: 7
                    hoverEnabled: true
                    text: qsTr("New Processor")
                    font.pixelSize: 12
                    font.bold: true
                    display: AbstractButton.TextBesideIcon
                    icon.source: root.ui.materialIcon("plus")
                    icon.width: 16
                    icon.height: 16
                    icon.color: root.ui.textStrong
                    palette.buttonText: root.ui.textStrong
                    palette.text: root.ui.textStrong
                    onClicked: newProcessorMenu.open()

                    HoverHandler {
                        cursorShape: Qt.PointingHandCursor
                    }

                    background: Rectangle {
                        radius: 7
                        color: newProcessorButton.down
                               ? root.ui.themePalette.buttonPressedBg
                               : (newProcessorButton.hovered
                                  ? root.ui.themePalette.buttonHoverBg
                                  : root.ui.themePalette.buttonBg)
                        border.color: newProcessorButton.hovered
                                      ? root.ui.themePalette.fieldBorder
                                      : root.ui.themePalette.buttonBorder
                    }

                    AppIconButton {
                        ui: root.ui
                        anchors.right: parent.right
                        anchors.rightMargin: 5
                        anchors.verticalCenter: parent.verticalCenter
                        implicitWidth: 26
                        implicitHeight: 26
                        iconSize: 15
                        iconSource: root.ui.materialIcon("chevron-down")
                        restBg: "transparent"
                        hoverBg: "transparent"
                        pressedBg: "transparent"
                        outlineColor: "transparent"
                        onClicked: newProcessorMenu.open()
                        Accessible.ignored: true
                    }

                    AppPopover {
                        id: newProcessorMenu

                        ui: root.ui
                        x: newProcessorButton.width - width
                        y: newProcessorButton.height + 6
                        width: 220
                        padding: 6
                        closePolicy: Popup.CloseOnEscape
                                     | Popup.CloseOnPressOutside
                                     | Popup.CloseOnReleaseOutside

                        background: Rectangle {
                            radius: 8
                            color: root.ui.themePalette.dialogBg
                            border.color: root.ui.themePalette.dialogBorder
                        }

                        contentItem: ColumnLayout {
                            spacing: 2

                            Button {
                                id: newLuaButton

                                Layout.fillWidth: true
                                Layout.preferredHeight: 38
                                leftPadding: 10
                                rightPadding: 10
                                hoverEnabled: true
                                onClicked: {
                                    newProcessorMenu.close()
                                    root.viewModel.newProcessor("lua")
                                }

                                contentItem: RowLayout {
                                    spacing: 8

                                    Label {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        text: qsTr("Lua Processor")
                                        color: root.ui.textStrong
                                        font.pixelSize: 12
                                        elide: Label.ElideRight
                                    }

                                    Label {
                                        text: "Lua 5.5"
                                        color: root.ui.textMuted
                                        font.pixelSize: 10
                                    }
                                }

                                background: Rectangle {
                                    radius: 6
                                    color: newLuaButton.down
                                           ? root.ui.themePalette.actionPressedBg
                                           : (newLuaButton.hovered
                                              ? root.ui.themePalette.actionHoverBg
                                              : "transparent")
                                }
                            }

                            Button {
                                id: newJavaScriptButton

                                Layout.fillWidth: true
                                Layout.preferredHeight: 38
                                leftPadding: 10
                                rightPadding: 10
                                hoverEnabled: true
                                onClicked: {
                                    newProcessorMenu.close()
                                    root.viewModel.newProcessor("javascript")
                                }

                                contentItem: RowLayout {
                                    spacing: 8

                                    Label {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        text: qsTr("JavaScript Processor")
                                        color: root.ui.textStrong
                                        font.pixelSize: 12
                                        elide: Label.ElideRight
                                    }

                                    Label {
                                        text: qsTr("Qt runtime")
                                        color: root.ui.textMuted
                                        font.pixelSize: 10
                                    }
                                }

                                background: Rectangle {
                                    radius: 6
                                    color: newJavaScriptButton.down
                                           ? root.ui.themePalette.actionPressedBg
                                           : (newJavaScriptButton.hovered
                                              ? root.ui.themePalette.actionHoverBg
                                              : "transparent")
                                }
                            }
                        }
                    }
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
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Layout.topMargin: 8
                Layout.bottomMargin: 8
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            color: root.ui.themePalette.headerBg

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: 270
                height: 1
                color: root.ui.themePalette.separator
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 288
                anchors.rightMargin: 12
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    text: root.editor.hasUnsavedChanges
                          ? qsTr("Unsaved changes")
                          : (root.editor.validationOk
                             ? qsTr("Validated · current content is available")
                             : root.editor.validationStatus)
                    color: root.editor.hasUnsavedChanges
                           ? root.ui.textMuted
                           : (root.editor.validationOk
                              ? root.ui.stateColor("completed")
                              : root.ui.textMuted)
                    font.pixelSize: 11
                    elide: Label.ElideRight
                }

                Button {
                    id: validateButton

                    implicitWidth: 86
                    implicitHeight: 34
                    leftPadding: 12
                    rightPadding: 12
                    spacing: 7
                    hoverEnabled: true
                    text: qsTr("Validate")
                    font.pixelSize: 12
                    font.bold: true
                    display: AbstractButton.TextBesideIcon
                    icon.source: root.ui.materialIcon("check")
                    icon.width: 16
                    icon.height: 16
                    icon.color: root.ui.textStrong
                    palette.buttonText: root.ui.textStrong
                    palette.text: root.ui.textStrong
                    onClicked: root.viewModel.validateEditor()

                    HoverHandler {
                        cursorShape: Qt.PointingHandCursor
                    }

                    background: Rectangle {
                        radius: 7
                        color: validateButton.down
                               ? root.ui.themePalette.buttonPressedBg
                               : (validateButton.hovered
                                  ? root.ui.themePalette.buttonHoverBg
                                  : root.ui.themePalette.buttonBg)
                        border.color: validateButton.hovered
                                      ? root.ui.themePalette.fieldBorder
                                      : root.ui.themePalette.buttonBorder
                    }
                }

                Button {
                    id: deleteButton

                    implicitWidth: 68
                    implicitHeight: 34
                    leftPadding: 12
                    rightPadding: 12
                    hoverEnabled: true
                    text: qsTr("Delete")
                    enabled: root.editor.currentProcessorId.length > 0
                    font.pixelSize: 12
                    font.bold: true
                    palette.buttonText: root.ui.themePalette.errorText
                    palette.text: root.ui.themePalette.errorText
                    onClicked: deleteProcessorDialog.open()

                    HoverHandler {
                        cursorShape: deleteButton.enabled
                                     ? Qt.PointingHandCursor
                                     : Qt.ArrowCursor
                    }

                    background: Rectangle {
                        radius: 7
                        color: !deleteButton.enabled
                               ? root.ui.themePalette.disabledButtonBg
                               : (deleteButton.down
                                  ? root.ui.themePalette.buttonDangerPressedBg
                                  : (deleteButton.hovered
                                     ? root.ui.themePalette.errorBg
                                     : root.ui.themePalette.buttonBg))
                        border.color: deleteButton.enabled
                                      ? root.ui.themePalette.errorText
                                      : root.ui.themePalette.buttonBorder
                    }
                }

                AppButton {
                    ui: root.ui
                    text: qsTr("Save")
                    primary: true
                    enabled: root.editor.canSave
                    minimumWidth: 68
                    onClicked: root.viewModel.saveEditor()
                }
            }
        }
    }

    AppDialog {
        id: deleteProcessorDialog

        ui: root.ui
        width: 460
        height: root.editor.hasUnsavedChanges ? 230 : 210
        closePolicy: Popup.CloseOnEscape
        header: Item { implicitHeight: 0; visible: false }
        background: Rectangle {
            radius: root.ui.radiusLg
            color: root.ui.themePalette.dialogBg
            border.color: root.ui.themePalette.dialogBorder
        }
        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            Label {
                Layout.fillWidth: true
                text: qsTr("Delete processor?")
                color: root.ui.textStrong
                font.pixelSize: root.ui.text2xl
                font.bold: true
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("“%1” will be permanently removed from the Processor Library. This cannot be undone.").arg(root.editor.name)
                color: root.ui.textMuted
                font.pixelSize: root.ui.textSm
                wrapMode: Text.Wrap
            }

            Label {
                Layout.fillWidth: true
                visible: root.editor.hasUnsavedChanges
                text: qsTr("Unsaved changes will also be discarded.")
                color: root.ui.themePalette.warningText
                font.pixelSize: root.ui.textSm
                wrapMode: Text.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Item { Layout.fillWidth: true }

                AppButton {
                    ui: root.ui
                    text: qsTr("Cancel")
                    minimumWidth: 76
                    onClicked: deleteProcessorDialog.close()
                }

                AppButton {
                    ui: root.ui
                    text: qsTr("Delete")
                    danger: true
                    minimumWidth: 76
                    onClicked: {
                        deleteProcessorDialog.close()
                        root.viewModel.deleteCurrent()
                    }
                }
            }
        }
    }
}
