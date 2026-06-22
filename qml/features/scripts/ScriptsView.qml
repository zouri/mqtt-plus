pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

Item {
    id: control

    required property AppUi ui
    required property var scriptLibrary

    property string currentScriptId: ""
    property string savedScriptName: ""
    property string savedScriptDescription: ""
    property string savedScriptCode: ""
    property string validationStatus: qsTr("Unsaved")
    property bool validationOk: false

    readonly property bool hasUnsavedChanges: nameField.text !== control.savedScriptName
                                               || descriptionField.text !== control.savedScriptDescription
                                               || codeField.text !== control.savedScriptCode
    readonly property bool canSave: control.hasUnsavedChanges || control.currentScriptId.length === 0

    Layout.fillWidth: true
    Layout.fillHeight: true

    function defaultCode() {
        return "function parse(ctx)\n"
                + "    return ctx.decoded\n"
                + "end\n"
    }

    function loadScript(row) {
        control.currentScriptId = row.id || ""
        nameField.text = row.name || ""
        descriptionField.text = row.description || ""
        codeField.text = row.code || control.defaultCode()
        control.savedScriptName = nameField.text
        control.savedScriptDescription = descriptionField.text
        control.savedScriptCode = codeField.text
        control.validationStatus = control.currentScriptId.length > 0 ? qsTr("Saved") : qsTr("Unsaved")
        control.validationOk = false
    }

    function newScript() {
        control.currentScriptId = ""
        nameField.text = qsTr("New Lua Script")
        descriptionField.text = qsTr("Decode MQTT payloads with Lua.")
        codeField.text = control.defaultCode()
        control.savedScriptName = nameField.text
        control.savedScriptDescription = descriptionField.text
        control.savedScriptCode = codeField.text
        control.validationStatus = qsTr("Unsaved")
        control.validationOk = false
        nameField.forceActiveFocus()
        nameField.selectAll()
    }

    function ensureSelection() {
        const scripts = control.scriptLibrary.scripts
        if (control.currentScriptId.length > 0) {
            if (scripts && scripts.indexOfId(control.currentScriptId) >= 0) {
                return
            }
        }

        if (scripts && scripts.count > 0 && control.currentScriptId.length > 0) {
            control.loadScript(scripts.rowAt(0))
        } else if (scripts && scripts.count > 0 && nameField.text.length === 0) {
            control.loadScript(scripts.rowAt(0))
        } else if ((!scripts || scripts.count === 0) && nameField.text.length === 0) {
            control.newScript()
        }
    }

    function validateStructure() {
        const code = codeField.text
        const hasParseFunction = code.indexOf("function parse") >= 0
        const hasEnd = code.trim().endsWith("end") || code.indexOf("\nend") >= 0
        control.validationOk = hasParseFunction && hasEnd
        control.validationStatus = control.validationOk
                ? qsTr("Structure valid")
                : qsTr("Structure invalid: define function parse(ctx) ... end")
    }

    function saveScript() {
        const savedId = control.scriptLibrary.upsertScript(
                    control.currentScriptId,
                    nameField.text,
                    descriptionField.text,
                    codeField.text)
        if (savedId.length === 0) {
            return
        }
        control.currentScriptId = savedId
        control.savedScriptName = nameField.text
        control.savedScriptDescription = descriptionField.text
        control.savedScriptCode = codeField.text
        control.validationStatus = qsTr("Saved")
        control.validationOk = true
    }

    Component.onCompleted: control.ensureSelection()

    Connections {
        target: control.scriptLibrary

        function onScriptLibraryChanged() {
            control.ensureSelection()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: control.ui.themePalette.windowBg

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 32
                anchors.rightMargin: 32
                spacing: 10

                Label {
                    text: qsTr("Script Manager")
                    color: control.ui.textStrong
                    font.pixelSize: 22
                    font.bold: true
                }

                AppBadge {
                    ui: control.ui
                    label: `${control.scriptLibrary.scripts.count}`
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
                anchors.leftMargin: 32
                anchors.rightMargin: 32
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
                scriptLibrary: control.scriptLibrary
                currentScriptId: control.currentScriptId
                onScriptRequested: (row) => control.loadScript(row)
            }

            Rectangle {
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                color: control.ui.themePalette.separator
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 20
                Layout.rightMargin: 24
                Layout.topMargin: 14
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
                            placeholderText: qsTr("Script name")
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
                            placeholderText: qsTr("Device protocol or payload structure")
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
                        font.family: "Menlo"
                        clip: true
                        showLineNumbers: false
                        showFocusBorder: false
                        wrapMode: TextEdit.NoWrap
                        placeholderText: qsTr("function parse(ctx)")
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 62
            color: control.ui.themePalette.windowBg

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: 304
                anchors.rightMargin: 32
                height: 1
                color: control.ui.themePalette.separator
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 304
                anchors.rightMargin: 32
                spacing: 10

                Label {
                    Layout.fillWidth: true
                    text: control.hasUnsavedChanges ? qsTr("Unsaved") : control.validationStatus
                    color: control.hasUnsavedChanges
                           ? control.ui.textMuted
                           : (control.validationOk
                              ? control.ui.stateColor("completed")
                              : control.ui.textMuted)
                    font.pixelSize: 13
                    elide: Label.ElideRight
                }

                AppButton {
                    ui: control.ui
                    text: qsTr("Validate structure")
                    minimumWidth: 98
                    onClicked: control.validateStructure()
                }

                AppButton {
                    ui: control.ui
                    text: qsTr("Save Script")
                    primary: true
                    enabled: control.canSave
                    minimumWidth: 92
                    onClicked: control.saveScript()
                }
            }
        }
    }
}
