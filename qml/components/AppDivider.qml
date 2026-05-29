pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

Rectangle {
    required property AppUi ui

    color: ui.themePalette.separator
    implicitHeight: 1
    Layout.fillWidth: true
}
