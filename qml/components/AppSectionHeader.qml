pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

RowLayout {
    id: control

    required property AppUi ui
    property string title: ""
    property string meta: ""
    property int titleSize: 16
    default property alias trailingContent: trailingRow.data

    Layout.fillWidth: true
    spacing: 12

    Label {
        text: control.title
        color: control.ui.textStrong
        font.pixelSize: control.titleSize
        font.bold: true
    }

    AppBadge {
        ui: control.ui
        visible: control.meta.length > 0
        label: control.meta
        horizontalPadding: 7
        verticalPadding: 4
    }

    Item {
        Layout.fillWidth: true
    }

    RowLayout {
        id: trailingRow
        spacing: 8
    }
}
