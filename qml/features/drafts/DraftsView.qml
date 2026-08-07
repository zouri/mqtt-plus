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
    property string pendingEditorAction: ""
    property string pendingEditorDraftId: ""

    Layout.fillWidth: true
    Layout.fillHeight: true

    function runEditorAction(action, draftId) {
        if (action === "select") {
            root.viewModel.selectDraftById(draftId)
        } else if (action === "new") {
            root.viewModel.newDraft()
            Qt.callLater(function() { nameField.forceActiveFocus() })
        } else if (action === "duplicate") {
            root.viewModel.duplicateCurrentDraft()
            Qt.callLater(function() { nameField.forceActiveFocus() })
        }
    }

    function requestEditorAction(action, draftId) {
        if (!root.editor.hasUnsavedChanges) {
            root.runEditorAction(action, draftId)
            return
        }
        root.pendingEditorAction = action
        root.pendingEditorDraftId = draftId
        unsavedChangesDialog.open()
    }

    function continuePendingEditorAction() {
        const action = root.pendingEditorAction
        const draftId = root.pendingEditorDraftId
        root.pendingEditorAction = ""
        root.pendingEditorDraftId = ""
        root.runEditorAction(action, draftId)
    }

    Component.onCompleted: root.viewModel.ensureEditorSelection()

    Connections {
        target: root.viewModel

        function onEditorSaveSucceeded() {
            if (root.pendingEditorAction.length > 0) {
                root.continuePendingEditorAction()
            }
        }

        function onLibraryStateChanged() {
            if (!root.viewModel.busy
                    && root.viewModel.storageError.length > 0
                    && root.pendingEditorAction.length > 0) {
                root.pendingEditorAction = ""
                root.pendingEditorDraftId = ""
            }
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
                    text: qsTr("Draft Library")
                    color: root.ui.textStrong
                    font.pixelSize: root.ui.text2xl
                    font.bold: true
                }

                AppBadge {
                    ui: root.ui
                    label: `${root.viewModel.filteredDrafts.count}`
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

                AppButton {
                    ui: root.ui
                    Layout.preferredHeight: 34
                    minimumWidth: 116
                    outlined: true
                    text: qsTr("New Draft")
                    icon.source: root.ui.materialIcon("plus")
                    icon.width: 16
                    icon.height: 16
                    enabled: root.viewModel.ready && !root.viewModel.busy && !root.viewModel.readOnly
                    onClicked: root.requestEditorAction("new", "")
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

            DraftListPane {
                ui: root.ui
                viewModel: root.viewModel
                currentDraftId: root.editor.currentDraftId
                onDraftRequested: draftId => root.requestEditorAction("select", draftId)
            }

            Rectangle {
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                color: root.ui.themePalette.separator
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Layout.topMargin: 8
                Layout.bottomMargin: 8
                spacing: 8

                RowLayout {
                    visible: root.viewModel.storageError.length > 0
                    Layout.fillWidth: true
                    spacing: 8

                    AppInlineAlert {
                        ui: root.ui
                        Layout.fillWidth: true
                        type: "error"
                        text: root.viewModel.storageError
                    }

                    AppButton {
                        ui: root.ui
                        visible: root.viewModel.canRecover
                        text: qsTr("Restore backup")
                        minimumWidth: 104
                        enabled: !root.viewModel.busy
                        onClicked: root.viewModel.recoverBackup()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 240
                        Layout.horizontalStretchFactor: 3
                        spacing: 5

                        Label {
                            text: qsTr("Draft name")
                            color: root.ui.textMuted
                            font.pixelSize: 11
                        }

                        AppTextField {
                            id: nameField

                            ui: root.ui
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.ui.compactControlHeight
                            enabled: root.viewModel.ready && !root.viewModel.busy && !root.viewModel.readOnly
                            maximumLength: 80
                            text: root.editor.name
                            placeholderText: qsTr("Required, unique across the library")
                            onTextEdited: root.editor.name = text
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 320
                        Layout.horizontalStretchFactor: 5
                        spacing: 5

                        Label {
                            text: qsTr("Description")
                            color: root.ui.textMuted
                            font.pixelSize: 11
                        }

                        AppTextField {
                            ui: root.ui
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.ui.compactControlHeight
                            enabled: root.viewModel.ready && !root.viewModel.busy && !root.viewModel.readOnly
                            maximumLength: 500
                            text: root.editor.description
                            placeholderText: qsTr("Optional searchable note")
                            onTextEdited: root.editor.description = text
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 280
                        spacing: 5

                        Label {
                            text: qsTr("Default Topic")
                            color: root.ui.textMuted
                            font.pixelSize: 11
                        }

                        AppTextField {
                            ui: root.ui
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.ui.compactControlHeight
                            enabled: root.viewModel.ready && !root.viewModel.busy && !root.viewModel.readOnly
                            text: root.editor.defaultTopic
                            placeholderText: qsTr("Optional; requested when sending if empty")
                            onTextEdited: root.editor.defaultTopic = text
                        }
                    }

                    ColumnLayout {
                        Layout.preferredWidth: 100
                        spacing: 5

                        Label {
                            text: qsTr("QoS")
                            color: root.ui.textMuted
                            font.pixelSize: 11
                        }

                        AppComboBox {
                            ui: root.ui
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.ui.compactControlHeight
                            enabled: root.viewModel.ready && !root.viewModel.busy && !root.viewModel.readOnly
                            model: [qsTr("QoS 0"), qsTr("QoS 1"), qsTr("QoS 2")]
                            currentIndex: root.editor.qos
                            onActivated: root.editor.qos = currentIndex
                        }
                    }

                    ColumnLayout {
                        Layout.preferredWidth: 132
                        spacing: 5

                        Label {
                            text: qsTr("Format")
                            color: root.ui.textMuted
                            font.pixelSize: 11
                        }

                        AppComboBox {
                            ui: root.ui
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.ui.compactControlHeight
                            enabled: root.viewModel.ready && !root.viewModel.busy && !root.viewModel.readOnly
                            model: root.viewModel.payloadFormats
                            currentIndex: root.editor.format
                            onActivated: root.editor.format = currentIndex
                        }
                    }

                    ColumnLayout {
                        Layout.preferredWidth: 84
                        spacing: 5

                        Label {
                            text: qsTr("Delivery")
                            color: root.ui.textMuted
                            font.pixelSize: 11
                        }

                        AppCheckBox {
                            ui: root.ui
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.ui.compactControlHeight
                            enabled: root.viewModel.ready && !root.viewModel.busy && !root.viewModel.readOnly
                            text: qsTr("Retain")
                            checked: root.editor.retain
                            onToggled: root.editor.retain = checked
                        }
                    }
                }

                AppInlineAlert {
                    ui: root.ui
                    visible: root.editor.retain
                    Layout.fillWidth: true
                    type: "warning"
                    text: qsTr("Retain is enabled. The broker may replace its retained message for this Topic.")
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 5

                    Label {
                        text: qsTr("Payload")
                        color: root.ui.textMuted
                        font.pixelSize: 11
                    }

                    AppTextArea {
                        ui: root.ui
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 260
                        enabled: root.viewModel.ready && !root.viewModel.busy && !root.viewModel.readOnly
                        text: root.editor.payload
                        wrapMode: TextEdit.Wrap
                        placeholderText: qsTr("Empty payload is valid")
                        onTextChanged: {
                            if (text !== root.editor.payload) {
                                root.editor.payload = text
                            }
                        }
                    }
                }
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
                    text: root.editor.validationError.length > 0
                          ? root.editor.validationError
                          : (root.viewModel.busy
                             ? qsTr("Saving…")
                             : (root.editor.hasUnsavedChanges ? qsTr("Unsaved changes") : qsTr("Saved")))
                    color: root.editor.validationError.length > 0
                           ? root.ui.themePalette.errorText
                           : root.ui.textMuted
                    font.pixelSize: 11
                    elide: Label.ElideRight
                }

                AppButton {
                    ui: root.ui
                    Layout.preferredHeight: 34
                    minimumWidth: 86
                    outlined: true
                    text: qsTr("Duplicate")
                    icon.source: root.ui.materialIcon("content-copy")
                    icon.width: 16
                    icon.height: 16
                    enabled: root.editor.currentDraftId.length > 0
                             && !root.viewModel.busy
                             && !root.viewModel.readOnly
                    onClicked: root.requestEditorAction("duplicate", "")
                }

                AppButton {
                    ui: root.ui
                    Layout.preferredHeight: 34
                    minimumWidth: 68
                    outlined: true
                    danger: true
                    text: qsTr("Delete")
                    enabled: root.editor.currentDraftId.length > 0
                             && !root.viewModel.busy
                             && !root.viewModel.readOnly
                    onClicked: deleteDraftDialog.open()
                }

                AppButton {
                    ui: root.ui
                    text: qsTr("Save Draft")
                    primary: true
                    minimumWidth: 92
                    enabled: root.editor.canSave
                             && root.viewModel.ready
                             && !root.viewModel.busy
                             && !root.viewModel.readOnly
                    onClicked: root.viewModel.saveEditor()
                }
            }
        }
    }

    AppDialog {
        id: unsavedChangesDialog

        ui: root.ui
        width: 470
        height: 210
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
                text: qsTr("Save changes to this draft?")
                color: root.ui.textStrong
                font.pixelSize: root.ui.text2xl
                font.bold: true
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("You have unsaved changes. Save them before continuing, or discard them.")
                color: root.ui.textMuted
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
                    onClicked: {
                        root.pendingEditorAction = ""
                        root.pendingEditorDraftId = ""
                        unsavedChangesDialog.close()
                    }
                }

                AppButton {
                    ui: root.ui
                    text: qsTr("Discard")
                    danger: true
                    minimumWidth: 76
                    onClicked: {
                        unsavedChangesDialog.close()
                        root.continuePendingEditorAction()
                    }
                }

                AppButton {
                    ui: root.ui
                    text: qsTr("Save")
                    primary: true
                    minimumWidth: 76
                    enabled: root.editor.canSave && !root.viewModel.busy
                    onClicked: {
                        if (root.viewModel.saveEditor()) {
                            unsavedChangesDialog.close()
                        }
                    }
                }
            }
        }
    }

    AppDialog {
        id: deleteDraftDialog

        ui: root.ui
        width: 440
        height: 190
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
                text: qsTr("Delete draft?")
                color: root.ui.textStrong
                font.pixelSize: root.ui.text2xl
                font.bold: true
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("“%1” will be permanently removed from the Draft Library.").arg(root.editor.name)
                color: root.ui.textMuted
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
                    onClicked: deleteDraftDialog.close()
                }

                AppButton {
                    ui: root.ui
                    text: qsTr("Delete")
                    danger: true
                    minimumWidth: 76
                    enabled: !root.viewModel.busy
                    onClicked: {
                        if (root.viewModel.deleteCurrentDraft()) {
                            deleteDraftDialog.close()
                        }
                    }
                }
            }
        }
    }

}
