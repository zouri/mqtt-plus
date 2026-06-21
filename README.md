# mqtt-plus

`mqtt-plus` 是一个基于 Qt/QML 的桌面 MQTT 客户端，目标是提供类似 MQTTX 的使用体验，并补充多会话、历史记录、载荷格式转换和 Lua 接收脚本能力。

## 功能特性

- 通过 Qt MQTT 连接 MQTT Broker，支持 TCP/TLS、用户名密码、Keep Alive 和 Clean Session。
- 默认使用 MQTT 5，会话级别可切换到 MQTT 3.1.1。
- 支持多会话管理：创建、复制、重命名、删除和切换会话。
- 支持订阅、取消订阅和暂停订阅输出。
- 订阅运行状态可见：`pending`、`subscribed`、`error`、`paused`。
- 支持发布消息，并跟踪 QoS 0/1 的发送状态。
- 统一事件流展示 Broker 生命周期事件、发布结果、订阅消息和系统提示。
- 事件流可暂停显示；暂停期间收到的 MQTT 消息仍会写入历史。
- 支持按订阅配置载荷格式：
  - Plaintext
  - JSON
  - Base64
  - Hex
  - CBOR
  - MsgPack
- 内置 Lua 接收脚本库，可为不同订阅选择不同解析脚本。
- 会话配置通过 `QSettings` 持久化。
- 消息和系统事件通过 SQLite 按会话持久化到 `history.db`。
- 支持浅色、深色和跟随系统主题。

## 技术栈

- C++20
- Qt 6.11
- Qt Quick / Qt Quick Controls / QML
- Qt MQTT (`Qt6::Mqtt`)
- Qt SQL / SQLite
- Lua 5.5 静态编译运行时
- CMake / CPack

## 项目结构

```text
.
├── src/                    # C++ 应用逻辑
│   ├── main.cpp            # Qt/QML 启动入口
│   ├── app/                # QML 门面和应用级协调逻辑
│   ├── controllers/        # 会话、MQTT、订阅、事件、脚本等控制器
│   ├── domain/             # 会话、订阅和脚本领域数据结构
│   ├── models/             # 暴露给 QML 的列表模型
│   ├── presentation/       # 事件流行渲染
│   └── services/           # SQLite、载荷编解码、Lua 脚本等服务
├── qml/                    # Qt Quick 界面
│   ├── Main.qml            # 主窗口
│   ├── features/           # 工作区、历史、脚本、设置等功能视图与组件
│   ├── features/           # 业务功能组件
│   └── components/         # 复用 UI 组件
├── assets/                 # 应用图标等资源
├── scripts/                # 平台打包脚本
├── CMakeLists.txt          # CMake 工程定义
└── CMakePresets.json       # 本地构建和打包 preset
```

`build/` 和 `dist/` 是生成目录，不需要手动编辑或提交其中产物。

## 环境要求

- CMake 3.24 或更高版本。
- Qt 6.11，并安装以下模块：
  - Core
  - Gui
  - Network
  - Qml
  - Quick
  - QuickControls2
  - Sql
  - Mqtt
  - Svg
  - LinguistTools
- 通过 Qt Creator Kit、Qt 自带的 `qt-cmake`，或本机专用的 `CMakeUserPresets.json` 指定 Qt 安装路径。

项目会在 CMake configure 阶段通过 `FetchContent` 从 `lua.org` 下载固定版本的 Lua 源码，并编译为项目内的静态库 `mqtt_plus_lua`。如果本机 Qt 没有安装 `Qt6Mqtt`，或 configure 阶段无法下载 Lua 源码，配置会失败。

如需在命令行固定本机 Qt 路径，可以传入 `-DCMAKE_PREFIX_PATH=/path/to/Qt`，或创建不提交到仓库的 `CMakeUserPresets.json`：

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "qt6.11-local",
      "inherits": "qt6.11-windows-release",
      "cacheVariables": {
        "CMAKE_PREFIX_PATH": "C:/Qt/6.11.1/msvc2022_64"
      }
    }
  ]
}
```

## 构建

macOS 调试构建：

```bash
cd /path/to/mqtts
cmake --preset qt6.11-debug
cmake --build --preset qt6.11-debug
cmake --build --preset qt6.11-debug --target all_qmllint
```

`all_qmllint` 用于检查 QML 文件，建议在提交 UI 变更前运行。

## 多语言

应用当前支持英文和简体中文。英文是源码基线，简体中文翻译文件位于 `i18n/mqtt_plus_zh_CN.ts`，CMake 构建会生成并打包对应的 `.qm` 资源。

更新翻译条目：

```bash
lupdate src qml -ts i18n/mqtt_plus_zh_CN.ts
```

发布翻译随正常构建生成：

```bash
cmake --build --preset qt6.11-debug
```

## 运行

macOS 调试构建完成后，可以直接运行 app bundle 内的可执行文件：

```bash
./build/qt6.11-debug/mqtt_plus_app.app/Contents/MacOS/mqtt_plus_app
```

## 打包

打包需要在目标平台本机执行。平台 preset 只会在对应系统上出现。
项目通过 Qt 官方 CMake Deployment API 生成部署脚本：CMake 先安装应用目标，再由 `qt_generate_deploy_qml_app_script()` 收集 Qt 运行库、QML 模块和需要的插件，最后交给 CPack 生成平台包。

### macOS

```bash
cd /path/to/mqtts
./scripts/package-macos.sh /path/to/Qt/6.11.x/macos
```

也可以通过环境变量指定 Qt 路径：

```bash
QT_PREFIX=/path/to/Qt/6.11.x/macos ./scripts/package-macos.sh
```

生成的 DMG 会写入 `dist/`。

### Linux

```bash
cd /path/to/mqtts
./scripts/package-linux.sh /opt/Qt/6.11.x/gcc_64
```

生成的 tarball 会写入 `dist/`。

### Windows

在 Visual Studio 2022 Developer PowerShell 或 x64 Native Tools Prompt 中执行：

```powershell
cd C:\path\to\mqtts
.\scripts\package-windows.ps1
```

指定 Qt 路径：

```powershell
.\scripts\package-windows.ps1 -QtPrefix C:/Qt/6.11.x/msvc2022_64
```

生成的 ZIP 会写入 `dist/`。

## Lua 接收脚本

接收脚本用于在消息进入事件流和历史记录前，对订阅消息进行解析或转换。每个脚本必须定义 `parse(ctx)` 函数：

```lua
function parse(ctx)
    return ctx.decoded
end
```

`ctx` 是应用传入脚本的消息上下文：

| 字段 | 说明 |
| --- | --- |
| `ctx.topic` | 收到消息的 MQTT Topic。 |
| `ctx.payload` | 原始 payload 字节，作为 Lua 字符串传入。 |
| `ctx.payloadBase64` | 原始 payload 的 Base64 文本。 |
| `ctx.payloadHex` | 原始 payload 的十六进制文本。 |
| `ctx.decoded` | 按订阅载荷格式解码后的内容。 |
| `ctx.decodeError` | 解码错误；成功时为空字符串。 |
| `ctx.format` | 订阅配置的载荷格式名，例如 `Plaintext` 或 `JSON`。 |
| `ctx.timestamp` | 应用生成的接收时间戳。 |

`parse(ctx)` 可以返回 `nil`、布尔值、数字、字符串或 Lua table。table 会被渲染为格式化 JSON。table 的 key 必须是字符串或数字，数组 table 需要使用连续的 1-based 整数下标，并且嵌套层级有限制。

示例：

```lua
function parse(ctx)
    if ctx.decodeError ~= "" then
        return {
            error = ctx.decodeError,
            raw = ctx.payloadHex
        }
    end

    return ctx.decoded
end
```

```lua
function parse(ctx)
    return {
        topic = ctx.topic,
        format = ctx.format,
        value = ctx.decoded,
        receivedAt = ctx.timestamp
    }
end
```

脚本运行在受限 Lua 环境中。应用开放 base、string、math 和 table 库，但禁用了文件加载、动态加载、`print` 和 `collectgarbage`。长时间运行的脚本会被指令数限制中断，过大的输出也会被拒绝。

脚本文件保存为 `.lua`，位于 Qt 的 `QStandardPaths::GenericConfigLocation` 下：

| 平台 | 目录 |
| --- | --- |
| Linux | `~/.config/mqtt_plus/scripts` |
| macOS | `~/Library/Preferences/mqtt_plus/scripts` |
| Windows | `%LOCALAPPDATA%\mqtt_plus\scripts` |

同目录下的 `index.json` 会保存脚本 id、显示名称、文件名和更新时间。

## 本地数据

- 会话配置：通过 `QSettings` 保存到系统标准配置位置。
- Lua 脚本：保存到 `mqtt_plus/scripts`。
- 历史记录：保存到 `history.db`，位于 `QStandardPaths::AppDataLocation`。

这些数据是本机运行时产物，不应提交到仓库。

当前版本会随会话配置保存 MQTT 用户名和密码。开源发布前请确认凭据策略：

- 若继续使用 `QSettings`，需要在发布说明和安全文档中明确本机凭据保存方式。
- 若希望默认更安全，建议接入 QtKeychain 或平台凭据存储，例如 macOS Keychain、Linux Secret Service、Windows Credential Manager。

## 开源发布准备

发布到 GitHub 前请先确认：

- 已选择并添加项目开源许可证。
- 已补充第三方依赖和资源许可说明，参见 `THIRD_PARTY_NOTICES.md`。
- 已阅读开源前检查清单，参见 `docs/OPEN_SOURCE_CHECKLIST.md`。
- 已确认安全说明中的凭据、Lua 脚本和本地数据策略，参见 `docs/SECURITY_NOTES.md`。

## 验证建议

当前工程已注册 Qt Test 单元测试。提交变更前建议至少执行：

```bash
cmake --build --preset qt6.11-debug
cmake --build --preset qt6.11-debug --target all_qmllint
ctest --test-dir build/qt6.11-debug --output-on-failure
```

涉及 MQTT 连接、界面交互或历史记录行为的变更，还应手动验证以下流程：

- 创建或编辑会话。
- 连接和断开 Broker。
- 添加订阅并接收消息。
- 发布不同格式的 payload。
- 重启后确认会话和历史记录仍然存在。
- 为订阅选择 Lua 脚本并确认解析结果。

## 贡献提示

- C++ 使用 4 空格缩进，类名使用 `PascalCase`，方法和 QML 属性使用 `camelCase`，私有成员以 `m_` 开头。
- QML `id` 保持简短清晰，例如 `sessionList`、`statusLabel`。
- 保持 PR 聚焦，说明用户可见变化和验证步骤。
- UI 变更建议附带截图。
