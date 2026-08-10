---
status: accepted
implementation: implemented
---

# Store publish drafts as versioned JSON

## Context

Publish drafts are structured records with potentially large payloads and a lifecycle separate from disposable message history. `QSettings` does not provide an explicit, evolvable document schema, while `history.db` should not own user-managed drafts.

## Decision

Store the global Draft Library in an independently versioned, unencrypted `drafts.json` file under the application's configuration directory. Write the file atomically with `QSaveFile` and restrict its permissions to the current user where the platform supports it.

Load and parse drafts in the background. Serialize mutations through one background save operation, update the visible library only after the durable write succeeds, and wait for active I/O during shutdown. Before replacing a valid primary file, retain it as `drafts.json.bak`.

Only the current pre-release schema is supported. An unknown newer schema or an unreadable primary file makes the library read-only instead of being overwritten or treated as empty. Recovery preserves the damaged primary as `drafts.json.corrupt-*`, restores a valid backup, and reloads the library in the current process.

## Consequences

Draft payloads are plaintext and must not be presented as secret storage. Older development schemas require manual removal or conversion. Failed saves and recovery attempts preserve the last durable data and surface an error without changing the in-memory library.
