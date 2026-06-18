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
    property string pendingDeleteTopic: ""
    property string pendingDeleteDisplayName: ""
    property int matchingSubscriptionCount: 0
    readonly property var filterModeValues: ["all", "subscribed", "paused"]
    readonly property var filterModeLabels: [
        qsTr("All", "subscription filter"),
        qsTr("Active", "subscription filter"),
        qsTr("Paused", "subscription filter")
    ]
    readonly property int filterModeIndex: Math.max(0, control.filterModeValues.indexOf(control.filterMode))
    readonly property var sessionStatus: control.appController ? control.appController.sessionStatus : ({})
    readonly property bool connected: control.sessionStatus.state === "connected"
    readonly property bool hasFilter: control.filterText.trim().length > 0 || control.filterMode !== "all"

    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.minimumHeight: 220

    Timer {
        id: subscriptionActionVisualResetTimer
        interval: 180
        repeat: false
        onTriggered: control.subscriptionActionVisualKey = ""
    }

    function rowMatches(topic, alias, displayName, formatName, paused) {
        if (control.filterMode === "subscribed" && paused) {
            return false
        }

        if (control.filterMode === "paused" && !paused) {
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

    function requestDeleteSubscription(topic, displayName) {
        control.pendingDeleteTopic = topic
        control.pendingDeleteDisplayName = displayName
        deleteSubscriptionDialog.open()
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
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        anchors.topMargin: 10
        anchors.bottomMargin: 10
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            AppTextField {
                ui: control.ui
                Layout.fillWidth: true
                placeholderText: qsTr("Filter topic")
                text: control.filterText
                onTextChanged: control.filterText = text
            }

            AppComboBox {
                ui: control.ui
                Layout.preferredWidth: 72
                leftPadding: 8
                rightPadding: 24
                model: control.filterModeLabels
                currentIndex: control.filterModeIndex
                onActivated: (index) => control.filterMode = control.filterModeValues[index] || "all"
            }

            AppIconButton {
                ui: control.ui
                iconSource: control.ui.materialIcon("plus")
                iconSize: 16
                implicitWidth: 34
                implicitHeight: 34
                cornerRadius: 17
                restBg: control.ui.themePalette.windowBg
                outlineColor: control.ui.themePalette.innerPanelBorder
                accessibleName: qsTr("Add topic")
                toolTipText: qsTr("Add subscription")
                toolTipPosition: AppToolTip.Position.Bottom
                onClicked: control.addSubscriptionDialog.openForCreate()
            }
        }

        Rectangle {
            visible: control.matchingSubscriptionCount === 0
            Layout.fillWidth: true
            Layout.preferredHeight: emptySubscriptionColumn.implicitHeight + 18
            radius: control.ui.innerRadius
            color: control.ui.themePalette.innerPanelBg
            border.color: control.ui.themePalette.innerPanelBorder

            ColumnLayout {
                id: emptySubscriptionColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 6

                Label {
                    Layout.fillWidth: true
                    text: control.hasFilter
                          ? qsTr("No matching subscriptions")
                          : (control.connected
                             ? qsTr("No subscriptions yet")
                             : qsTr("Subscriptions are ready after connecting"))
                    color: control.ui.textStrong
                    font.pixelSize: 12
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }

                Label {
                    Layout.fillWidth: true
                    text: control.hasFilter
                          ? qsTr("Adjust the filter or show all subscriptions.")
                          : (control.connected
                             ? qsTr("Add a topic to start listening.")
                             : qsTr("You can add topics now; they will start listening once connected."))
                    color: control.ui.textMuted
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }

                AppButton {
                    ui: control.ui
                    visible: !control.hasFilter
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Add subscription")
                    minimumWidth: 112
                    primary: true
                    toolTipText: qsTr("Add subscription")
                    toolTipPosition: AppToolTip.Position.Bottom
                    onClicked: control.addSubscriptionDialog.openForCreate()
                }
            }
        }

        ListView {
            id: subscriptionList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 7
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
                readonly property bool matchesFilter: control.rowMatches(
                                                          subscriptionDelegate.topic,
                                                          subscriptionDelegate.alias,
                                                          subscriptionDelegate.displayName,
                                                          subscriptionDelegate.formatName,
                                                          subscriptionDelegate.paused)
                readonly property string metaText: qsTr("QoS %1 · %2/s")
                                                   .arg(subscriptionDelegate.requestedQos)
                                                   .arg(Number(subscriptionDelegate.topicFps || 0).toFixed(1))
                readonly property string menuVisualKey: `${subscriptionDelegate.topic}::menu`
                width: ListView.view.width
                visible: subscriptionDelegate.matchesFilter
                radius: control.ui.innerRadius
                color: control.ui.themePalette.itemBg
                border.color: subscriptionDelegate.lastError.length > 0
                              ? control.ui.themePalette.errorText
                              : control.ui.themePalette.innerPanelBorder
                implicitHeight: subscriptionDelegate.matchesFilter
                                ? (subscriptionDelegate.lastError.length > 0 ? 88 : 70)
                                : 0
                activeFocusOnTab: true
                Accessible.role: Accessible.ListItem
                Accessible.name: subscriptionDelegate.displayName

                function showSubscriptionContextMenu(globalPosition) {
                    control.subscriptionActionVisualKey = subscriptionDelegate.menuVisualKey
                    subscriptionActionVisualResetTimer.stop()
                    const action = control.appController.showSubscriptionContextMenu(
                                subscriptionDelegate.topic, globalPosition)
                    if (action === "edit") {
                        control.addSubscriptionDialog.openForEdit(
                                    control.appController.subscriptions.rowAt(subscriptionDelegate.index))
                    } else if (action === "delete") {
                        control.requestDeleteSubscription(subscriptionDelegate.topic,
                                                          subscriptionDelegate.displayName)
                    }
                    subscriptionActionVisualResetTimer.restart()
                }

                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Menu
                            || (event.key === Qt.Key_F10 && event.modifiers & Qt.ShiftModifier)) {
                        subscriptionDelegate.showSubscriptionContextMenu(
                                    subscriptionDelegate.mapToGlobal(
                                        Qt.point(subscriptionDelegate.width - 10,
                                                 Math.round(subscriptionDelegate.height / 2))))
                        event.accepted = true
                    }
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 7

                        Rectangle {
                            Layout.preferredWidth: 7
                            Layout.preferredHeight: 7
                            radius: 4
                            color: control.ui.stateColor(subscriptionDelegate.subscriptionState)
                        }

                        Label {
                            Layout.fillWidth: true
                            text: subscriptionDelegate.displayName
                            color: control.ui.textStrong
                            font.pixelSize: 13
                            font.bold: true
                            elide: Label.ElideRight
                        }

                        AppBadge {
                            ui: control.ui
                            visible: subscriptionDelegate.scriptName.length === 0
                            label: subscriptionDelegate.formatName
                            Layout.maximumWidth: 92
                            badgeRadius: 8
                            horizontalPadding: 8
                            verticalPadding: 4
                            maximumLabelWidth: 76
                        }

                        AppBadge {
                            ui: control.ui
                            visible: subscriptionDelegate.scriptName.length > 0
                            label: subscriptionDelegate.scriptName
                            Layout.maximumWidth: 108
                            badgeRadius: 8
                            horizontalPadding: 8
                            verticalPadding: 4
                            maximumLabelWidth: 92
                            strong: false
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Label {
                            text: subscriptionDelegate.metaText
                            color: control.ui.textMuted
                            font.pixelSize: 12
                            elide: Label.ElideRight
                            Layout.fillWidth: true
                        }

                        Item {
                            Layout.preferredWidth: 4
                        }

                        RowLayout {
                            spacing: 1

                            AppIconButton {
                                ui: control.ui
                                id: subscriptionPauseButton
                                iconSource: control.ui.materialIcon(subscriptionDelegate.paused ? "play" : "pause")
                                implicitWidth: 28
                                implicitHeight: 28
                                iconSize: 13
                                cornerRadius: 14
                                restBg: control.ui.themePalette.itemBg
                                outlineColor: control.ui.themePalette.innerPanelBorder

                                forceActive: control.subscriptionActionVisualKey === visualKey
                                readonly property string visualKey: `${subscriptionDelegate.topic}::pause`
                                accessibleName: subscriptionDelegate.paused ? qsTr("Resume topic") : qsTr("Pause topic")

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
                                id: subscriptionMenuButton
                                iconSource: control.ui.materialIcon("more-horiz")
                                implicitWidth: 28
                                implicitHeight: 28
                                iconSize: 16
                                cornerRadius: 14
                                restBg: control.ui.themePalette.itemBg
                                outlineColor: control.ui.themePalette.innerPanelBorder

                                forceActive: control.subscriptionActionVisualKey === subscriptionDelegate.menuVisualKey
                                accessibleName: qsTr("More actions")

                                onClicked: {
                                    subscriptionDelegate.showSubscriptionContextMenu(
                                                subscriptionMenuButton.mapToGlobal(
                                                    Qt.point(Math.round(subscriptionMenuButton.width / 2),
                                                             subscriptionMenuButton.height)))
                                }
                            }
                        }
                    }

                    Label {
                        visible: subscriptionDelegate.lastError.length > 0
                        Layout.fillWidth: true
                        text: subscriptionDelegate.lastError
                        color: control.ui.themePalette.errorText
                        font.pixelSize: 11
                        elide: Label.ElideRight
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    onPressed: (mouse) => {
                        if (mouse.button === Qt.RightButton) {
                            subscriptionDelegate.forceActiveFocus()
                            subscriptionDelegate.showSubscriptionContextMenu(
                                        subscriptionDelegate.mapToGlobal(Qt.point(mouse.x, mouse.y)))
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: deleteSubscriptionDialog

        modal: true
        dim: true
        focus: true
        standardButtons: Dialog.NoButton
        anchors.centerIn: Overlay.overlay
        width: Math.min(340, Overlay.overlay.width - 32)

        Overlay.modal: Rectangle {
            color: control.ui.themePalette.dialogOverlay
        }

        header: Item {
            implicitHeight: 0
            visible: false
        }

        background: Rectangle {
            radius: control.ui.innerRadius
            color: control.ui.themePalette.dialogBg
            border.color: control.ui.themePalette.dialogBorder
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 14

            Label {
                Layout.fillWidth: true
                text: qsTr("Delete subscription?")
                color: control.ui.textStrong
                font.pixelSize: 15
                font.bold: true
                wrapMode: Text.Wrap
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Delete %1 from this connection?").arg(control.pendingDeleteDisplayName)
                color: control.ui.textMuted
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Item {
                    Layout.fillWidth: true
                }

                AppButton {
                    ui: control.ui
                    text: qsTr("Cancel")
                    minimumWidth: 78
                    onClicked: deleteSubscriptionDialog.close()
                }

                AppButton {
                    ui: control.ui
                    text: qsTr("Delete")
                    minimumWidth: 78
                    danger: true
                    onClicked: {
                        control.appController.removeCurrentSubscription(control.pendingDeleteTopic)
                        deleteSubscriptionDialog.close()
                    }
                }
            }
        }

        onClosed: {
            control.pendingDeleteTopic = ""
            control.pendingDeleteDisplayName = ""
        }
    }
}
