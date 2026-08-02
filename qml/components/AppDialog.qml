pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

Dialog {
    id: control

    required property AppUi ui

    modal: true
    dim: true
    focus: true
    anchors.centerIn: Overlay.overlay
    transformOrigin: Popup.Center
    standardButtons: Dialog.NoButton

    enter: Transition {
        ParallelAnimation {
            OpacityAnimator {
                from: 0
                to: 1
                duration: control.ui.motionModalEnterDuration
                easing.type: control.ui.motionEnterEasing
            }

            ScaleAnimator {
                from: 0.98
                to: 1
                duration: control.ui.motionModalEnterDuration
                easing.type: control.ui.motionEnterEasing
            }
        }
    }

    exit: Transition {
        ParallelAnimation {
            OpacityAnimator {
                to: 0
                duration: control.ui.motionModalExitDuration
                easing.type: control.ui.motionExitEasing
            }

            ScaleAnimator {
                to: 0.98
                duration: control.ui.motionModalExitDuration
                easing.type: control.ui.motionExitEasing
            }
        }
    }

    Overlay.modal: AppDialogOverlay {
        ui: control.ui
    }
}
