pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

Popup {
    id: control

    required property AppUi ui

    enter: Transition {
        ParallelAnimation {
            OpacityAnimator {
                from: 0
                to: 1
                duration: control.ui.motionPopoverEnterDuration
                easing.type: control.ui.motionEnterEasing
            }

            ScaleAnimator {
                from: 0.98
                to: 1
                duration: control.ui.motionPopoverEnterDuration
                easing.type: control.ui.motionEnterEasing
            }
        }
    }

    exit: Transition {
        ParallelAnimation {
            OpacityAnimator {
                from: 1
                to: 0
                duration: control.ui.motionPopoverExitDuration
                easing.type: control.ui.motionExitEasing
            }

            ScaleAnimator {
                from: 1
                to: 0.98
                duration: control.ui.motionPopoverExitDuration
                easing.type: control.ui.motionExitEasing
            }
        }
    }
}
