# Security Notes

This document records security-relevant behavior that should be reviewed before each public release.

## Credential Storage

`mqtt-plus` currently stores session settings with `QSettings`. Session settings include MQTT connection fields such as host, port, username, and password.

For a public release, choose one of these strategies:

- Short-term disclosure: keep the current storage behavior and clearly document that credentials are stored in the platform settings backend used by Qt.
- Safer default: store passwords and other sensitive fields through QtKeychain or native platform credential stores, and keep only non-secret connection metadata in `QSettings`.

Do not describe the current behavior as encrypted unless the implementation changes to provide that guarantee.

## Lua Receive Scripts

Receive scripts run inside an embedded Lua state with only selected standard libraries opened. File loading, dynamic loading, `print`, and `collectgarbage` are disabled. The runner also enforces an instruction limit and a maximum display output length.

Known hardening work:

- Add a custom Lua allocator with a memory budget.
- Add a maximum script source size.
- Add clearer UI feedback for scripts stopped by limits.
- Keep payload and script output limits documented in the UI or release notes.

## Message Payloads and History

Incoming MQTT messages may be stored in the local SQLite history database. The app has configurable retention limits, but individual large payloads can still consume memory and disk space while being decoded, rendered, and persisted.

Current hardening:

- Incoming payloads are limited by the configurable `maxIncomingPayloadBytes` setting.
- Payload history stores raw bytes as a BLOB plus preview/status metadata instead of duplicating full text and Base64 for new messages.
- Large, binary, truncated, or skipped payloads produce clear Payload events and render preview/status text in history.

Known hardening work:

- Add targeted HistoryStore tests for full, truncated, raw-only, and skipped payload rows.
- Consider a lower hard cap or streaming hash strategy if broker-side payload sizes can exceed available memory before Qt delivers the message.

## TLS Settings

TLS sessions can be configured to skip peer verification. This is useful for local brokers and self-signed development setups, but it weakens transport security.

Public release notes should describe the risk of disabling secure peer verification and should keep secure verification enabled by default.

## Reporting Security Issues

Add a project-specific security contact or GitHub Security Advisory process before publishing the repository broadly.
