pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

Item {
    id: control

    required property AppUi ui
    required property ListView view
    required property string keyRole
    property string titleRole: control.keyRole
    property string subtitleRole: ""
    property string colorRole: ""
    property real actionAreaWidth: 34
    property string scopeKey: ""
    property string sourceKey: ""
    property string targetKey: ""
    property string sourceScope: ""
    property bool dragging: false
    property bool suppressClick: false
    property bool insertAfter: false
    property real pressY: 0
    property real pressX: 0
    property real pointerX: 0
    property real pointerY: 0
    property real indicatorY: 0
    property real grabOffsetY: 0
    property real previewHeight: 48
    property string previewTitle: ""
    property string previewSubtitle: ""
    property color previewColor: control.ui.accentColor

    signal moveRequested(string scope, string source, string anchor, bool after)
    signal rowClicked(int index)
    signal rowDoubleClicked(int index)

    z: 10

    function keyAt(index) {
        if (!control.view.model || index < 0 || index >= control.view.count) {
            return "";
        }
        return String(control.view.model.rowAt(index)[control.keyRole] || "");
    }

    function indexOfKey(key) {
        for (let index = 0; index < control.view.count; ++index) {
            if (control.keyAt(index) === key) {
                return index;
            }
        }
        return -1;
    }

    function canMove(key, offset) {
        const index = control.indexOfKey(key);
        return index >= 0 && index + offset >= 0 && index + offset < control.view.count;
    }

    function moveByOffset(key, offset) {
        if (control.canMove(key, offset)) {
            control.moveRequested(control.scopeKey, key,
                                  control.keyAt(control.indexOfKey(key) + offset), offset > 0);
        }
    }

    function updateTarget() {
        const y = control.view.contentY + Math.max(0, Math.min(control.view.height - 1, control.pointerY));
        let index = control.view.indexAt(1, y);
        if (index < 0) {
            index = control.view.indexAt(1, y + control.view.spacing + 1);
        }
        if (index < 0) {
            index = control.view.indexAt(1, y - control.view.spacing - 1);
        }
        if (index < 0) {
            index = control.pointerY < control.view.height / 2 ? 0 : control.view.count - 1;
        }
        const item = control.view.itemAtIndex(index);
        control.targetKey = item ? control.keyAt(index) : "";
        if (item) {
            control.insertAfter = y >= item.y + item.height / 2;
            control.indicatorY = item.y - control.view.contentY
                                + (control.insertAfter ? item.height : 0);
        }
    }

    function cancel() {
        control.dragging = false;
        control.sourceKey = "";
        control.targetKey = "";
        control.previewTitle = "";
        control.previewSubtitle = "";
    }

    function press(point) {
        control.cancel();
        control.suppressClick = false;
        if (!control.enabled || control.view.count < 2) {
            return;
        }
        const index = control.view.indexAt(1, control.view.contentY + point.y);
        control.sourceKey = control.keyAt(index);
        if (!control.sourceKey) {
            return;
        }
        control.sourceScope = control.scopeKey;
        const item = control.view.itemAtIndex(index);
        const row = control.view.model.rowAt(index);
        // Copy presentation data so recycled delegates cannot change the drag card.
        control.previewTitle = String(row[control.titleRole] || control.sourceKey);
        control.previewSubtitle = String(row[control.subtitleRole] || "");
        control.previewColor = row[control.colorRole] || control.ui.accentColor;
        control.previewHeight = item ? item.height : 48;
        control.grabOffsetY = item ? control.view.contentY + point.y - item.y : 24;
        control.pressX = point.x;
        control.pointerX = point.x;
        control.pressY = point.y;
        control.pointerY = point.y;
    }

    function move(point, threshold) {
        if (!control.sourceKey) {
            return;
        }
        control.pointerY = point.y;
        control.pointerX = point.x;
        if (Math.hypot(point.x - control.pressX, point.y - control.pressY) >= threshold) {
            control.dragging = true;
            control.suppressClick = true;
            control.view.cancelFlick();
        }
        if (control.dragging) {
            control.updateTarget();
        }
    }

    function release(point) {
        const source = control.sourceKey;
        const target = control.targetKey;
        const scope = control.sourceScope;
        const after = control.insertAfter;
        const valid = control.dragging && target && source !== target
                      && scope === control.scopeKey && point.x >= 0 && point.x <= control.width
                      && point.y >= 0 && point.y <= control.height;
        control.cancel();
        if (valid) {
            control.moveRequested(scope, source, target, after);
        }
    }

    onScopeKeyChanged: control.cancel()
    onEnabledChanged: { if (!control.enabled) control.cancel(); }

    Rectangle {
        z: 0
        x: 0
        y: Math.max(0, Math.min(control.height - height, control.indicatorY - height / 2))
        width: control.width
        height: 2
        color: control.ui.themePalette.selectedBorder
        visible: control.dragging && control.targetKey.length > 0
                 && control.targetKey !== control.sourceKey
    }

    Loader {
        active: control.dragging
        x: Math.max(2, Math.min(control.width - width - 2, control.pointerX - control.pressX + 4))
        y: Math.max(2, Math.min(control.height - height - 4, control.pointerY - control.grabOffsetY))
        width: Math.max(0, control.width - 8)
        height: control.previewHeight
        z: 1

        sourceComponent: Item {
            objectName: "reorderDragPreview"
            Accessible.ignored: true

            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 3
                anchors.bottomMargin: -3
                radius: 7
                color: control.ui.withAlpha("#000000", control.ui.isDarkTheme ? 0.35 : 0.16)
            }

            Rectangle {
                anchors.fill: parent
                radius: 7
                color: control.ui.themePalette.dialogBg
                border.color: control.ui.themePalette.selectedBorder

                Rectangle {
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    width: 4
                    height: parent.height - 16
                    radius: 2
                    color: control.previewColor
                }

                Column {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 24
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2

                    Label {
                        width: parent.width
                        text: control.previewTitle
                        font.pixelSize: 12
                        font.bold: true
                        color: control.ui.textStrong
                        elide: Text.ElideRight
                    }

                    Label {
                        width: parent.width
                        visible: control.previewSubtitle.length > 0
                                 && control.previewSubtitle !== control.previewTitle
                        text: control.previewSubtitle
                        font.pixelSize: 10
                        color: control.ui.textMuted
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }

    MouseArea {
        id: rowArea

        width: Math.max(0, control.width - control.actionAreaWidth)
        height: control.height
        acceptedButtons: Qt.LeftButton
        scrollGestureEnabled: false
        enabled: control.enabled && control.view.count > 1
        preventStealing: true
        hoverEnabled: true
        cursorShape: Qt.ArrowCursor

        onPressed: mouse => {
            control.press(Qt.point(mouse.x, mouse.y));
            if (!control.sourceKey) {
                mouse.accepted = false;
            }
        }
        onPositionChanged: mouse => {
            if (rowArea.pressed) {
                control.move(Qt.point(mouse.x, mouse.y), rowArea.drag.threshold);
            }
        }
        onReleased: mouse => control.release(Qt.point(mouse.x, mouse.y))
        onCanceled: control.cancel()
        onClicked: mouse => {
            if (!control.suppressClick) {
                const index = control.view.indexAt(1, control.view.contentY + mouse.y);
                const item = control.view.itemAtIndex(index);
                if (item) {
                    item.forceActiveFocus();
                    control.rowClicked(index);
                }
            }
        }
        onDoubleClicked: mouse => {
            if (!control.suppressClick) {
                const index = control.view.indexAt(1, control.view.contentY + mouse.y);
                if (index >= 0) {
                    control.rowDoubleClicked(index);
                }
            }
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: control.dragging
        onActivated: control.cancel()
    }

    Timer {
        interval: 16
        repeat: true
        running: control.dragging
        onTriggered: {
            const step = control.pointerY < 32 ? -8
                         : (control.pointerY > control.height - 32 ? 8 : 0);
            const minimum = control.view.originY;
            const maximum = minimum + Math.max(0, control.view.contentHeight - control.view.height);
            control.view.contentY = Math.max(minimum, Math.min(maximum, control.view.contentY + step));
            control.updateTarget();
        }
    }

    Connections {
        target: control.view.model
        ignoreUnknownSignals: true
        function onModelReset() { control.cancel(); }
        function onRowsRemoved() { control.cancel(); }
        function onRowsInserted() { control.cancel(); }
        function onLayoutChanged() { control.cancel(); }
    }
}
