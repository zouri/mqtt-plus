# Model MQTT 5 properties in the domain

- Status: Accepted
- Implementation: Implemented
- Date: 2026-08-15

## Context

MQTT 5 properties were previously applied only at connection time. Publish,
subscription, Last Will, import/export, and history paths either had no property
model or discarded broker-provided metadata. Qt MQTT types also cannot be stored
directly in JSON, settings, or SQLite.

## Decision

Keep ordered user properties and typed MQTT 5 property values in domain structs.
Translate them to and from Qt MQTT types only in the MQTT service adapter. Encode
publish properties as versioned CBOR when they cross persistence and recent-item
boundaries.

Use Qt MQTT's external `QWebSocket` transport for `ws` and `wss` connections so a
session can retain its WebSocket path and the required `mqtt` subprotocol. Enable
the same transport sources when the bundled Qt MQTT fallback is built.

History schema upgrades add the property column in place when the previous schema
is otherwise complete. Existing message rows must not be deleted for this additive
migration.

## Consequences

- MQTT 5 metadata survives publish, receive, history inspection, drafts, and
  configuration transfer.
- MQTT 3.1.1 sessions keep the same domain data but do not send MQTT 5 properties.
- New property fields require updates to the domain codec and its persistence
  round-trip tests.
- WebSocket support adds a Qt WebSockets runtime dependency.
