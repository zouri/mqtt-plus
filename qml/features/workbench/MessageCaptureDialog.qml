pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

AppDialog {
    id: root

    required property var viewModel

    width: 440
    height: captureDialogContent.implicitHeight + 40
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 0

    function filtersText(filters) {
        return filters && filters.length > 0 ? filters.join(", ") : "";
    }

    function syncControls() {
        const policy = root.viewModel.messageCapturePolicy || {};
        captureIncomingCheck.checked = policy.captureIncoming !== false;
        captureOutgoingCheck.checked = policy.captureOutgoing !== false;
        captureIncludeField.text = root.filtersText(policy.includeTopicFilters);
        captureExcludeField.text = root.filtersText(policy.excludeTopicFilters);
    }

    function openForCurrentSession() {
        root.syncControls();
        root.open();
    }

    function applyPolicy() {
        if (root.viewModel.setCurrentMessageCapturePolicy(
                captureIncomingCheck.checked,
                captureOutgoingCheck.checked,
                captureIncludeField.text,
                captureExcludeField.text)) {
            root.close();
            return;
        }
        root.syncControls();
    }

    background: Rectangle {
        radius: 10
        color: root.ui.themePalette.dialogBg
        border.color: root.ui.themePalette.dialogBorder
    }

    contentItem: ColumnLayout {
        id: captureDialogContent

        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        Label {
            Layout.fillWidth: true
            text: qsTr("Capture settings")
            color: root.ui.textStrong
            font.pixelSize: 16
            font.bold: true
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Choose which messages are saved and parsed for this connection.")
            color: root.ui.textMuted
            font.pixelSize: 11
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            AppCheckBox {
                id: captureIncomingCheck

                ui: root.ui
                Layout.fillWidth: true
                text: qsTr("Received messages")
                checked: true
                Accessible.name: text
            }

            AppCheckBox {
                id: captureOutgoingCheck

                ui: root.ui
                Layout.fillWidth: true
                text: qsTr("Sent messages")
                checked: true
                Accessible.name: text
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Label {
                Layout.fillWidth: true
                text: qsTr("Include topics")
                color: root.ui.textStrong
                font.pixelSize: 11
                font.bold: true
            }

            AppTextField {
                id: captureIncludeField

                ui: root.ui
                Layout.fillWidth: true
                placeholderText: qsTr("All topics")
                Accessible.name: qsTr("Capture Topic include filters")
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Label {
                Layout.fillWidth: true
                text: qsTr("Exclude topics")
                color: root.ui.textStrong
                font.pixelSize: 11
                font.bold: true
            }

            AppTextField {
                id: captureExcludeField

                ui: root.ui
                Layout.fillWidth: true
                placeholderText: qsTr("For example sensors/private/#")
                Accessible.name: qsTr("Capture Topic exclude filters")
            }
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Separate MQTT filters with commas. Exclude filters take priority.")
            color: root.ui.textMuted
            font.pixelSize: 10
            wrapMode: Text.Wrap
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: root.ui.themePalette.separator
            Accessible.ignored: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Item {
                Layout.fillWidth: true
            }

            AppButton {
                ui: root.ui
                text: qsTr("Cancel")
                minimumWidth: 78
                onClicked: root.close()
            }

            AppButton {
                ui: root.ui
                text: qsTr("Apply")
                primary: true
                minimumWidth: 78
                onClicked: root.applyPolicy()
            }
        }
    }
}
