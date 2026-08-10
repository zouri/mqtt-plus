# Repository Guidelines

## Project Structure & Module Organization
`src/` contains the C++20 application. Keep domain types in `src/domain/`, orchestration in `src/usecases/`, infrastructure in `src/services/`, Qt models in `src/models/`, QML-facing state in `src/viewmodels/`, and startup/composition in `src/app/`. The Qt Quick UI lives in `qml/`: shared controls belong in `qml/components/`, while page-specific views belong in `qml/features/`. Tests are in `tests/`; translations, icons, and bundled resources are in `i18n/`, `assets/`, and `resources/`. Record significant design choices in `docs/adr/`. Treat `build/` and `dist/` as generated output.

## Build, Test, and Development Commands
Use Qt 6.11, CMake, and Ninja with the checked-in macOS preset:

```bash
cmake --preset qt6.11-debug
cmake --build --preset qt6.11-debug
cmake --build --preset qt6.11-debug --target all_qmllint
ctest --test-dir build/qt6.11-debug --output-on-failure
```

The commands configure, build, lint application QML, and run all registered Qt Test executables. After building, launch the macOS app with `./build/qt6.11-debug/mqtt_plus_app.app/Contents/MacOS/mqtt_plus_app`. Platform release scripts live in `scripts/package-{macos,linux}.*` and `scripts/package-windows.ps1`.

## Coding Style & Naming Conventions
Match nearby Qt code: use 4 spaces, no tabs, and C++ opening braces on the next line. Classes and QML components use `PascalCase`; functions, properties, signals, and QML `id` values use `camelCase`; private C++ members use the `m_` prefix. Keep C++ filenames lowercase (`historystore.cpp`) and QML component filenames descriptive (`MessageInspector.qml`). Prefer Qt value types, signals/slots, and `QStringLiteral` where existing code does. No repository-wide C++ formatter is configured, so preserve local formatting and run `all_qmllint` for QML changes.

## Testing Guidelines
Tests use Qt Test and follow `tests/test_<subject>.cpp`. Add new executables and `add_test` registrations in `CMakeLists.txt`; name test classes `<Subject>Test` and test slots after behavior, such as `rejectsInvalidSessionIndexes()`. Run the full suite before submitting. For UI or MQTT workflow changes, also manually verify the affected connect, subscribe, publish, persistence, or editor path.

## Commit & Pull Request Guidelines
Recent commits favor short, imperative subjects such as `Fix release packaging workflow` and `Add QoS 2 support`. Keep each commit focused. Pull requests should explain the behavior and motivation, link relevant issues, list validation commands, and call out platform-specific effects. Include before/after screenshots for visible `qml/` changes and update translations or ADRs when user-facing text or architecture changes.

## Configuration & Security
Keep local Qt paths in uncommitted `CMakeUserPresets.json` or pass `CMAKE_PREFIX_PATH`. Do not commit broker credentials, generated packages, local settings, or SQLite history databases.
