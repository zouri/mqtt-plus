pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: root

    required property AppUi ui
    required property var notificationModel

    implicitWidth: 368
    implicitHeight: notificationList.contentHeight

    ListView {
        id: notificationList

        anchors.fill: parent
        model: root.notificationModel
        spacing: 8
        interactive: false
        boundsBehavior: Flickable.StopAtBounds

        delegate: Rectangle {
            id: notificationDelegate

            required property string notificationId
            required property string title
            required property string message
            required property string severity
            required property string actionLabel
            required property string actionId

            readonly property color severityColor: notificationDelegate.severity === "error"
                                                   ? root.ui.themePalette.errorText
                                                   : (notificationDelegate.severity === "warning"
                                                      ? root.ui.themePalette.warningText
                                                      : (notificationDelegate.severity === "success"
                                                         ? root.ui.themePalette.successText
                                                         : root.ui.themePalette.infoText))

            width: ListView.view.width
            height: notificationContent.implicitHeight + 20
            radius: root.ui.radiusLg
            color: root.ui.themePalette.dialogBg
            border.color: root.ui.themePalette.dialogBorder
            border.width: 1

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 4
                radius: 2
                color: notificationDelegate.severityColor
                Accessible.ignored: true
            }

            HoverHandler {
                id: notificationHover

                onHoveredChanged: root.notificationModel.setHovered(
                                      notificationDelegate.notificationId,
                                      notificationHover.hovered)
            }

            RowLayout {
                id: notificationContent

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 16
                anchors.rightMargin: 8
                spacing: 10

                Rectangle {
                    Layout.preferredWidth: 8
                    Layout.preferredHeight: 8
                    Layout.alignment: Qt.AlignTop
                    Layout.topMargin: 5
                    radius: 4
                    color: notificationDelegate.severityColor
                    Accessible.ignored: true
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: 3

                    Label {
                        Layout.fillWidth: true
                        text: notificationDelegate.title
                        color: root.ui.textStrong
                        font.pixelSize: root.ui.textMd
                        font.bold: true
                        elide: Label.ElideRight
                    }

                    Label {
                        visible: notificationDelegate.message.length > 0
                        Layout.fillWidth: true
                        text: notificationDelegate.message
                        color: root.ui.textMuted
                        font.pixelSize: root.ui.textSm
                        wrapMode: Text.Wrap
                    }

                    AppButton {
                        ui: root.ui
                        visible: notificationDelegate.actionLabel.length > 0
                        Layout.topMargin: 3
                        Layout.preferredWidth: implicitWidth
                        text: notificationDelegate.actionLabel
                        minimumWidth: 72
                        onClicked: root.notificationModel.triggerAction(
                                       notificationDelegate.notificationId)
                    }
                }

                AppIconButton {
                    ui: root.ui
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: 26
                    Layout.preferredHeight: 26
                    cornerRadius: root.ui.radiusSm
                    iconSource: root.ui.materialIcon("xmark")
                    iconSize: 12
                    restBg: "transparent"
                    outlineColor: "transparent"
                    accessibleName: qsTr("Dismiss notification")
                    toolTipText: accessibleName
                    onClicked: root.notificationModel.dismiss(
                                   notificationDelegate.notificationId)
                }
            }
        }
    }
}
