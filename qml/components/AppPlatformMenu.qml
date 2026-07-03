pragma ComponentBehavior: Bound

import QtQuick
import Qt.labs.platform as Platform

Item {
    id: control

    property alias model: actionRepeater.model
    property var actionText: actionId => actionId
    property var actionEnabled: actionId => true

    signal triggered(string actionId)
    signal aboutToHide

    width: 0
    height: 0
    visible: false

    function open() {
        platformMenu.open();
    }

    function close() {
        platformMenu.close();
    }

    Platform.Menu {
        id: platformMenu

        onAboutToHide: control.aboutToHide()
    }

    Repeater {
        id: actionRepeater

        delegate: Item {
            id: actionDelegate

            required property string actionId

            width: 0
            height: 0
            visible: false

            property Platform.MenuItem menuItem: Platform.MenuItem {
                text: control.actionText(actionDelegate.actionId)
                enabled: control.actionEnabled(actionDelegate.actionId)
                onTriggered: control.triggered(actionDelegate.actionId)
            }

            Component.onCompleted: platformMenu.addItem(menuItem)
            Component.onDestruction: platformMenu.removeItem(menuItem)
        }
    }
}
