pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

ColumnLayout {
    id: root

    required property AppUi ui
    required property var viewModel
    required property var editor

    spacing: 8

    RowLayout {
        Layout.fillWidth: true
        spacing: 10

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 220
            Layout.horizontalStretchFactor: 7
            spacing: 5

            Label {
                text: qsTr("Processor name")
                color: root.ui.textMuted
                font.pixelSize: 11
            }

            AppTextField {
                id: nameField

                ui: root.ui
                Layout.fillWidth: true
                Layout.preferredHeight: root.ui.compactControlHeight
                text: root.editor.name
                placeholderText: qsTr("Processor name")
                onTextEdited: root.editor.name = text
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 280
            Layout.horizontalStretchFactor: 13
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
                text: root.editor.description
                placeholderText: qsTr("Device protocol or payload structure")
                onTextEdited: root.editor.description = text
            }
        }

        ColumnLayout {
            Layout.preferredWidth: 170
            Layout.minimumWidth: 160
            Layout.maximumWidth: 190
            spacing: 5

            Label {
                text: qsTr("Language")
                color: root.ui.textMuted
                font.pixelSize: 11
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 5

                AppComboBox {
                    ui: root.ui
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.ui.compactControlHeight
                    model: root.editor.languageOptionNames
                    currentIndex: root.editor.languageIndex
                    onActivated: root.editor.languageIndex = currentIndex
                }

                AppIconButton {
                    ui: root.ui
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: root.ui.compactControlHeight
                    iconSize: 15
                    iconSource: root.ui.materialIcon("info")
                    restBg: "transparent"
                    hoverBg: root.ui.themePalette.actionHoverBg
                    outlineColor: "transparent"
                    symbolColor: root.ui.textMuted
                    accessibleName: qsTr("Runtime information")
                    toolTipText: qsTr("Runtime: %1").arg(root.editor.runtimeName)
                    toolTipPosition: AppToolTip.Position.Bottom
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 8

        AppTextArea {
            ui: root.ui
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 310
            text: root.editor.source
            clip: true
            showLineNumbers: true
            wrapMode: TextEdit.NoWrap
            placeholderText: root.editor.languageId === "javascript" ? qsTr("function process(context)") : qsTr("function process(context)")
            onTextChanged: {
                if (text !== root.editor.source) {
                    root.editor.source = text;
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: diagnosticsText.implicitHeight + 18
            visible: root.editor.diagnostics.length > 0
            radius: 8
            color: root.ui.themePalette.innerPanelBg
            border.color: root.editor.validationOk ? root.ui.stateColor("completed") : root.ui.themePalette.innerPanelBorder

            Label {
                id: diagnosticsText

                anchors.fill: parent
                anchors.margins: 9
                text: root.editor.diagnostics
                color: root.ui.textMuted
                font.pixelSize: 11
                wrapMode: Text.Wrap
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
