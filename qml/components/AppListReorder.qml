pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: control

    required property AppUi ui
    required property ListView view
    required property string keyRole
    property string scopeKey: ""
    property string sourceKey: ""
    property string targetKey: ""
    property string sourceScope: ""
    property bool dragging: false
    property bool insertAfter: false
    property real pressY: 0
    property real pointerY: 0
    property real indicatorY: 0

    signal moveRequested(string scope, string source, string anchor, bool after)

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
    }

    onScopeKeyChanged: control.cancel()
    onEnabledChanged: { if (!control.enabled) control.cancel(); }

    Rectangle {
        x: 0
        y: Math.max(0, Math.min(control.height - height, control.indicatorY - height / 2))
        width: control.width
        height: 2
        color: control.ui.themePalette.selectedBorder
        visible: control.dragging && control.targetKey.length > 0
                 && control.targetKey !== control.sourceKey
    }

    MouseArea {
        id: gripArea

        width: 24
        height: control.height
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true
        preventStealing: true
        enabled: control.view.count > 1
        cursorShape: control.dragging ? Qt.ClosedHandCursor : Qt.OpenHandCursor
        Accessible.role: Accessible.Grip
        Accessible.name: qsTr("Reorder")

        onPressed: mouse => {
            control.sourceKey = control.keyAt(control.view.indexAt(1, control.view.contentY + mouse.y));
            if (!control.sourceKey) {
                mouse.accepted = false;
                return;
            }
            control.sourceScope = control.scopeKey;
            control.pressY = mouse.y;
            control.pointerY = mouse.y;
            gripArea.forceActiveFocus();
        }
        onPositionChanged: mouse => {
            if (!gripArea.pressed || !control.sourceKey) {
                return;
            }
            control.pointerY = mouse.y;
            if (Math.abs(mouse.y - control.pressY) >= gripArea.drag.threshold) {
                control.dragging = true;
                control.view.cancelFlick();
            }
            if (control.dragging) {
                control.updateTarget();
            }
        }
        onReleased: mouse => {
            const source = control.sourceKey;
            const target = control.targetKey;
            const scope = control.sourceScope;
            const after = control.insertAfter;
            const valid = control.dragging && target && source !== target
                          && scope === control.scopeKey && mouse.x >= 0 && mouse.x <= control.width
                          && mouse.y >= 0 && mouse.y <= control.height;
            control.cancel();
            if (valid) {
                control.moveRequested(scope, source, target, after);
            }
        }
        onCanceled: control.cancel()
        Keys.onEscapePressed: control.cancel()
    }

    Item {
        width: 24
        height: 24
        y: Math.max(0, Math.min(control.height - height, gripArea.mouseY - height / 2))
        AppToolTip {
            ui: control.ui
            text: qsTr("Reorder")
            active: gripArea.enabled && gripArea.containsMouse && !gripArea.pressed
            position: AppToolTip.Position.Right
        }
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
