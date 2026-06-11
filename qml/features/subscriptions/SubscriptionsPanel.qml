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
    property string filterText: ""
    property string filterMode: "all"
    property int matchingSubscriptionCount: 0
    readonly property var sessionStatus: control.appController ? control.appController.sessionStatus : ({})
    readonly property bool connected: control.sessionStatus.state === "connected"

    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.minimumHeight: 260

    Timer {
        id: subscriptionActionVisualResetTimer
        interval: 180
        repeat: false
        onTriggered: control.subscriptionActionVisualKey = ""
    }

    function rowMatches(topic, alias, displayName, formatName, paused) {
        if (control.filterMode === "active" && paused) {
            return false
        }

        const needle = control.filterText.trim().toLowerCase()
        if (needle.length === 0) {
            return true
        }

        return `${topic} ${alias} ${displayName} ${formatName}`.toLowerCase().indexOf(needle) >= 0
    }

    function recomputeVisibleCount() {
        let visibleRows = 0
        const model = control.appController ? control.appController.subscriptions : null
        const rowCount = model ? model.count : 0
        for (let i = 0; i < rowCount; ++i) {
            const row = model.rowAt(i)
            if (control.rowMatches(row.topic || "",
                                   row.alias || "",
                                   row.displayName || "",
                                   row.formatName || "",
                                   Boolean(row.paused))) {
                visibleRows += 1
            }
        }
        control.matchingSubscriptionCount = visibleRows
    }

    onFilterTextChanged: control.recomputeVisibleCount()
    onFilterModeChanged: control.recomputeVisibleCount()
    Component.onCompleted: control.recomputeVisibleCount()

    Connections {
        target: control.appController ? control.appController.subscriptions : null

        function onCountChanged() {
            control.recomputeVisibleCount()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.topMargin: 18
        anchors.bottomMargin: 16
        spacing: 12

        AppSectionHeader {
            ui: control.ui
            title: qsTr("Subscriptions")
            titleSize: 16
            meta: control.filterText.length > 0 || control.filterMode !== "all"
                  ? qsTr("%1/%2").arg(control.matchingSubscriptionCount).arg(subscriptionList.count)
                  : `${subscriptionList.count}`

            AppIconButton {
                ui: control.ui
                iconSource: control.ui.materialIcon("plus")
                iconSize: 16
                implicitWidth: 34
                implicitHeight: 34
                cornerRadius: 17
                restBg: control.ui.themePalette.windowBg
                outlineColor: control.ui.themePalette.innerPanelBorder
                toolTipText: qsTr("Add topic")
                onClicked: control.addSubscriptionDialog.openForCreate()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            AppTextField {
                ui: control.ui
                Layout.fillWidth: true
                placeholderText: qsTr("Filter Topic, e.g. sensor/+/temp")
                text: control.filterText
                onTextChanged: control.filterText = text
            }

            RowLayout {
                spacing: 0

                AppButton {
                    ui: control.ui
                    text: qsTr("All")
                    minimumWidth: 68
                    primary: control.filterMode === "all"
                    onClicked: control.filterMode = "all"
                }

                AppButton {
                    ui: control.ui
                    text: qsTr("Running")
                    minimumWidth: 88
                    primary: control.filterMode === "active"
                    onClicked: control.filterMode = "active"
                }
            }
        }

        Label {
            visible: control.matchingSubscriptionCount === 0
            text: control.connected
                  ? qsTr("No topics yet. Add a topic to start listening.")
                  : qsTr("No topics yet. Add subscriptions now; they will listen after connecting.")
            color: control.ui.textMuted
            font.pixelSize: 12
            wrapMode: Text.Wrap
            Layout.fillWidth: true
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
                required property real receivedMessageCount
                required property string lastMessageTimestamp
                readonly property bool matchesFilter: control.rowMatches(
                                                          subscriptionDelegate.topic,
                                                          subscriptionDelegate.alias,
                                                          subscriptionDelegate.displayName,
                                                          subscriptionDelegate.formatName,
                                                          subscriptionDelegate.paused)
                readonly property string statusText: subscriptionDelegate.paused
                                                     ? qsTr("Paused")
                                                     : control.ui.statusLabel(subscriptionDelegate.subscriptionState)
                width: ListView.view.width
                visible: subscriptionDelegate.matchesFilter
                radius: 16
                color: control.ui.themePalette.itemBg
                border.color: subscriptionDelegate.lastError.length > 0
                              ? control.ui.themePalette.errorText
                              : control.ui.themePalette.innerPanelBorder
                implicitHeight: subscriptionDelegate.matchesFilter ? 132 : 0

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 10

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
                            font.pixelSize: 14
                            font.bold: true
                            elide: Label.ElideRight
                        }

                        AppBadge {
                            ui: control.ui
                            label: subscriptionDelegate.formatName
                            badgeRadius: 8
                            horizontalPadding: 8
                            verticalPadding: 4
                        }

                        AppBadge {
                            ui: control.ui
                            visible: subscriptionDelegate.scriptName.length > 0
                            label: subscriptionDelegate.scriptName
                            badgeRadius: 8
                            horizontalPadding: 8
                            verticalPadding: 4
                            strong: false
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            text: qsTr("QoS %1 · %2 · %3 msg · %4/s")
                                  .arg(subscriptionDelegate.requestedQos)
                                  .arg(subscriptionDelegate.statusText)
                                  .arg(Math.round(Number(subscriptionDelegate.receivedMessageCount || 0)))
                                  .arg(Number(subscriptionDelegate.topicFps || 0).toFixed(1))
                            color: control.ui.textMuted
                            font.pixelSize: 13
                            elide: Label.ElideRight
                            Layout.fillWidth: true
                        }

                        Item {
                            Layout.preferredWidth: 4
                        }

                        RowLayout {
                            spacing: 1

                            AppIconButton {
                                id: subscriptionEditButton
                                ui: control.ui
                                iconSource: control.ui.materialIcon("edit")
                                implicitWidth: 34
                                implicitHeight: 34
                                iconSize: 13
                                cornerRadius: 17
                                restBg: control.ui.themePalette.itemBg
                                outlineColor: control.ui.themePalette.innerPanelBorder

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
                                implicitWidth: 34
                                implicitHeight: 34
                                iconSize: 13
                                cornerRadius: 17
                                restBg: control.ui.themePalette.itemBg
                                outlineColor: control.ui.themePalette.innerPanelBorder

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
                                implicitWidth: 34
                                implicitHeight: 34
                                iconSize: 13
                                cornerRadius: 17
                                restBg: Qt.rgba(0.86, 0.15, 0.15, control.ui.isDarkTheme ? 0.22 : 0.06)
                                outlineColor: control.ui.themePalette.innerPanelBorder
                                symbolColor: control.ui.themePalette.errorText

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
                        Layout.fillWidth: true
                        text: subscriptionDelegate.lastMessageTimestamp.length > 0
                              ? qsTr("Last message: %1").arg(subscriptionDelegate.lastMessageTimestamp)
                              : qsTr("Last message: none")
                        color: control.ui.textMuted
                        font.pixelSize: 12
                        elide: Label.ElideRight
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
