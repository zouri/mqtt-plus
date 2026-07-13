# Subscription Toolbar Interaction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the icon-only add-subscription action visually prominent and dismiss the Topic filter focus after taps elsewhere in the subscription panel.

**Architecture:** Keep both changes inside `SubscriptionsPanel.qml`. Reuse `AppIconButton.primary` for the emphasized action and add one passive panel-level `TapHandler` that maps tap coordinates into the filter field before clearing focus; source-boundary assertions lock down both behaviors without changing models or services.

**Tech Stack:** Qt 6.11, Qt Quick, Qt Quick Controls Basic, Qt Quick pointer handlers, Qt Test, CMake presets.

## Global Constraints

- The add action remains icon-only and uses a 30 px square primary button.
- Taps inside the Topic filter preserve focus.
- Left-button taps elsewhere inside the subscription panel clear filter focus without consuming row or toolbar actions.
- The active Topic shadow remains static and has no per-message animation.
- No C++ model, service, persistence, editor, message-stream, or translation changes are required.

---

### Task 1: Emphasize Add And Dismiss Filter Focus

**Files:**
- Modify: `tests/test_architecture_boundaries.cpp:877-897`
- Modify: `qml/features/workbench/SubscriptionsPanel.qml:103-181`
- Test: `tests/test_architecture_boundaries.cpp`

**Interfaces:**
- Consumes: `AppIconButton.primary: bool`, `TapHandler.tapped(EventPoint, MouseButton)`, `Item.mapFromItem(Item, point)`, and `Item.contains(point)`.
- Produces: QML ids `addSubscriptionButton` and `filterFocusDismissHandler`; preserves `subscriptionCreateRequested()` and the existing filter-model binding.

- [ ] **Step 1: Add failing toolbar interaction assertions**

```cpp
QVERIFY2(subscriptionsSource.contains(QStringLiteral("id: addSubscriptionButton")),
    "Subscription toolbar must expose its primary add action");
QVERIFY2(subscriptionsSource.contains(QStringLiteral("primary: true")),
    "The icon-only add action must use the primary button treatment");
QVERIFY2(subscriptionsSource.contains(QStringLiteral("id: filterFocusDismissHandler")),
    "Subscription panel must observe taps outside the Topic filter");
QVERIFY2(subscriptionsSource.contains(QStringLiteral("filterTopicField.mapFromItem(control, eventPoint.position)")),
    "Filter focus dismissal must distinguish taps inside the field");
QVERIFY2(subscriptionsSource.contains(QStringLiteral("filterTopicField.focus = false")),
    "Taps outside the Topic filter must release its focus");
```

- [ ] **Step 2: Build and run the focused test to verify it fails**

Run:

```bash
cmake --build --preset qt6.11-debug --target test_architecture_boundaries
./build/qt6.11-debug/test_architecture_boundaries workbenchMiddlePaneUsesCompactHeaderControls
```

Expected: FAIL because the add button has no stable id or primary treatment and the panel has no focus-dismiss handler.

- [ ] **Step 3: Add the passive outside-tap handler**

Insert at `SubscriptionsPanel.qml` root scope:

```qml
TapHandler {
    id: filterFocusDismissHandler
    acceptedButtons: Qt.LeftButton

    onTapped: eventPoint => {
        const filterPoint = filterTopicField.mapFromItem(control, eventPoint.position);
        if (!filterTopicField.contains(filterPoint)) {
            filterTopicField.focus = false;
        }
    }
}
```

The handler uses the default passive tap recognition and does not add an exclusive `MouseArea` over the panel.

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

Expected: build succeeds and all 19 registered tests pass. In the application, verify the primary add icon in light and dark themes, then click the filter, type text, click an idle row, and confirm the focus border disappears while row actions still work.

- [ ] **Step 7: Commit**

```bash
git add qml/features/workbench/SubscriptionsPanel.qml tests/test_architecture_boundaries.cpp
git commit -m "Improve subscription toolbar interaction"
```
