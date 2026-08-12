# MQTT Plus

[![Build and package](https://github.com/zouri/mqtt-plus/actions/workflows/build-packages.yml/badge.svg)](https://github.com/zouri/mqtt-plus/actions/workflows/build-packages.yml)

English | [简体中文](README.md)

MQTT Plus is a cross-platform desktop MQTT client built with Qt Quick.

[Download the latest release](https://github.com/zouri/mqtt-plus/releases/latest) · [Report an issue](https://github.com/zouri/mqtt-plus/issues)

## Screenshots

### MQTT Workbench

![MQTT Plus workbench with connections, subscriptions, message stream, and publish composer](docs/images/mqtt-plus-workbench.png)

### Message Processors

![MQTT Plus message processor with Lua script editing and validation](docs/images/mqtt-plus-processors.png)

### Preferences

![MQTT Plus preferences for theme, font, language, and workbench behavior](docs/images/mqtt-plus-settings.png)

## Features

- MQTT 5.0 and MQTT 3.1.1 over TCP or TLS, with username/password authentication, server certificate verification, and client certificates.
- Multiple connection management; QoS 0/1/2 subscriptions and publishing; retained messages, subscription pausing, and message filtering.
- Plaintext, JSON, Base64, Hex, CBOR, and MsgPack payload encoding and decoding.
- Messages and runtime logs stored separately in SQLite, with pagination, filtering, and cleanup controls.
- Publish drafts, recent publish history, and quick draft creation from messages.
- Lua 5.5 and JavaScript message processors that can be bound to subscriptions and retain revision history.
- MQTT Plus configuration import/export and MQTTX connection configuration import.
- English and Simplified Chinese interfaces, with system, light, and dark themes.

## Downloads

[GitHub Releases](https://github.com/zouri/mqtt-plus/releases) provides installers for the following platforms:

| Platform | Package |
| --- | --- |
| Windows x64 | NSIS installer (`.exe`) |
| Linux x64 | Debian package (`.deb`) and AppImage |
| macOS | Intel x64 and Apple Silicon arm64 (`.dmg`) |

## Building from Source

### Requirements

- CMake 3.29+
- A compiler with C++20 support
- Qt 6.11
- Ninja or another CMake generator

Qt must include Concurrent, Core, Gui, Network, Qml, Quick, Quick Controls 2, Sql, Svg, Test, and LinguistTools. During the first configuration, CMake downloads pinned versions of Lua, KSyntaxHighlighting, and Extra CMake Modules. If Qt MQTT is not installed locally, CMake also downloads and builds Qt MQTT 6.11.1.

### Configure and Build

```bash
cmake -S . -B build/dev -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.11.x/toolchain
cmake --build build/dev --parallel
```

On macOS, you can also use the repository preset:

```bash
cmake --preset qt6.11-debug -DCMAKE_PREFIX_PATH=/path/to/Qt/6.11.x/macos
cmake --build --preset qt6.11-debug
```

### Checks

```bash
cmake --build build/dev --target all_qmllint
ctest --test-dir build/dev --output-on-failure
```

When using the preset, replace `build/dev` above with `build/qt6.11-debug`.

## Packaging

The packaging scripts perform a Release build, run QML lint, and write the artifacts to `dist/`.

```bash
# macOS: arm64 or x86_64
./scripts/package-macos.sh /path/to/Qt/6.11.x/macos arm64

# Linux x64
./scripts/package-linux.sh /path/to/Qt/6.11.x/gcc_64
```

Windows requires NSIS. Run the following command in Developer PowerShell:

```powershell
.\scripts\package-windows.ps1 -QtPrefix C:/Qt/6.11.x/msvc2022_64
```

## Message Processors

Processors are bound to subscriptions and receive decoded results before messages are written to history and displayed in the interface. The entry point is always `process(context)`:

```lua
function process(context)
    return {
        topic = context.topic,
        value = context.decoded
    }
end
```

```javascript
function process(context) {
    return {
        topic: context.topic,
        value: context.decoded
    };
}
```

`context` contains `topic`, `payload`, `receivedAt`, `format`, `decoded`, `decodeError`, and `parameters`. Processors run in a restricted runtime with limits on execution time, output size, and nesting depth.

## Project Structure

```text
src/domain/       Domain types
src/usecases/     Application use cases and orchestration
src/services/     MQTT, storage, codecs, and processor runtimes
src/models/       Qt list models
src/viewmodels/   QML-facing view models
src/app/          Startup and dependency composition
qml/components/   Shared QML components
qml/features/     Feature views
tests/            Qt Test suites
docs/adr/         Architecture decision records
```

## Local Data

- Sessions and preferences are stored with `QSettings`.
- Messages and logs are stored in `QStandardPaths::AppDataLocation/history.db`.
- Drafts and processors are stored under `QStandardPaths::GenericConfigLocation/mqtt_plus/`.

Session passwords are stored in the local `QSettings` store rather than the system credential vault. Configuration exports exclude passwords and certificates by default. Exports that include sensitive data should be treated as private files.

## Roadmap

- [ ] MQTT topic tree: build an expandable hierarchy from observed topics, with search, quick subscriptions, and the latest message and activity state for each node.
- [ ] Broker status monitoring dashboard: summarize connection status, uptime, client and subscription counts, message throughput, and resource usage; automatically collect and visualize metrics from `$SYS` topics when the broker provides them.

## Contributing

Before submitting a pull request, run the build, `all_qmllint`, and the complete test suite. For UI or MQTT workflow changes, describe the manual verification steps in the pull request and include screenshots for visible UI changes.

## License

This project is licensed under the [MIT License](LICENSE).
