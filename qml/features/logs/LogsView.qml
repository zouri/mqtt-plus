pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

AppPanel {
    id: root

    required property var viewModel
    property bool loadingOlderLogs: false
    property bool reachedLogStart: false
    property bool shouldFollowOutput: true

    showTopBorder: false
    showRightBorder: false
    showBottomBorder: false
    showLeftBorder: false
    color: root.ui.themePalette.windowBg
    Layout.fillWidth: true
    Layout.fillHeight: true

    function isNearBottom() {
        const maxContentY = Math.max(0, logTextArea.contentHeight - logTextArea.viewportHeight)
        return maxContentY <= 0 || Math.max(0, maxContentY - logTextArea.contentY) <= 24
    }

    function refreshFollowState() {
        root.shouldFollowOutput = root.isNearBottom()
    }

    function rebuildLogText(scrollToEnd) {
        if (scrollToEnd) {
            logTextArea.scrollToBottom()
        }
    }

    function resetStreamPosition() {
        root.loadingOlderLogs = false
        root.reachedLogStart = false
        root.shouldFollowOutput = true
        root.rebuildLogText(true)
    }

    function noteStreamRowAppended(row) {
        root.rebuildLogText(root.shouldFollowOutput)
    }

    function loadOlderLogs() {
        if (root.loadingOlderLogs || root.reachedLogStart
                || logTextArea.contentHeight <= logTextArea.viewportHeight) {
            return
        }

        root.loadingOlderLogs = true
        const previousContentHeight = logTextArea.contentHeight
        const previousContentY = logTextArea.contentY
        const insertedRows = root.viewModel.loadOlderCurrentSessionLogs()
        if (insertedRows === 0) {
            root.reachedLogStart = true
            root.loadingOlderLogs = false
            return
        }

        Qt.callLater(function() {
            logTextArea.setContentY(previousContentY + logTextArea.contentHeight - previousContentHeight)
            root.loadingOlderLogs = false
        })
    }

    Component.onCompleted: root.resetStreamPosition()

    Connections {
        target: root.viewModel

        function onLogStreamChanged() {
            root.resetStreamPosition()
        }

        function onLogStreamRowAppended(row) {
            root.noteStreamRowAppended(row)
        }
    }

    Connections {
        target: root.viewModel.logs

        function onCountChanged() {
            if (!root.loadingOlderLogs) {
                root.rebuildLogText(root.shouldFollowOutput)
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: root.ui.themePalette.headerBg

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 10

                Label {
                    text: qsTr("Logs")
                    color: root.ui.textStrong
                    font.pixelSize: 18
                    font.bold: true
                }

                AppBadge {
                    ui: root.ui
                    label: `${root.viewModel.logs.count}`
                    badgeRadius: 11
                    horizontalPadding: 8
                    verticalPadding: 4
                    badgeBg: root.ui.themePalette.selectedBg
                    badgeBorder: "transparent"
                    badgeText: root.ui.themePalette.infoText
                }

                Item {
                    Layout.fillWidth: true
                }

                AppButton {
                    ui: root.ui
                    text: qsTr("Clear Log")
                    minimumWidth: 88
                    enabled: root.viewModel.logs.count > 0
                    onClicked: root.viewModel.clearCurrentLogs()
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: root.ui.themePalette.separator
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            Layout.topMargin: 14
            Layout.bottomMargin: 14
            spacing: 0

            AppTextArea {
                id: logTextArea

                ui: root.ui
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: root.viewModel.logText
                readOnly: true
                color: root.ui.textStrong
                placeholderText: qsTr("No logs yet.")
                showLineNumbers: false
                showFocusBorder: false
                backgroundRadius: 10
                backgroundBorderWidth: 1
                backgroundColor: root.ui.themePalette.innerPanelBg
                selectByMouse: true
                wrapMode: TextEdit.WrapAnywhere
                font.family: "Menlo"
                font.pixelSize: 14

                onContentYChanged: {
                    root.refreshFollowState()
                    if (contentY <= 48) {
                        root.loadOlderLogs()
                    }
                }
            }
        }
    }
}
