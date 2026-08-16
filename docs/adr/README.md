# Architecture decisions

Repository documentation is intentionally kept shallow. Durable architecture decisions live directly in this directory; temporary implementation plans and generated agent notes are not stored under `docs/`.

| ADR | Decision status | Implementation | Summary |
| --- | --- | --- | --- |
| [0001](0001-store-publish-drafts-as-versioned-json.md) | Accepted | Implemented | Persist publish drafts as recoverable, versioned JSON. |
| [0002](0002-store-message-processors-as-immutable-sqlite-revisions.md) | Accepted | Implemented | Persist processors as immutable SQLite revisions. |
| [0003](0003-run-python-and-native-processors-out-of-process.md) | Accepted | Planned | Isolate future Python and native runtimes in helper processes. |
| [0004](0004-model-mqtt5-properties-in-the-domain.md) | Accepted | Implemented | Preserve MQTT 5 properties across transports, workflows, and persistence. |
| [0005](0005-reclaim-free-pages-after-history-pruning.md) | Accepted | Implemented | Reclaim SQLite free pages after history pruning and on startup. |

`status` records whether the decision is accepted, superseded, or rejected. `implementation` records whether the code currently implements it. When a decision changes, keep the old ADR as `superseded` and link to its replacement rather than rewriting history.
