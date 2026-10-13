---
status: accepted
implementation: implemented
---

# Edit processors in a local browser

## Context

The native text editor synchronized full source strings on input. Script selection, library refresh, validation, and persistence shared the GUI thread. Extending that editor would require implementing IDE functionality while continuing to expose the application to editing and validation latency.

## Decision

The desktop application manages processor identities and subscription bindings, while a bundled TypeScript/Monaco page edits recoverable multi-file drafts in the default browser. A lazily started HTTP host listens only on IPv4 loopback with an ephemeral port. One-use launch tickets and per-tab bearer sessions authorize the editor; the HTTP host exposes neither MQTT credentials nor arbitrary filesystem operations.

The GUI owns read-only processor snapshots. A dedicated storage thread owns SQLite connections and serializes draft writes, conditional saves, and deletion. Processor metadata carries a persistent change sequence, and saves record an idempotency ledger in the same transaction. Successful persistence is acknowledged to the browser only after the GUI applies the saved snapshot and invalidates message admission contexts.

Validation uses the application's internal headless mode with framed CBOR over process pipes. Each validation gets a fresh process, an existing runtime budget, and a parent-enforced deadline. It does not change the in-process runtime used for normal message processing and is not a complete operating-system sandbox.

## Consequences

The application runs without Node or internet access, but building requires Node/npm and Qt HTTP Server. Static web resources are rebuilt during CMake configuration and embedded in the executable. Native editing state and its QML view are removed.

Frontend inputs participate in CMake configure dependencies; a dependency-lock hash controls reinstalling npm packages. Frontend regression tests and production compilation must succeed before resources are embedded. Saves update the in-memory library incrementally; draft count polling performs a grouped query instead of decoding every source package. Save completion reconciles acknowledged content with any surviving draft and branches when identity or content differs. Replay of an earlier receipt does not mutate later drafts. Independent draft/library databases mean cleanup failure is a warning after a successful version commit, not a falsely failed save.

The native syntax highlighter and its KSyntaxHighlighting / Extra CMake Modules dependencies are removed as well. Shared text controls retain plain editing, scrolling, and optional line numbers for publishing and draft workflows. Qt Test is required only when tests are enabled.

Drafts live in a separate local database, survive port changes and restarts, and never become active automatically. Saved scripts retain the existing immutable-revision model. Old applications reject the upgraded library schema. Browser tabs can branch drafts rather than overwrite one another; a stale script base requires preserving the draft or saving a new processor, not force-overwriting the current version.

Browser extensions and processes with access to the user's account are outside the authentication threat model. Close-after-disconnect unsynced input is not guaranteed recoverable. Cross-platform packaging and performance measurements remain release acceptance activities, not properties established by the architecture alone.

The browser can switch its editing target using the existing authenticated processor summaries and draft APIs. A switch persists the current draft without publishing it and resumes the tab's last valid branch, or the sole remaining draft. Multiple unfamiliar branches require explicit selection; the Drafts action always exposes manual selection and fresh-branch creation. Per-processor preferences contain identifiers only, survive refresh, and reset on new authorization. The selected draft and target remain per-tab; navigating does not modify MQTT subscription bindings. In-flight validation/save and uncertain save receipts prevent switching, so a late completion cannot be applied to a different processor's draft. Automatic recovery retains conditional draft writes and processor base-version checks.

See [local verification and release gates](../browser-editor-verification.md) for dated evidence and remaining checks.

Opening an editor with no recoverable draft is read-only with respect to draft storage: the current formal package or a new template becomes an in-memory baseline. The first content change creates the draft, while explicit branch creation/import remain persistent actions. Discarding the last draft reloads that baseline without an immediate replacement write. Validation still requires a stored snapshot; unchanged validation-only snapshots are conditionally removed on completion, never overwriting or deleting concurrent edits.

## Single Editable Window Refinement

The earlier multi-tab editing policy is superseded by one global editable document per running editor service, across all processors. Sessions remain independent for authentication, while each document generates a non-persisted window identity and obtains a renewable thirty-second editing lease. Other documents remain read-only; there is no force takeover. Conditional draft/version protection and branch recovery remain necessary for old drafts, stale clients, and separate app processes.

The HTTP and storage workers share a mutex-protected monotonic lease gate. All authenticated mutating routes require the current window and grant token; storage operations recheck the gate when dequeued, and asynchronous validation rechecks before saving. Grant/release changes serialize with already-admitted storage transactions. Expired or old grant tokens cannot release a successor or commit a pending validation result. Frontend deadlines precede server expiry, renewal runs every five seconds while visible (also during jobs), and pagehide attempts conditional release. A crash or lost release falls back to expiration. No database schema change is required; restarting the app removes all leases and sessions. This is not an OS-wide single-instance policy.
