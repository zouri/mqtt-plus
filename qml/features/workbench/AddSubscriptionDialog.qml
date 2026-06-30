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
        root.editor.openForCreate()
        open()
        topicField.forceActiveFocus()
    }

    function openForEdit(subscription) {
        root.editor.openForEdit(subscription)
        open()
        aliasField.forceActiveFocus()
    }

    function submit() {
        if (root.viewModel.submitSubscriptionEditor()) {
            close()
        }
    }

    modal: true
    focus: true
    width: 420
    anchors.centerIn: Overlay.overlay
    standardButtons: Dialog.NoButton

    Overlay.modal: Rectangle {
        color: root.ui.themePalette.dialogOverlay
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
                enabled: !root.editor.editMode
                onActivated: root.editor.qos = currentIndex
            }

            AppComboBox {
                ui: root.ui
                id: formatField
                Layout.fillWidth: true
                model: root.viewModel.payloadFormats
                currentIndex: root.editor.format
                enabled: !root.editor.editMode
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
