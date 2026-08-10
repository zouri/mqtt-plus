---
status: accepted
implementation: implemented
---

# Store message processors as immutable SQLite revisions

## Context

Message processors can contain multiple source files. Subscriptions and message history also need stable processor and revision identities, so mutable code rows or an index backed by loose files would make updates and recovery difficult to reproduce.

## Decision

Store the Processor Library in a dedicated SQLite `library.db`. A processor has a stable identity and points to a current immutable revision; each revision contains its source files and content hash. Saving creates or reuses a content-addressed revision and atomically advances the processor's current revision.

Lua and JavaScript use this model now. Additional runtimes use the same processor, revision, and file records. Generated runtime artifacts remain disposable cache data outside the database.

## Consequences

Existing script index files and mutable Lua entries are not migrated. Processors enter the library only through an explicit save, and the UI has no archive action.

Deleting a processor permanently cascades to all of its revisions and files in `library.db`. The library does not currently prevent deletion when a saved subscription references that processor; such a reference becomes unresolved until the subscription is updated. Historical message rows are stored outside the Processor Library and are not foreign-key owners of its revisions.
