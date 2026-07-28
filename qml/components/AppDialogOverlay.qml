pragma ComponentBehavior: Bound

import QtQuick

Rectangle {
    id: control

    required property AppUi ui
    property bool animationsEnabled: control.ui.animationsEnabled
    property int enterDelay: 0
    property int enterDuration: 200

    color: control.ui.themePalette.dialogOverlay
    opacity: control.animationsEnabled ? 0 : 1

    SequentialAnimation {
        running: control.animationsEnabled

        PauseAnimation {
            duration: control.enterDelay
        }

        OpacityAnimator {
            target: control
            from: 0
            to: 1
            duration: control.enterDuration
            easing.type: Easing.OutCubic
        }
    }
}
