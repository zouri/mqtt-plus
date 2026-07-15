pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

Dialog {
    id: root

    required property AppUi ui
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

    modal: true
    dim: true
    focus: true
    width: 420
    anchors.centerIn: Overlay.overlay
    transformOrigin: Popup.Center
    standardButtons: Dialog.NoButton

    enter: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0
            to: 1
            duration: 200
            easing.type: Easing.OutCubic
        }

        NumberAnimation {
            property: "scale"
            from: 0.92
            to: 1
            duration: 200
            easing.type: Easing.OutCubic
        }
    }

    exit: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1
            to: 0
            duration: 160
            easing.type: Easing.InCubic
        }

        NumberAnimation {
            property: "scale"
            from: 1
            to: 0.96
            duration: 160
            easing.type: Easing.InCubic
        }
    }

    Overlay.modal: AppDialogOverlay {
        ui: root.ui
    }

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

        AppTextField {
            ui: root.ui
            id: topicField
            Layout.fillWidth: true
            text: root.editor.topic
            placeholderText: qsTr("sensor/+/temperature")
            onTextEdited: root.editor.topic = text
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
                model: [qsTr("QoS 0"), qsTr("QoS 1")]
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

        AppComboBox {
            ui: root.ui
            id: scriptField
            Layout.fillWidth: true
            model: root.editor.scriptOptionNames
            currentIndex: root.editor.scriptIndex
            onActivated: root.editor.scriptIndex = currentIndex
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
}
