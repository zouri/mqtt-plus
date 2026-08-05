---
status: accepted
---

# Store Message Processors as immutable SQLite revisions

The Processor Library will use a dedicated SQLite database containing stable processor identities, immutable multi-file revisions, and their source files. Saving creates or reuses a content-addressed revision and atomically advances the processor's current revision; runtime artifacts remain disposable cache data outside the database. This is preferred over mutable code rows or an index plus loose files because subscriptions and message history need reproducible revision identity, multi-file saves need transactional consistency, and future JavaScript, Lua, Python, and C++ packages must share one storage model.

## Consequences

Existing script index files and mutable Lua entries are not migrated. Normal deletion archives a processor, while immutable revisions remain available for pinned bindings and historical identity.
