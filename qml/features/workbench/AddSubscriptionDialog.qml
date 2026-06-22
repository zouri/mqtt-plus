pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

Dialog {
    id: root

    required property AppUi ui
    required property var workbench
    required property var scriptLibrary
    property var scriptOptionIds: []
    property var scriptOptionNames: [qsTr("None")]
    property bool editMode: false
    property string editTopic: ""

    function syncScriptOptions() {
        const selectedScriptId = root.scriptOptionIds[scriptField.currentIndex] || ""
        const scripts = root.scriptLibrary.scripts
        const ids = [""]
        const names = [qsTr("None")]
        for (let i = 0; scripts && i < scripts.count; ++i) {
            const row = scripts.rowAt(i)
            ids.push(row.id)
            names.push(row.name)
        }
        root.scriptOptionIds = ids
        root.scriptOptionNames = names
        scriptField.model = root.scriptOptionNames
        selectScript(selectedScriptId)
    }

    function selectScript(scriptId) {
        const index = root.scriptOptionIds.indexOf(scriptId || "")
        scriptField.currentIndex = index >= 0 ? index : 0
    }

    function openForCreate() {
        root.editMode = false
        root.editTopic = ""
        syncScriptOptions()
        topicField.text = ""
        aliasField.text = ""
        qosField.currentIndex = 0
        formatField.currentIndex = 0
        selectScript("")
        open()
        topicField.forceActiveFocus()
    }

    function openForEdit(subscription) {
        root.editMode = true
        root.editTopic = subscription.topic || ""
        syncScriptOptions()
        topicField.text = root.editTopic
        aliasField.text = subscription.alias || ""
        qosField.currentIndex = Math.max(0, Math.min(qosField.count - 1, Number(subscription.requestedQos || 0)))
        formatField.currentIndex = Math.max(0, Math.min(formatField.count - 1, Number(subscription.format || 0)))
        selectScript(subscription.scriptId || "")
        open()
        aliasField.forceActiveFocus()
    }

    function submit() {
        const scriptId = root.scriptOptionIds[scriptField.currentIndex] || ""
        if (root.editMode) {
            if (root.workbench.updateCurrentSubscription(root.editTopic, topicField.text, aliasField.text, scriptId)) {
                close()
            }
            return
        }

        if (root.workbench.upsertCurrentSubscription(
                    topicField.text,
                    qosField.currentIndex,
                    formatField.currentIndex,
                    scriptId,
                    aliasField.text)) {
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

    Connections {
        target: root.scriptLibrary

        function onScriptLibraryChanged() {
            root.syncScriptOptions()
        }
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        Label {
            text: root.editMode ? qsTr("Edit Subscription") : qsTr("Add Subscription")
            color: root.ui.textStrong
            font.pixelSize: 18
            font.bold: true
        }

        AppTextField {
            ui: root.ui
            id: topicField
            Layout.fillWidth: true
            placeholderText: qsTr("sensor/+/temperature")
        }

        AppTextField {
            ui: root.ui
            id: aliasField
            Layout.fillWidth: true
            placeholderText: qsTr("Alias (optional)")
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            AppComboBox {
                ui: root.ui
                id: qosField
                Layout.fillWidth: true
                model: [qsTr("QoS 0"), qsTr("QoS 1")]
                enabled: !root.editMode
            }

            AppComboBox {
                ui: root.ui
                id: formatField
                Layout.fillWidth: true
                model: root.workbench.payloadFormats
                enabled: !root.editMode
            }
        }

        AppComboBox {
            ui: root.ui
            id: scriptField
            Layout.fillWidth: true
            model: root.scriptOptionNames
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
                iconSource: root.ui.materialIcon(root.editMode ? "check" : "plus")
                iconSize: 15
                implicitWidth: 36
                implicitHeight: 36
                primary: true
                accessibleName: root.editMode ? qsTr("Save subscription") : qsTr("Add subscription")
                onClicked: root.submit()
            }
        }
    }
}
