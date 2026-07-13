# Subscription List Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn the subscription list into a compact management surface that keeps Topic rate visible and uses a static elevated state only while messages are actively arriving.

**Architecture:** Keep the change inside `SubscriptionsPanel.qml`. The delegate derives presentation state from existing model roles, while the architecture-boundary test locks down row density, static activity state, and removal of duplicated metadata; no C++ model or service changes are needed.

**Tech Stack:** Qt 6.11, Qt Quick, Qt Quick Controls Basic, Qt Quick Layouts, Qt Quick Effects, Qt Test, CMake presets.

## Global Constraints

- Normal subscription rows are exactly 50 px high; error rows are exactly 64 px high.
- Requested QoS, payload format, and script name do not appear in the list.
- Topic rate remains right-aligned in a stable-width column.
- Active traffic means `topicFps > 0`, not paused, and no error.
- Active feedback is static; no pulse, breathing, shimmer, or looping animation is allowed.
- Topic color represents Topic identity only and never falls back to subscription state color.
- Search, add, pause, resume, context menu, edit, and delete behavior remain unchanged.
- `SubscriptionListModel`, `SubscriptionFilterModel`, ViewModels, services, persistence, editor, and message stream remain unchanged.

---

### Task 1: Lock Down the Compact Management Delegate

**Files:**
- Modify: `tests/test_architecture_boundaries.cpp:869`
- Test: `tests/test_architecture_boundaries.cpp`

**Interfaces:**
- Consumes: QML source text at `qml/features/workbench/SubscriptionsPanel.qml`.
- Produces: source-boundary assertions for dense row height, active-traffic shadow gating, stable rate text, and removal of duplicate metadata.

- [ ] **Step 1: Add failing delegate-contract assertions**

```cpp
QVERIFY2(subscriptionsSource.contains(QStringLiteral("readonly property bool activeTraffic")),
    "Subscription rows must distinguish recent traffic from idle subscriptions");
QVERIFY2(subscriptionsSource.contains(QStringLiteral("implicitHeight: subscriptionDelegate.hasError ? 64 : 50")),
    "Subscription rows must use the compact management height");
QVERIFY2(subscriptionsSource.contains(QStringLiteral("layer.enabled: subscriptionDelegate.activeTraffic")),
    "Only rows with recent traffic may render the elevated shadow");
QVERIFY2(subscriptionsSource.contains(QStringLiteral("readonly property string rateText")),
    "Subscription rows must keep a stable live-rate value");
QVERIFY2(!subscriptionsSource.contains(QStringLiteral("required property int requestedQos")),
    "Subscription rows must not consume QoS for permanent display");
QVERIFY2(!subscriptionsSource.contains(QStringLiteral("required property string formatName")),
    "Subscription rows must not duplicate payload format from the message stream");
QVERIFY2(!subscriptionsSource.contains(QStringLiteral("required property string scriptName")),
    "Subscription rows must not render script metadata as a badge");
QVERIFY2(!subscriptionsSource.contains(QStringLiteral("SequentialAnimation")),
    "Subscription activity feedback must not use a looping breathing animation");
```

- [ ] **Step 2: Build and run the focused test to verify it fails**

Run:

```bash
cmake --build --preset qt6.11-debug --target test_architecture_boundaries
./build/qt6.11-debug/test_architecture_boundaries workbenchMiddlePaneUsesCompactHeaderControls
```

Expected: the test fails because the current delegate has no `activeTraffic` property and still renders QoS, format, and script metadata at 66 px.

### Task 2: Implement the Static Active Subscription Row

**Files:**
- Modify: `qml/features/workbench/SubscriptionsPanel.qml:245-447`
- Test: `tests/test_architecture_boundaries.cpp`

**Interfaces:**
- Consumes: `topic`, `alias`, `displayName`, `topicColor`, `paused`, `lastError`, and `topicFps` model roles.
- Produces: delegate-local `hasError: bool`, `activeTraffic: bool`, `rateText: string`, `secondaryTopic: string`, `topicSwatchColor: color`, and `accessibleDescription: string`.
- Preserves: `toggleCurrentSubscriptionPaused(QString, bool)`, `subscriptionEditRequested(int)`, `requestSubscriptionDelete(QString, QString)`, keyboard menu handling, and right-click handling.

- [ ] **Step 1: Replace duplicate metadata properties with management-state properties**

```qml
required property string topic
required property string alias
required property string displayName
required property string topicColor
required property bool paused
required property string lastError
required property real topicFps
readonly property bool hasError: subscriptionDelegate.lastError.length > 0
readonly property bool activeTraffic: subscriptionDelegate.topicFps > 0
                                              && !subscriptionDelegate.paused
                                              && !subscriptionDelegate.hasError
readonly property string rateText: qsTr("%1/s").arg(subscriptionDelegate.topicFps > 0
                                                     ? Number(subscriptionDelegate.topicFps).toFixed(1)
                                                     : "0")
readonly property string secondaryTopic: subscriptionDelegate.alias.length > 0
                                                ? subscriptionDelegate.topic
                                                : ""
readonly property color topicSwatchColor: subscriptionDelegate.topicColor.length > 0
                                                 ? subscriptionDelegate.topicColor
                                                 : control.ui.themePalette.selectedBorder
readonly property string accessibleDescription: subscriptionDelegate.hasError
                                                       ? subscriptionDelegate.lastError
                                                       : (subscriptionDelegate.paused
                                                          ? qsTr("Paused, %1").arg(subscriptionDelegate.rateText)
                                                          : subscriptionDelegate.rateText)
```

- [ ] **Step 2: Apply compact geometry and static activity styling**

```qml
width: ListView.view.width
implicitHeight: subscriptionDelegate.hasError ? 64 : 50
radius: 6
color: subscriptionDelegate.paused
       ? control.ui.themePalette.innerPanelBg
       : (subscriptionRowHover.hovered
          ? control.ui.themePalette.rowHover
          : control.ui.themePalette.itemBg)
border.color: subscriptionDelegate.hasError
              ? control.ui.themePalette.errorText
              : (subscriptionDelegate.activeTraffic
                 ? control.ui.themePalette.selectedBorder
                 : control.ui.themePalette.innerPanelBorder)
border.width: 1
Accessible.role: Accessible.ListItem
Accessible.name: subscriptionDelegate.displayName
Accessible.description: subscriptionDelegate.accessibleDescription
layer.enabled: subscriptionDelegate.activeTraffic
layer.effect: MultiEffect {
    shadowEnabled: true
    shadowBlur: 0.34
    shadowColor: Qt.rgba(subscriptionDelegate.topicSwatchColor.r,
                         subscriptionDelegate.topicSwatchColor.g,
                         subscriptionDelegate.topicSwatchColor.b,
                         control.ui.isDarkTheme ? 0.42 : 0.24)
    shadowHorizontalOffset: 0
    shadowVerticalOffset: 3
}

HoverHandler {
    id: subscriptionRowHover
}
```

No animation object is added. The layer exists only while `activeTraffic` is true.

- [ ] **Step 3: Replace the two metadata rows with one compact management row**

```qml
ColumnLayout {
    anchors.fill: parent
    anchors.leftMargin: 8
    anchors.rightMargin: 6
    anchors.topMargin: 6
    anchors.bottomMargin: 6
    spacing: 3

    RowLayout {
        Layout.fillWidth: true
        spacing: 7

        Rectangle {
            Layout.preferredWidth: 7
            Layout.preferredHeight: 7
            radius: 2
            color: subscriptionDelegate.topicSwatchColor
            opacity: subscriptionDelegate.paused ? 0.5 : 1.0
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 1

            Label {
                Layout.fillWidth: true
                text: subscriptionDelegate.displayName
                color: subscriptionDelegate.paused ? control.ui.textMuted : control.ui.textStrong
                font.pixelSize: 12
                font.bold: true
                elide: Label.ElideRight
            }

            Label {
                visible: subscriptionDelegate.secondaryTopic.length > 0
                Layout.fillWidth: true
                text: subscriptionDelegate.secondaryTopic
                color: control.ui.themePalette.textSubtle
                font.pixelSize: 10
                elide: Label.ElideRight
            }
        }

        Label {
            Layout.preferredWidth: 48
            Layout.minimumWidth: 48
            Layout.maximumWidth: 48
            text: subscriptionDelegate.rateText
            color: subscriptionDelegate.activeTraffic ? control.ui.textStrong : control.ui.textMuted
            font.family: "Menlo"
            font.pixelSize: 10
            font.bold: subscriptionDelegate.activeTraffic
            horizontalAlignment: Text.AlignRight
        }

        RowLayout {
            spacing: 1

            AppIconButton {
                id: subscriptionPauseButton
                ui: control.ui
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                iconSource: control.ui.materialIcon(subscriptionDelegate.paused ? "play" : "pause")
                iconSize: 13
                cornerRadius: 6
                accessibleName: subscriptionDelegate.paused ? qsTr("Resume topic") : qsTr("Pause topic")
                toolTipText: subscriptionPauseButton.accessibleName
                onClicked: {
                    control.subscriptionActionVisualKey = visualKey;
                    subscriptionActionVisualResetTimer.restart();
                    control.viewModel.toggleCurrentSubscriptionPaused(subscriptionDelegate.topic,
                                                                       subscriptionDelegate.paused);
                }
            }

            AppIconButton {
                id: subscriptionMenuButton
                ui: control.ui
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                iconSource: control.ui.materialIcon("more-horiz")
                iconSize: 16
                accessibleName: qsTr("More actions")
                toolTipText: subscriptionMenuButton.accessibleName
                onClicked: subscriptionDelegate.openSubscriptionContextMenu()
            }
        }
    }

    Label {
        visible: subscriptionDelegate.hasError
        Layout.fillWidth: true
        text: subscriptionDelegate.lastError
        color: control.ui.themePalette.errorText
        font.pixelSize: 10
        elide: Label.ElideRight
    }
}
```

Retain the existing pause-button visual-state bindings (`restBg`, `hoverBg`, `pressedBg`, `outlineColor`, `symbolColor`, `forceActive`, and `visualKey`) and the existing menu-button visual-state bindings around this structure.

- [ ] **Step 4: Reduce list spacing**

```qml
spacing: 4
```

- [ ] **Step 5: Run QML lint and the focused test**

Run:

```bash
cmake --build --preset qt6.11-debug --target all_qmllint
cmake --build --preset qt6.11-debug --target test_architecture_boundaries
./build/qt6.11-debug/test_architecture_boundaries workbenchMiddlePaneUsesCompactHeaderControls
```

Expected: QML lint completes successfully and the focused architecture test passes.

### Task 3: Update Translation And Verify The Workbench

**Files:**
- Modify: `i18n/mqtt_plus_zh_CN.ts`
- Verify: `qml/features/workbench/SubscriptionsPanel.qml`

**Interfaces:**
- Consumes: the new `%1/s` and `Paused, %1` QML source strings.
- Produces: finished Simplified Chinese translations and complete build/test/visual evidence.

- [ ] **Step 1: Add translations in the `SubscriptionsPanel` context**

```xml
<message>
    <source>%1/s</source>
    <translation>%1/s</translation>
</message>
<message>
    <source>Paused, %1</source>
    <translation>已暂停，%1</translation>
</message>
```

- [ ] **Step 2: Build the application and run all registered tests**

Run:

```bash
cmake --build --preset qt6.11-debug
cmake --build --preset qt6.11-debug --target all_qmllint
ctest --test-dir build/qt6.11-debug --output-on-failure
```

Expected: all commands exit with status 0, the translation compiler reports no unfinished strings, and all 19 registered tests pass.

- [ ] **Step 3: Launch and inspect the application**

Run:

```bash
open build/qt6.11-debug/mqtt_plus_app.app
```

Verify in light and dark themes and at the minimum application size:

- normal, active, paused, and error rows are visually distinct;
- only non-zero, non-paused, error-free rows have a static shadow;
- no row contains a looping or breathing effect;
- QoS, payload type, and script badges are absent;
- long aliases and Topics elide before the rate and actions;
- rate changes do not resize or move the action buttons;
- pause, resume, right-click, keyboard menu, edit, and delete behavior remains intact.

- [ ] **Step 4: Commit the focused implementation**

```bash
git add qml/features/workbench/SubscriptionsPanel.qml tests/test_architecture_boundaries.cpp i18n/mqtt_plus_zh_CN.ts
git commit -m "Simplify subscription management list"
```
