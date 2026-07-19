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
    readonly property var subscriptionModel: control.viewModel ? control.viewModel.filteredSubscriptions : null
    readonly property int matchingSubscriptionCount: control.subscriptionModel ? control.subscriptionModel.count : 0
    readonly property var sessionStatus: control.viewModel ? control.viewModel.sessionStatus : ({})
    readonly property bool connected: control.sessionStatus.state === "connected"
    readonly property bool allSubscriptionsPaused: control.viewModel.allSubscriptionsPaused
    showTopBorder: false
    showRightBorder: false
    showBottomBorder: false
    showLeftBorder: false
    color: control.ui.themePalette.panelBg

    Layout.fillWidth: true
    Layout.fillHeight: true

    signal subscriptionCreateRequested
    signal subscriptionEditRequested(int index)
    signal replaceMessageTopicFilter(string topic)
    signal addMessageTopicFilter(string topic)
    signal collapseRequested

    function subscriptionActionLabel(actionId) {
        if (actionId === "edit") {
            return qsTr("Edit");
        }
        if (actionId === "filter") {
            return qsTr("Filter this Topic");
        }
        if (actionId === "add-filter") {
            return qsTr("Add Topic to filter");
        }
        if (actionId === "copy") {
            return qsTr("Copy Topic");
        }
        if (actionId === "delete") {
            return qsTr("Delete");
        }
        return "";
    }

    function subscriptionActionIcon(actionId) {
        if (actionId === "filter" || actionId === "add-filter") {
            return control.ui.materialIcon("filter");
        }
        if (actionId === "copy") {
            return control.ui.materialIcon("content-copy");
        }
        return control.ui.materialIcon(actionId);
    }

    function prepareSubscriptionContextMenu(index, topic, displayName, visualKey) {
        control.subscriptionContextIndex = index;
        control.subscriptionContextTopic = topic;
        control.subscriptionContextDisplayName = displayName;
        control.subscriptionActionVisualKey = visualKey;
        subscriptionActionVisualResetTimer.stop();
    }

    function openSubscriptionContextMenuAt(index, topic, displayName, visualKey, sourceItem, localX, localY) {
        control.prepareSubscriptionContextMenu(index, topic, displayName, visualKey);
        subscriptionContextMenu.openAtPoint(sourceItem, localX, localY);
    }

    function openSubscriptionActionsMenu(index, topic, displayName, visualKey, anchorItem) {
        control.prepareSubscriptionContextMenu(index, topic, displayName, visualKey);
        subscriptionContextMenu.openForItem(anchorItem);
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

    Binding {
        target: control.subscriptionModel
        property: "filterModeIndex"
        value: 0
        when: control.subscriptionModel !== null
    }

    ListModel {
        id: subscriptionContextActions

        ListElement {
            actionId: "filter"
        }
        ListElement {
            actionId: "add-filter"
        }
        ListElement {
            actionId: "copy"
        }
        ListElement {
            actionId: "edit"
        }
        ListElement {
            actionId: "delete"
        }
    }

    AppMenu {
        id: subscriptionContextMenu

        ui: control.ui
        accessibleName: qsTr("More actions")
        model: subscriptionContextActions
        actionText: actionId => control.subscriptionActionLabel(actionId)
        actionIcon: actionId => control.subscriptionActionIcon(actionId)
        actionDanger: actionId => actionId === "delete"
        actionSeparatorBefore: actionId => actionId === "edit"

        onTriggered: actionId => {
            if (actionId === "filter") {
                control.replaceMessageTopicFilter(control.subscriptionContextTopic);
            } else if (actionId === "add-filter") {
                control.addMessageTopicFilter(control.subscriptionContextTopic);
            } else if (actionId === "copy") {
                control.viewModel.copyMessageTopic(control.subscriptionContextTopic);
            } else if (actionId === "edit") {
                control.subscriptionEditRequested(control.subscriptionContextIndex);
            } else if (actionId === "delete") {
                control.viewModel.requestSubscriptionDelete(control.subscriptionContextTopic, control.subscriptionContextDisplayName);
            }
            control.subscriptionContextIndex = -1;
            control.subscriptionContextTopic = "";
            control.subscriptionContextDisplayName = "";
        }

        onAboutToHide: Qt.callLater(function () {
            subscriptionActionVisualResetTimer.restart();
        })

        onClosed: {
            control.subscriptionContextIndex = -1;
            control.subscriptionContextTopic = "";
            control.subscriptionContextDisplayName = "";
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            spacing: 8

            AppTextField {
                id: filterTopicField

                ui: control.ui
                Layout.fillWidth: true
                Layout.preferredHeight: 28
                leftPadding: 34
                placeholderText: qsTr("Filter topics...")
                text: control.subscriptionModel ? control.subscriptionModel.filterText : ""
                onTextEdited: {
                    if (control.subscriptionModel) {
                        control.subscriptionModel.filterText = text;
                    }
                }

                AppIconButton {
                    ui: control.ui
                    anchors.left: parent.left
                    anchors.leftMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    implicitWidth: 24
                    implicitHeight: 24
                    iconSource: control.ui.materialIcon("search")
                    iconSize: 15
                    restBg: "transparent"
                    hoverBg: "transparent"
                    pressedBg: "transparent"
                    outlineColor: "transparent"
                    symbolColor: control.ui.textMuted
                    enabled: false
                    Accessible.ignored: true
                }

                background: Rectangle {
                    radius: 8
                    color: control.ui.themePalette.innerPanelBg
                    border.color: control.ui.themePalette.fieldBorder

                    Behavior on border.color {
                        ColorAnimation {
                            duration: 120
                            easing.type: Easing.OutCubic
                        }
                    }
                }
            }

            AppIconButton {
                ui: control.ui
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                iconSource: control.ui.materialIcon(control.allSubscriptionsPaused ? "play" : "pause")
                iconSize: 14
                cornerRadius: 7
                restBg: control.allSubscriptionsPaused ? control.ui.themePalette.selectedBg : "transparent"
                hoverBg: control.ui.themePalette.rowHover
                outlineColor: control.allSubscriptionsPaused ? control.ui.themePalette.selectedBorder : "transparent"
                symbolColor: control.allSubscriptionsPaused ? control.ui.themePalette.infoText : control.ui.textMuted
                accessibleName: control.allSubscriptionsPaused ? qsTr("Resume all topics") : qsTr("Pause all topics")
                toolTipText: accessibleName
                toolTipPosition: AppToolTip.Position.Bottom
                onClicked: control.viewModel.setAllCurrentSubscriptionsPaused(!control.allSubscriptionsPaused)
            }

            AppIconButton {
                ui: control.ui
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                iconSource: control.ui.materialIcon("plus")
                iconSize: 16
                cornerRadius: 7
                restBg: control.ui.themePalette.itemBg
                hoverBg: control.ui.themePalette.rowHover
                outlineColor: control.ui.themePalette.fieldBorder
                symbolColor: control.ui.textMuted
                accessibleName: qsTr("Add topic")
                toolTipText: qsTr("Add subscription")
                toolTipPosition: AppToolTip.Position.Bottom
                onClicked: control.subscriptionCreateRequested()
            }

            AppIconButton {
                ui: control.ui
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                iconSource: control.ui.materialIcon("chevron-left")
                iconSize: 16
                cornerRadius: 7
                restBg: "transparent"
                hoverBg: control.ui.themePalette.rowHover
                outlineColor: "transparent"
                symbolColor: control.ui.textMuted
                accessibleName: qsTr("Hide subscription list")
                toolTipText: qsTr("Hide subscription list")
                toolTipPosition: AppToolTip.Position.Bottom
                onClicked: control.collapseRequested()
            }
        }

        Rectangle {
            visible: control.matchingSubscriptionCount === 0
            Layout.fillWidth: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            Layout.topMargin: 8
            Layout.preferredHeight: emptySubscriptionColumn.implicitHeight + 18
            radius: control.ui.innerRadius
            color: control.ui.themePalette.innerPanelBg
            border.width: 0

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
            Layout.margins: 8
            clip: true
            spacing: 4
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
                required property string topicColor
                required property bool paused
                required property string lastError
                required property real topicFps
                required property var topicRateHistory
                readonly property bool hasError: subscriptionDelegate.lastError.length > 0
                readonly property bool subscriptionActive: !subscriptionDelegate.paused
                readonly property bool activeTraffic: subscriptionDelegate.topicFps > 0
                                                      && !subscriptionDelegate.paused
                                                      && !subscriptionDelegate.hasError
                readonly property bool filtersMessages: control.viewModel.filteredMessages.selectedTopics.indexOf(subscriptionDelegate.topic) >= 0
                readonly property string rateText: qsTr("%1/s").arg(subscriptionDelegate.topicFps > 0
                                                                     ? Number(subscriptionDelegate.topicFps).toFixed(1)
                                                                     : "0")
                readonly property string secondaryTopic: subscriptionDelegate.alias.length > 0
                                                         ? subscriptionDelegate.topic
                                                         : ""
                readonly property color topicSwatchColor: subscriptionDelegate.topicColor.length > 0
                                                          ? subscriptionDelegate.topicColor
                                                          : control.ui.themePalette.selectedBorder
                readonly property string accessibleDescription: subscriptionDelegate.hasError
                                                                ? subscriptionDelegate.lastError
                                                                : (subscriptionDelegate.paused
                                                                   ? qsTr("Paused, %1").arg(subscriptionDelegate.rateText)
                                                                   : subscriptionDelegate.rateText)
                readonly property string menuVisualKey: `${subscriptionDelegate.topic}::menu`
                width: ListView.view.width
                implicitHeight: subscriptionDelegate.hasError ? 60 : 46
                radius: 7
                color: subscriptionDelegate.subscriptionActive
                       ? (subscriptionDelegate.filtersMessages
                          ? control.ui.themePalette.selectedBg
                          : control.ui.themePalette.innerPanelBg)
                       : (subscriptionRowHover.hovered ? control.ui.themePalette.rowHover : "transparent")
                border.color: subscriptionDelegate.hasError
                              ? control.ui.themePalette.errorText
                              : (subscriptionDelegate.filtersMessages
                                 ? control.ui.themePalette.selectedBorder
                              : (subscriptionDelegate.subscriptionActive
                                 ? control.ui.themePalette.panelBorder
                                 : "transparent"))
                border.width: subscriptionDelegate.hasError
                              ? 1
                              : (subscriptionDelegate.subscriptionActive ? 1 : 0)
                Accessible.role: Accessible.ListItem
                Accessible.name: subscriptionDelegate.displayName
                Accessible.description: subscriptionDelegate.accessibleDescription
                activeFocusOnTab: true

                function toggleMessageFilter() {
                    const selectedTopics = control.viewModel.filteredMessages.selectedTopics;
                    if (selectedTopics.length === 1 && selectedTopics[0] === subscriptionDelegate.topic) {
                        control.replaceMessageTopicFilter("");
                    } else {
                        control.replaceMessageTopicFilter(subscriptionDelegate.topic);
                    }
                }

                function openSubscriptionContextMenuAt(localX, localY) {
                    control.openSubscriptionContextMenuAt(subscriptionDelegate.index,
                                                          subscriptionDelegate.topic,
                                                          subscriptionDelegate.displayName,
                                                          subscriptionDelegate.menuVisualKey,
                                                          subscriptionDelegate,
                                                          localX,
                                                          localY);
                }

                function openSubscriptionActionsMenu(anchorItem) {
                    control.openSubscriptionActionsMenu(subscriptionDelegate.index,
                                                        subscriptionDelegate.topic,
                                                        subscriptionDelegate.displayName,
                                                        subscriptionDelegate.menuVisualKey,
                                                        anchorItem);
                }

                Keys.onPressed: event => {
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
                        subscriptionDelegate.toggleMessageFilter();
                        event.accepted = true;
                    } else if (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && event.modifiers & Qt.ShiftModifier)) {
                        subscriptionDelegate.openSubscriptionActionsMenu(subscriptionDelegate);
                        event.accepted = true;
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: mouse => {
                        if (mouse.button === Qt.LeftButton) {
                            subscriptionList.currentIndex = subscriptionDelegate.index;
                            subscriptionDelegate.forceActiveFocus();
                            subscriptionDelegate.toggleMessageFilter();
                        } else if (mouse.button === Qt.RightButton) {
                            subscriptionDelegate.openSubscriptionContextMenuAt(mouse.x, mouse.y);
                        }
                    }
                }

                HoverHandler {
                    id: subscriptionRowHover
                }

                AppToolTip {
                    ui: control.ui
                    text: subscriptionDelegate.topic
                    position: AppToolTip.Position.Right
                    active: subscriptionRowHover.hovered
                            && (subscriptionDisplayName.truncated || subscriptionTopicLabel.truncated)
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 6
                    anchors.topMargin: 6
                    anchors.bottomMargin: 6
                    spacing: 3

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 7

                        Rectangle {
                            Layout.preferredWidth: 6
                            Layout.preferredHeight: 30
                            Layout.alignment: Qt.AlignVCenter
                            radius: 3
                            color: subscriptionDelegate.topicSwatchColor
                            opacity: subscriptionDelegate.paused ? 0.5 : 1.0
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 1

                            Label {
                                id: subscriptionDisplayName
                                Layout.fillWidth: true
                                text: subscriptionDelegate.displayName
                                color: subscriptionDelegate.paused ? control.ui.textMuted : control.ui.textStrong
                                font.pixelSize: 12
                                font.bold: true
                                elide: Label.ElideRight
                            }

                            Label {
                                id: subscriptionTopicLabel
                                visible: subscriptionDelegate.secondaryTopic.length > 0
                                Layout.fillWidth: true
                                text: subscriptionDelegate.secondaryTopic
                                color: control.ui.themePalette.textSubtle
                                font.pixelSize: 10
                                elide: Label.ElideRight
                            }
                        }

                        RowLayout {
                            id: subscriptionActionGroup
                            spacing: 2

                            Row {
                                Layout.preferredWidth: 30
                                Layout.preferredHeight: 16
                                Layout.alignment: Qt.AlignVCenter
                                spacing: 1
                                visible: subscriptionDelegate.topicRateHistory.some(
                                             value => Number(value) > 0)

                                Repeater {
                                    model: subscriptionDelegate.topicRateHistory

                                    delegate: Rectangle {
                                        id: rateBar
                                        required property int index
                                        required property var modelData
                                        readonly property real sample: Number(rateBar.modelData || 0)
                                        readonly property real peak: Math.max(
                                                                         1,
                                                                         ...subscriptionDelegate.topicRateHistory.map(
                                                                             value => Number(value || 0)))
                                        width: 2.5
                                        height: Math.max(2, 14 * rateBar.sample / rateBar.peak)
                                        anchors.bottom: parent.bottom
                                        radius: 1
                                        color: control.ui.withAlpha(
                                                   subscriptionDelegate.topicSwatchColor,
                                                   rateBar.index === subscriptionDelegate.topicRateHistory.length - 1
                                                   ? 0.95
                                                   : 0.45)
                                    }
                                }
                            }

                            Label {
                                Layout.preferredWidth: 42
                                Layout.minimumWidth: 42
                                Layout.maximumWidth: 42
                                text: subscriptionDelegate.rateText
                                color: subscriptionDelegate.activeTraffic
                                       ? control.ui.textStrong
                                       : control.ui.withAlpha(control.ui.textMuted, 0.55)
                                font.pixelSize: 10
                                font.bold: subscriptionDelegate.activeTraffic
                                horizontalAlignment: Text.AlignRight
                            }

                            AppIconButton {
                                id: subscriptionPauseButton
                                ui: control.ui
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                                iconSource: control.ui.materialIcon(subscriptionDelegate.paused ? "play" : "pause")
                                iconSize: 13
                                cornerRadius: 6
                                restBg: subscriptionDelegate.paused ? control.ui.themePalette.selectedBg : "transparent"
                                hoverBg: subscriptionDelegate.paused ? control.ui.themePalette.buttonPrimaryHoverBg : "transparent"
                                pressedBg: subscriptionDelegate.paused ? control.ui.themePalette.buttonPrimaryPressedBg : "transparent"
                                outlineColor: "transparent"
                                symbolColor: subscriptionDelegate.paused
                                             ? (subscriptionPauseButton.hovered || subscriptionPauseButton.down
                                                ? control.ui.themePalette.buttonPrimaryText
                                                : control.ui.themePalette.infoText)
                                             : (subscriptionPauseButton.hovered || subscriptionPauseButton.forceActive
                                                ? control.ui.themePalette.infoText
                                                : control.ui.textMuted)

                                forceActive: control.subscriptionActionVisualKey === visualKey
                                readonly property string visualKey: `${subscriptionDelegate.topic}::pause`
                                accessibleName: subscriptionDelegate.paused ? qsTr("Resume topic") : qsTr("Pause topic")
                                toolTipText: subscriptionPauseButton.accessibleName

                                onClicked: {
                                    control.subscriptionActionVisualKey = visualKey;
                                    subscriptionActionVisualResetTimer.restart();
                                    control.viewModel.toggleCurrentSubscriptionPaused(subscriptionDelegate.topic, subscriptionDelegate.paused);
                                }
                            }

                            AppIconButton {
                                id: subscriptionMenuButton
                                ui: control.ui
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                                iconSource: control.ui.materialIcon("more-horiz")
                                iconSize: 16
                                cornerRadius: 12
                                restBg: "transparent"
                                hoverBg: "transparent"
                                pressedBg: "transparent"
                                outlineColor: "transparent"
                                symbolColor: subscriptionMenuButton.hovered || subscriptionMenuButton.forceActive ? control.ui.themePalette.infoText : control.ui.textMuted

                                forceActive: control.subscriptionActionVisualKey === subscriptionDelegate.menuVisualKey
                                accessibleName: qsTr("More actions")
                                toolTipText: subscriptionMenuButton.accessibleName

                                onClicked: {
                                    subscriptionDelegate.openSubscriptionActionsMenu(subscriptionMenuButton);
                                }
                            }
                        }
                    }

                    Label {
                        visible: subscriptionDelegate.hasError
                        Layout.fillWidth: true
                        text: subscriptionDelegate.lastError
                        color: control.ui.themePalette.errorText
                        font.pixelSize: 10
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
        transformOrigin: Popup.Center
        width: Math.min(340, Overlay.overlay.width - 32)

        enter: Transition {
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: 200
                easing.type: Easing.OutCubic
            }

            NumberAnimation {
                property: "scale"
                from: 0.92
                to: 1
                duration: 200
                easing.type: Easing.OutCubic
            }
        }

        exit: Transition {
            NumberAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: 160
                easing.type: Easing.InCubic
            }

            NumberAnimation {
                property: "scale"
                from: 1
                to: 0.96
                duration: 160
                easing.type: Easing.InCubic
            }
        }

        Overlay.modal: AppDialogOverlay {
            ui: control.ui
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
