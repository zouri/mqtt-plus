# Session Overview Redesign

## Goal

Make the selected MQTT connection immediately identifiable without reducing the subscription list's role as the primary content area. The overview must answer three questions at a glance: which connection is selected, whether it is healthy, and which broker endpoint it uses.

## Layout

`SessionOverviewPanel` remains a compact header above `SubscriptionsPanel` and targets a fixed preferred height of 96 px.

- The primary row contains a connection-state dot, the session name, the edit action, and the connect or disconnect action.
- The secondary information area contains the broker endpoint, transport label, protocol version, client ID, and Keep Alive interval.
- The endpoint is the strongest secondary value. Transport and protocol use compact badges.
- Client ID and Keep Alive use subdued text beneath the endpoint. A long client ID elides instead of changing the panel height.
- The header uses the existing panel and theme tokens. It does not introduce statistic cards or a separate details drawer.

## Connection State

The interface represents connection state with color only; it does not render a visible state label.

- Connected uses the existing success color.
- Connecting uses the existing connecting or warning color.
- Disconnected and disconnecting use the existing neutral state colors.
- A disconnected session with `hasError` uses the existing error color.

The state dot exposes `ui.statusLabel(status.state)` through its tooltip and accessible name. When `status.hasError` is true and `status.lastError` is non-empty, the tooltip includes the error message. This preserves diagnostics and accessibility without adding visible status text.

## Behavior

The existing intent-based actions remain unchanged.

- Edit is enabled only while disconnected.
- Connect, retry, cancel-connect, and disconnect continue to use `toggleCurrentSessionConnection()` and the existing intent signal.
- Button icons, tooltips, and accessible names continue to reflect the current action.
- The panel reads only the existing `session` and `status` maps supplied by `WorkbenchViewModel`.

## Information Scope

The overview displays only values already available to QML:

- session name;
- connection state and error state;
- host and port;
- transport;
- MQTT protocol version;
- client ID;
- Keep Alive interval.

Connection duration, sent and received totals, recent-message time, QoS distribution, and per-topic message counts are intentionally excluded. They would add backend telemetry and compete with the subscription list for space without improving the primary connection-identification workflow.

`SubscriptionsPanel`, its model, and its interactions remain unchanged.

## Implementation Scope

- Redesign `qml/features/workbench/SessionOverviewPanel.qml` using the existing `AppPanel`, `AppBadge`, `AppIconButton`, theme palette, and material icon helpers.
- Update the focused architecture-boundary assertion that currently requires the 86 px compact header.
- Update translation catalogs only if the QML changes introduce or remove translatable strings that require regeneration.
- Do not change C++ ViewModels, MQTT services, subscription models, persistence, or connection behavior.

## Validation

- Configure and build with the `qt6.11-debug` preset.
- Build the `all_qmllint` target.
- Run `ctest --test-dir build/qt6.11-debug --output-on-failure`.
- Launch the macOS application and inspect the normal and minimum middle-pane widths.
- Verify connected, connecting, disconnected, and error colors in both light and dark themes.
- Verify the status tooltip, full error tooltip, keyboard focus, and accessible names.
- Verify long session names, endpoints, and client IDs elide without overlapping the action buttons or increasing the header height.
- Verify edit, connect, retry, cancel-connect, and disconnect actions behave exactly as before.
