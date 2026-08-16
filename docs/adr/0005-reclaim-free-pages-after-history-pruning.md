# Reclaim free pages after history pruning

- Status: Accepted
- Implementation: Implemented
- Date: 2026-08-16

## Context

Message history, event logs, and their retention/cleanup paths use DELETE-based
pruning (`pruneMessages`, `pruneLogs`) and clearing (`clearMessages`,
`clearLogs`, `clearSessionHistory`, `clearAllHistory`). SQLite in WAL mode
reuses freed pages internally but never returns them to the operating system,
and the code performed no `VACUUM` or `incremental_vacuum`. In practice the
history database grew to 5.9 GB while holding about 0.5 MB of live data:
1,541,475 of 1,541,609 pages were free-list pages (99.93%). The file only ever
grew.

## Decision

Enable incremental auto-vacuum and reclaim free pages after every deletion path
and once at startup:

- `initialize()` sets `PRAGMA auto_vacuum = INCREMENTAL` before table creation.
  In WAL mode this mode is deferred: it takes effect only when a full `VACUUM`
  runs (verified against SQLite 3.54), so the first large prune activates it.
- New `HistoryStore::reclaimFreePages()` reads `page_size`, `page_count`, and
  `freelist_count`. When free space is at least 128 MiB or at least 25% of the
  file, it runs a full `VACUUM`, which rewrites the database and truncates the
  file to the live data. Below the threshold it does nothing: SQLite reuses the
  free pages for later writes, and the file self-heals at the next
  threshold-crossing cleanup or startup maintenance.
- `PRAGMA incremental_vacuum` is intentionally not used: under WAL mode the Qt
  QSQLITE driver applies it only partially (observed: all 7 free pages
  reclaimed with the `sqlite3` CLI, 1 of 7 via the Qt driver), while a full
  `VACUUM` is reliable on the same database.
- `pruneMessages`, `pruneLogs`, and `executeDeletes` (backing the four `clear*`
  methods) call `reclaimFreePages()` after a successful commit. Reclamation is
  best-effort: a failure is logged with `qWarning` and does not turn a
  successful delete into a reported error.
- The history writer worker calls `reclaimFreePages()` once after its store is
  ready at startup, on the worker thread, so legacy oversized databases are
  compacted in the background without blocking the UI.

## Consequences

- The history database file shrinks after pruning, clearing, and on application
  startup; disk usage stays proportional to retained data.
- A full `VACUUM` can temporarily run on the GUI thread when a deletion frees a
  large fraction of a large database (for example, clearing all history), which
  pauses that thread for the duration; startup compaction runs on the worker
  thread.
- `VACUUM` requires exclusive write access for its duration; concurrent access
  is handled by the existing `busy_timeout` and WAL readers are unaffected.
- No schema change and no data migration; existing rows are preserved.
