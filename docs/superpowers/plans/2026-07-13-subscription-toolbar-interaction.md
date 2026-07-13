# Subscription Toolbar Interaction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the icon-only add-subscription action visually prominent while keeping the Topic filter border neutral during keyboard input.

**Architecture:** Keep both changes inside `SubscriptionsPanel.qml`. Reuse `AppIconButton.primary` for the emphasized action and keep the filter background border bound to the neutral field-border color regardless of real keyboard focus; source-boundary assertions lock down both behaviors without changing models or services.

**Tech Stack:** Qt 6.11, Qt Quick, Qt Quick Controls Basic, Qt Test, CMake presets.

## Global Constraints

- The add action remains icon-only and uses a 30 px square primary button.
- The Topic filter keeps real keyboard focus and accepts normal text input.
- The Topic filter border remains neutral while focused.
- No panel-wide pointer observer is added for focus management.
- The active Topic shadow remains static and has no per-message animation.
- No C++ model, service, persistence, editor, message-stream, or translation changes are required.

---

### Task 1: Emphasize Add And Keep Filter Border Neutral

**Files:**
- Modify: `tests/test_architecture_boundaries.cpp:877-897`
- Modify: `qml/features/workbench/SubscriptionsPanel.qml:103-181`
- Test: `tests/test_architecture_boundaries.cpp`

**Interfaces:**
- Consumes: `AppIconButton.primary: bool` and `themePalette.fieldBorder: color`.
- Produces: QML id `addSubscriptionButton`; preserves `subscriptionCreateRequested()` and the existing filter-model binding.

- [ ] **Step 1: Add failing toolbar interaction assertions**

```cpp
const qsizetype addSubscriptionButtonStart = subscriptionsSource.indexOf(QStringLiteral("id: addSubscriptionButton"));
QVERIFY2(addSubscriptionButtonStart >= 0,
    "Subscription toolbar must expose its primary add action");
const QString addSubscriptionButtonSource = subscriptionsSource.mid(addSubscriptionButtonStart, 500);
QVERIFY2(addSubscriptionButtonSource.contains(QStringLiteral("primary: true")),
    "The icon-only add action must use the primary button treatment");
QVERIFY2(addSubscriptionButtonSource.contains(QStringLiteral("Layout.preferredWidth: 30"))
        && addSubscriptionButtonSource.contains(QStringLiteral("Layout.preferredHeight: 30")),
    "The primary add action must retain its compact 30 px square geometry");
QVERIFY2(!subscriptionsSource.contains(QStringLiteral("filterFocusDismissHandler")),
    "Subscription panel must not install a panel-wide pointer observer for filter focus");
QVERIFY2(subscriptionsSource.contains(QStringLiteral("border.color: control.ui.themePalette.fieldBorder")),
    "The Topic filter must retain a neutral border while receiving keyboard input");
```

- [ ] **Step 2: Build and run the focused test to verify it fails**

Run:

```bash
cmake --build --preset qt6.11-debug --target test_architecture_boundaries
./build/qt6.11-debug/test_architecture_boundaries workbenchMiddlePaneUsesCompactHeaderControls
```

Expected: FAIL because the add button has no stable id or primary treatment and the filter border still changes with `activeFocus`.

- [ ] **Step 3: Keep the filter border neutral**

Replace the active-focus border binding in the filter background:

```qml
background: Rectangle {
    radius: 8
    color: control.ui.themePalette.innerPanelBg
    border.color: control.ui.themePalette.fieldBorder
}
```

Do not add a `TapHandler`, `PointHandler`, or `MouseArea` for focus dismissal. The text field keeps its actual keyboard focus and caret.

- [ ] **Step 4: Apply the primary add-button treatment**

Update the existing toolbar `AppIconButton`:

```qml
AppIconButton {
    id: addSubscriptionButton
    ui: control.ui
    Layout.preferredWidth: 30
    Layout.preferredHeight: 30
    primary: true
    iconSource: control.ui.materialIcon("plus")
    iconSize: 16
    cornerRadius: 7
    accessibleName: qsTr("Add topic")
    toolTipText: qsTr("Add subscription")
    toolTipPosition: AppToolTip.Position.Bottom
    onClicked: control.subscriptionCreateRequested()
}
```

Remove the old neutral `restBg`, `hoverBg`, `outlineColor`, and `symbolColor` overrides so `AppIconButton.primary` supplies the light/dark theme colors.

- [ ] **Step 5: Run the focused test and QML lint**

Run:

```bash
cmake --build --preset qt6.11-debug --target test_architecture_boundaries
./build/qt6.11-debug/test_architecture_boundaries workbenchMiddlePaneUsesCompactHeaderControls
cmake --build --preset qt6.11-debug --target all_qmllint
```

Expected: focused test passes and `qmllint` exits with code 0.

- [ ] **Step 6: Run complete verification and inspect the application**

Run:

```bash
cmake --build --preset qt6.11-debug
ctest --test-dir build/qt6.11-debug --output-on-failure
```

Expected: build succeeds and all 19 registered tests pass. In the application, verify the primary add icon in light and dark themes, then click the filter, type text, and confirm the border remains neutral while input still works.

- [ ] **Step 7: Commit**

```bash
git add qml/features/workbench/SubscriptionsPanel.qml tests/test_architecture_boundaries.cpp
git commit -m "Improve subscription toolbar interaction"
```
