# Message Retention Lifecycle Design

## Goal

Keep every MQTT message received or published during the current application run, while still enforcing the configured per-connection saved-message limit at application lifecycle boundaries.

## Scope

- Apply the saved-message retention limit when the application starts and when it exits normally.
- Apply the limit to every configured connection.
- Do not prune messages while the application is running, including after a persistence flush.
- Changing the saved-message limit only persists the preference; it does not immediately prune history or reload the message stream.
- Keep explicit manual cleanup commands immediate. Clearing current messages, all messages, or all history continues to delete stored data and update the visible runtime state at once.
- Keep log retention behavior unchanged.
- Treat a retention limit of `0` as unlimited and skip automatic pruning.

## Architecture

`ApplicationCoreState` owns the automatic retention lifecycle because it already coordinates startup, pending-history flushing, and exit cleanup. A focused helper will iterate the loaded sessions and call `HistoryStore::pruneMessages()` with the current configured limit.

`EventHistoryService` remains responsible for buffering and flushing messages, but no longer decides when retention pruning occurs. Its periodic flush path will persist queued messages without deleting older rows.

`SettingsViewModel` will persist a changed message-retention preference without calling the history store, reloading history, or emitting a message-stream refresh. Existing manual cleanup methods remain unchanged and immediate.

## Lifecycle Flow

### Startup

1. Load scripts and configured sessions.
2. Apply the saved-message limit to every loaded session.
3. Select the initial session and load its already-pruned history into the runtime models.

Running startup cleanup also covers a previous crash or forced termination that prevented normal exit cleanup.

### Normal Exit

1. Flush all pending messages to storage.
2. Apply the saved-message limit to every loaded session.
3. Apply the existing clear-on-exit policies for messages and logs.

The clear-on-exit policy remains authoritative. For example, an `all` message cleanup may delete rows that were just reduced by retention pruning.

### During Runtime

Message flushes only persist queued messages. The stored row count may exceed the configured limit until the next normal exit or startup. The in-memory visible-window cap remains a rendering safeguard and is not part of persistent retention.

### Manual Cleanup

Manual cleanup is an explicit user command and bypasses deferred retention timing. Current-message cleanup, all-message cleanup, and all-history cleanup continue to delete immediately and refresh the affected models and counters.

## Settings Copy

Update the saved-message setting detail to make the deferred timing explicit:

- English: `Maximum MQTT messages kept per connection. Cleanup runs when the app starts or exits.`
- Simplified Chinese: `每个连接最多保留的 MQTT 消息数；应用将在启动或退出时执行清理。`

The manual-cleanup copy remains `Clear stored data immediately.` because those actions still run synchronously.

## Error Handling

Retention cleanup remains best effort, matching the existing `HistoryStore::pruneMessages()` contract. Database errors continue to be recorded in `HistoryStore::lastError()`. A cleanup failure must not prevent startup or shutdown from continuing.

## Testing

- Verify repeated message-history flushes can leave more stored rows than the configured limit during a run.
- Verify changing the message-retention setting persists the new value without pruning or reloading history.
- Verify startup orchestration applies retention after sessions are loaded and before current-session history is reloaded.
- Verify exit orchestration flushes pending messages before applying retention.
- Verify the unlimited option skips automatic pruning.
- Preserve tests proving manual cleanup deletes immediately.
- Update source/translation assertions for the new deferred-cleanup copy where applicable.

## Non-Goals

- Changing log-retention timing.
- Changing manual cleanup behavior.
- Changing the maximum number of message rows rendered in memory.
- Adding background, timer-based, or threshold-based pruning.
