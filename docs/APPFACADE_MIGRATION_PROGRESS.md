# AppFacade Migration Progress

Last updated: 2026-06-22

## Goal

Migrate `AppFacade` from a central QML/API god object into a root composer with domain-specific facades, then remove controller reverse dependencies on `AppFacade`.

The migration intentionally preserves user-visible behavior, settings keys, history schema, and QML model roles.

## Completed

- Added QML domain entrypoints on root `AppFacade`:
  - `workbench`
  - `settings`
  - `scriptLibrary`
  - `logStream`
- Added domain facade classes:
  - `src/app/workbenchfacade.*`
  - `src/app/appsettingsfacade.*`
  - `src/app/scriptlibraryfacade.*`
  - `src/app/logstreamfacade.*`
- Migrated QML away from old root `appController` usage.
  - `Main.qml` now extracts the four child facades from `root.app`.
  - Workbench QML uses `workbench`, with `scriptLibrary` passed only where script data is needed.
  - Logs, scripts, and settings views use their matching child facade.
- Removed old root `AppFacade` QML compatibility surface.
  - Old root `Q_PROPERTY` entries are gone.
  - Old root `Q_INVOKABLE` methods are gone.
  - Test coverage verifies old root properties/invokables are not exposed through the meta-object.
- Removed old root forwarding implementation files:
  - `src/app/appfacademenus.cpp`
  - `src/app/appfacadepreferences.cpp`
  - `src/app/appfacadetheme.cpp`
- Moved domain behavior into child facades instead of root forwarding methods.
- Decoupled these controllers from `AppFacade` using injected `Dependencies` structs:
  - `SessionController`
  - `SubscriptionController`
  - `MqttController`
  - `EventController`
- Removed `friend class` declarations for:
  - `SessionController`
  - `SubscriptionController`
  - `MqttController`
  - `EventController`
  - `AppSettingsFacade`
  - `LogStreamFacade`
  - `ScriptLibraryFacade`
  - `WorkbenchFacade`
- Added facade contract test:
  - `tests/test_appfacadefacades.cpp`
- Removed unused root event forwarding helpers after the controller split.
- Replaced child facade direct `AppFacade` private access with injected `Dependencies` structs.
- Moved child facade ownership in `AppFacade` to `std::unique_ptr`.

## Current State

`AppFacade` currently exposes only the four child facade QML properties.

There are no remaining controller or child facade reverse dependencies on `AppFacade`.
`EventController` now receives current/session lookup, history storage, event stream models,
preference values, launch timestamp, script/subscription lookup, FPS refresh coordination,
and UI signal callbacks through its `Dependencies` struct.
The child facades now receive only the controllers, models, and callbacks they need through
their own `Dependencies` structs.

`AppFacade` owns child facades through `std::unique_ptr`. The child facade headers remain
included by `appfacade.h` because Qt 6 MOC requires complete types for the root
`Q_PROPERTY(Facade*)` declarations.

## Last Verified Green

The following validation commands have passed during this migration:

```bash
cmake --build --preset qt6.11-debug
cmake --build --preset qt6.11-debug --target all_qmllint
ctest --test-dir build/qt6.11-debug --output-on-failure
```

Latest reported result: `3/3 tests passed`.

## Next Steps

No known migration tasks remain. Future cleanup can continue splitting `AppFacade` internals
into smaller composer/service objects if new feature work makes that useful.
