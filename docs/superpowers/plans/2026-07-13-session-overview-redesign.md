# Session Overview Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the current text-heavy MQTT session header with the approved compact hierarchy and a color-only connection-state indicator while preserving the subscription list as the primary content area.

**Architecture:** Keep the change inside the existing QML presentation boundary. `SessionOverviewPanel` derives all display state from its current `session`, `status`, and `ui` inputs; the architecture-boundary test locks down the compact height, color-only indicator, tooltip accessibility, and removal of the visible status metric.

**Tech Stack:** Qt 6.11, Qt Quick, Qt Quick Controls Basic, Qt Quick Layouts, Qt Test, CMake presets.

## Global Constraints

- The panel preferred height is exactly 96 px.
- The connection state is visible as a color dot only; no visible state label is rendered.
- The state label and error details remain available through tooltip and accessibility metadata.
- Only existing `session` and `status` map fields are consumed.
- `SubscriptionsPanel`, C++ ViewModels, MQTT services, persistence, and connection behavior remain unchanged.
- Existing intent signals and `toggleCurrentSessionConnection()` remain the only connection-action path.

---

### Task 1: Lock Down the Approved Session Header Contract

**Files:**
- Modify: `tests/test_architecture_boundaries.cpp:853`
- Test: `tests/test_architecture_boundaries.cpp`

**Interfaces:**
- Consumes: the QML source text at `qml/features/workbench/SessionOverviewPanel.qml`.
- Produces: source-boundary assertions for `Layout.preferredHeight: 96`, `id: statusDot`, `statusToolTipText`, `Accessible.name`, and the absence of the old visible Status metric.

- [ ] **Step 1: Replace the old height assertion and add the new presentation assertions**

```cpp
QVERIFY2(overviewSource.contains(QStringLiteral("Layout.preferredHeight: 96")),
    "Session overview must leave the subscription list as the primary content area");
QVERIFY2(overviewSource.contains(QStringLiteral("id: statusDot")),
    "Session overview must expose connection state through a compact color dot");
QVERIFY2(overviewSource.contains(QStringLiteral("statusToolTipText")),
    "Session overview must keep connection details available in a tooltip");
QVERIFY2(overviewSource.contains(QStringLiteral("Accessible.name: control.statusToolTipText")),
    "The color-only connection state must have an accessible name");
QVERIFY2(!overviewSource.contains(QStringLiteral("\"label\": qsTr(\"Status\")")),
    "Session overview must not render a visible connection-state label");
```

- [ ] **Step 2: Build and run the architecture-boundary test to verify it fails**

Run:

```bash
cmake --build --preset qt6.11-debug --target test_architecture_boundaries
./build/qt6.11-debug/test_architecture_boundaries workbenchMiddlePaneUsesCompactHeaderControls
```

Expected: the test fails because the current QML still uses `Layout.preferredHeight: 86` and has no `statusDot` or `statusToolTipText`.

### Task 2: Implement the Compact Color-Only Session Header

**Files:**
- Modify: `qml/features/workbench/SessionOverviewPanel.qml:8-183`
- Modify: `i18n/mqtt_plus_zh_CN.ts`
- Test: `tests/test_architecture_boundaries.cpp`

**Interfaces:**
- Consumes: `session.name`, `session.host`, `session.port`, `session.transportLabel`, `session.protocolVersionName`, `session.clientId`, `session.keepAliveSeconds`, `status.state`, `status.hasError`, and `status.lastError`.
- Produces: `effectiveState: string`, `statusDotColor: color`, and `statusToolTipText: string` as private QML readonly properties.
- Preserves: `sessionEditRequested(int)`, `connectionConnectRequested()`, and `viewModel.toggleCurrentSessionConnection()`.

- [ ] **Step 1: Add derived state properties without changing connection behavior**

```qml
readonly property string effectiveState: control.hasError ? "error" : (control.status.state || "idle")
readonly property color statusDotColor: control.ui.stateColor(control.effectiveState)
readonly property string statusToolTipText: control.hasError && control.status.lastError
                                                    ? qsTr("%1: %2").arg(control.ui.statusLabel(control.status.state || "disconnected")).arg(control.status.lastError)
                                                    : control.ui.statusLabel(control.status.state || "idle")
```

- [ ] **Step 2: Replace the old two-row metric layout with the approved 96 px hierarchy**

```qml
ColumnLayout {
    anchors.fill: parent
    anchors.margins: 10
    spacing: 5

    RowLayout {
        Layout.fillWidth: true
        spacing: 9

        Rectangle {
            id: statusDot

            Layout.preferredWidth: 9
            Layout.preferredHeight: 9
            radius: 5
            color: control.statusDotColor
            Accessible.name: control.statusToolTipText

            HoverHandler { id: statusDotHover }

            AppToolTip {
                ui: control.ui
                text: control.statusToolTipText
                position: AppToolTip.Position.Bottom
                active: statusDotHover.hovered
            }
        }

        Label {
            Layout.fillWidth: true
            text: control.session.name || qsTr("No session")
            color: control.ui.textStrong
            font.pixelSize: 15
            font.bold: true
            elide: Label.ElideRight
        }

        AppIconButton {
            ui: control.ui
            enabled: control.status.state === "disconnected"
            iconSource: control.ui.materialIcon("edit")
            accessibleName: qsTr("Edit connection")
            onClicked: control.sessionEditRequested(control.viewModel.currentSessionIndex)
        }

        AppIconButton {
            ui: control.ui
            iconSource: control.connectionActionIcon
            primary: !control.canDisconnect
            danger: control.canDisconnect
            accessibleName: control.connectionActionText
            toolTipText: control.connectionActionText
            onClicked: {
                if (!control.canDisconnect) {
                    control.connectionConnectRequested();
                }
                control.viewModel.toggleCurrentSessionConnection();
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        Label {
            Layout.fillWidth: true
            text: control.endpointText
            color: control.ui.textStrong
            font.pixelSize: 12
            font.bold: true
            elide: Label.ElideRight
        }

        AppBadge {
            ui: control.ui
            label: control.session.transportLabel || "TCP"
            horizontalPadding: 6
            verticalPadding: 1
            badgeRadius: 4
        }

        AppBadge {
            ui: control.ui
            label: control.session.protocolVersionName || "MQTT 5"
            horizontalPadding: 6
            verticalPadding: 1
            badgeRadius: 4
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 10

        Label {
            Layout.fillWidth: true
            text: qsTr("Client ID %1").arg(control.session.clientId || "-")
            color: control.ui.themePalette.textSubtle
            font.pixelSize: 10
            elide: Label.ElideRight
        }

        Label {
            text: qsTr("Keep Alive %1s").arg(control.session.keepAliveSeconds || 30)
            color: control.ui.themePalette.textSubtle
            font.pixelSize: 10
        }
    }
}
```

- [ ] **Step 3: Add translations for the two new metadata formats**

```xml
<message>
    <source>Client ID %1</source>
    <translation>客户端 ID %1</translation>
</message>
<message>
    <source>Keep Alive %1s</source>
    <translation>Keep Alive %1 秒</translation>
</message>
```

- [ ] **Step 4: Run QML lint and the focused architecture test**

Run:

```bash
cmake --build --preset qt6.11-debug --target all_qmllint
cmake --build --preset qt6.11-debug --target test_architecture_boundaries
./build/qt6.11-debug/test_architecture_boundaries workbenchMiddlePaneUsesCompactHeaderControls
```

Expected: QML lint completes successfully and the focused test passes.

- [ ] **Step 5: Commit the focused implementation**

```bash
git add qml/features/workbench/SessionOverviewPanel.qml tests/test_architecture_boundaries.cpp i18n/mqtt_plus_zh_CN.ts
git commit -m "Redesign session connection overview"
```

### Task 3: Verify the Complete Workbench Change

**Files:**
- Verify: `qml/features/workbench/SessionOverviewPanel.qml`
- Verify: `qml/features/workbench/SubscriptionsPanel.qml`

**Interfaces:**
- Consumes: the completed Task 2 QML and the existing workbench composition.
- Produces: build, lint, automated-test, and visual evidence that the approved layout works without regressing subscription behavior.

- [ ] **Step 1: Build the application and run all registered tests**

Run:

```bash
cmake --build --preset qt6.11-debug
cmake --build --preset qt6.11-debug --target all_qmllint
ctest --test-dir build/qt6.11-debug --output-on-failure
```

Expected: all commands exit with status 0 and all registered tests pass.

- [ ] **Step 2: Launch and inspect the application**

Run:

```bash
./build/qt6.11-debug/mqtt_plus_app.app/Contents/MacOS/mqtt_plus_app
```

Verify the following in both normal and minimum middle-pane widths:

- the status dot, session name, endpoint, badges, Client ID, and Keep Alive do not overlap;
- long values elide instead of increasing the 96 px header height;
- the status tooltip shows the state and includes `lastError` for failures;
- edit, connect, retry, cancel-connect, and disconnect controls remain usable;
- the subscription toolbar and list retain the remaining middle-pane height;
- light and dark theme state colors remain legible.

- [ ] **Step 3: Review the final diff for unintended scope**

Run:

```bash
git diff --check HEAD^ HEAD
git show --stat --oneline HEAD
```

Expected: no whitespace errors; the implementation commit contains only the QML panel, its focused test, and the translation-catalog update.
