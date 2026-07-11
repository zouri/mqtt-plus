pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Material

QtObject {
    id: root

    required property bool isDarkTheme

    readonly property int panelRadius: 0
    readonly property int innerRadius: 10
    readonly property int compactControlHeight: 32
    readonly property int compactCheckHeight: 28
    readonly property int compactFontSize: 12

    readonly property int materialTheme: root.isDarkTheme ? Material.Dark : Material.Light
    readonly property int materialAccent: Material.Blue
    readonly property int materialPrimary: Material.Blue

    readonly property var themePalette: ({
            "windowBg": root.isDarkTheme ? "#0f1419" : "#ffffff",
            "navigationBg": root.isDarkTheme ? "#1a212b" : "#f2f3f5",
            "headerBg": root.isDarkTheme ? "#1a212b" : "#ffffff",
            "headerBorder": root.isDarkTheme ? "#2c3644" : "#dee0e3",
            "sidebarBg": root.isDarkTheme ? "#1a212b" : "#f7f8fa",
            "sidebarBorder": root.isDarkTheme ? "#2c3644" : "#dee0e3",
            "panelBg": root.isDarkTheme ? "#0f1419" : "#ffffff",
            "panelBorder": root.isDarkTheme ? "#2c3644" : "#dee0e3",
            "rowHover": root.isDarkTheme ? "#222b38" : "#f2f3f5",
            "textStrong": root.isDarkTheme ? "#e6edf5" : "#1f2329",
            "textMuted": root.isDarkTheme ? "#9fb0c3" : "#646a73",
            "textSubtle": root.isDarkTheme ? "#6b7889" : "#6f757e",
            "selectedBg": root.isDarkTheme ? "#1c3156" : "#e8f0ff",
            "selectedBorder": root.isDarkTheme ? "#4f8bff" : "#3370ff",
            "selectedItemBg": root.isDarkTheme ? "#1a212b" : "#e8f0ff",
            "selectedItemBorder": root.isDarkTheme ? "#37424f" : "#e8f0ff",
            "itemBg": root.isDarkTheme ? "#1a212b" : "#ffffff",
            "itemBorder": root.isDarkTheme ? "#2c3644" : "#dee0e3",
            "innerPanelBg": root.isDarkTheme ? "#222b38" : "#f5f6f7",
            "innerPanelBorder": root.isDarkTheme ? "#37424f" : "#dee0e3",
            "separator": root.isDarkTheme ? "#2c3644" : "#dee0e3",
            "infoText": root.isDarkTheme ? "#4f8bff" : "#245bdb",
            "warningText": root.isDarkTheme ? "#fbbf24" : "#b85b00",
            "errorText": root.isDarkTheme ? "#ff9aa5" : "#d83931",
            "successText": root.isDarkTheme ? "#34d399" : "#2b881f",
            "successBg": root.isDarkTheme ? "#0c3326" : "#e8f7e6",
            "warningBg": root.isDarkTheme ? "#3a2e10" : "#fff3dc",
            "actionPressedBg": root.isDarkTheme ? "#2c3644" : "#e5e6e8",
            "actionHoverBg": root.isDarkTheme ? "#222b38" : "#f2f3f5",
            "eventBorder": root.isDarkTheme ? "#37424f" : "#dee0e3",
            "eventTitle": root.isDarkTheme ? "#fbbf24" : "#b85b00",
            "messageTitle": root.isDarkTheme ? "#4f8bff" : "#245bdb",
            "timestampText": root.isDarkTheme ? "#6b7889" : "#6f757e",
            "chipBg": root.isDarkTheme ? "#1f2733" : "#f2f3f5",
            "chipText": root.isDarkTheme ? "#9fb0c3" : "#646a73",
            "followBg": root.isDarkTheme ? "#4f8bff" : "#3370ff",
            "followBorder": root.isDarkTheme ? "#4f8bff" : "#3370ff",
            "followText": "#ffffff",
            "followBadgeText": "#ffffff",
            "dialogBg": root.isDarkTheme ? "#1a212b" : "#ffffff",
            "dialogBorder": root.isDarkTheme ? "#2c3644" : "#dee0e3",
            "accentPanelBg": root.isDarkTheme ? "#1c3156" : "#e8f0ff",
            "accentPanelBorder": root.isDarkTheme ? "#4f8bff" : "#3370ff",
            "dividerLine": root.isDarkTheme ? "#2c3644" : "#dee0e3",
            "dividerLabelBg": root.isDarkTheme ? "#0f1419" : "#ffffff",
            "buttonBg": root.isDarkTheme ? "#1a212b" : "#ffffff",
            "buttonBorder": root.isDarkTheme ? "#37424f" : "#dee0e3",
            "buttonHoverBg": root.isDarkTheme ? "#222b38" : "#f5f6f7",
            "buttonPressedBg": root.isDarkTheme ? "#2c3644" : "#e5e6e8",
            "buttonPrimaryBg": root.isDarkTheme ? "#4f8bff" : "#3370ff",
            "buttonPrimaryHoverBg": root.isDarkTheme ? "#6fa1ff" : "#4e83fd",
            "buttonPrimaryPressedBg": root.isDarkTheme ? "#3675df" : "#245bdb",
            "buttonPrimaryText": "#ffffff",
            "buttonDangerBg": root.isDarkTheme ? "#df5669" : "#c92f46",
            "buttonDangerHoverBg": root.isDarkTheme ? "#ee6879" : "#da445b",
            "buttonDangerPressedBg": root.isDarkTheme ? "#bd3d50" : "#a82639",
            "buttonDangerText": "#fff7f8",
            "fieldBg": root.isDarkTheme ? "#222b38" : "#ffffff",
            "fieldBorder": root.isDarkTheme ? "#37424f" : "#c9cdd4",
            "fieldFocusBorder": root.isDarkTheme ? "#4f8bff" : "#3370ff",
            "fieldPlaceholder": root.isDarkTheme ? "#6b7889" : "#6f757e",
            "dialogOverlay": root.isDarkTheme ? "#730f1419" : "#661f2329"
        })
    readonly property color panelBg: root.themePalette.panelBg
    readonly property color panelBorder: root.themePalette.panelBorder
    readonly property color rowHover: root.themePalette.rowHover
    readonly property color textStrong: root.themePalette.textStrong
    readonly property color textMuted: root.themePalette.textMuted

    readonly property var stateColors: ({
            "connected": root.isDarkTheme ? "#34d399" : "#2b881f",
            "subscribed": root.isDarkTheme ? "#34d399" : "#2b881f",
            "acknowledged": root.isDarkTheme ? "#34d399" : "#2b881f",
            "completed": root.isDarkTheme ? "#34d399" : "#2b881f",
            "connecting": root.isDarkTheme ? "#fbbf24" : "#b85b00",
            "pending": root.isDarkTheme ? "#fbbf24" : "#b85b00",
            "queued": root.isDarkTheme ? "#fbbf24" : "#b85b00",
            "sent": root.isDarkTheme ? "#fbbf24" : "#b85b00",
            "published": root.isDarkTheme ? "#fbbf24" : "#b85b00",
            "disconnecting": root.isDarkTheme ? "#9aa4b2" : "#8f959e",
            "paused": root.isDarkTheme ? "#9aa4b2" : "#8f959e",
            "saved": root.isDarkTheme ? "#9aa4b2" : "#8f959e",
            "unsubscribed": root.isDarkTheme ? "#9aa4b2" : "#8f959e",
            "error": root.isDarkTheme ? "#f87171" : "#d83931",
            "failed": root.isDarkTheme ? "#f87171" : "#d83931"
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
        return root.stateColors[state] || (root.isDarkTheme ? "#7f90a8" : "#6f757e");
    }

    function materialIcon(name) {
        return Qt.resolvedUrl(`../resources/${name}.svg`);
    }

    function themeModeMeta(mode) {
        return root.themeModeMetaByMode[mode] || root.themeModeMetaByMode.system;
    }

    function statusLabel(state) {
        switch (state) {
        case "connected":
            return qsTr("Connected");
        case "connecting":
            return qsTr("Connecting");
        case "disconnecting":
            return qsTr("Disconnecting");
        case "disconnected":
            return qsTr("Disconnected");
        case "subscribed":
            return qsTr("Subscribed");
        case "pending":
            return qsTr("Pending");
        case "queued":
            return qsTr("Queued");
        case "sent":
            return qsTr("Sent");
        case "published":
            return qsTr("Published");
        case "acknowledged":
            return qsTr("Acknowledged");
        case "completed":
            return qsTr("Completed");
        case "paused":
            return qsTr("Paused");
        case "saved":
            return qsTr("Saved");
        case "unsubscribed":
            return qsTr("Unsubscribed");
        case "error":
            return qsTr("Error");
        case "failed":
            return qsTr("Failed");
        case "idle":
            return qsTr("Idle");
        default:
            return state || qsTr("Idle");
        }
    }
}
