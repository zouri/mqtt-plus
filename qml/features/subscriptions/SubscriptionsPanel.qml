pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

AppPanel {
    id: control

    required property var appController
    required property AddSubscriptionDialog addSubscriptionDialog

    property string subscriptionActionVisualKey: ""

    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.minimumHeight: 260

    Timer {
        id: subscriptionActionVisualResetTimer
        interval: 180
        repeat: false
        onTriggered: control.subscriptionActionVisualKey = ""
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        AppSectionHeader {
            ui: control.ui
            title: qsTr("Subscriptions")
            titleSize: 16
            meta: `${subscriptionList.count}`

            AppIconButton {
                ui: control.ui
                iconSource: control.ui.materialIcon("plus")
                iconSize: 16
                implicitWidth: 34
                implicitHeight: 34
                primary: true
                toolTipText: qsTr("Add topic")
                onClicked: control.addSubscriptionDialog.openForCreate()
            }
        }

        AppDivider {
            ui: control.ui
        }

        Label {
            visible: subscriptionList.count === 0
            text: qsTr("No topics yet. Use the + button to start listening.")
            color: control.ui.textMuted
            font.pixelSize: 12
        }

        ListView {
            id: subscriptionList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            model: control.appController.subscriptions
            reuseItems: true

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: Rectangle {
                id: subscriptionDelegate
                required property int index
                required property string topic
                required property string alias
                required property string displayName
                required property int requestedQos
                required property int format
                required property string formatName
                required property string scriptId
                required property string scriptName
                required property bool paused
                required property string subscriptionState
                required property string lastError
                required property real topicFps
                width: ListView.view.width
                radius: control.ui.innerRadius
                color: control.ui.themePalette.itemBg
                border.color: subscriptionDelegate.lastError.length > 0
                              ? control.ui.themePalette.errorText
                              : control.ui.themePalette.innerPanelBorder
                implicitHeight: 82

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 11
                    spacing: 7

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Rectangle {
                            Layout.preferredWidth: 8
                            Layout.preferredHeight: 8
                            radius: 4
                            color: control.ui.stateColor(subscriptionDelegate.subscriptionState)
                        }

                        Label {
                            Layout.fillWidth: true
                            text: subscriptionDelegate.displayName
                            color: control.ui.textStrong
                            font.pixelSize: 12
                            font.bold: true
                            elide: Label.ElideRight
                        }

                        AppBadge {
                            ui: control.ui
                            label: subscriptionDelegate.formatName
                            badgeRadius: 8
                            horizontalPadding: 6
                            verticalPadding: 3
                        }

                        AppBadge {
                            ui: control.ui
                            visible: subscriptionDelegate.scriptName.length > 0
                            label: subscriptionDelegate.scriptName
                            badgeRadius: 8
                            horizontalPadding: 6
                            verticalPadding: 3
                            strong: false
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            text: qsTr("FPS %1").arg(Math.round(Number(subscriptionDelegate.topicFps || 0)))
                            color: control.ui.textMuted
                            font.pixelSize: 11
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            spacing: 1

                            AppIconButton {
                                id: subscriptionEditButton
                                ui: control.ui
                                iconSource: control.ui.materialIcon("edit")
                                implicitWidth: 26
                                implicitHeight: 26
                                iconSize: 13
                                cornerRadius: 6
                                restBg: "transparent"
                                outlineColor: "transparent"

                                forceActive: control.subscriptionActionVisualKey === visualKey
                                readonly property string visualKey: `${subscriptionDelegate.topic}::edit`
                                toolTipText: qsTr("Edit script")

                                onClicked: {
                                    control.subscriptionActionVisualKey = visualKey
                                    subscriptionActionVisualResetTimer.restart()
                                    control.addSubscriptionDialog.openForEdit(
                                                control.appController.subscriptions.rowAt(subscriptionDelegate.index))
                                }
                            }

                            AppIconButton {
                                ui: control.ui
                                id: subscriptionPauseButton
                                iconSource: control.ui.materialIcon(subscriptionDelegate.paused ? "play" : "pause")
                                implicitWidth: 26
                                implicitHeight: 26
                                iconSize: 13
                                cornerRadius: 6
                                restBg: "transparent"
                                outlineColor: "transparent"

                                forceActive: control.subscriptionActionVisualKey === visualKey
                                readonly property string visualKey: `${subscriptionDelegate.topic}::pause`
                                toolTipText: subscriptionDelegate.paused ? qsTr("Resume topic") : qsTr("Pause topic")

                                onClicked: {
                                    control.subscriptionActionVisualKey = visualKey
                                    subscriptionActionVisualResetTimer.restart()
                                    control.appController.setCurrentSubscriptionPaused(
                                                subscriptionDelegate.topic,
                                                !subscriptionDelegate.paused)
                                }
                            }

                            AppIconButton {
                                ui: control.ui
                                id: subscriptionDeleteButton
                                iconSource: control.ui.materialIcon("xmark")
                                implicitWidth: 26
                                implicitHeight: 26
                                iconSize: 13
                                cornerRadius: 6
                                restBg: "transparent"
                                outlineColor: "transparent"

                                forceActive: control.subscriptionActionVisualKey === visualKey
                                readonly property string visualKey: `${subscriptionDelegate.topic}::delete`
                                toolTipText: qsTr("Delete topic")

                                onClicked: {
                                    control.subscriptionActionVisualKey = visualKey
                                    subscriptionActionVisualResetTimer.restart()
                                    control.appController.removeCurrentSubscription(subscriptionDelegate.topic)
                                }
                            }
                        }
                    }

                    Label {
                        visible: subscriptionDelegate.lastError.length > 0
                        text: subscriptionDelegate.lastError
                        color: control.ui.themePalette.errorText
                        font.pixelSize: 11
                        elide: Label.ElideRight
                    }
                }
            }
        }
    }
}
