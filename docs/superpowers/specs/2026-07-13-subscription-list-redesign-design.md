# Subscription List Redesign

## Goal

Make the subscription list a compact management surface while retaining the one live signal users need there: the recent message rate for each Topic. Remove metadata that duplicates the message stream or belongs in the subscription editor.

## Information Hierarchy

Each subscription row displays only:

- the configured Topic color as an identity swatch;
- the alias or Topic as the primary label;
- the raw Topic as secondary text only when an alias exists;
- the recent message rate;
- pause or resume and more-actions controls;
- an inline error message when the subscription has failed.

Requested QoS, payload data type, and script name no longer appear in the list. They remain available when creating or editing a subscription. Payload data type continues to appear on message rows where it describes the rendered payload.

## Row Layout

The list retains individual rounded subscription items but increases density.

- A normal row targets a 50 px height instead of 66 px.
- An error row expands only enough to show its single-line error message.
- List spacing is reduced from 7 px to 4 px.
- The primary label elides before it collides with the rate or actions.
- The message rate is right-aligned in a stable-width monospace column.
- Pause or resume and more-actions buttons remain visible because this is a management surface.

The Topic filter keeps its existing geometry; the add-subscription action is emphasized as described below.

## Toolbar Interaction

The add-subscription action remains an icon-only button so the Topic filter keeps most of the available width. It uses a 30 px square primary-color background, a high-contrast plus icon, and the existing accessible name and tooltip. This gives the primary management action a clear visual priority without adding a permanent text label.

The Topic filter releases focus after a left-button tap anywhere else inside the subscription panel. Taps inside the filter keep focus. The outside-tap observer remains passive so subscription rows, pause and menu buttons, scrolling, and context-menu behavior continue to receive their existing pointer events.

## Activity State

A row is actively receiving when all of these conditions are true:

- `topicFps > 0`;
- the subscription is not paused;
- `lastError` is empty.

An active row uses a stable emphasized border, a static elevated shadow, and stronger rate text. It does not pulse, breathe, shimmer, or loop any animation. Entering or leaving the active state may use the existing short color transition, but there is no continuous motion.

An enabled row with a zero rate remains visually static. This avoids presenting “subscribed” as “currently receiving.”

The shadow is static and is not restarted by individual messages. `topicFps` only controls whether the effect layer is enabled, so high message frequency does not create a per-message animation workload.

## Topic Color And Status

The Topic swatch represents Topic identity only. A configured `topicColor` is used directly; a neutral theme accent is used when no color was configured. It never falls back to a connection or subscription state color.

- Paused rows use subdued text and background and show the resume action.
- Error rows use an error border and inline error text and never receive the active shadow.
- Normal idle rows use the standard item border and no shadow.
- Active rows use the static active border and shadow.

This removes the current ambiguity where the same dot can mean either Topic identity or subscription state.

## Rate Formatting

The list continues to consume the existing `topicFps` model role, which is calculated from the recent one-second message window.

- Zero displays as `0/s`.
- Non-zero rates display with one decimal place, such as `3.1/s`.
- The rate column keeps a stable width so updates do not shift the action buttons.

No C++ model or telemetry changes are required.

## Behavior And Accessibility

- Existing right-click, keyboard context-menu, edit, delete, pause, and resume behavior remains unchanged.
- The delegate accessible name remains the subscription display name.
- The accessible description includes the formatted rate and reports paused or error state when applicable.
- Long aliases, Topics, and errors elide without overlapping the rate or actions.

## Implementation Scope

- Redesign the delegate in `qml/features/workbench/SubscriptionsPanel.qml`.
- Remove unused QML delegate role declarations for requested QoS, payload format, and script metadata.
- Keep all roles in `SubscriptionListModel`; other workflows still use the underlying subscription configuration.
- Update the focused architecture-boundary assertions for the new row density and removed metadata.
- Update translations only for new or removed user-facing QML strings.
- Do not change `SubscriptionListModel`, `SubscriptionFilterModel`, C++ ViewModels, MQTT services, persistence, the subscription editor, or the message stream.

## Validation

- Build the application with the `qt6.11-debug` preset.
- Build the `all_qmllint` target.
- Run `ctest --test-dir build/qt6.11-debug --output-on-failure`.
- Verify normal, active, paused, and error rows in both light and dark themes.
- Verify the minimum workbench width with long aliases and Topics.
- Verify that the row shadow is static and appears only while the recent rate is non-zero.
- Verify that rate updates do not resize or shift the action controls.
- Verify pause, resume, context menu, edit, and delete behavior.
- Verify the primary add button remains legible in light and dark themes.
- Verify tapping outside the Topic filter removes its focus without swallowing row or toolbar actions.
