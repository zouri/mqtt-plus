pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

AppPanel {
    id: root

    required property var logs
    property bool loadingOlderLogs: false
    property bool reachedLogStart: false
    property bool shouldFollowOutput: true
    property string logText: ""

    showTopBorder: false
    showRightBorder: false
    showBottomBorder: false
    showLeftBorder: false
    color: root.ui.themePalette.windowBg
    Layout.fillWidth: true
    Layout.fillHeight: true

    function logLevel(title, payload) {
        const text = `${title} ${payload}`.toUpperCase()
        if (text.indexOf("ERROR") >= 0 || text.indexOf("FAILED") >= 0
                || text.indexOf("TIMEOUT") >= 0 || text.indexOf("REJECTED") >= 0) {
            return "ERROR"
        }
        if (text.indexOf("WARN") >= 0 || text.indexOf("INVALID") >= 0) {
            return "WARN"
        }
        if (text.indexOf("DEBUG") >= 0 || text.indexOf("PACKET") >= 0) {
            return "DEBUG"
        }
        return "INFO"
    }

    function indentedPayload(payload) {
        return String(payload || "").replace(/\n/g, "\n    ")
    }

    function formattedLogRow(row) {
        if (!row) {
            return ""
        }
        if ((row.kind || "") === "divider") {
            return `--- ${row.title || ""} ---`
        }

        const title = row.title || ""
        const payload = row.payload || ""
        const level = root.logLevel(title, payload)
        const channel = title.length > 0 && title.toUpperCase() !== level ? ` [${title}]` : ""
        return `[${row.timestamp || ""}] [${level}]${channel} ${root.indentedPayload(payload)}`
    }

    function renderedLogText() {
        const model = root.logs.logs
        const rowCount = model ? model.count : 0
        let rows = []
        for (let i = 0; i < rowCount; ++i) {
            rows.push(root.formattedLogRow(model.rowAt(i)))
        }
        return rows.join("\n")
    }

    function isNearBottom() {
        const maxContentY = Math.max(0, logTextArea.contentHeight - logTextArea.viewportHeight)
        return maxContentY <= 0 || Math.max(0, maxContentY - logTextArea.contentY) <= 24
    }

    function refreshFollowState() {
        root.shouldFollowOutput = root.isNearBottom()
    }

    function rebuildLogText(scrollToEnd) {
        root.logText = root.renderedLogText()
        if (scrollToEnd) {
            logTextArea.scrollToBottom()
        }
    }

    onLogTextChanged: {
        if (logTextArea.text !== root.logText) {
            logTextArea.text = root.logText
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
        const insertedRows = root.logs.loadOlderCurrentSessionLogs()
        if (insertedRows === 0) {
            root.reachedLogStart = true
            root.loadingOlderLogs = false
            return
        }

        root.logText = root.renderedLogText()
        Qt.callLater(function() {
            logTextArea.setContentY(previousContentY + logTextArea.contentHeight - previousContentHeight)
            root.loadingOlderLogs = false
        })
    }

    Component.onCompleted: root.resetStreamPosition()

    Connections {
        target: root.logs

        function onLogsChanged() {
            root.resetStreamPosition()
        }

        function onLogsRowAppended(row) {
            root.noteStreamRowAppended(row)
        }
    }

    Connections {
        target: root.logs.logs

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
            Layout.preferredHeight: 60
            color: root.ui.themePalette.windowBg

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 32
                anchors.rightMargin: 32
                spacing: 10

                Label {
                    text: qsTr("Logs")
                    color: root.ui.textStrong
                    font.pixelSize: 22
                    font.bold: true
                }

                AppBadge {
                    ui: root.ui
                    label: `${root.logs.logs.count}`
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
                    enabled: root.logs.logs.count > 0
                    onClicked: root.logs.clearCurrentLogs()
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.leftMargin: 32
                anchors.rightMargin: 32
                height: 1
                color: root.ui.themePalette.separator
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 20
            Layout.rightMargin: 24
            Layout.topMargin: 14
            Layout.bottomMargin: 14
            spacing: 0

            AppTextArea {
                id: logTextArea

                ui: root.ui
                Layout.fillWidth: true
                Layout.fillHeight: true
                readOnly: true
                color: root.ui.textStrong
                placeholderText: qsTr("No logs yet.")
                showLineNumbers: true
                showFocusBorder: false
                backgroundRadius: 0
                backgroundBorderWidth: 0
                backgroundColor: root.ui.themePalette.windowBg
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
