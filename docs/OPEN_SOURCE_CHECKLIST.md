# Open Source Checklist

This checklist tracks the remaining work before publishing `mqtt-plus` on GitHub.

## Must Decide Before Publishing

- Choose the project license and add the final `LICENSE` file.
- Decide whether MQTT credentials may be stored through `QSettings` for the first public release, or whether the first release must use QtKeychain or native platform credential stores.
- Confirm whether bundled image and SVG assets are original project assets or copied from third-party icon sets.
- Confirm whether the project is distributed under an open-source Qt license or a commercial Qt license, and document the expected contributor setup.

## Must Complete Before Publishing

- Add a final `LICENSE` file once the license is chosen.
- Keep `THIRD_PARTY_NOTICES.md` updated with Qt, Lua, icons, and packaging dependencies.
- Keep `docs/SECURITY_NOTES.md` aligned with the actual credential and script-sandbox behavior.
- Replace machine-local paths in docs and scripts with placeholders, command arguments, or environment variables.
- Run:

```bash
cmake --preset qt6.11-debug
cmake --build --preset qt6.11-debug
cmake --build --preset qt6.11-debug --target all_qmllint
ctest --test-dir build/qt6.11-debug
```

## First Test Targets

- Done: `PayloadCodec` has initial tests for format mapping, encoding, decoding, topic filter matching, and topic-format resolution.
- Done: `SessionConfig` has initial tests for defaults, bounds checks, protocol version, transport, QoS, keep-alive, and optional integer sanitation.
- Next: `LuaRunner`: successful scripts, missing `parse(ctx)`, instruction limit, unsupported return types, nesting limit, and output length limit.
- Next: `HistoryStore`: append, flush, load latest, load before id, prune, clear, and database-open failure behavior.
- Next: list models: `rowCount`, `data`, `roleNames`, `countChanged`, insert/remove/reset signal behavior, and invalid index handling.

## Quality Follow-Ups

- Replace high-value QML `property var` declarations with typed properties or registered QML/C++ types.
- Cache `roleNames()` hashes in list models that currently rebuild them on each call.
- Add explicit enum underlying types for model role enums.
- Separate durable session configuration from runtime `QObject` pointers in `SessionState`.
- Decide whether QML style-lint noise should be cleaned mechanically or suppressed in favor of behavior-focused review.
