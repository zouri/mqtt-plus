pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

Item {
    id: root

    required property var publisher
    required property var publishStatus
    required property var status
    required property AppUi ui

    signal manageDraftsRequested

    property bool expanded: true
    property int composerHeight: 166
    property real expansionProgress: 1
    property string fontFamily: ""
    readonly property int collapsedHeight: root.ui.compactControlHeight + 2
    readonly property int metadataControlHeight: root.ui.compactCheckHeight
    property bool propertiesExpanded: false
    readonly property int minComposerHeight: 150
    readonly property int propertiesMinComposerHeight: 280
    readonly property int maxComposerHeight: 300
    readonly property int effectiveMinComposerHeight: root.propertiesExpanded
                                                      ? root.propertiesMinComposerHeight
                                                      : root.minComposerHeight
    readonly property int effectiveComposerHeight: Math.max(root.composerHeight,
                                                             root.effectiveMinComposerHeight)
    readonly property real animatedComposerHeight: root.collapsedHeight
                                                   + (root.effectiveComposerHeight - root.collapsedHeight)
                                                   * root.expansionProgress
    readonly property bool expansionInProgress: Math.abs(root.expansionProgress
                                                         - (root.expanded ? 1 : 0)) > 0.001
    readonly property color surfaceBg: root.ui.themePalette.panelBg
    readonly property string publishFeedback: root.publishStatus.state && root.publishStatus.state !== "idle" ? (root.publishStatus.reason && root.publishStatus.reason.length > 0 ? root.publishStatus.reason : qsTr("Publish status: %1").arg(root.ui.statusLabel(root.publishStatus.state))) : ""
    readonly property string publishDisabledReason: root.status.state !== "connected" ? qsTr("Connect before publishing") : (root.publisher.topic.trim().length === 0 ? qsTr("Enter a topic before publishing") : "")
    readonly property color publishFeedbackColor: root.publishStatus.state === "failed" ? root.ui.themePalette.errorText : (root.publishStatus.state === "sent" || root.publishStatus.state === "acknowledged" || root.publishStatus.state === "completed" ? root.ui.themePalette.successText : root.ui.textMuted)
    property bool publishPulseActive: false
    property int sendLibraryTabIndex: 0
    property int pendingDraftIndex: -1
    property string pendingDraftId: ""
    property string saveDraftError: ""

    SplitView.fillWidth: true
    SplitView.preferredHeight: root.animatedComposerHeight
    SplitView.minimumHeight: root.expansionInProgress
                             ? root.animatedComposerHeight
                             : (root.expanded ? root.effectiveMinComposerHeight : root.collapsedHeight)
    SplitView.maximumHeight: root.expansionInProgress
                             ? root.animatedComposerHeight
                             : (root.expanded ? root.maxComposerHeight : root.collapsedHeight)
    clip: root.expansionInProgress

    function updateExpansion(animate) {
        const targetProgress = root.expanded ? 1 : 0;
        composerExpansionAnimation.stop();
        if (!animate || !root.ui.animationsEnabled || root.ui.motionPanelDuration <= 0) {
            root.expansionProgress = targetProgress;
            return;
        }
        composerExpansionAnimation.to = targetProgress;
        composerExpansionAnimation.restart();
    }

    onExpandedChanged: root.updateExpansion(true)

    NumberAnimation {
        id: composerExpansionAnimation

        target: root
        property: "expansionProgress"
        duration: root.ui.motionPanelDuration
        easing.type: root.ui.motionEnterEasing
    }

    Connections {
        target: root.ui

        function onAnimationsEnabledChanged() {
            if (!root.ui.animationsEnabled) {
                root.updateExpansion(false);
            }
        }
    }

    function resizeComposer(height) {
        root.composerHeight = Math.max(root.effectiveMinComposerHeight,
                                       Math.min(root.maxComposerHeight, Math.round(height)));
    }

    function revealDraftEditor() {
        root.expanded = true;
    }

    function publishDraft() {
        if (root.publisher.canPublish) {
            root.publisher.publishDraft();
        }
    }

    function requestDraftLoad(index, draftId) {
        root.pendingDraftIndex = index
        root.pendingDraftId = draftId
        if (root.publisher.wouldReplaceWithDraft(index)) {
            replaceComposerDialog.open()
            return
        }
        root.publisher.useSavedDraft(index)
        sendLibraryPopup.close()
    }

    function requestDraftQuickPublish(index, draftId) {
        root.pendingDraftIndex = index
        root.pendingDraftId = draftId
        if (root.publisher.draftNeedsTopic(index)) {
            draftTopicField.text = ""
            draftTopicDialog.open()
            return
        }
        root.publisher.quickPublishDraft(index)
    }

    onPublishStatusChanged: {
        const state = String(root.publishStatus.state || "");
        if (state === "failed" || state === "sent" || state === "acknowledged" || state === "completed") {
            root.publishPulseActive = true;
            publishPulseTimer.restart();
        }
    }

    Timer {
        id: publishPulseTimer
        interval: 300
        repeat: false
        onTriggered: root.publishPulseActive = false
    }

    onHeightChanged: {
        if (root.expanded && !root.expansionInProgress && height > 0) {
            root.resizeComposer(height);
        }
    }

    Rectangle {
        anchors.fill: parent
        color: root.surfaceBg
    }

    ColumnLayout {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: root.expansionInProgress ? root.composerHeight : root.height
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: root.collapsedHeight

            Button {
                id: composerHeader

                anchors.fill: parent
                Accessible.name: root.expanded ? qsTr("Collapse publish composer") : qsTr("Expand publish composer")

                contentItem: Item {}

                background: Item {}

                onClicked: root.expanded = !root.expanded
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: root.ui.spaceSm
                anchors.rightMargin: root.ui.spaceSm
                anchors.topMargin: 3
                anchors.bottomMargin: 3
                spacing: 6

                Label {
                    text: qsTr("Publish Message")
                    color: root.ui.textStrong
                    font.pixelSize: root.ui.textSm
                    font.bold: true
                }

                Label {
                    visible: root.publishFeedback.length > 0
                    Layout.fillWidth: true
                    text: root.publishFeedback
                    color: root.publishFeedbackColor
                    font.pixelSize: root.ui.textXs
                    elide: Label.ElideRight
                }

                Item {
                    visible: root.publishFeedback.length === 0
                    Layout.fillWidth: true
                }

                AppIconButton {
                    id: sendLibraryButton

                    ui: root.ui
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    cornerRadius: 7
                    iconSource: root.ui.materialIcon("drafts")
                    iconSize: 14
                    restBg: sendLibraryPopup.visible ? root.ui.themePalette.selectedBg : "transparent"
                    hoverBg: root.ui.themePalette.rowHover
                    outlineColor: sendLibraryPopup.visible ? root.ui.themePalette.selectedBorder : "transparent"
                    symbolColor: sendLibraryPopup.visible ? root.ui.themePalette.infoText : root.ui.textMuted
                    accessibleName: qsTr("Send Library")
                    toolTipText: qsTr("Drafts and recent publishes")
                    toolTipPosition: AppToolTip.Position.Bottom
                    onClicked: sendLibraryPopup.visible ? sendLibraryPopup.close() : sendLibraryPopup.open()
                }

                AppIconButton {
                    id: saveAsDraftButton

                    ui: root.ui
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    cornerRadius: 7
                    iconSource: root.ui.materialIcon("plus")
                    iconSize: 14
                    restBg: "transparent"
                    hoverBg: root.ui.themePalette.rowHover
                    outlineColor: "transparent"
                    symbolColor: root.ui.textMuted
                    enabled: root.publisher.draftsReady && !root.publisher.draftsBusy
                    accessibleName: qsTr("Save composer as draft")
                    toolTipText: accessibleName
                    toolTipPosition: AppToolTip.Position.Bottom
                    onClicked: {
                        root.saveDraftError = ""
                        saveAsDraftNameField.text = ""
                        saveAsDraftDialog.open()
                    }
                }

                AppIconButton {
                    id: collapseButton

                    ui: root.ui
                    readonly property bool collapseHoverActive: collapseButton.hovered
                                                                 || (composerHeader.hovered
                                                                     && !sendLibraryButton.hovered
                                                                     && !saveAsDraftButton.hovered)
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    cornerRadius: 7
                    iconSource: root.ui.materialIcon(root.expanded ? "chevron-down" : "chevron-up")
                    iconSize: 18
                    restBg: root.ui.themePalette.itemBg
                    hoverBg: root.ui.themePalette.rowHover
                    outlineColor: root.ui.themePalette.fieldBorder
                    symbolColor: collapseButton.collapseHoverActive
                                 ? root.ui.textStrong
                                 : root.ui.textMuted
                    forceActive: collapseButton.collapseHoverActive
                    accessibleName: composerHeader.Accessible.name
                    toolTipText: root.expanded ? qsTr("Collapse") : qsTr("Expand")
                    toolTipPosition: AppToolTip.Position.Bottom
                    onClicked: root.expanded = !root.expanded
                }
            }
        }

        RowLayout {
            visible: root.expanded || root.expansionInProgress
            Layout.fillWidth: true
            Layout.leftMargin: root.ui.spaceSm
            Layout.rightMargin: root.ui.spaceSm
            Layout.topMargin: 0
            Layout.bottomMargin: 7
            Layout.preferredHeight: visible ? root.metadataControlHeight : 0
            spacing: 6

            AppTextField {
                id: publishTopicField
                ui: root.ui
                Layout.fillWidth: true
                Layout.minimumWidth: 180
                Layout.preferredWidth: 340
                Layout.maximumWidth: 380
                Layout.preferredHeight: root.metadataControlHeight
                text: root.publisher.topic
                placeholderText: qsTr("home/living-room/light/set")
                onTextEdited: root.publisher.topic = text
            }

            AppComboBox {
                id: publishQosBox
                ui: root.ui
                Layout.preferredWidth: 88
                Layout.preferredHeight: root.metadataControlHeight
                model: [qsTr("QoS 0"), qsTr("QoS 1"), qsTr("QoS 2")]
                currentIndex: root.publisher.qos
                onActivated: root.publisher.qos = currentIndex
            }

            AppComboBox {
                id: publishFormatBox
                ui: root.ui
                Layout.preferredWidth: 104
                Layout.preferredHeight: root.metadataControlHeight
                model: root.publisher.payloadFormats
                currentIndex: root.publisher.format
                onActivated: root.publisher.format = currentIndex
            }

            AppCheckBox {
                id: retainCheck
                ui: root.ui
                Layout.preferredWidth: 68
                Layout.preferredHeight: root.metadataControlHeight
                text: qsTr("Retain")
                checked: root.publisher.retain
                onToggled: root.publisher.retain = checked
            }

            Label {
                visible: root.publisher.retain
                text: qsTr("Retain enabled")
                color: root.ui.themePalette.warningText
                font.pixelSize: root.ui.textXs
                font.bold: true
            }

            AppIconButton {
                ui: root.ui
                Layout.preferredWidth: root.metadataControlHeight
                Layout.preferredHeight: root.metadataControlHeight
                iconSource: root.ui.materialIcon("settings")
                iconSize: 15
                forceActive: root.propertiesExpanded
                accessibleName: qsTr("MQTT 5 properties")
                toolTipText: qsTr("MQTT 5 properties")
                toolTipPosition: AppToolTip.Position.Bottom
                onClicked: root.propertiesExpanded = !root.propertiesExpanded
            }

            Item {
                Layout.fillWidth: true
            }
        }

        RowLayout {
            visible: root.propertiesExpanded && (root.expanded || root.expansionInProgress)
            Layout.fillWidth: true
            Layout.leftMargin: root.ui.spaceSm
            Layout.rightMargin: root.ui.spaceSm
            Layout.bottomMargin: 6
            Layout.preferredHeight: visible ? root.metadataControlHeight : 0
            spacing: 6

            AppCheckBox {
                ui: root.ui
                Layout.preferredWidth: 82
                Layout.preferredHeight: root.metadataControlHeight
                text: qsTr("UTF-8")
                checked: root.publisher.payloadUtf8
                onToggled: root.publisher.payloadUtf8 = checked
            }
            AppTextField {
                ui: root.ui
                Layout.fillWidth: true
                Layout.preferredHeight: root.metadataControlHeight
                placeholderText: qsTr("Content type")
                text: root.publisher.contentType
                onTextEdited: root.publisher.contentType = text
            }
            AppTextField {
                ui: root.ui
                Layout.preferredWidth: 112
                Layout.preferredHeight: root.metadataControlHeight
                inputMethodHints: Qt.ImhDigitsOnly
                placeholderText: qsTr("Expiry (s)")
                text: root.publisher.messageExpiryText
                onTextEdited: root.publisher.messageExpiryText = text
            }
            AppTextField {
                ui: root.ui
                Layout.preferredWidth: 106
                Layout.preferredHeight: root.metadataControlHeight
                inputMethodHints: Qt.ImhDigitsOnly
                placeholderText: qsTr("Topic alias")
                text: root.publisher.topicAliasText
                onTextEdited: root.publisher.topicAliasText = text
            }
        }

        RowLayout {
            visible: root.propertiesExpanded && (root.expanded || root.expansionInProgress)
            Layout.fillWidth: true
            Layout.leftMargin: root.ui.spaceSm
            Layout.rightMargin: root.ui.spaceSm
            Layout.bottomMargin: 6
            Layout.preferredHeight: visible ? root.metadataControlHeight : 0
            spacing: 6

            AppTextField {
                ui: root.ui
                Layout.fillWidth: true
                Layout.preferredHeight: root.metadataControlHeight
                placeholderText: qsTr("Response topic")
                text: root.publisher.responseTopic
                onTextEdited: root.publisher.responseTopic = text
            }
            AppTextField {
                ui: root.ui
                Layout.fillWidth: true
                Layout.preferredHeight: root.metadataControlHeight
                placeholderText: qsTr("Correlation data (Base64)")
                text: root.publisher.correlationDataBase64
                onTextEdited: root.publisher.correlationDataBase64 = text
            }
        }

        AppTextArea {
            visible: root.propertiesExpanded && (root.expanded || root.expansionInProgress)
            ui: root.ui
            Layout.fillWidth: true
            Layout.leftMargin: root.ui.spaceSm
            Layout.rightMargin: root.ui.spaceSm
            Layout.bottomMargin: 6
            Layout.preferredHeight: visible ? 58 : 0
            placeholderText: qsTr("MQTT 5 user properties: name=value, one per line")
            text: root.publisher.userPropertiesText
            onTextChanged: {
                if (root.publisher.userPropertiesText !== text) {
                    root.publisher.userPropertiesText = text
                }
            }
        }

        Item {
            visible: root.expanded || root.expansionInProgress
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: root.ui.spaceSm
            Layout.rightMargin: root.ui.spaceSm
            Layout.bottomMargin: 12

            AppTextArea {
                id: publishPayloadArea
                ui: root.ui
                anchors.fill: parent
                font.family: root.fontFamily
                placeholderText: publishFormatBox.currentText === "JSON" ? "{\"value\": 23.7}" : qsTr("Payload")
                text: root.publisher.payload
                wrapMode: TextEdit.Wrap
                submitOnCtrlEnter: true
                onTextChanged: {
                    if (root.publisher.payload !== text) {
                        root.publisher.payload = text;
                    }
                }
                onSubmitRequested: root.publishDraft()
            }

            AppIconButton {
                id: publishButton

                ui: root.ui
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: 8
                anchors.bottomMargin: 8
                implicitWidth: 30
                implicitHeight: 30
                cornerRadius: 8
                iconSource: root.ui.materialIcon("send")
                iconSize: 16
                primary: true
                forceActive: root.publishPulseActive
                danger: root.publishPulseActive && root.publishStatus.state === "failed"
                enabled: root.publisher.canPublish
                accessibleName: qsTr("Publish message")
                toolTipText: root.publisher.canPublish ? qsTr("Publish message (%1+Enter)").arg(Qt.platform.os === "osx" ? qsTr("Command") : qsTr("Ctrl")) : root.publishDisabledReason
                onClicked: root.publishDraft()
            }
        }
    }

    AppPopover {
        id: sendLibraryPopup

        ui: root.ui
        width: Math.min(470, root.width - 24)
        height: 376
        x: Math.max(12, root.width - width - 12)
        y: -height - 6
        padding: 10
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: root.ui.radiusMd
            color: root.ui.themePalette.dialogBg
            border.color: root.ui.themePalette.dialogBorder
        }

        contentItem: ColumnLayout {
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Send Library")
                    color: root.ui.textStrong
                    font.pixelSize: root.ui.textMd
                    font.bold: true
                }

                AppButton {
                    ui: root.ui
                    text: qsTr("Manage drafts")
                    minimumWidth: 104
                    onClicked: {
                        sendLibraryPopup.close()
                        root.manageDraftsRequested()
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                AppButton {
                    ui: root.ui
                    Layout.fillWidth: true
                    text: qsTr("Drafts")
                    primary: root.sendLibraryTabIndex === 0
                    onClicked: root.sendLibraryTabIndex = 0
                }

                AppButton {
                    ui: root.ui
                    Layout.fillWidth: true
                    text: qsTr("Recent")
                    primary: root.sendLibraryTabIndex === 1
                    onClicked: root.sendLibraryTabIndex = 1
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.sendLibraryTabIndex

                ColumnLayout {
                    spacing: 7

                    AppTextField {
                        ui: root.ui
                        Layout.fillWidth: true
                        placeholderText: qsTr("Search draft name, description, Topic, or Payload")
                        text: root.publisher.drafts.filterText
                        onTextEdited: root.publisher.setDraftFilterText(text)
                    }

                    Label {
                        visible: root.publisher.draftsLoading
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: qsTr("Loading draft library…")
                        color: root.ui.textMuted
                        font.pixelSize: root.ui.textSm
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    Label {
                        visible: !root.publisher.draftsLoading
                                 && root.publisher.drafts.count === 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: root.publisher.drafts.filterText.length > 0
                              ? qsTr("No matching drafts")
                              : qsTr("Save a composer message to build your Draft Library.")
                        color: root.ui.textMuted
                        font.pixelSize: root.ui.textSm
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        wrapMode: Text.Wrap
                    }

                    ListView {
                        id: savedDraftList

                        visible: !root.publisher.draftsLoading
                                 && root.publisher.drafts.count > 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: root.publisher.drafts
                        spacing: 4
                        clip: true
                        reuseItems: true

                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                        }

                        delegate: Rectangle {
                            id: savedDraftDelegate

                            required property int index
                            required property string id
                            required property string name
                            required property string description
                            required property string defaultTopic
                            required property string payloadPreview
                            required property string formatName
                            required property int qos
                            required property bool retain

                            width: ListView.view.width
                            height: 58
                            radius: root.ui.radiusSm
                            color: savedDraftHover.hovered
                                   ? root.ui.themePalette.rowHover
                                   : root.ui.themePalette.innerPanelBg

                            HoverHandler {
                                id: savedDraftHover
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 9
                                anchors.rightMargin: 6
                                spacing: 7

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    spacing: 2

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6

                                        Label {
                                            Layout.fillWidth: true
                                            text: savedDraftDelegate.name
                                            color: root.ui.textStrong
                                            font.pixelSize: root.ui.textSm
                                            font.bold: true
                                            elide: Label.ElideRight
                                        }

                                        Label {
                                            visible: savedDraftDelegate.retain
                                            text: qsTr("RETAIN")
                                            color: root.ui.themePalette.warningText
                                            font.pixelSize: root.ui.textXs
                                            font.bold: true
                                        }
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: savedDraftDelegate.defaultTopic.length > 0
                                              ? savedDraftDelegate.defaultTopic
                                              : qsTr("Topic requested when sending")
                                        color: root.ui.textMuted
                                        font.pixelSize: root.ui.textXs
                                        elide: Label.ElideMiddle
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("%1 · QoS %2 · %3")
                                              .arg(savedDraftDelegate.formatName)
                                              .arg(savedDraftDelegate.qos)
                                              .arg(savedDraftDelegate.payloadPreview.length > 0
                                                   ? savedDraftDelegate.payloadPreview
                                                   : qsTr("Empty payload"))
                                        color: root.ui.themePalette.textSubtle
                                        font.pixelSize: root.ui.textXs
                                        elide: Label.ElideRight
                                    }
                                }

                                AppIconButton {
                                    ui: root.ui
                                    Layout.preferredWidth: 26
                                    Layout.preferredHeight: 26
                                    cornerRadius: root.ui.radiusSm
                                    iconSource: root.ui.materialIcon("content-copy")
                                    iconSize: 12
                                    accessibleName: qsTr("Load draft into composer")
                                    toolTipText: accessibleName
                                    onClicked: root.requestDraftLoad(
                                                   savedDraftDelegate.index,
                                                   savedDraftDelegate.id)
                                }

                                AppIconButton {
                                    ui: root.ui
                                    Layout.preferredWidth: 26
                                    Layout.preferredHeight: 26
                                    cornerRadius: root.ui.radiusSm
                                    iconSource: root.ui.materialIcon("send")
                                    iconSize: 12
                                    primary: true
                                    enabled: root.status.state === "connected"
                                    accessibleName: qsTr("Quick publish draft")
                                    toolTipText: enabled
                                                 ? (savedDraftDelegate.retain
                                                    ? qsTr("Quick publish with Retain enabled")
                                                    : accessibleName)
                                                 : qsTr("Connect before publishing")
                                    onClicked: root.requestDraftQuickPublish(
                                                   savedDraftDelegate.index,
                                                   savedDraftDelegate.id)
                                }
                            }
                        }
                    }
                }

                ColumnLayout {
                    spacing: 7

                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Runtime-only recent publishes")
                            color: root.ui.textMuted
                            font.pixelSize: root.ui.textXs
                        }

                        AppIconButton {
                            ui: root.ui
                            visible: root.publisher.recentPublishes.length > 0
                            Layout.preferredWidth: 26
                            Layout.preferredHeight: 26
                            cornerRadius: root.ui.radiusSm
                            iconSource: root.ui.materialIcon("delete")
                            iconSize: 12
                            restBg: "transparent"
                            outlineColor: "transparent"
                            accessibleName: qsTr("Clear recent publishes")
                            toolTipText: accessibleName
                            onClicked: root.publisher.clearRecentPublishes()
                        }
                    }

                    Label {
                        visible: root.publisher.recentPublishes.length === 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: qsTr("Published messages will appear here for quick reuse.")
                        color: root.ui.textMuted
                        font.pixelSize: root.ui.textSm
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        wrapMode: Text.Wrap
                    }

                    ListView {
                        id: recentPublishList

                        visible: root.publisher.recentPublishes.length > 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: root.publisher.recentPublishes
                        spacing: 4
                        clip: true
                        reuseItems: true

                        delegate: Rectangle {
                            id: recentPublishDelegate

                            required property int index
                            required property var modelData

                            width: ListView.view.width
                            height: 52
                            radius: root.ui.radiusSm
                            color: recentPublishHover.hovered
                                   ? root.ui.themePalette.rowHover
                                   : root.ui.themePalette.innerPanelBg

                            HoverHandler {
                                id: recentPublishHover
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 9
                                anchors.rightMargin: 6
                                spacing: 7

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    spacing: 2

                                    Label {
                                        Layout.fillWidth: true
                                        text: String(recentPublishDelegate.modelData.topic || "")
                                        color: root.ui.textStrong
                                        font.pixelSize: root.ui.textSm
                                        font.bold: true
                                        elide: Label.ElideMiddle
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("%1 · QoS %2 · %3")
                                              .arg(String(recentPublishDelegate.modelData.formatName || ""))
                                              .arg(Number(recentPublishDelegate.modelData.qos || 0))
                                              .arg(String(recentPublishDelegate.modelData.payload || qsTr("Empty payload")))
                                        color: root.ui.textMuted
                                        font.pixelSize: root.ui.textXs
                                        elide: Label.ElideRight
                                    }
                                }

                                Label {
                                    visible: Boolean(recentPublishDelegate.modelData.retain)
                                    text: qsTr("RETAIN")
                                    color: root.ui.themePalette.warningText
                                    font.pixelSize: root.ui.textXs
                                    font.bold: true
                                }

                                AppIconButton {
                                    ui: root.ui
                                    Layout.preferredWidth: 26
                                    Layout.preferredHeight: 26
                                    cornerRadius: root.ui.radiusSm
                                    iconSource: root.ui.materialIcon("content-copy")
                                    iconSize: 12
                                    accessibleName: qsTr("Load into composer")
                                    toolTipText: accessibleName
                                    onClicked: {
                                        root.publisher.useRecentPublish(recentPublishDelegate.index)
                                        sendLibraryPopup.close()
                                    }
                                }

                                AppIconButton {
                                    ui: root.ui
                                    Layout.preferredWidth: 26
                                    Layout.preferredHeight: 26
                                    cornerRadius: root.ui.radiusSm
                                    iconSource: root.ui.materialIcon("send")
                                    iconSize: 12
                                    primary: true
                                    enabled: root.status.state === "connected"
                                    accessibleName: qsTr("Publish again")
                                    toolTipText: enabled
                                                 ? (Boolean(recentPublishDelegate.modelData.retain)
                                                    ? qsTr("Publish again with Retain enabled")
                                                    : accessibleName)
                                                 : qsTr("Connect before publishing")
                                    onClicked: root.publisher.quickPublishRecent(recentPublishDelegate.index)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    AppDialog {
        id: replaceComposerDialog

        ui: root.ui
        width: 450
        height: 200
        closePolicy: Popup.CloseOnEscape
        header: Item { implicitHeight: 0; visible: false }
        background: Rectangle {
            radius: root.ui.radiusLg
            color: root.ui.themePalette.dialogBg
            border.color: root.ui.themePalette.dialogBorder
        }
        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            Label {
                Layout.fillWidth: true
                text: qsTr("Replace composer contents?")
                color: root.ui.textStrong
                font.pixelSize: root.ui.text2xl
                font.bold: true
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Loading this draft replaces the current Topic, Payload, format, QoS, and Retain values.")
                color: root.ui.textMuted
                font.pixelSize: root.ui.textSm
                wrapMode: Text.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Item { Layout.fillWidth: true }

                AppButton {
                    ui: root.ui
                    text: qsTr("Cancel")
                    minimumWidth: 76
                    onClicked: replaceComposerDialog.close()
                }

                AppButton {
                    ui: root.ui
                    text: qsTr("Replace")
                    primary: true
                    minimumWidth: 82
                    onClicked: {
                        const draftIndex = root.publisher.draftIndexOfId(root.pendingDraftId)
                        if (root.publisher.useSavedDraft(draftIndex)) {
                            replaceComposerDialog.close()
                            sendLibraryPopup.close()
                        }
                    }
                }
            }
        }
    }

    AppDialog {
        id: draftTopicDialog

        ui: root.ui
        width: 460
        height: 250
        closePolicy: Popup.CloseOnEscape
        header: Item { implicitHeight: 0; visible: false }
        background: Rectangle {
            radius: root.ui.radiusLg
            color: root.ui.themePalette.dialogBg
            border.color: root.ui.themePalette.dialogBorder
        }
        onOpened: draftTopicField.forceActiveFocus()
        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            Label {
                Layout.fillWidth: true
                text: qsTr("Topic for this publish")
                color: root.ui.textStrong
                font.pixelSize: root.ui.text2xl
                font.bold: true
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("This draft has no default Topic. This value is used once without changing the draft or composer.")
                color: root.ui.textMuted
                font.pixelSize: root.ui.textSm
                wrapMode: Text.Wrap
            }

            AppTextField {
                id: draftTopicField

                ui: root.ui
                Layout.fillWidth: true
                placeholderText: qsTr("devices/example/set")
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Item { Layout.fillWidth: true }

                AppButton {
                    ui: root.ui
                    text: qsTr("Cancel")
                    minimumWidth: 76
                    onClicked: draftTopicDialog.close()
                }

                AppButton {
                    ui: root.ui
                    text: qsTr("Quick Publish")
                    primary: true
                    minimumWidth: 104
                    enabled: draftTopicField.text.trim().length > 0
                    onClicked: {
                        if (root.publisher.quickPublishDraft(
                                    root.publisher.draftIndexOfId(root.pendingDraftId),
                                    draftTopicField.text)) {
                            draftTopicDialog.close()
                        }
                    }
                }
            }
        }
    }

    AppDialog {
        id: saveAsDraftDialog

        ui: root.ui
        width: 450
        height: root.saveDraftError.length > 0 ? 250 : 220
        closePolicy: Popup.CloseOnEscape
        header: Item { implicitHeight: 0; visible: false }
        background: Rectangle {
            radius: root.ui.radiusLg
            color: root.ui.themePalette.dialogBg
            border.color: root.ui.themePalette.dialogBorder
        }
        onOpened: saveAsDraftNameField.forceActiveFocus()
        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            Label {
                Layout.fillWidth: true
                text: qsTr("Save composer as draft")
                color: root.ui.textStrong
                font.pixelSize: root.ui.text2xl
                font.bold: true
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("The draft is saved as an independent copy. Later composer edits do not update it automatically.")
                color: root.ui.textMuted
                font.pixelSize: root.ui.textSm
                wrapMode: Text.Wrap
            }

            AppTextField {
                id: saveAsDraftNameField

                ui: root.ui
                Layout.fillWidth: true
                maximumLength: 80
                placeholderText: qsTr("Unique draft name")
                onTextEdited: root.saveDraftError = ""
            }

            Label {
                visible: root.saveDraftError.length > 0
                Layout.fillWidth: true
                text: root.saveDraftError
                color: root.ui.themePalette.errorText
                font.pixelSize: root.ui.textXs
                wrapMode: Text.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Item { Layout.fillWidth: true }

                AppButton {
                    ui: root.ui
                    text: qsTr("Cancel")
                    minimumWidth: 76
                    onClicked: saveAsDraftDialog.close()
                }

                AppButton {
                    ui: root.ui
                    text: qsTr("Save Draft")
                    primary: true
                    minimumWidth: 92
                    enabled: saveAsDraftNameField.text.trim().length > 0
                             && root.publisher.draftsReady
                             && !root.publisher.draftsBusy
                    onClicked: {
                        if (root.publisher.saveAsDraft(saveAsDraftNameField.text)) {
                            saveAsDraftDialog.close()
                        } else {
                            root.saveDraftError = root.publisher.draftError.length > 0
                                                  ? root.publisher.draftError
                                                  : qsTr("The draft could not be saved.")
                        }
                    }
                }
            }
        }
    }
}
