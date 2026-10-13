pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

Item {
    id: root

    required property AppUi ui
    required property var viewModel
    readonly property var editor: root.viewModel
    readonly property var details: root.viewModel.selectedDetails

    Component.onCompleted: root.viewModel.ensureEditorSelection()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            Layout.leftMargin: 16
            Layout.rightMargin: 16
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
            }
            Item { Layout.fillWidth: true }
            AppComboBox {
                id: newLanguage
                ui: root.ui
                Layout.preferredWidth: 140
                model: ["Lua", "JavaScript"]
            }
            AppButton {
                ui: root.ui
                text: qsTr("New Processor")
                                icon.source: root.ui.materialIcon("plus")
                onClicked: root.viewModel.newProcessor(newLanguage.currentIndex === 0 ? "lua" : "javascript")
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: root.ui.themePalette.separator
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
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth

                ColumnLayout {
                    width: parent.width
                    spacing: 16

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.margins: 24
                        spacing: 16

                        Label {
                            Layout.fillWidth: true
                            text: root.details.name || qsTr("No processor selected")
                            color: root.ui.textStrong
                            font.pixelSize: 20
                            font.bold: true
                            wrapMode: Text.Wrap
                        }
                        Label {
                            Layout.fillWidth: true
                            text: root.details.description || ""
                            color: root.ui.textMuted
                            wrapMode: Text.Wrap
                        }
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 24
                            rowSpacing: 12
                            visible: root.editor.currentProcessorId.length > 0

                            Label { text: qsTr("Language"); color: root.ui.textMuted }
                            Label { text: root.editor.runtimeName; color: root.ui.textStrong; Layout.fillWidth: true; wrapMode: Text.Wrap }
                            Label { text: qsTr("Current revision"); color: root.ui.textMuted }
                            Label { text: root.details.revisionNumber || ""; color: root.ui.textStrong }
                            Label { text: qsTr("Updated"); color: root.ui.textMuted }
                            Label { text: root.details.updatedAt || ""; color: root.ui.textStrong; Layout.fillWidth: true; wrapMode: Text.Wrap }
                            Label { text: qsTr("Used by"); color: root.ui.textMuted }
                            Label { text: root.details.usage || qsTr("None"); color: root.ui.textStrong; Layout.fillWidth: true; wrapMode: Text.Wrap }
                            Label { text: qsTr("Drafts"); color: root.ui.textMuted }
                            Label { text: `${root.details.draftCount || 0}`; color: root.ui.textStrong }
                            Label { text: qsTr("Validation"); color: root.ui.textMuted }
                            Label { text: root.details.readiness === "ready" ? qsTr("Ready") : (root.details.readiness === "invalid_source" ? qsTr("Validation failed") : qsTr("Not validated")); color: root.ui.textStrong }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            AppButton {
                                ui: root.ui
                                primary: true
                                text: qsTr("Edit in Browser")
                                icon.source: root.ui.materialIcon("edit")
                                enabled: root.editor.currentProcessorId.length > 0
                                onClicked: root.viewModel.openBrowserEditor()
                            }
                            AppButton {
                                ui: root.ui
                                danger: true
                                text: qsTr("Delete")
                                icon.source: root.ui.materialIcon("delete")
                                enabled: root.editor.currentProcessorId.length > 0
                                onClicked: deleteDialog.open()
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: root.editor.diagnostics.length > 0
                            text: root.editor.diagnostics
                            color: root.ui.themePalette.errorText
                            wrapMode: Text.Wrap
                        }
                        Label {
                            visible: root.viewModel.editorLink.length > 0
                            text: qsTr("Editor link")
                            color: root.ui.textMuted
                        }
                        TextArea {
                            Layout.fillWidth: true
                            visible: root.viewModel.editorLink.length > 0
                            text: root.viewModel.editorLink
                            readOnly: true
                            selectByMouse: true
                            wrapMode: TextEdit.WrapAnywhere
                            color: root.ui.textMuted
                            font.pixelSize: 11
                            background: null
                        }
                    }
                }
            }
        }
    }

    AppDialog {
        id: deleteDialog
        ui: root.ui
        width: Math.min(460, root.width - 32)
        height: 240
        closePolicy: Popup.CloseOnEscape
        header: Item { implicitHeight: 0; visible: false }
        contentItem: ColumnLayout {
            spacing: 16
            Label {
                text: qsTr("Delete processor?")
                color: root.ui.textStrong
                font.pixelSize: 18
                font.bold: true
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("%1 will be permanently removed. %2 drafts will become detached and can only be recovered as a new processor.").arg(root.editor.name).arg(root.details.draftCount || 0)
                color: root.ui.textMuted
                wrapMode: Text.Wrap
            }
            Item { Layout.fillHeight: true }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                AppButton { ui: root.ui; text: qsTr("Cancel"); onClicked: deleteDialog.close() }
                AppButton {
                    ui: root.ui
                    text: qsTr("Delete")
                    danger: true
                    onClicked: { deleteDialog.close(); root.viewModel.deleteCurrent() }
                }
            }
        }
    }
}
