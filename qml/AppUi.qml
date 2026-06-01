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
    readonly property int compactFontSize: 13

    readonly property int materialTheme: root.isDarkTheme ? Material.Dark : Material.Light
    readonly property int materialAccent: Material.Cyan
    readonly property int materialPrimary: Material.BlueGrey

    readonly property var themePalette: ({
        "windowBg": root.isDarkTheme ? "#0c1117" : "#eef3f7",
        "headerBg": root.isDarkTheme ? "#10161f" : "#f6f9fc",
        "headerBorder": root.isDarkTheme ? "#1b2432" : "#d8e0ea",
        "sidebarBg": root.isDarkTheme ? "#0f151d" : "#e9eff5",
        "sidebarBorder": root.isDarkTheme ? "#1c2735" : "#d4dde8",
        "panelBg": root.isDarkTheme ? "#141b24" : "#fbfdff",
        "panelBorder": root.isDarkTheme ? "#223042" : "#d8e1ec",
        "rowHover": root.isDarkTheme ? "#182130" : "#e3ecf5",
        "textStrong": root.isDarkTheme ? "#edf3fd" : "#172231",
        "textMuted": root.isDarkTheme ? "#94a3bb" : "#617084",
        "selectedBg": root.isDarkTheme ? "#1a2a40" : "#ddeaf8",
        "selectedBorder": root.isDarkTheme ? "#5d88c4" : "#6b93c8",
        "itemBg": root.isDarkTheme ? "#131a24" : "#f7fafc",
        "itemBorder": root.isDarkTheme ? "#202b3b" : "#d7e0ea",
        "innerPanelBg": root.isDarkTheme ? "#101824" : "#f4f8fc",
        "innerPanelBorder": root.isDarkTheme ? "#263448" : "#d9e2ed",
        "separator": root.isDarkTheme ? "#223144" : "#d7e0eb",
        "infoText": root.isDarkTheme ? "#8bc8ff" : "#2f77c1",
        "warningText": root.isDarkTheme ? "#f3c37d" : "#a26409",
        "errorText": root.isDarkTheme ? "#ffb8c3" : "#bb3e5e",
        "actionPressedBg": root.isDarkTheme ? "#314258" : "#d9e4f0",
        "actionHoverBg": root.isDarkTheme ? "#223142" : "#e5eef7",
        "eventBorder": root.isDarkTheme ? "#35547f" : "#8eafd6",
        "eventTitle": root.isDarkTheme ? "#ffd494" : "#8f5a05",
        "messageTitle": root.isDarkTheme ? "#91cbff" : "#2e6da8",
        "timestampText": root.isDarkTheme ? "#8094b1" : "#728296",
        "chipBg": root.isDarkTheme ? "#1c2a3b" : "#e8f0f8",
        "chipText": root.isDarkTheme ? "#b2caeb" : "#356a9c",
        "followBg": root.isDarkTheme ? "#162437" : "#edf4fb",
        "followBorder": root.isDarkTheme ? "#5a82ba" : "#7295c2",
        "followText": root.isDarkTheme ? "#ecf4ff" : "#225f95",
        "followBadgeText": root.isDarkTheme ? "#dceaff" : "#386894",
        "dialogBg": root.isDarkTheme ? "#0f1822" : "#fcfeff",
        "dialogBorder": root.isDarkTheme ? "#26394f" : "#d4ddeb",
        "accentPanelBg": root.isDarkTheme ? "#162231" : "#edf4fb",
        "accentPanelBorder": root.isDarkTheme ? "#2f4765" : "#bfd4ea",
        "dividerLine": root.isDarkTheme ? "#2b3b50" : "#c8d5e3",
        "dividerLabelBg": root.isDarkTheme ? "#101a25" : "#eef4fa",
        "buttonBg": root.isDarkTheme ? "#1c2735" : "#f3f7fb",
        "buttonBorder": root.isDarkTheme ? "#304155" : "#ccd9e7",
        "buttonHoverBg": root.isDarkTheme ? "#243447" : "#e7eff7",
        "buttonPressedBg": root.isDarkTheme ? "#2e4158" : "#dbe7f2",
        "buttonPrimaryBg": root.isDarkTheme ? "#c9defd" : "#17324e",
        "buttonPrimaryHoverBg": root.isDarkTheme ? "#d7e7ff" : "#224364",
        "buttonPrimaryPressedBg": root.isDarkTheme ? "#b8d3fb" : "#10273d",
        "buttonPrimaryText": root.isDarkTheme ? "#102136" : "#eef5ff",
        "buttonDangerBg": root.isDarkTheme ? "#df5669" : "#c92f46",
        "buttonDangerHoverBg": root.isDarkTheme ? "#ee6879" : "#da445b",
        "buttonDangerPressedBg": root.isDarkTheme ? "#bd3d50" : "#a82639",
        "buttonDangerText": "#fff7f8",
        "fieldBg": root.isDarkTheme ? "#0e1722" : "#f8fbfe",
        "fieldBorder": root.isDarkTheme ? "#26364a" : "#d3ddea",
        "fieldFocusBorder": root.isDarkTheme ? "#6a93cb" : "#5e88bf",
        "fieldPlaceholder": root.isDarkTheme ? "#64778f" : "#8b99ab",
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

    readonly property var statusFills: ({
        "dark": {
            "connected": "#17472a",
            "subscribed": "#17472a",
            "acknowledged": "#17472a",
            "completed": "#17472a",
            "connecting": "#47361a",
            "pending": "#47361a",
            "queued": "#47361a",
            "sent": "#47361a",
            "published": "#47361a",
            "error": "#43242a",
            "failed": "#43242a"
        },
        "light": {
            "connected": "#e4f6ea",
            "subscribed": "#e4f6ea",
            "acknowledged": "#e4f6ea",
            "completed": "#e4f6ea",
            "connecting": "#fff1d6",
            "pending": "#fff1d6",
            "queued": "#fff1d6",
            "sent": "#fff1d6",
            "published": "#fff1d6",
            "error": "#fde7eb",
            "failed": "#fde7eb"
        }
    })

    readonly property var themeModeMetaByMode: ({
        "system": {
            "icon": "contrast",
            "label": "System",
            "next": "light"
        },
        "light": {
            "icon": "light-mode",
            "label": "Light",
            "next": "dark"
        },
        "dark": {
            "icon": "dark-mode",
            "label": "Dark",
            "next": "system"
        }
    })

    function stateColor(state) {
        return root.stateColors[state] || "#7f90a8"
    }

    function statusFill(state) {
        const mode = root.isDarkTheme ? "dark" : "light"
        const fills = root.statusFills[mode]
        return fills[state] || (root.isDarkTheme ? "#253040" : "#e6edf5")
    }

    function materialIcon(name) {
        return Qt.resolvedUrl(`icons/material/${name}.svg`)
    }

    function themeModeMeta(mode) {
        return root.themeModeMetaByMode[mode] || root.themeModeMetaByMode.system
    }
}
