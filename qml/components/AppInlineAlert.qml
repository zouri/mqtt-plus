pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root

    required property AppUi ui
    property string type: "info"   // "info" | "success" | "warning" | "error"
    property string text: ""
    property bool dismissible: false
    property alias dismissButton: dismissButton
    signal dismissed()

    radius: root.ui.radiusSm
    color: root.ui.themePalette[root.type === "success" ? "successBg"
                                : root.type === "warning" ? "warningBg"
                                : root.type === "error" ? "errorBg"
                                : "accentPanelBg"]
    border.color: root.ui.themePalette[root.type === "success" ? "successText"
                                      : root.type === "warning" ? "warningText"
                                      : root.type === "error" ? "errorText"
                                      : "infoText"]
    border.width: 1
    implicitHeight: row.implicitHeight + root.ui.spaceSm * 2

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.leftMargin: root.ui.spaceMd
        anchors.rightMargin: root.ui.spaceMd
        anchors.topMargin: root.ui.spaceSm
        anchors.bottomMargin: root.ui.spaceSm
        spacing: root.ui.spaceSm

        Rectangle {
            Layout.preferredWidth: 3
            Layout.fillHeight: true
            radius: 2
            color: root.ui.themePalette[root.type === "success" ? "successText"
                                        : root.type === "warning" ? "warningText"
                                        : root.type === "error" ? "errorText"
                                        : "infoText"]
        }

        Label {
            Layout.fillWidth: true
            text: root.text
            color: root.ui.textStrong
            font.pixelSize: root.ui.textSm
            wrapMode: Text.Wrap
        }

        AppIconButton {
            id: dismissButton
            ui: root.ui
            visible: root.dismissible
            symbol: "✕"
            accessibleName: qsTr("Dismiss")
            toolTipText: qsTr("Dismiss")
            implicitWidth: 22
            implicitHeight: 22
            cornerRadius: root.ui.radiusSm
            onClicked: root.dismissed()
        }
    }
}
