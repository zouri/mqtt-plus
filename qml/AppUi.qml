pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Material

QtObject {
    id: root

    required property bool isDarkTheme

    readonly property int panelRadius: 0
    readonly property int innerRadius: 12
    readonly property int compactControlHeight: 30
    readonly property int compactCheckHeight: 28
    readonly property int compactFontSize: 12

    readonly property int materialTheme: root.isDarkTheme ? Material.Dark : Material.Light
    readonly property int materialAccent: Material.Blue
    readonly property int materialPrimary: Material.Blue

    readonly property var themePalette: ({
        "windowBg": root.isDarkTheme ? "#101114" : "#fdfdff",
        "headerBg": root.isDarkTheme ? "#1f2024" : "#fbfbfd",
        "headerBorder": root.isDarkTheme ? "#34363c" : "#e8e8ed",
        "sidebarBg": root.isDarkTheme ? "#1f2024" : "#fbfbfd",
        "sidebarBorder": root.isDarkTheme ? "#34363c" : "#e8e8ed",
        "panelBg": root.isDarkTheme ? "#18191d" : "#f5f5f7",
        "panelBorder": root.isDarkTheme ? "#34363c" : "#e8e8ed",
        "rowHover": root.isDarkTheme ? "#25262b" : "#fbfbfd",
        "textStrong": root.isDarkTheme ? "#f5f5f7" : "#1d1d1f",
        "textMuted": root.isDarkTheme ? "#a1a1a6" : "#6e6e73",
        "selectedBg": root.isDarkTheme ? "#122b47" : "#eef6ff",
        "selectedBorder": root.isDarkTheme ? "#2997ff" : "#0071e3",
        "itemBg": root.isDarkTheme ? "#15161a" : "#ffffff",
        "itemBorder": root.isDarkTheme ? "#2d2f35" : "#e1e1e6",
        "innerPanelBg": root.isDarkTheme ? "#222328" : "#fbfbfd",
        "innerPanelBorder": root.isDarkTheme ? "#34363c" : "#d2d2d7",
        "separator": root.isDarkTheme ? "#34363c" : "#e8e8ed",
        "infoText": root.isDarkTheme ? "#8cc8ff" : "#0066cc",
        "warningText": root.isDarkTheme ? "#ffd66b" : "#946200",
        "errorText": root.isDarkTheme ? "#ff9aa5" : "#dc2626",
        "actionPressedBg": root.isDarkTheme ? "#34363c" : "#ececf0",
        "actionHoverBg": root.isDarkTheme ? "#2a2b30" : "#f5f5f7",
        "eventBorder": root.isDarkTheme ? "#3f6f9d" : "#b7d4f5",
        "eventTitle": root.isDarkTheme ? "#ffd66b" : "#8f5a05",
        "messageTitle": root.isDarkTheme ? "#8cc8ff" : "#0066cc",
        "timestampText": root.isDarkTheme ? "#8e8e93" : "#86868b",
        "chipBg": root.isDarkTheme ? "#2a2b30" : "#f5f5f7",
        "chipText": root.isDarkTheme ? "#d1d1d6" : "#424245",
        "followBg": root.isDarkTheme ? "#172a43" : "#edf6ff",
        "followBorder": root.isDarkTheme ? "#2e6fb4" : "#b7d4f5",
        "followText": root.isDarkTheme ? "#dceeff" : "#0066cc",
        "followBadgeText": root.isDarkTheme ? "#b8dcff" : "#424245",
        "dialogBg": root.isDarkTheme ? "#1c1c1e" : "#ffffff",
        "dialogBorder": root.isDarkTheme ? "#34363c" : "#d2d2d7",
        "accentPanelBg": root.isDarkTheme ? "#102943" : "#edf6ff",
        "accentPanelBorder": root.isDarkTheme ? "#2e6fb4" : "#b7d4f5",
        "dividerLine": root.isDarkTheme ? "#34363c" : "#e8e8ed",
        "dividerLabelBg": root.isDarkTheme ? "#121214" : "#ffffff",
        "buttonBg": root.isDarkTheme ? "#2a2b30" : "#f5f5f7",
        "buttonBorder": root.isDarkTheme ? "#42444a" : "#d2d2d7",
        "buttonHoverBg": root.isDarkTheme ? "#34363c" : "#fbfbfd",
        "buttonPressedBg": root.isDarkTheme ? "#3f4148" : "#ececf0",
        "buttonPrimaryBg": root.isDarkTheme ? "#0a84ff" : "#0071e3",
        "buttonPrimaryHoverBg": root.isDarkTheme ? "#2997ff" : "#0077ed",
        "buttonPrimaryPressedBg": root.isDarkTheme ? "#006edb" : "#0066cc",
        "buttonPrimaryText": "#ffffff",
        "buttonDangerBg": root.isDarkTheme ? "#df5669" : "#c92f46",
        "buttonDangerHoverBg": root.isDarkTheme ? "#ee6879" : "#da445b",
        "buttonDangerPressedBg": root.isDarkTheme ? "#bd3d50" : "#a82639",
        "buttonDangerText": "#fff7f8",
        "fieldBg": root.isDarkTheme ? "#15161a" : "#ffffff",
        "fieldBorder": root.isDarkTheme ? "#42444a" : "#d2d2d7",
        "fieldFocusBorder": root.isDarkTheme ? "#2997ff" : "#0071e3",
        "fieldPlaceholder": root.isDarkTheme ? "#77777d" : "#86868b",
        "dialogOverlay": root.isDarkTheme ? "#9f081019" : "#730b1420"
    })
    readonly property color panelBg: root.themePalette.panelBg
    readonly property color panelBorder: root.themePalette.panelBorder
    readonly property color rowHover: root.themePalette.rowHover
    readonly property color textStrong: root.themePalette.textStrong
    readonly property color textMuted: root.themePalette.textMuted

    readonly property var stateColors: ({
        "connected": "#49d17d",
        "subscribed": "#49d17d",
        "acknowledged": "#49d17d",
        "completed": "#49d17d",
        "connecting": "#f0bb63",
        "pending": "#f0bb63",
        "queued": "#f0bb63",
        "sent": "#f0bb63",
        "published": "#f0bb63",
        "disconnecting": "#7f90a8",
        "paused": "#7f90a8",
        "saved": "#7f90a8",
        "unsubscribed": "#7f90a8",
        "error": "#ff8d94",
        "failed": "#ff8d94"
    })

    readonly property var themeModeMetaByMode: ({
        "system": {
            "label": qsTr("System"),
            "next": "light"
        },
        "light": {
            "label": qsTr("Light"),
            "next": "dark"
        },
        "dark": {
            "label": qsTr("Dark"),
            "next": "system"
        }
    })

    function stateColor(state) {
        return root.stateColors[state] || "#7f90a8"
    }

    function materialIcon(name) {
        return Qt.resolvedUrl(`icons/${name}.svg`)
    }

    function themeModeMeta(mode) {
        return root.themeModeMetaByMode[mode] || root.themeModeMetaByMode.system
    }

    function statusLabel(state) {
        switch (state) {
        case "connected":
            return qsTr("Connected")
        case "connecting":
            return qsTr("Connecting")
        case "disconnecting":
            return qsTr("Disconnecting")
        case "disconnected":
            return qsTr("Disconnected")
        case "subscribed":
            return qsTr("Subscribed")
        case "pending":
            return qsTr("Pending")
        case "queued":
            return qsTr("Queued")
        case "sent":
            return qsTr("Sent")
        case "published":
            return qsTr("Published")
        case "acknowledged":
            return qsTr("Acknowledged")
        case "completed":
            return qsTr("Completed")
        case "paused":
            return qsTr("Paused")
        case "saved":
            return qsTr("Saved")
        case "unsubscribed":
            return qsTr("Unsubscribed")
        case "error":
            return qsTr("Error")
        case "failed":
            return qsTr("Failed")
        case "idle":
            return qsTr("Idle")
        default:
            return state || qsTr("Idle")
        }
    }
}
