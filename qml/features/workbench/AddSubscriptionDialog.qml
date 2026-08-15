pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import "../../components"

AppDialog {
    id: root

    required property var viewModel
    readonly property var editor: root.viewModel.subscriptionEditor
    function openForCreate() {
        root.viewModel.openSubscriptionEditorForCreate()
        open()
    }

    function openForEdit(index) {
        if (root.viewModel.openSubscriptionEditorForEdit(index)) {
            open()
        }
    }

    function submit() {
        if (root.viewModel.submitSubscriptionEditor()) {
            close()
        }
    }

    function openColorDialog() {
        topicColorDialog.selectedColor = root.editor.color.length > 0
                                         ? root.editor.color
                                         : "#0071E3"
        topicColorDialog.open()
    }

    width: 420

    header: Item {
        implicitHeight: 0
        visible: false
    }

    background: Rectangle {
        radius: 18
        color: root.ui.themePalette.dialogBg
        border.color: root.ui.themePalette.dialogBorder
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        Label {
            text: root.editor.editMode ? qsTr("Edit Subscription") : qsTr("Add Subscription")
            color: root.ui.textStrong
            font.pixelSize: 18
            font.bold: true
        }

        Loader {
            Layout.fillWidth: true
            Layout.preferredHeight: root.editor.editMode ? 38 : 82
            sourceComponent: root.editor.editMode ? topicFieldComponent : topicsFieldComponent
        }

        AppTextField {
            ui: root.ui
            id: aliasField
            Layout.fillWidth: true
            text: root.editor.alias
            placeholderText: qsTr("Alias (optional)")
            onTextEdited: root.editor.alias = text
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            AppComboBox {
                ui: root.ui
                id: qosField
                Layout.fillWidth: true
                model: [qsTr("QoS 0"), qsTr("QoS 1"), qsTr("QoS 2")]
                currentIndex: root.editor.qos
                onActivated: root.editor.qos = currentIndex
            }

            AppComboBox {
                ui: root.ui
                id: formatField
                Layout.fillWidth: true
                model: root.viewModel.payloadFormats
                currentIndex: root.editor.format
                onActivated: root.editor.format = currentIndex
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            AppCheckBox {
                ui: root.ui
                Layout.fillWidth: true
                text: qsTr("No Local")
                checked: root.editor.noLocal
                onToggled: root.editor.noLocal = checked
            }

            AppTextField {
                ui: root.ui
                Layout.fillWidth: true
                inputMethodHints: Qt.ImhDigitsOnly
                placeholderText: qsTr("Subscription ID")
                text: root.editor.subscriptionIdentifierText
                onTextEdited: root.editor.subscriptionIdentifierText = text
            }
        }

        AppTextArea {
            ui: root.ui
            Layout.fillWidth: true
            Layout.preferredHeight: 66
            placeholderText: qsTr("MQTT 5 user properties: name=value")
            text: root.editor.userPropertiesText
            onTextChanged: {
                if (root.editor.userPropertiesText !== text) {
                    root.editor.userPropertiesText = text
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: qsTr("Message Processor")
                color: root.ui.textMuted
                font.pixelSize: 12
            }

            AppComboBox {
                ui: root.ui
                Layout.fillWidth: true
                model: root.editor.processorOptionNames
                currentIndex: root.editor.processorIndex
                onActivated: root.editor.processorIndex = currentIndex
            }

            Label {
                Layout.fillWidth: true
                visible: root.editor.processorBindingDetail.length > 0
                text: root.editor.processorBindingDetail
                color: root.ui.stateColor("error")
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: qsTr("Color")
                color: root.ui.textMuted
                font.pixelSize: 12
            }

            Repeater {
                model: root.editor.colorOptions

                delegate: ToolButton {
                    id: colorButton

                    required property int index
                    required property string modelData
                    readonly property bool selected: root.editor.color === colorButton.modelData

                    Layout.preferredWidth: 26
                    Layout.preferredHeight: 26
                    padding: 0
                    text: colorButton.modelData.length === 0 ? "-" : ""
                    font.pixelSize: 13
                    font.bold: true
                    Accessible.name: colorButton.modelData.length === 0
                                     ? qsTr("No topic color")
                                     : qsTr("Topic color %1").arg(colorButton.index)
                    onClicked: root.editor.color = colorButton.modelData

                    background: Rectangle {
                        radius: 13
                        color: colorButton.modelData.length > 0
                               ? colorButton.modelData
                               : root.ui.themePalette.innerPanelBg
                        border.width: colorButton.selected ? 2 : 1
                        border.color: colorButton.selected
                                      ? root.ui.themePalette.selectedBorder
                                      : root.ui.themePalette.innerPanelBorder
                    }
                }
            }

            ToolButton {
                id: customColorButton

                readonly property bool customColorSelected: root.editor.color.length > 0
                                                            && root.editor.colorOptions.indexOf(root.editor.color) < 0

                Layout.preferredWidth: 26
                Layout.preferredHeight: 26
                padding: 0
                Accessible.name: qsTr("Choose custom topic color")
                onClicked: root.openColorDialog()

                contentItem: Label {
                    text: customColorButton.customColorSelected ? "" : "+"
                    color: root.ui.textStrong
                    font.pixelSize: 16
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: 13
                    color: customColorButton.customColorSelected
                           ? root.editor.color
                           : root.ui.themePalette.innerPanelBg
                    border.width: customColorButton.customColorSelected ? 2 : 1
                    border.color: customColorButton.customColorSelected
                                  ? root.ui.themePalette.selectedBorder
                                  : root.ui.themePalette.innerPanelBorder
                }

                AppToolTip {
                    ui: root.ui
                    text: qsTr("Choose custom color")
                    position: AppToolTip.Position.Bottom
                    active: customColorButton.hovered
                }
            }

            Item {
                Layout.fillWidth: true
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Item {
                Layout.fillWidth: true
            }

            AppIconButton {
                ui: root.ui
                iconSource: root.ui.materialIcon("xmark")
                iconSize: 15
                implicitWidth: 36
                implicitHeight: 36
                accessibleName: qsTr("Cancel")
                onClicked: root.close()
            }

            AppIconButton {
                ui: root.ui
                iconSource: root.ui.materialIcon(root.editor.editMode ? "check" : "plus")
                iconSize: 15
                implicitWidth: 36
                implicitHeight: 36
                primary: true
                enabled: root.editor.canSubmit
                accessibleName: root.editor.editMode ? qsTr("Save subscription") : qsTr("Add subscription")
                onClicked: root.submit()
            }
        }
    }

    Component {
        id: topicFieldComponent

        AppTextField {
            ui: root.ui
            text: root.editor.topic
            placeholderText: qsTr("sensor/+/temperature")
            Accessible.name: qsTr("Topic filter")
            onTextEdited: root.editor.topic = text
        }
    }

    Component {
        id: topicsFieldComponent

        AppTextArea {
            ui: root.ui
            text: root.editor.topic
            placeholderText: qsTr("sensor/one, sensor/two")
            wrapMode: TextEdit.Wrap
            Accessible.name: qsTr("Topic filters")
            onTextEdited: root.editor.topic = text
        }
    }

    ColorDialog {
        id: topicColorDialog

        title: qsTr("Choose topic color")
        onAccepted: root.editor.color = String(selectedColor).toUpperCase()
    }
}
