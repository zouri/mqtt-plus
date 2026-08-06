---
status: accepted
---

# Store Message Processors as immutable SQLite revisions

The Processor Library will use a dedicated SQLite database containing stable processor identities, immutable multi-file revisions, and their source files. Saving creates or reuses a content-addressed revision and atomically advances the processor's current revision; runtime artifacts remain disposable cache data outside the database. This is preferred over mutable code rows or an index plus loose files because subscriptions and message history need reproducible revision identity, multi-file saves need transactional consistency, and future JavaScript, Lua, Python, and C++ packages must share one storage model.

## Consequences

Existing script index files and mutable Lua entries are not migrated. The UI has no archive action; processors enter the library only through an explicit save. A processor may be permanently deleted only when no saved subscription references it, and its internal revisions are deleted with it. Immutable revisions otherwise remain available internally for historical message identity and reproducible queued work.
