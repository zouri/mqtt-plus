pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

Window {
    id: root

    required property AppUi ui
    required property var appController
    required property string fontFamily
    required property var ownerWindow
    property bool allowUnsavedClose: false

    function openLibrary() {
        if (!visible) {
            width = Math.max(minimumWidth, Math.min(1180, ownerWindow.width - 80))
            height = Math.max(minimumHeight, Math.min(760, ownerWindow.height - 80))
            x = ownerWindow.x + Math.round((ownerWindow.width - width) / 2)
            y = ownerWindow.y + Math.round((ownerWindow.height - height) / 2)
        }
        show()
        raise()
        requestActivate()
    }

    function discardAndClose() {
        root.allowUnsavedClose = true
        root.close()
    }

    function saveAndClose() {
        workspace.saveScript()
        root.close()
    }

    title: ""
    width: 1180
    height: 760
    minimumWidth: 960
    minimumHeight: 620
    visible: false
    flags: Qt.Window | Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint
    color: root.ui.themePalette.windowBg
    modality: Qt.NonModal
    transientParent: root.ownerWindow

    onClosing: (close) => {
        if (!root.allowUnsavedClose && workspace.hasUnsavedChanges) {
            close.accepted = false
            unsavedChangesDialog.open()
            return
        }

        root.allowUnsavedClose = false
    }

    Connections {
        target: root.ownerWindow

        function onClosing() {
            root.close()
        }
    }

    ScriptWorkspacePanel {
        id: workspace
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        anchors.topMargin: 30
        anchors.bottomMargin: 10
        ui: root.ui
        appController: root.appController
        fontFamily: root.fontFamily
    }

    Dialog {
        id: unsavedChangesDialog

        modal: true
        focus: true
        width: 430
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.NoButton
        closePolicy: Popup.NoAutoClose

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
            spacing: 14

            Label {
                Layout.fillWidth: true
                text: qsTr("Unsaved script changes")
                color: root.ui.textStrong
                font.pixelSize: 18
                font.bold: true
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("The current script has unsaved changes. Save them before closing the script manager?")
                color: root.ui.textMuted
                font.pixelSize: 13
                wrapMode: Text.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Item {
                    Layout.fillWidth: true
                }

                AppButton {
                    ui: root.ui
                    text: qsTr("Discard")
                    minimumWidth: 92
                    onClicked: {
                        unsavedChangesDialog.close()
                        root.discardAndClose()
                    }
                }

                AppButton {
                    ui: root.ui
                    text: qsTr("Save and Close")
                    primary: true
                    minimumWidth: 124
                    onClicked: {
                        unsavedChangesDialog.close()
                        root.saveAndClose()
                    }
                }
            }
        }
    }

    MouseArea {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.leftMargin: Qt.platform.os === "osx" ? 86 : 0
        width: parent.width - anchors.leftMargin
        height: 30
        acceptedButtons: Qt.LeftButton

        onPressed: (mouse) => {
            if (mouse.button === Qt.LeftButton) {
                root.startSystemMove()
            }
        }
    }
}
