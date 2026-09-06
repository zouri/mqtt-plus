# MQTT Plus

[![Build and package](https://github.com/zouri/mqtt-plus/actions/workflows/build-packages.yml/badge.svg)](https://github.com/zouri/mqtt-plus/actions/workflows/build-packages.yml)

English | [简体中文](README.md)

MQTT Plus is an open-source MQTT desktop client for IoT developers, device engineers, and testing teams. Connect to brokers, explore topics, decode messages, send commands, and revisit history in one local workbench.

**Windows · macOS · Linux | MQTT 5.0 / 3.1.1 | TCP / TLS / WebSocket | English / Chinese**

[Download the latest release](https://github.com/zouri/mqtt-plus/releases/latest) · [Quick start](#downloads-and-quick-start) · [Report an issue](https://github.com/zouri/mqtt-plus/issues)

![MQTT Plus workbench with topic tree, latest message details, MQTT 5 properties, and publish composer](docs/images/mqtt-plus-workbench.png)

*Screenshots show the actual 0.5.0 interface with isolated demo configuration and fictional device data. Connection addresses use localhost only; no external broker was connected.*

## Why MQTT Plus

- **Debug MQTT in one place:** manage multiple connections, browse traffic through the topic tree, inspect messages, and compose publishes in the same workbench.
- **Make payloads easier to understand:** inspect common text and binary formats directly, or bind Lua and JavaScript processors to subscriptions.
- **Keep reproducible debugging context:** store messages, logs, publish drafts, processor revisions, and connection configuration locally.

## What You Can Do

| Workflow | How MQTT Plus helps |
| --- | --- |
| Integrate devices and test commands | Manage multiple broker connections, subscribe to device reports, and publish JSON or binary commands |
| Explore an unfamiliar topic structure | Expand observed topics, inspect recent payloads and activity, and quickly subscribe to a topic or subtree |
| Investigate unexpected message behavior | Inspect message details, QoS, Retain, MQTT 5 properties, and separate runtime logs |
| Decode custom device data | Use built-in codecs or transform decoded results with Lua / JavaScript |
| Repeat a command during testing | Save publish drafts, create drafts from messages, and reuse recent publishes |
| Migrate a debugging setup | Import MQTTX connections or import and export MQTT Plus configuration |

## Downloads and Quick Start

[GitHub Releases](https://github.com/zouri/mqtt-plus/releases) provides installers for the following platforms:

| Platform | Package |
| --- | --- |
| Windows x64 | NSIS installer (`.exe`) |
| Linux x64 | Debian package (`.deb`) and AppImage |
| macOS | Intel x64 and Apple Silicon arm64 (`.dmg`) |

1. Download and install the package for your platform from Releases.
2. Start MQTT Plus, create a connection, and enter the broker address, port, and any required authentication or TLS settings.
3. Connect, add a subscription, and choose its QoS and payload format to start inspecting the message stream. Once messages arrive, use the Topics tab to browse observed topics as a hierarchy.
4. Use the publish composer at the bottom of the workbench to send messages. Bind a message processor when custom parsing is required.

MQTT Plus does not include a broker. You need access to an MQTT broker before getting started.

### Send Your First Message

Use a test broker you are authorized to access and verify a round trip on the same connection:

| Setting | Example |
| --- | --- |
| Subscription topic | `demo/my-device-001/#` |
| Subscription QoS / payload format | `0` / `JSON` |
| Publish topic | `demo/my-device-001/telemetry` |
| Publish QoS / payload format | `0` / `JSON` |
| Retain | Off |

Subscribe first, then send this simulated payload using the publish composer:

```json
{
  "deviceId": "my-device-001",
  "temperature": 23.7,
  "online": true
}
```

The incoming message should appear in the stream. In the Topics tab, expand `demo → my-device-001 → telemetry` to see the topic and latest payload preview. If the MQTT 5 No Local subscription option is enabled, disable it first or publish from a second connection.

The example topic and data are fictional. On shared brokers, replace the prefix with a unique test prefix, send only non-sensitive data, and never publish commands to someone else's topics.

## Features

- MQTT 5.0 and MQTT 3.1.1 over TCP, TLS, WebSocket, and secure WebSocket, with username/password authentication, server certificate verification, and client certificates.
- Last Will messages and MQTT 5 properties, including session expiry, message expiry, content type, response topic, correlation data, and user properties.
- Multiple connection management; QoS 0/1/2 subscriptions and publishing; retained messages, subscription pausing, and message filtering.
- An expandable topic tree built from received traffic, with live activity, latest-payload previews, and quick actions for subscribing to a topic or subtree.
- Plaintext, JSON, Base64, Hex, CBOR, and MsgPack payload encoding and decoding.
- Messages and runtime logs stored separately in SQLite, with pagination, filtering, and cleanup controls.
- Message capture rules for incoming/outgoing traffic and topic include/exclude filters to keep unrelated traffic out of history.
- Publish drafts, recent publish history, and quick draft creation from messages.
- Lua 5.5 and JavaScript message processors that can be bound to subscriptions and retain revision history.
- MQTT Plus configuration import/export and MQTTX connection configuration import.
- English and Simplified Chinese interfaces, with system, light, and dark themes.

## More Screenshots

### Subscriptions and Message Stream

Distinguish device traffic with subscription aliases and colors, inspect JSON messages, and prepare commands in the same workbench. The opening screenshot shows the Topics view: expand the hierarchy and select a node to inspect its latest message, MQTT 5 properties, and decoded result.

![MQTT Plus subscriptions with device groups, JSON message stream, and QoS publish composer](docs/images/mqtt-plus-messages.png)

### Message Processors

Turn device reports into readable results, validate scripts in the editor, and bind them to subscriptions. Revision history preserves changes to the processing logic.

![MQTT Plus message processor with Lua script editing and validation](docs/images/mqtt-plus-processors.png)

### Preferences

Choose the interface language, light or dark theme, accent color, and font, then adjust message expansion and the auto-follow refresh rate.

![MQTT Plus preferences for theme, font, language, and workbench behavior](docs/images/mqtt-plus-settings.png)

## Building from Source

### Requirements

- CMake 3.29+
- A compiler with C++20 support
- Qt 6.11
- Ninja or another CMake generator

Qt must include Concurrent, Core, Gui, Network, Qml, Quick, Quick Controls 2, Sql, Svg, Test, LinguistTools, and WebSockets. During the first configuration, CMake downloads pinned versions of Lua, KSyntaxHighlighting, and Extra CMake Modules. If Qt MQTT is not installed locally, CMake also downloads and builds Qt MQTT 6.11.1.

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

## FAQ

**Why is the topic tree empty?**

The tree is built from received messages, not a directory of every topic on the broker. Subscribe to an authorized topic filter, such as `demo/my-device-001/#`, then send messages from a device or another client. Topics cannot be discovered without traffic.

**Why are there no messages after connecting?**

Check that the subscription succeeded, the published topic matches, the subscription is not paused, and message filters or capture rules are not excluding the traffic. Use runtime logs to investigate connection or permission issues.

**Can I migrate from MQTTX?**

MQTTX connection configuration can be imported. MQTT Plus also supports importing and exporting its own configuration. Check broker addresses, authentication, and certificate paths after migration.

## Local Data and Privacy

- Sessions and preferences are stored with `QSettings`.
- Messages and logs are stored in `QStandardPaths::AppDataLocation/history.db`.
- Drafts and processors are stored under `QStandardPaths::GenericConfigLocation/mqtt_plus/`.

Session passwords are stored in the local `QSettings` store rather than the system credential vault. Configuration exports exclude passwords and certificates by default. Exports that include sensitive data should be treated as private files.

Payloads, topics, logs, and publish drafts may also contain business data. Before sharing screenshots or filing issues, inspect the connection list, broker address, client ID, topics, payloads, certificate paths, and status bar. Use a separate demo configuration with fictional data rather than capturing a production or private test environment. See the [screenshot privacy checklist](docs/images/README.md).

## Roadmap

- [ ] Broker status monitoring dashboard: summarize connection status, uptime, client and subscription counts, message throughput, and resource usage; automatically collect and visualize metrics from `$SYS` topics when the broker provides them.

## Help and Contributing

Use [GitHub Issues](https://github.com/zouri/mqtt-plus/issues) for problems and feature requests. Include the operating system, MQTT Plus version, reproduction steps, and relevant logs when reporting a bug. Remove passwords, certificates, and sensitive configuration data first.

Before submitting a pull request, run the build, `all_qmllint`, and the complete test suite. For UI or MQTT workflow changes, describe the manual verification steps in the pull request and include screenshots for visible UI changes.

MQTT Plus is maintained by [zouri](https://github.com/zouri). Thanks to [all contributors](https://github.com/zouri/mqtt-plus/graphs/contributors).

## License

This project is licensed under the [MIT License](LICENSE).
