# Workbench Middle Pane Header Design

## Goal

Reduce visual clutter and reclaim vertical space at the top of the workbench middle pane without removing connection diagnostics or changing subscription behavior.

## Session Overview

`SessionOverviewPanel` uses two compact rows and targets an 84-88 px height.

- The first row contains the session name, endpoint, edit action, and connect/disconnect action.
- The second row contains protocol, MQTT client ID, and connection status as compact inline metadata.
- Existing text elision, status coloring, button states, accessibility names, and tooltips remain intact.
- Transport information remains visible only when it differs from the default TCP transport.

## Subscription Toolbar

`SubscriptionsPanel` replaces its title row and filter row with one compact toolbar.

- Remove the `Subscriptions` title and matching-count badge.
- Remove the Active/Paused filter mode combo box.
- Place the topic filter field and add-subscription button on the same row.
- Reset the model filter mode to `All` so a previously selected hidden mode cannot keep subscriptions out of view.
- Preserve text filtering and the existing add-subscription action.

## Scope

This change only affects the two workbench QML panels. It does not change subscription delegates, connection behavior, view-model APIs, persistence, or the message pane.

## Validation

- Build the application with the `qt6.11-debug` preset.
- Run the `all_qmllint` target.
- Run the registered tests with `ctest`.
- Launch the macOS application and verify the middle pane at its normal and minimum widths.
- Confirm that connection actions, topic text filtering, and adding a subscription still work.
