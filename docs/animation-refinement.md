# Workbench motion refinement

Implemented on 2026-10-21.

## Behavior

- Icon buttons keep a fixed scale. Press feedback is immediate; hover/release
  feedback retains the shared micro-transition.
- Connection and publishing panels open in 180 ms and close in 120 ms.
- Publishing feedback waits for the QoS-specific terminal state, deduplicates
  request IDs, and limits transient highlights to once per second. Status text
  remains available independently of animation, including failures.
- The message inspector's Popup transition owns its slide animation. Closing
  preserves the displayed payload until the popup has finished exiting, and
  reopening during exit reverses from the current position. Escape closes it.
- The latest-message button reserves space for its unread count, displaying
  `9999+` above that threshold while retaining the exact accessible count.
- Connection and subscription reordering shows a floating card with the source
  title and host/topic, a theme-aware border and shadow, and a faded source row.
  The card follows the pointer without animation lag and is created only during
  dragging. Its text is copied at drag start so delegate recycling cannot change
  the represented item. Release, Escape, and session changes dismiss it.
- Reordering starts from the row body after the platform drag threshold; there
  is no separate grip column. The right-side action area remains independent.
  Single clicks retain selection/focus behavior, and double-clicking a topic
  toggles its subscription without interpreting a completed drag as a click.

## Verification

`tests/test_workbenchviewmodel.cpp` exercises publish acknowledgement stages,
duplicate completion notifications, burst coalescing, disabled animation,
fixed button scale, and inspector close/reopen/close transitions in a real Qt
window. All fixtures use temporary storage and synthetic identifiers, without
connecting to a broker.

The list-reordering cases additionally check drag-card visibility, pointer
tracking, cleanup on cancellation, stable selection/subscription state, and
edge auto-scrolling in light/wide and dark/narrow layouts. They also cover row-body
dragging, action/context menus, wheel and phased touchpad-style scrolling, and double-clicking
subscriptions with both single-row and reorderable lists. Optional captures use
the same `MQTT_PLUS_TEST_CAPTURE_DIR` setting described below.

Run the suite with `./scripts/dev.sh`. GUI and local HTTP service tests require
window-server and loopback-network access; a restricted sandbox cannot run
those tests successfully.

For optional inspector screenshots, set `MQTT_PLUS_TEST_CAPTURE_DIR` to an
existing temporary directory and run the workbench test's
`inspectorReversesCloseWithoutClearingSelection` slot. These captures show the
isolated component's open/closed states, not before/after product screenshots.
No personal configuration is loaded. Follow `docs/images/README.md` before
publishing any captures; temporary captures are not committed.

## Remaining checks

- High-throughput broker traffic and frame-time profiling have not been measured
  as part of this change.
- System reduced-motion preference integration and notification-stack animation
  are deferred; the existing application animation toggle remains supported.
