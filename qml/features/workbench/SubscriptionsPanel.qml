pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

AppPanel {
    id: control

    required property var viewModel

    property string subscriptionActionVisualKey: ""
    property int subscriptionContextIndex: -1
    property string subscriptionContextTopic: ""
    property string subscriptionContextDisplayName: ""
    readonly property var filterModeLabels: [qsTr("All", "subscription filter"), qsTr("Active", "subscription filter"), qsTr("Paused", "subscription filter")]
    readonly property var subscriptionModel: control.viewModel ? control.viewModel.filteredSubscriptions : null
    readonly property int matchingSubscriptionCount: control.subscriptionModel ? control.subscriptionModel.count : 0
    readonly property var sessionStatus: control.viewModel ? control.viewModel.sessionStatus : ({})
    readonly property bool connected: control.sessionStatus.state === "connected"
    showRightBorder: false

    Layout.fillWidth: true
    Layout.fillHeight: true

    signal subscriptionCreateRequested
    signal subscriptionEditRequested(int index)

    function subscriptionActionLabel(actionId) {
        if (actionId === "edit") {
            return qsTr("Edit");
        }
        if (actionId === "delete") {
            return qsTr("Delete");
        }
        return "";
    }

    function openSubscriptionContextMenu(index, topic, displayName, visualKey) {
        control.subscriptionContextIndex = index;
        control.subscriptionContextTopic = topic;
        control.subscriptionContextDisplayName = displayName;
        control.subscriptionActionVisualKey = visualKey;
        subscriptionActionVisualResetTimer.stop();
        subscriptionContextMenu.open();
    }

    Timer {
        id: subscriptionActionVisualResetTimer
        interval: 180
        repeat: false
        onTriggered: control.subscriptionActionVisualKey = ""
    }

    Connections {
        target: control.viewModel

        function onSubscriptionDeleteRequested() {
            deleteSubscriptionDialog.open();
        }
    }

    ListModel {
        id: subscriptionContextActions

        ListElement { actionId: "edit" }
        ListElement { actionId: "delete" }
    }

    AppPlatformMenu {
        id: subscriptionContextMenu
        model: subscriptionContextActions
        actionText: actionId => control.subscriptionActionLabel(actionId)

        onTriggered: actionId => {
            if (actionId === "edit") {
                control.subscriptionEditRequested(control.subscriptionContextIndex);
            } else if (actionId === "delete") {
                control.viewModel.requestSubscriptionDelete(
                    control.subscriptionContextTopic,
                    control.subscriptionContextDisplayName);
            }
            control.subscriptionContextIndex = -1;
            control.subscriptionContextTopic = "";
            control.subscriptionContextDisplayName = "";
        }

        onAboutToHide: Qt.callLater(function() {
            subscriptionActionVisualResetTimer.restart();
        })
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
                text: control.subscriptionModel ? control.subscriptionModel.filterText : ""
                onTextEdited: {
                    if (control.subscriptionModel) {
                        control.subscriptionModel.filterText = text
                    }
                }
            }

            AppComboBox {
                ui: control.ui
                Layout.preferredWidth: 72
                leftPadding: 8
                rightPadding: 24
                model: control.filterModeLabels
                currentIndex: control.subscriptionModel ? control.subscriptionModel.filterModeIndex : 0
                onActivated: index => {
                    if (control.subscriptionModel) {
                        control.subscriptionModel.filterModeIndex = index
                    }
                }
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
                onClicked: control.subscriptionCreateRequested()
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
                    text: control.subscriptionModel && control.subscriptionModel.hasFilter ? qsTr("No matching subscriptions") : (control.connected ? qsTr("No subscriptions yet") : qsTr("Subscriptions are ready after connecting"))
                    color: control.ui.textStrong
                    font.pixelSize: 12
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }

                Label {
                    Layout.fillWidth: true
                    text: control.subscriptionModel && control.subscriptionModel.hasFilter ? qsTr("Adjust the filter or show all subscriptions.") : (control.connected ? qsTr("Add a topic to start listening.") : qsTr("You can add topics now; they will start listening once connected."))
                    color: control.ui.textMuted
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }

                AppButton {
                    ui: control.ui
                    visible: !(control.subscriptionModel && control.subscriptionModel.hasFilter)
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Add subscription")
                    minimumWidth: 112
                    primary: true
                    toolTipText: qsTr("Add subscription")
                    toolTipPosition: AppToolTip.Position.Bottom
                    onClicked: control.subscriptionCreateRequested()
                }
            }
        }

        ListView {
            id: subscriptionList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 7
            model: control.subscriptionModel
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
                required property string topicColor
                required property bool paused
                required property string subscriptionState
                required property string lastError
                required property real topicFps
                readonly property string metaText: qsTr("QoS %1 · %2/s").arg(subscriptionDelegate.requestedQos).arg(Number(subscriptionDelegate.topicFps || 0).toFixed(1))
                readonly property string menuVisualKey: `${subscriptionDelegate.topic}::menu`
                width: ListView.view.width
                radius: control.ui.innerRadius
                color: control.ui.themePalette.itemBg
                border.color: subscriptionDelegate.lastError.length > 0 ? control.ui.themePalette.errorText : control.ui.themePalette.innerPanelBorder
                implicitHeight: subscriptionDelegate.lastError.length > 0 ? 88 : 70
                activeFocusOnTab: true
                Accessible.role: Accessible.ListItem
                Accessible.name: subscriptionDelegate.displayName

                function openSubscriptionContextMenu() {
                    control.openSubscriptionContextMenu(
                        subscriptionDelegate.index,
                        subscriptionDelegate.topic,
                        subscriptionDelegate.displayName,
                        subscriptionDelegate.menuVisualKey);
                }

                Keys.onPressed: event => {
                    if (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && event.modifiers & Qt.ShiftModifier)) {
                        subscriptionDelegate.openSubscriptionContextMenu();
                        event.accepted = true;
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    onClicked: mouse => {
                        if (mouse.button === Qt.RightButton) {
                            subscriptionDelegate.forceActiveFocus();
                            subscriptionDelegate.openSubscriptionContextMenu();
                        }
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
                            color: subscriptionDelegate.topicColor.length > 0
                                   ? subscriptionDelegate.topicColor
                                   : control.ui.stateColor(subscriptionDelegate.subscriptionState)
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
                                id: subscriptionPauseButton
                                ui: control.ui
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
                                    control.subscriptionActionVisualKey = visualKey;
                                    subscriptionActionVisualResetTimer.restart();
                                    control.viewModel.toggleCurrentSubscriptionPaused(subscriptionDelegate.topic, subscriptionDelegate.paused);
                                }
                            }

                            AppIconButton {
                                id: subscriptionMenuButton
                                ui: control.ui
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
                                    subscriptionDelegate.openSubscriptionContextMenu();
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
                text: qsTr("Delete %1 from this connection?").arg(control.viewModel.pendingSubscriptionDeleteDisplayName)
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
                    onClicked: {
                        control.viewModel.cancelPendingSubscriptionDelete();
                        deleteSubscriptionDialog.close();
                    }
                }

                AppButton {
                    ui: control.ui
                    text: qsTr("Delete")
                    minimumWidth: 78
                    danger: true
                    onClicked: {
                        control.viewModel.confirmPendingSubscriptionDelete();
                        deleteSubscriptionDialog.close();
                    }
                }
            }
        }

        onClosed: {
            control.viewModel.cancelPendingSubscriptionDelete();
        }
    }
}
