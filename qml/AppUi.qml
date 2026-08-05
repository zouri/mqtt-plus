pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Material

QtObject {
    id: root

    required property bool isDarkTheme
    required property string themeColor
    required property bool animationsEnabled

    readonly property int panelRadius: 10
    readonly property int innerRadius: 10
    readonly property int compactControlHeight: 32
    readonly property int compactCheckHeight: 28
    readonly property int compactFontSize: 12

    // ---- Motion tokens ----
    readonly property int motionMicroDuration: root.animationsEnabled ? 100 : 0
    readonly property int motionPopoverEnterDuration: root.animationsEnabled ? 120 : 0
    readonly property int motionPopoverExitDuration: root.animationsEnabled ? 80 : 0
    readonly property int motionPanelDuration: root.animationsEnabled ? 180 : 0
    readonly property int motionModalEnterDuration: root.animationsEnabled ? 180 : 0
    readonly property int motionModalExitDuration: root.animationsEnabled ? 140 : 0
    readonly property int motionEnterEasing: Easing.OutCubic
    readonly property int motionExitEasing: Easing.InCubic

    // ---- Design tokens: spacing scale (4pt base) ----
    readonly property int spaceXs: 4
    readonly property int spaceSm: 8
    readonly property int spaceMd: 12
    readonly property int spaceLg: 16
    readonly property int spaceXl: 24
    readonly property int space2xl: 32

    // ---- Design tokens: radius scale ----
    readonly property int radiusSm: 6
    readonly property int radiusMd: 8
    readonly property int radiusLg: 10
    readonly property int radiusPill: 999

    // ---- Design tokens: typography scale ----
    readonly property int textXs: 10
    readonly property int textSm: 12
    readonly property int textMd: 13
    readonly property int textLg: 14
    readonly property int textXl: 16
    readonly property int text2xl: 18
    readonly property int text3xl: 22

    readonly property int materialTheme: root.isDarkTheme ? Material.Dark : Material.Light
    readonly property var themeColors: ({
            "mint": {
                "bright": "#35d0aa",
                "solid": "#238b75",
                "text": "#14725f"
            },
            "blue": {
                "bright": "#6aa3ff",
                "solid": "#356fc5",
                "text": "#245bdb"
            },
            "violet": {
                "bright": "#ad8cff",
                "solid": "#7351c7",
                "text": "#6541b5"
            },
            "amber": {
                "bright": "#f1b86a",
                "solid": "#a85f16",
                "text": "#934d0b"
            },
            "rose": {
                "bright": "#ff879d",
                "solid": "#b94762",
                "text": "#a93450"
            }
        })
    readonly property var activeThemeColor: root.themeColors[root.themeColor] || root.themeColors.mint
    readonly property color accentColor: root.isDarkTheme
                                                  ? root.activeThemeColor.bright
                                                  : root.activeThemeColor.text
    readonly property color accentSolid: root.activeThemeColor.solid
    readonly property color accentHover: Qt.lighter(root.accentSolid, 1.12)
    readonly property color accentPressed: Qt.darker(root.accentSolid, 1.14)
    readonly property color accentSoft: root.withAlpha(root.accentColor, root.isDarkTheme ? 0.19 : 0.10)
    readonly property color accentSubtle: root.withAlpha(root.accentColor, root.isDarkTheme ? 0.11 : 0.08)
    readonly property color accentMuted: root.withAlpha(root.accentColor, root.isDarkTheme ? 0.34 : 0.20)
    readonly property color materialAccent: root.accentColor
    readonly property color materialPrimary: root.accentColor

    readonly property var themePalette: ({
            "windowBg": root.isDarkTheme ? "#0b0e11" : "#ffffff",
            "navigationBg": root.isDarkTheme ? "#14191e" : "#f2f3f5",
            "headerBg": root.isDarkTheme ? "#101419" : "#ffffff",
            "headerBorder": root.isDarkTheme ? "#293139" : "#dee0e3",
            "sidebarBg": root.isDarkTheme ? "#171c21" : "#f7f8fa",
            "sidebarBorder": root.isDarkTheme ? "#303840" : "#dee0e3",
            "panelBg": root.isDarkTheme ? "#0f1317" : "#ffffff",
            "panelBorder": root.isDarkTheme ? "#293139" : "#dee0e3",
            "rowHover": root.isDarkTheme ? "#20272d" : "#f2f3f5",
            "textStrong": root.isDarkTheme ? "#f0f3f5" : "#1f2329",
            "textMuted": root.isDarkTheme ? "#a7b1ba" : "#646a73",
            "textSubtle": root.isDarkTheme ? "#7f8b95" : "#6f757e",
            "selectedBg": root.accentSoft,
            "selectedBorder": root.accentColor,
            "selectedItemBg": root.accentSubtle,
            "selectedItemBorder": root.accentMuted,
            "itemBg": root.isDarkTheme ? "#171c21" : "#ffffff",
            "itemBorder": root.isDarkTheme ? "#303840" : "#dee0e3",
            "innerPanelBg": root.isDarkTheme ? "#1b2228" : "#f5f6f7",
            "innerPanelBorder": root.isDarkTheme ? "#364049" : "#dee0e3",
            "separator": root.isDarkTheme ? "#293139" : "#dee0e3",
            "infoText": root.accentColor,
            "warningText": root.isDarkTheme ? "#f1bd72" : "#a85400",
            "errorText": root.isDarkTheme ? "#ff8793" : "#d83931",
            "errorBg": root.isDarkTheme ? "#351a20" : "#fde8e8",
            "successText": root.isDarkTheme ? "#65d6a5" : "#2b881f",
            "successBg": root.isDarkTheme ? "#153229" : "#e8f7e6",
            "warningBg": root.isDarkTheme ? "#342a18" : "#fff3dc",
            "actionPressedBg": root.isDarkTheme ? "#303940" : "#e5e6e8",
            "actionHoverBg": root.isDarkTheme ? "#242c32" : "#f2f3f5",
            "eventBorder": root.isDarkTheme ? "#364049" : "#dee0e3",
            "eventTitle": root.isDarkTheme ? "#f1bd72" : "#b85b00",
            "messageTitle": root.isDarkTheme ? "#72c7e8" : "#245bdb",
            "timestampText": root.isDarkTheme ? "#87949e" : "#6f757e",
            "chipBg": root.isDarkTheme ? "#1d2429" : "#f2f3f5",
            "chipText": root.isDarkTheme ? "#b1bbc3" : "#646a73",
            "followBg": root.accentSolid,
            "followBorder": root.accentColor,
            "followText": "#ffffff",
            "followBadgeText": "#ffffff",
            "dialogBg": root.isDarkTheme ? "#171c21" : "#ffffff",
            "dialogBorder": root.isDarkTheme ? "#364049" : "#dee0e3",
            "accentPanelBg": root.accentSoft,
            "accentPanelBorder": root.accentColor,
            "dividerLine": root.isDarkTheme ? "#303840" : "#dee0e3",
            "dividerLabelBg": root.isDarkTheme ? "#0f1317" : "#ffffff",
            "buttonBg": root.isDarkTheme ? "#1b2228" : "#ffffff",
            "buttonBorder": root.isDarkTheme ? "#364049" : "#dee0e3",
            "buttonHoverBg": root.isDarkTheme ? "#252d33" : "#f5f6f7",
            "buttonPressedBg": root.isDarkTheme ? "#303940" : "#e5e6e8",
            "buttonPrimaryBg": root.accentSolid,
            "buttonPrimaryHoverBg": root.accentHover,
            "buttonPrimaryPressedBg": root.accentPressed,
            "buttonPrimaryText": root.isDarkTheme ? "#f4fffb" : "#ffffff",
            "buttonDangerBg": root.isDarkTheme ? "#c94f61" : "#c92f46",
            "buttonDangerHoverBg": root.isDarkTheme ? "#df6373" : "#da445b",
            "buttonDangerPressedBg": root.isDarkTheme ? "#a83d4e" : "#a82639",
            "buttonDangerText": "#fff7f8",
            "disabledButtonBg": root.isDarkTheme ? "#20262b" : "#e5e6e8",
            "disabledText": root.isDarkTheme ? "#66727b" : "#8f959e",
            "fieldBg": root.isDarkTheme ? "#1b2228" : "#ffffff",
            "fieldBorder": root.isDarkTheme ? "#3a4650" : "#c9cdd4",
            "fieldPlaceholder": root.isDarkTheme ? "#6f7c86" : "#6f757e",
            "dialogOverlay": root.isDarkTheme ? "#99070a0d" : "#661f2329"
        })
    readonly property color panelBg: root.themePalette.panelBg
    readonly property color panelBorder: root.themePalette.panelBorder
    readonly property color rowHover: root.themePalette.rowHover
    readonly property color textStrong: root.themePalette.textStrong
    readonly property color textMuted: root.themePalette.textMuted

    function withAlpha(value, alpha) {
        return Qt.rgba(value.r, value.g, value.b, alpha);
    }

    readonly property var stateColors: ({
            "connected": root.isDarkTheme ? "#65d6a5" : "#2b881f",
            "subscribed": root.isDarkTheme ? "#65d6a5" : "#2b881f",
            "acknowledged": root.isDarkTheme ? "#65d6a5" : "#2b881f",
            "completed": root.isDarkTheme ? "#65d6a5" : "#2b881f",
            "connecting": root.isDarkTheme ? "#f1bd72" : "#b85b00",
            "pending": root.isDarkTheme ? "#f1bd72" : "#b85b00",
            "queued": root.isDarkTheme ? "#f1bd72" : "#b85b00",
            "sent": root.isDarkTheme ? "#f1bd72" : "#b85b00",
            "published": root.isDarkTheme ? "#f1bd72" : "#b85b00",
            "disconnecting": root.isDarkTheme ? "#87949e" : "#8f959e",
            "disconnected": root.isDarkTheme ? "#87949e" : "#8f959e",
            "idle": root.isDarkTheme ? "#87949e" : "#8f959e",
            "paused": root.isDarkTheme ? "#87949e" : "#8f959e",
            "saved": root.isDarkTheme ? "#87949e" : "#8f959e",
            "unsubscribed": root.isDarkTheme ? "#87949e" : "#8f959e",
            "error": root.isDarkTheme ? "#ff8793" : "#d83931",
            "failed": root.isDarkTheme ? "#ff8793" : "#d83931"
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
        return root.stateColors[state] || (root.isDarkTheme ? "#7f8b95" : "#6f757e");
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
        case "received":
            return qsTr("Received");
        case "released":
            return qsTr("Released");
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
