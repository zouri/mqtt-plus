pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

Popup {
    id: control

    required property AppUi ui
    required property var filterModel
    required property var subscriptionsModel

    width: 280
    implicitHeight: 292
    padding: 10
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

    function topicChecked(topic) {
        return control.filterModel.selectedTopics.indexOf(topic) >= 0;
    }

    function setTopicChecked(topic, checked) {
        const selected = control.filterModel.selectedTopics.slice();
        const index = selected.indexOf(topic);
        if (checked && index < 0) {
            selected.push(topic);
        } else if (!checked && index >= 0) {
            selected.splice(index, 1);
        }
        control.filterModel.selectedTopics = selected;
    }

    background: Rectangle {
        radius: 8
        color: control.ui.themePalette.dialogBg
        border.color: control.ui.themePalette.dialogBorder
    }

    contentItem: ColumnLayout {
        spacing: 8

        RowLayout {
            Layout.fillWidth: true

            Label {
                Layout.fillWidth: true
                text: qsTr("Topic")
                color: control.ui.textStrong
                font.pixelSize: 11
                font.bold: true
            }

            Button {
                id: clearTopicsButton
                text: qsTr("Clear all")
                flat: true
                enabled: control.filterModel.selectedTopics.length > 0
                Accessible.name: text

                contentItem: Label {
                    text: clearTopicsButton.text
                    color: clearTopicsButton.enabled ? control.ui.themePalette.infoText : control.ui.textMuted
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Item {}
                onClicked: control.filterModel.selectedTopics = []
            }
        }

        ListView {
            id: topicOptions
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(148, contentHeight)
            Layout.minimumHeight: 36
            model: control.subscriptionsModel
            clip: true
            spacing: 2
            reuseItems: true

            delegate: CheckBox {
                id: topicOption
                required property string topic
                required property string displayName
                width: ListView.view.width
                height: 32
                text: displayName
                checked: control.topicChecked(topic)
                Accessible.name: displayName
                onToggled: control.setTopicChecked(topic, checked)
            }

            Label {
                anchors.centerIn: parent
                visible: topicOptions.count === 0
                text: qsTr("No topics")
                color: control.ui.textMuted
                font.pixelSize: 11
            }
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Direction")
            color: control.ui.textStrong
            font.pixelSize: 11
            font.bold: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 3

            Repeater {
                model: [
                    { value: "all", label: qsTr("All") },
                    { value: "incoming", label: qsTr("Received") },
                    { value: "outgoing", label: qsTr("Sent") }
                ]

                delegate: Button {
                    id: directionButton
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    text: modelData.label
                    checkable: true
                    checked: control.filterModel.direction === modelData.value
                    Accessible.name: text

                    contentItem: Label {
                        text: directionButton.text
                        color: directionButton.checked ? control.ui.textStrong : control.ui.textMuted
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        radius: 6
                        color: directionButton.checked
                               ? control.ui.themePalette.itemBg
                               : control.ui.themePalette.innerPanelBg
                        border.color: directionButton.checked
                                      ? control.ui.themePalette.panelBorder
                                      : "transparent"
                    }

                    onClicked: control.filterModel.direction = modelData.value
                }
            }
        }

        Label {
            visible: control.filterModel.filterActive
            Layout.fillWidth: true
            text: qsTr("Showing filtered messages")
            color: control.ui.textMuted
            font.pixelSize: 11
        }
    }
}
