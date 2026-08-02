pragma ComponentBehavior: Bound

import QtQuick

Rectangle {
    id: control

    required property AppUi ui
    property bool animationsEnabled: control.ui.animationsEnabled
    property int enterDelay: 0
    property int enterDuration: control.ui.motionModalEnterDuration

    color: control.ui.themePalette.dialogOverlay
    opacity: 1

    Component.onCompleted: {
        if (control.animationsEnabled) {
            enterAnimation.start()
        }
    }

    SequentialAnimation {
        id: enterAnimation

        PauseAnimation {
            duration: control.enterDelay
        }

        OpacityAnimator {
            target: control
            from: 0
            to: 1
            duration: control.enterDuration
            easing.type: control.ui.motionEnterEasing
        }
    }
}
