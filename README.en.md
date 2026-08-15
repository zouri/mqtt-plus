# MQTT Plus

[![Build and package](https://github.com/zouri/mqtt-plus/actions/workflows/build-packages.yml/badge.svg)](https://github.com/zouri/mqtt-plus/actions/workflows/build-packages.yml)

English | [简体中文](README.md)

MQTT Plus is a cross-platform desktop client for MQTT development, integration testing, and troubleshooting. It brings multiple connections, subscriptions and publishing, persistent message streams, and programmable payload processing into one local workbench.

[Download the latest release](https://github.com/zouri/mqtt-plus/releases/latest) · [Quick start](#downloads-and-quick-start) · [Report an issue](https://github.com/zouri/mqtt-plus/issues)

![MQTT Plus workbench with connections, subscriptions, message stream, and publish composer](docs/images/mqtt-plus-workbench.png)

## Why MQTT Plus

- **Debug MQTT in one place:** manage multiple connections, subscribe to topics, inspect messages, and compose publishes in the same workbench.
- **Make payloads easier to understand:** inspect common text and binary formats directly, or bind Lua and JavaScript processors to subscriptions.
- **Keep reproducible debugging context:** store messages, logs, publish drafts, processor revisions, and connection configuration locally.

## Downloads and Quick Start

[GitHub Releases](https://github.com/zouri/mqtt-plus/releases) provides installers for the following platforms:

| Platform | Package |
| --- | --- |
| Windows x64 | NSIS installer (`.exe`) |
| Linux x64 | Debian package (`.deb`) and AppImage |
| macOS | Intel x64 and Apple Silicon arm64 (`.dmg`) |

1. Download and install the package for your platform from Releases.
2. Start MQTT Plus, create a connection, and enter the broker address, port, and any required authentication or TLS settings.
3. Connect, add a subscription, and choose its QoS and payload format to start inspecting the message stream.
4. Use the publish composer at the bottom of the workbench to send messages. Bind a message processor when custom parsing is required.

MQTT Plus does not include a broker. You need access to an MQTT broker before getting started.

## Features

- MQTT 5.0 and MQTT 3.1.1 over TCP or TLS, with username/password authentication, server certificate verification, and client certificates.
- Multiple connection management; QoS 0/1/2 subscriptions and publishing; retained messages, subscription pausing, and message filtering.
- Plaintext, JSON, Base64, Hex, CBOR, and MsgPack payload encoding and decoding.
- Messages and runtime logs stored separately in SQLite, with pagination, filtering, and cleanup controls.
- Publish drafts, recent publish history, and quick draft creation from messages.
- Lua 5.5 and JavaScript message processors that can be bound to subscriptions and retain revision history.
- MQTT Plus configuration import/export and MQTTX connection configuration import.
- English and Simplified Chinese interfaces, with system, light, and dark themes.

## More Screenshots

### Message Processors

![MQTT Plus message processor with Lua script editing and validation](docs/images/mqtt-plus-processors.png)

### Preferences

![MQTT Plus preferences for theme, font, language, and workbench behavior](docs/images/mqtt-plus-settings.png)

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

Processors are bound to subscriptions and receive decoded message data in the background. Their results update the stored message and the interface when processing completes. The entry point is always `process(context)`:

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

## Help and Contributing

Use [GitHub Issues](https://github.com/zouri/mqtt-plus/issues) for problems and feature requests. Include the operating system, MQTT Plus version, reproduction steps, and relevant logs when reporting a bug. Remove passwords, certificates, and sensitive configuration data first.

Before submitting a pull request, run the build, `all_qmllint`, and the complete test suite. For UI or MQTT workflow changes, describe the manual verification steps in the pull request and include screenshots for visible UI changes.

MQTT Plus is maintained by [zouri](https://github.com/zouri). Thanks to [all contributors](https://github.com/zouri/mqtt-plus/graphs/contributors).

## License

This project is licensed under the [MIT License](LICENSE).
