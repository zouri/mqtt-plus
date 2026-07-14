# Workbench Redesign

## Goal

Rebuild the MQTT Plus workbench to match `mqtt-plus-workbench-review.html` while keeping the UI of navigation, logs, scripts, and settings unchanged. Supporting C++ data, storage, and ViewModel changes are allowed where they are required to make the workbench interactions real and internally consistent.

## Scope

The work includes:

- the connection list and its collapsed state;
- the current-session summary and subscription list;
- message search, Topic filters, and direction filters;
- the compact message stream and message inspector;
- the collapsible publish composer;
- unified incoming and outgoing message history;
- workbench-specific models, ViewModel commands, and tests.

The work does not redesign the application title bar, primary navigation, logs page, scripts page, or settings page. Their shared data dependencies may continue to consume the updated message model, but their visible UI must not change.

Existing uncommitted edits in `SubscriptionsPanel.qml` and its architecture test must be preserved and incorporated rather than reverted.

## Canonical Message Data

Incoming and outgoing MQTT messages use one canonical record shape throughout storage, history services, rendering, and the workbench model. A record contains:

- stable message and session identifiers;
- timestamp and direction (`incoming` or `outgoing`);
- Topic;
- QoS, represented as `-1` when unavailable;
- nullable Retain state when the receive path cannot provide it;
- raw payload bytes, original size, preview, storage state, hash, and payload format;
- parsed payload, parsed format, parse error, script identifier, and script name.

Topic aliases and colors are presentation metadata resolved from the current subscription set. They are not duplicated in persistent history.

Published messages persist direction, QoS, Retain, and format. Incoming messages persist the metadata available from the Qt MQTT receive path. Missing protocol metadata is displayed as unavailable rather than inferred.

## Storage

`mqtt_messages` is recreated around the canonical record. Compatibility with older message-table layouts is intentionally out of scope. When an existing table does not match the new required schema, the message table is rebuilt and old message history may be discarded. Session settings and unrelated application data remain untouched.

History queries return the same canonical fields for recent and paged records. Pending-message batching, payload size limits, pruning, and lazy payload loading remain supported.

## Models And Data Flow

`MqttSessionService` supplies publish metadata to `EventHistoryService`. The history service constructs canonical records for both message directions, sends them to `HistoryStore`, and appends equivalent visible rows for the active session.

`EventRenderer` converts canonical records into display roles without inventing protocol metadata. `EventStreamModel` exposes direction, alias, QoS, Retain availability, parsed payload, and the existing payload and history roles.

A dedicated message filter proxy owns workbench filtering. It supports:

- case-insensitive search across alias, Topic, Payload, and format;
- multiple selected Topics;
- all, incoming, and outgoing directions;
- a filtered count and mapped row lookup for the inspector;
- correct invalidation after history paging, real-time appends, session switches, or subscription metadata changes.

Launch dividers remain visible without filters. While a filter is active, dividers are hidden so they cannot appear without matching messages.

`WorkbenchViewModel` exposes both the canonical source and filtered workbench stream, filter state, message detail loading, bulk subscription pause, Topic-to-filter commands, copy commands, and use-as-draft commands. QML coordinates interactions but does not implement data filtering with delegate-local JavaScript.

## Workbench UI

### Layout

The workbench uses four visual regions: existing application navigation, a 208-pixel connection pane, an approximately 320-pixel subscription pane, and a flexible message workspace. Only the latter three are owned by this redesign.

The connection pane collapses to a compact reveal control. At narrower widths it hides automatically before the subscription pane. At mobile-width layouts the message workspace remains the primary visible surface. Split handles retain user resizing where it does not conflict with the responsive minimum widths.

### Connection And Subscription Panes

Connection rows use a status dot, name, endpoint or error, and contextual actions. The selected row has a restrained surface and border treatment matching the reference.

The session summary shows connection state, name, endpoint, protocol, transport/TLS state, Keep Alive, edit, and connect or disconnect actions. Subscription tools provide Topic search, pause or resume all, and add subscription. Rows show the Topic color, alias, Topic path, live rate, pause state, and contextual menu.

The Topic menu provides message-filter actions in addition to edit, copy, and delete. Filtering a Topic updates the message proxy without coupling the subscription delegate to the message list implementation.

### Message Stream

The header contains title and filtered count, search, a filter popover, follow mode, output pause, and a compact actions menu. The filter popover supports multiple Topics and all, incoming, or outgoing direction segments.

Message rows use the compact reference layout: direction and timestamp, alias and Topic, single-line payload preview, format or parser label, payload size, and Topic color. Selecting a message applies a clear selection treatment and opens the inspector. Empty, paused, and no-match states have dedicated messaging.

History paging and smart-follow behavior continue to work with the filter proxy. New matching rows increment unread state when the user is not following. Rows excluded by the active filter do not create false unread counts.

### Message Inspector

The inspector slides in from the right inside the message workspace. It shows parsed output when present, stored payload or preview, alias, Topic, direction, time, QoS, format, size, and Retain state. It provides copy parsed output, copy Payload, copy Topic, and use-as-draft actions.

Full stored payload is loaded lazily by history identifier. Skipped or truncated oversized payloads state the storage limitation and show the available preview, original size, and hash instead of claiming that the content is complete.

### Publish Composer

The composer is a compact bottom section with a collapsible header. Its body contains Topic, QoS, format, Retain, payload editor, validation feedback, and an icon-only publish action. Existing publisher validation and status feedback remain authoritative.

## Interaction And Accessibility

Built-in Qt Quick Controls remain the default. Icon buttons keep accessible names and tooltips. Custom selectable rows expose roles, names, keyboard activation, focus treatment, and context-menu keys. Popovers and the inspector restore focus to their trigger when closed.

Animations are limited to connection-pane movement, popover appearance, composer expansion, and inspector translation. They use short durations and do not animate large subtree width or height when a transform can provide the same feedback.

## Error And Empty States

The workbench explicitly handles disconnected sessions, empty subscriptions, no matching subscriptions, empty history, no matching messages, paused output, missing full payloads, and publish validation errors. Unknown QoS or Retain metadata is rendered as unavailable.

Storage failures continue to enter the application event log and must not crash or block the active stream. A failed lazy detail load leaves the inspector open with an explanatory unavailable state.

## Testing And Verification

Automated tests cover:

- the new message schema and database recreation behavior;
- incoming and outgoing history round trips;
- QoS, Retain, direction, payload, and parser metadata;
- message filtering by text, Topic, and direction;
- proxy updates after append, prepend, reset, and session switch;
- inspector detail loading and draft reuse;
- bulk subscription pause and Topic filter commands;
- QML architecture boundaries for the workbench.

Validation consists of the configured debug build, `all_qmllint`, and the complete `ctest` suite. The running macOS application is visually checked against the HTML reference at desktop and narrow window sizes, including collapsed connections, filters, selected message and inspector, paused output, and expanded and collapsed composer states.
