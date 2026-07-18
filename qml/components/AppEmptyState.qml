pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ColumnLayout {
    id: root

    required property AppUi ui
    property string title: qsTr("Nothing here yet")
    property string description: ""
    property url iconSource: ""
    property string mode: "empty"   // "empty" | "loading" | "error"
    property string actionLabel: ""
    property alias actionButton: actionButton

    signal actionTriggered

    Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
    spacing: ui.spaceMd

    BusyIndicator {
        visible: root.mode === "loading"
        running: root.mode === "loading"
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: 36
        Layout.preferredHeight: 36
        palette.dark: root.ui.themePalette.infoText
    }

    Image {
        visible: root.mode !== "loading" && root.iconSource.toString().length > 0
        source: root.iconSource
        sourceSize.width: 40
        sourceSize.height: 40
        Layout.alignment: Qt.AlignHCenter
        opacity: root.mode === "error" ? 1 : 0.85
    }

    Label {
        text: root.title
        color: root.ui.textStrong
        font.pixelSize: root.ui.textLg
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        Layout.alignment: Qt.AlignHCenter
    }

    Label {
        visible: root.description.length > 0
        text: root.description
        color: root.ui.themePalette.textMuted
        font.pixelSize: root.ui.textSm
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        Layout.maximumWidth: 280
        Layout.alignment: Qt.AlignHCenter
    }

    AppButton {
        id: actionButton
        ui: root.ui
        text: root.actionLabel
        visible: root.actionLabel.length > 0
        Layout.alignment: Qt.AlignHCenter
        onClicked: root.actionTriggered()
    }
}
