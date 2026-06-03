pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../components"

Rectangle {
    id: root

    required property var appController
    required property AppUi ui

    property bool testSuccess: false
    property string testState: ""
    property string testOutput: ""
    property string scriptCode: ""

    SplitView.preferredWidth: 370
    SplitView.minimumWidth: 310
    SplitView.maximumWidth: 480
    radius: root.ui.innerRadius
    color: root.ui.themePalette.innerPanelBg
    border.color: root.ui.themePalette.innerPanelBorder

    function resetResult() {
        root.testState = ""
        root.testOutput = ""
        root.testSuccess = false
    }

    function runTest() {
        const result = root.appController.testScript(
                    root.scriptCode,
                    testTopicField.text,
                    testPayloadField.text,
                    testFormatField.currentIndex)
        root.testSuccess = result.success
        root.testState = result.success ? qsTr("OK") : qsTr("Error")
        root.testOutput = result.success ? result.output : result.error
        if (result.inputError && result.inputError.length > 0) {
            root.testOutput = `${root.testOutput}\n${result.inputError}`
        }
    }

    function applySample(sample) {
        testTopicField.text = sample.topic || qsTr("test/topic")
        testPayloadField.text = sample.payload || ""
        testFormatField.currentIndex = Math.max(
                    0,
                    Math.min(testFormatField.count - 1, Number(sample.format || 0)))
        root.resetResult()
        root.runTest()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        Label {
            Layout.fillWidth: true
            text: qsTr("Test")
            color: root.ui.textStrong
            font.pixelSize: 14
            font.bold: true
        }

        AppTextField {
            id: testTopicField
            ui: root.ui
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            placeholderText: qsTr("test/topic")
            text: "test/topic"
        }

        AppComboBox {
            id: testFormatField
            ui: root.ui
            Layout.fillWidth: true
            model: root.appController.payloadFormats
        }

        AppTextArea {
            id: testPayloadField
            ui: root.ui
            Layout.fillWidth: true
            Layout.preferredHeight: 120
            Layout.minimumHeight: 90
            Layout.minimumWidth: 0
            font.family: "Menlo"
            clip: true
            text: "{\"value\": 42}"
            placeholderText: qsTr("Test payload")
        }

        AppButton {
            ui: root.ui
            text: qsTr("Run Test")
            primary: true
            minimumWidth: 96
            onClicked: root.runTest()
        }

        AppTextArea {
            id: testResultField
            ui: root.ui
            Layout.fillWidth: true
            Layout.preferredHeight: 126
            Layout.minimumHeight: 90
            Layout.minimumWidth: 0
            readOnly: true
            font.family: "Menlo"
            color: root.testState.length === 0
                   ? root.ui.textMuted
                   : (root.testSuccess
                      ? root.ui.stateColor("subscribed")
                      : root.ui.themePalette.errorText)
            text: root.testState.length > 0
                  ? `${root.testState}: ${root.testOutput}`
                  : qsTr("Run a script test to see output.")
            wrapMode: TextEdit.WrapAnywhere
        }

        AppDivider {
            ui: root.ui
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: qsTr("Recent Message Samples")
                color: root.ui.textStrong
                font.pixelSize: 13
                font.bold: true
                elide: Label.ElideRight
            }

            AppBadge {
                ui: root.ui
                label: `${sampleList.count}`
                horizontalPadding: 7
                verticalPadding: 4
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Label {
                anchors.centerIn: parent
                width: parent.width - 24
                visible: sampleList.count === 0
                text: qsTr("No current session messages yet.")
                color: root.ui.textMuted
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                font.pixelSize: 12
            }

            ListView {
                id: sampleList
                anchors.fill: parent
                clip: true
                spacing: 7
                model: root.appController.scriptTestSamplesModel
                reuseItems: true

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                delegate: Rectangle {
                    id: sampleDelegate
                    required property var modelData
                    width: ListView.view.width
                    implicitHeight: sampleColumn.implicitHeight + 18
                    radius: root.ui.innerRadius
                    color: sampleMouse.containsMouse || activeFocus
                           ? root.ui.rowHover
                           : root.ui.themePalette.itemBg
                    border.color: sampleMouse.containsMouse || activeFocus
                                  ? root.ui.themePalette.selectedBorder
                                  : root.ui.themePalette.itemBorder
                    activeFocusOnTab: true
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Use sample from %1").arg(sampleDelegate.modelData.topic)

                    Column {
                        id: sampleColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: 10
                        spacing: 5

                        RowLayout {
                            width: parent.width
                            spacing: 8

                            Label {
                                Layout.fillWidth: true
                                text: sampleDelegate.modelData.topic
                                color: root.ui.textStrong
                                font.pixelSize: 12
                                font.bold: true
                                elide: Label.ElideRight
                            }

                            AppBadge {
                                ui: root.ui
                                label: sampleDelegate.modelData.formatName
                                badgeRadius: 7
                                horizontalPadding: 6
                                verticalPadding: 3
                            }
                        }

                        Label {
                            width: parent.width
                            text: qsTr("%1 • %2 B")
                                  .arg(sampleDelegate.modelData.timestamp)
                                  .arg(sampleDelegate.modelData.payloadSize)
                            color: root.ui.textMuted
                            font.pixelSize: 10
                            elide: Label.ElideRight
                        }
                    }

                    Keys.onPressed: (event) => {
                        if (event.key === Qt.Key_Return
                                || event.key === Qt.Key_Enter
                                || event.key === Qt.Key_Space) {
                            root.applySample(sampleDelegate.modelData)
                            event.accepted = true
                        }
                    }

                    MouseArea {
                        id: sampleMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor

                        onClicked: {
                            sampleDelegate.forceActiveFocus()
                            root.applySample(sampleDelegate.modelData)
                        }
                    }
                }
            }
        }
    }
}
