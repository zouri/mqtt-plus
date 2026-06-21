# Repository Guidelines

## Project Structure & Module Organization
`src/` contains the C++20 application logic. `src/app/main.cpp` boots Qt/QML, `src/app/appfacade.*` exposes the app state to QML, `src/controllers/` coordinates sessions, MQTT, subscriptions, events, scripts, theme, and language, while `src/services/` contains storage, payload, and scripting services. `qml/` contains the UI entry point in `Main.qml`, feature views and components in `qml/features/`, and reusable controls in `qml/components/`. `build/qt6.11-debug/` is generated output from CMake and should not be edited by hand.

## Build, Test, and Development Commands
Use the checked-in CMake preset for local work:

```bash
cmake --preset qt6.11-debug
cmake --build --preset qt6.11-debug
cmake --build --preset qt6.11-debug --target all_qmllint
ctest --test-dir build/qt6.11-debug --output-on-failure
```

The first command configures the project against Qt 6.11. The second builds `mqtt_plus_app` and test targets. The `all_qmllint` target catches QML issues early, and `ctest` runs the registered Qt Test unit tests. Run the macOS app bundle directly for manual testing:

```bash
./build/qt6.11-debug/mqtt_plus_app.app/Contents/MacOS/mqtt_plus_app
```

## Coding Style & Naming Conventions
Follow the existing Qt style: 4-space indentation, opening braces on the next line in C++, and no tabs. Use `PascalCase` for C++ classes, `camelCase` for methods and QML properties, and keep private member fields prefixed with `m_`. Source filenames are lowercase, matching the current pattern such as `appfacade.cpp` and `historystore.h`. Keep QML `id` values short and descriptive, such as `sessionList` or `statusLabel`.

## Testing Guidelines
Automated tests live under `tests/` and are registered in CMake so they run through `ctest`. Contributors should:
- run a debug build successfully,
- run `all_qmllint`,
- run `ctest --test-dir build/qt6.11-debug --output-on-failure`,
- manually verify connect, subscribe, publish, and history persistence flows.

When adding tests, keep them under `tests/` and register them in CMake.

## Commit & Pull Request Guidelines
This workspace does not include `.git` history, so no house style can be inferred from prior commits. Use short imperative commit subjects such as `Add session duplication action` or `Fix TLS reconnect handling`. Keep pull requests focused, describe user-visible behavior changes, list validation steps, and include screenshots for `qml/` UI changes.

## Security & Configuration Tips
Use `CMAKE_PREFIX_PATH`, `CMakeUserPresets.json`, or the platform packaging scripts to point CMake at the local Qt install. Runtime settings and `history.db` are stored through Qt system paths, so avoid committing machine-local data or generated build artifacts.
