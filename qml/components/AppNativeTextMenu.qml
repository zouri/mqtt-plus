pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

Menu {
    id: menu

    required property var editor

    // Qt 6.11 must create the native handle before a text context request opens it.
    popupType: Popup.Native

    Action {
        text: qsTr("Undo")
        shortcut: StandardKey.Undo
        enabled: menu.editor.canUndo
        onTriggered: menu.editor.undo()
    }

    Action {
        text: qsTr("Redo")
        shortcut: StandardKey.Redo
        enabled: menu.editor.canRedo
        onTriggered: menu.editor.redo()
    }

    MenuSeparator {}

    Action {
        text: qsTr("Cut")
        shortcut: StandardKey.Cut
        enabled: !menu.editor.readOnly && menu.editor.selectedText.length > 0
        onTriggered: menu.editor.cut()
    }

    Action {
        text: qsTr("Copy")
        shortcut: StandardKey.Copy
        enabled: menu.editor.selectedText.length > 0
        onTriggered: menu.editor.copy()
    }

    Action {
        text: qsTr("Paste")
        shortcut: StandardKey.Paste
        enabled: !menu.editor.readOnly && menu.editor.canPaste
        onTriggered: menu.editor.paste()
    }

    Action {
        text: qsTr("Delete")
        shortcut: StandardKey.Delete
        enabled: !menu.editor.readOnly && menu.editor.selectedText.length > 0
        onTriggered: menu.editor.remove(menu.editor.selectionStart, menu.editor.selectionEnd)
    }

    MenuSeparator {}

    Action {
        text: qsTr("Select All")
        shortcut: StandardKey.SelectAll
        onTriggered: menu.editor.selectAll()
    }
}
