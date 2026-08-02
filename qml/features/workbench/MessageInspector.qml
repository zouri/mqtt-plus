pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

Item {
    id: control

    required property AppUi ui
    required property var viewModel
    property string historyId: ""
    property var details: ({})
    property int payloadViewFormat: 0
    property string displayedPayload: ""
    property bool opened: false
    property real revealProgress: 0.0
    readonly property bool motionEnabled: control.ui.animationsEnabled
    readonly property int payloadTextMaximumHeight: 220
    readonly property int parsedTextMaximumHeight: 180

    signal closeRequested
    signal draftUsed

    component InspectorActionButton: AppButton {
        id: actionButton

        minimumWidth: 0
        leftPadding: 10
        rightPadding: 10

        background: Rectangle {
            radius: actionButton.ui.radiusMd
            color: actionButton.down
                   ? actionButton.ui.themePalette.buttonPressedBg
                   : (actionButton.hovered
                      ? actionButton.ui.themePalette.buttonHoverBg
                      : actionButton.ui.themePalette.itemBg)
            border.color: actionButton.ui.themePalette.panelBorder
            border.width: 1
        }
    }

    width: Math.min(400, parent ? parent.width * 0.88 : 400)
    transform: Translate {
        x: (1.0 - control.revealProgress) * control.width
    }
    visible: control.opened || control.revealProgress > 0.0
    enabled: control.opened
    Accessible.role: Accessible.Pane
    Accessible.ignored: !control.opened
    Accessible.name: qsTr("Message inspector")

    onHistoryIdChanged: {
        control.reloadDetails();
    }

    onOpenedChanged: {
        if (control.opened) {
            control.payloadViewFormat = Number(control.details.testFormat || 0);
            control.refreshDisplayedPayload();
        }
        control.syncRevealProgress();
    }

    onMotionEnabledChanged: control.syncRevealProgress()
    onPayloadViewFormatChanged: control.refreshDisplayedPayload()
    Component.onCompleted: control.syncRevealProgress()

    Connections {
        target: control.viewModel

        function onMessageDetailsChanged(changedHistoryId) {
            if (control.opened && changedHistoryId === control.historyId) {
                control.reloadDetails();
            }
        }
    }

    function reloadDetails() {
        control.details = control.historyId.length > 0
                          ? control.viewModel.messageDetails(control.historyId)
                          : ({});
        control.refreshDisplayedPayload();
    }

    function syncRevealProgress() {
        const targetProgress = control.opened ? 1.0 : 0.0;
        inspectorRevealAnimation.stop();
        if (!control.motionEnabled
                || Math.abs(control.revealProgress - targetProgress) < 0.001) {
            control.revealProgress = targetProgress;
            return;
        }
        inspectorRevealAnimation.to = targetProgress;
        inspectorRevealAnimation.easing.type = control.opened
                                                 ? control.ui.motionEnterEasing
                                                 : control.ui.motionExitEasing;
        inspectorRevealAnimation.restart();
    }

    function refreshDisplayedPayload() {
        const fallback = String(control.details.fullPayload || qsTr("Select a message to inspect"));
        if (control.historyId.length === 0) {
            control.displayedPayload = fallback;
            return;
        }
        control.displayedPayload = control.viewModel.messagePayloadForDisplay(
            control.historyId,
            fallback,
            control.payloadViewFormat);
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        enabled: control.opened
        onActivated: control.closeRequested()
    }

    NumberAnimation {
        id: inspectorRevealAnimation

        target: control
        property: "revealProgress"
        duration: control.ui.motionPanelDuration
    }

    Rectangle {
        id: inspectorEdgeShadow

        anchors.top: parent.top
        anchors.right: inspectorSurface.left
        anchors.bottom: parent.bottom
        width: 10
        color: "transparent"
        gradient: Gradient {
            orientation: Gradient.Horizontal

            GradientStop {
                position: 0.0
                color: "transparent"
            }

            GradientStop {
                position: 1.0
                color: control.ui.isDarkTheme ? "#52000000" : "#1f000000"
            }
        }
        Accessible.ignored: true
    }

    Rectangle {
        id: inspectorSurface

        anchors.fill: parent
        color: control.ui.themePalette.panelBg
        border.color: control.ui.themePalette.panelBorder
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            Layout.leftMargin: 14
            Layout.rightMargin: 10
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: qsTr("Message Viewer")
                color: control.ui.textStrong
                font.pixelSize: 13
                font.bold: true
            }

            AppIconButton {
                id: closeInspectorButton

                ui: control.ui
                iconSource: control.ui.materialIcon("xmark")
                iconSize: 15
                implicitWidth: 28
                implicitHeight: 28
                cornerRadius: 7
                restBg: "transparent"
                outlineColor: "transparent"
                accessibleName: qsTr("Close message inspector")
                onClicked: control.closeRequested()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: control.ui.themePalette.separator
        }

        ScrollView {
            id: inspectorScroll

            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: inspectorScroll.availableWidth
                spacing: 8

                Item { Layout.preferredHeight: 1 }

                GridLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                    columns: 2
                    columnSpacing: 10
                    rowSpacing: 0

                    Label { Layout.preferredHeight: 28; verticalAlignment: Text.AlignVCenter; text: qsTr("Alias"); color: control.ui.textMuted; font.pixelSize: 11 }
                    Label { Layout.fillWidth: true; Layout.preferredHeight: 28; verticalAlignment: Text.AlignVCenter; text: String(control.details.alias || qsTr("-")); color: String(control.details.alias || "").length > 0 ? control.ui.textStrong : control.ui.textMuted; elide: Label.ElideRight; font.pixelSize: 11 }
                    Rectangle { id: metadataSeparator1; Layout.columnSpan: 2; Layout.fillWidth: true; Layout.preferredHeight: 1; color: control.ui.themePalette.separator }
                    Label { Layout.preferredHeight: 28; verticalAlignment: Text.AlignVCenter; text: qsTr("Topic"); color: control.ui.textMuted; font.pixelSize: 11 }
                    Label { Layout.fillWidth: true; Layout.preferredHeight: 28; verticalAlignment: Text.AlignVCenter; text: String(control.details.topic || qsTr("-")); color: String(control.details.topic || "").length > 0 ? control.ui.textStrong : control.ui.textMuted; elide: Label.ElideRight; font.pixelSize: 11 }
                    Rectangle { Layout.columnSpan: 2; Layout.fillWidth: true; Layout.preferredHeight: 1; color: control.ui.themePalette.separator }
                    Label { Layout.preferredHeight: 28; verticalAlignment: Text.AlignVCenter; text: qsTr("Direction"); color: control.ui.textMuted; font.pixelSize: 11 }
                    Label { Layout.preferredHeight: 28; verticalAlignment: Text.AlignVCenter; text: control.details.direction === "outgoing" ? qsTr("Sent") : (control.details.direction === "incoming" ? qsTr("Received") : qsTr("-")); color: control.details.direction === "outgoing" || control.details.direction === "incoming" ? control.ui.textStrong : control.ui.textMuted; font.pixelSize: 11 }
                    Rectangle { Layout.columnSpan: 2; Layout.fillWidth: true; Layout.preferredHeight: 1; color: control.ui.themePalette.separator }
                    Label { Layout.preferredHeight: 28; verticalAlignment: Text.AlignVCenter; text: qsTr("Time"); color: control.ui.textMuted; font.pixelSize: 11 }
                    Label { Layout.preferredHeight: 28; verticalAlignment: Text.AlignVCenter; text: String(control.details.timestamp || qsTr("-")); color: String(control.details.timestamp || "").length > 0 ? control.ui.textStrong : control.ui.textMuted; font.pixelSize: 11 }
                    Rectangle { Layout.columnSpan: 2; Layout.fillWidth: true; Layout.preferredHeight: 1; color: control.ui.themePalette.separator }
                    Label { Layout.preferredHeight: 28; verticalAlignment: Text.AlignVCenter; text: qsTr("QoS"); color: control.ui.textMuted; font.pixelSize: 11 }
                    Label { Layout.preferredHeight: 28; verticalAlignment: Text.AlignVCenter; text: Number(control.details.qos) >= 0 ? String(control.details.qos) : qsTr("-"); color: Number(control.details.qos) >= 0 ? control.ui.textStrong : control.ui.textMuted; font.pixelSize: 11 }
                    Rectangle { Layout.columnSpan: 2; Layout.fillWidth: true; Layout.preferredHeight: 1; color: control.ui.themePalette.separator }
                    Label { Layout.preferredHeight: 28; verticalAlignment: Text.AlignVCenter; text: qsTr("Format"); color: control.ui.textMuted; font.pixelSize: 11 }
                    Label { Layout.preferredHeight: 28; verticalAlignment: Text.AlignVCenter; text: String(control.details.payloadFormat || qsTr("-")); color: String(control.details.payloadFormat || "").length > 0 ? control.ui.textStrong : control.ui.textMuted; font.pixelSize: 11 }
                    Rectangle { Layout.columnSpan: 2; Layout.fillWidth: true; Layout.preferredHeight: 1; color: control.ui.themePalette.separator }
                    Label { Layout.preferredHeight: 28; verticalAlignment: Text.AlignVCenter; text: qsTr("Size"); color: control.ui.textMuted; font.pixelSize: 11 }
                    Label { Layout.preferredHeight: 28; verticalAlignment: Text.AlignVCenter; text: control.details.payloadSize === undefined || control.details.payloadSize === null ? qsTr("-") : qsTr("%1 B").arg(Number(control.details.payloadSize)); color: control.details.payloadSize === undefined || control.details.payloadSize === null ? control.ui.textMuted : control.ui.textStrong; font.pixelSize: 11 }
                    Rectangle { Layout.columnSpan: 2; Layout.fillWidth: true; Layout.preferredHeight: 1; color: control.ui.themePalette.separator }
                    Label { Layout.preferredHeight: 28; verticalAlignment: Text.AlignVCenter; text: qsTr("Retain"); color: control.ui.textMuted; font.pixelSize: 11 }
                    Label { Layout.preferredHeight: 28; verticalAlignment: Text.AlignVCenter; text: control.details.retainKnown ? (control.details.retain ? qsTr("Yes") : qsTr("No")) : qsTr("-"); color: control.details.retainKnown ? control.ui.textStrong : control.ui.textMuted; font.pixelSize: 11 }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            Layout.fillWidth: true
                            text: control.details.fullPayloadAvailable === false
                                  ? qsTr("Payload preview")
                                  : qsTr("Payload")
                            color: control.ui.textMuted
                            font.pixelSize: 11
                        }

                        AppComboBox {
                            ui: control.ui
                            Layout.preferredWidth: 116
                            Layout.preferredHeight: 28
                            model: control.viewModel.payloadFormats
                            currentIndex: control.payloadViewFormat
                            font.pixelSize: 11
                            Accessible.name: qsTr("Payload display format")
                            onActivated: index => control.payloadViewFormat = index
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(
                                                    control.payloadTextMaximumHeight,
                                                    Math.max(40, payloadBodyText.contentHeight + 20))
                        radius: control.ui.radiusSm
                        color: control.ui.themePalette.innerPanelBg
                        border.color: control.ui.themePalette.fieldBorder
                        border.width: 1

                        ScrollView {
                            id: payloadScroll

                            anchors.fill: parent
                            anchors.margins: 1
                            contentWidth: availableWidth
                            clip: true
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                            ScrollBar.vertical.policy: ScrollBar.AsNeeded

                            TextEdit {
                                id: payloadBodyText

                                width: payloadScroll.availableWidth
                                leftPadding: 10
                                rightPadding: 10
                                topPadding: 10
                                bottomPadding: 10
                                text: control.displayedPayload
                                color: control.ui.textStrong
                                font.pixelSize: 11
                                textFormat: TextEdit.PlainText
                                readOnly: true
                                selectByMouse: true
                                wrapMode: TextEdit.WrapAnywhere

                                ContextMenu.menu: AppNativeTextMenu {
                                    editor: payloadBodyText
                                }
                            }
                        }
                    }

                    Label {
                        visible: control.details.fullPayloadAvailable === false
                        Layout.fillWidth: true
                        text: qsTr("The full payload was not stored. Hash: %1")
                                  .arg(String(control.details.payloadHash || qsTr("Unavailable")))
                        color: control.ui.themePalette.warningText
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                    }
                }

                AppInlineAlert {
                    visible: String(control.details.parseError || "").length > 0
                    ui: control.ui
                    type: "error"
                    text: qsTr("Parser error: %1").arg(String(control.details.parseError || ""))
                    Layout.fillWidth: true
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                }

                ColumnLayout {
                    visible: String(control.details.parsedPayload || "").length > 0
                    Layout.fillWidth: true
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                    spacing: 6

                    Label {
                        text: qsTr("Parsed result")
                        color: control.ui.textMuted
                        font.pixelSize: 11
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(
                                                    control.parsedTextMaximumHeight,
                                                    Math.max(40, parsedResultText.contentHeight + 20))
                        radius: control.ui.radiusSm
                        color: control.ui.themePalette.innerPanelBg
                        border.color: control.ui.themePalette.fieldBorder
                        border.width: 1

                        ScrollView {
                            id: parsedResultScroll

                            anchors.fill: parent
                            anchors.margins: 1
                            contentWidth: availableWidth
                            clip: true
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                            ScrollBar.vertical.policy: ScrollBar.AsNeeded

                            TextEdit {
                                id: parsedResultText

                                width: parsedResultScroll.availableWidth
                                leftPadding: 10
                                rightPadding: 10
                                topPadding: 10
                                bottomPadding: 10
                                text: String(control.details.parsedPayload || "")
                                color: control.ui.textStrong
                                font.pixelSize: 11
                                textFormat: TextEdit.PlainText
                                readOnly: true
                                selectByMouse: true
                                wrapMode: TextEdit.WrapAnywhere

                                ContextMenu.menu: AppNativeTextMenu {
                                    editor: parsedResultText
                                }
                            }
                        }
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                    spacing: 7

                    InspectorActionButton {
                        ui: control.ui
                        visible: String(control.details.parsedPayload || "").length > 0
                        text: qsTr("Copy parsed result")
                        onClicked: control.viewModel.copyMessagePayload("0", String(control.details.parsedPayload), "", Number(control.details.testFormat || 0))
                    }

                    InspectorActionButton {
                        ui: control.ui
                        text: qsTr("Copy Payload")
                        onClicked: control.viewModel.copyMessagePayload("0", control.displayedPayload, "", 0)
                    }

                    InspectorActionButton {
                        ui: control.ui
                        text: qsTr("Copy Topic")
                        onClicked: control.viewModel.copyMessageTopic(String(control.details.topic || ""))
                    }

                    InspectorActionButton {
                        ui: control.ui
                        text: qsTr("Use as draft")
                        onClicked: {
                            control.viewModel.useMessageAsDraft(
                                control.historyId,
                                String(control.details.topic || ""),
                                String(control.details.fullPayload || ""),
                                String(control.details.testPayload || ""),
                                Number(control.details.testFormat || 0));
                            control.draftUsed();
                        }
                    }
                }

                Item { Layout.preferredHeight: 8 }
            }
        }
    }
}
