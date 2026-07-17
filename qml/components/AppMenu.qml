pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import QtQml.Models

Menu {
    id: control

    required property AppUi ui
    required property string accessibleName
    property alias model: actionInstantiator.model
    property var actionText: actionId => actionId
    property var actionIcon: actionId => ""
    property var actionEnabled: actionId => true
    property var actionDanger: actionId => false
    property var actionSeparatorBefore: actionId => false
    property int preferredWidth: 208
    property int anchorGap: 5
    property int pointerGap: 2
    readonly property var hostWindow: control.parent ? control.parent.Window.window : null

    signal triggered(string actionId)

    title: control.accessibleName
    popupType: Popup.Item
    modal: true
    dim: false
    margins: 8
    overlap: 0
    padding: 5
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside | Popup.CloseOnReleaseOutside

    function openAtPoint(sourceItem, localX, localY) {
        if (!sourceItem) {
            return;
        }
        const scenePoint = sourceItem.mapToItem(null, localX, localY);
        const menuWidth = control.width > 0 ? control.width : control.implicitWidth;
        const menuHeight = control.height > 0 ? control.height : control.implicitHeight;
        const availableRight = control.hostWindow
                               ? control.hostWindow.width - control.margins - scenePoint.x
                               : menuWidth;
        const availableBelow = control.hostWindow
                               ? control.hostWindow.height - control.margins - scenePoint.y
                               : menuHeight;
        const availableLeft = scenePoint.x - control.margins;
        const availableAbove = scenePoint.y - control.margins;
        const popupX = availableRight < menuWidth + control.pointerGap
                       && availableLeft > availableRight
                       ? localX - menuWidth - control.pointerGap
                       : localX + control.pointerGap;
        const popupY = availableBelow < menuHeight + control.pointerGap
                       && availableAbove > availableBelow
                       ? localY - menuHeight - control.pointerGap
                       : localY + control.pointerGap;
        control.popup(sourceItem, popupX, popupY);
    }

    function openForItem(anchorItem, alignRight) {
        if (!anchorItem) {
            return;
        }
        const menuWidth = control.width > 0 ? control.width : control.implicitWidth;
        const menuHeight = control.height > 0 ? control.height : control.implicitHeight;
        const anchorScenePosition = anchorItem.mapToItem(null, 0, 0);
        const availableBelow = control.hostWindow
                               ? control.hostWindow.height - control.margins
                                 - anchorScenePosition.y - anchorItem.height
                               : menuHeight + control.anchorGap;
        const availableAbove = anchorScenePosition.y - control.margins;
        const localX = alignRight === false ? 0 : anchorItem.width - menuWidth;
        const localY = availableBelow < menuHeight + control.anchorGap
                       && availableAbove > availableBelow
                       ? -menuHeight - control.anchorGap
                       : anchorItem.height + control.anchorGap;
        control.popup(anchorItem, localX, localY);
    }

    enter: Transition {
        ParallelAnimation {
            OpacityAnimator {
                from: 0
                to: 1
                duration: 100
                easing.type: Easing.OutCubic
            }

            ScaleAnimator {
                from: 0.97
                to: 1
                duration: 120
                easing.type: Easing.OutCubic
            }
        }
    }

    exit: Transition {
        OpacityAnimator {
            from: 1
            to: 0
            duration: 80
            easing.type: Easing.OutCubic
        }
    }

    background: Rectangle {
        implicitWidth: control.preferredWidth
        implicitHeight: 40
        radius: 8
        color: control.ui.themePalette.dialogBg
        border.color: control.ui.themePalette.dialogBorder
        border.width: 1
        layer.enabled: control.visible
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowBlur: 0.42
            shadowColor: control.ui.isDarkTheme ? "#8a000000" : "#2b000000"
            shadowHorizontalOffset: 0
            shadowVerticalOffset: 6
        }
    }

    Instantiator {
        id: actionInstantiator

        delegate: MenuItem {
            id: actionItem

            required property string actionId
            required property int index
            readonly property bool danger: control.actionDanger(actionItem.actionId)
            readonly property bool separated: control.actionSeparatorBefore(actionItem.actionId)
            readonly property color foregroundColor: !actionItem.enabled
                                                        ? control.ui.themePalette.disabledText
                                                        : (actionItem.danger
                                                           ? control.ui.themePalette.errorText
                                                           : control.ui.textStrong)

            implicitHeight: 34 + topPadding
            leftPadding: 10
            rightPadding: 10
            topPadding: actionItem.separated ? 8 : 0
            bottomPadding: 0
            spacing: 10
            hoverEnabled: true
            text: control.actionText(actionItem.actionId)
            enabled: control.actionEnabled(actionItem.actionId)
            display: icon.source.toString().length > 0 ? AbstractButton.TextBesideIcon : AbstractButton.TextOnly
            font.pixelSize: control.ui.textSm
            icon.source: control.actionIcon(actionItem.actionId)
            icon.width: 16
            icon.height: 16
            icon.color: actionItem.foregroundColor
            palette.windowText: actionItem.foregroundColor
            palette.text: actionItem.foregroundColor
            Accessible.name: actionItem.text
            Accessible.role: Accessible.MenuItem

            HoverHandler {
                cursorShape: actionItem.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            }

            background: Item {
                Rectangle {
                    visible: actionItem.separated
                    x: 8
                    y: 3
                    width: parent.width - 16
                    height: 1
                    color: control.ui.themePalette.separator
                    Accessible.ignored: true
                }

                Rectangle {
                    x: 2
                    y: actionItem.topPadding
                    width: parent.width - 4
                    height: parent.height - actionItem.topPadding
                    radius: 6
                    color: {
                        if (!actionItem.enabled) {
                            return "transparent";
                        }
                        if (actionItem.down) {
                            return actionItem.danger
                                   ? control.ui.themePalette.errorBg
                                   : control.ui.themePalette.actionPressedBg;
                        }
                        if (actionItem.highlighted) {
                            return actionItem.danger
                                   ? control.ui.themePalette.errorBg
                                   : control.ui.themePalette.actionHoverBg;
                        }
                        return "transparent";
                    }
                    Accessible.ignored: true
                }
            }

            onTriggered: control.triggered(actionItem.actionId)
        }

        onObjectAdded: (index, object) => control.insertItem(index, object)
        onObjectRemoved: (index, object) => control.removeItem(object)
    }
}
