# MQTT Plus

[![Build and package](https://github.com/zouri/mqtt-plus/actions/workflows/build-packages.yml/badge.svg)](https://github.com/zouri/mqtt-plus/actions/workflows/build-packages.yml)

[English](README.en.md) | 简体中文

MQTT Plus 是面向物联网开发者、设备工程师和测试团队的开源 MQTT 桌面客户端。连接 Broker、浏览主题、解析消息、发送指令、回查历史，在一个本地工作台中完成日常联调。

**Windows · macOS · Linux | MQTT 5.0 / 3.1.1 | TCP / TLS / WebSocket | 中文 / English**

[下载最新版本](https://github.com/zouri/mqtt-plus/releases/latest) · [快速开始](#下载与快速开始) · [提交问题](https://github.com/zouri/mqtt-plus/issues)

![MQTT Plus 工作台：主题树、最新消息详情、MQTT 5 属性与发布编辑器](docs/images/mqtt-plus-workbench.png)

*截图来自 0.5.0 版本的真实界面，使用隔离的演示配置与虚构设备数据；连接地址仅为 localhost，未连接外部 Broker。*

## 为什么选择 MQTT Plus

- **集中完成 MQTT 调试**：在同一个工作台中管理多个连接，通过主题树浏览流量、检查消息并构造发布请求。
- **让载荷更容易理解**：直接查看常用文本和二进制格式，也可以为订阅绑定 Lua 或 JavaScript 处理器。
- **保留可复现的调试上下文**：本地保存消息、日志、发布草稿、处理器版本和连接配置。

## 你可以用它做什么

| 使用场景 | 对应能力 |
| --- | --- |
| 设备接入与指令联调 | 管理多个 Broker 连接，订阅设备上报，发送 JSON 或二进制指令 |
| 梳理陌生项目的主题结构 | 按层级展开已收到的主题，查看最新载荷和活动，快速订阅某个主题或子树 |
| 排查“消息发了，但结果不对” | 结合消息详情、QoS、Retain、MQTT 5 属性和独立运行日志检查问题 |
| 解析设备自定义数据 | 选择内置编解码格式，或用 Lua / JavaScript 转换解码结果 |
| 反复验证同一条指令 | 保存发布草稿，从历史消息创建草稿，复用最近发布记录 |
| 迁移已有调试环境 | 导入 MQTTX 连接，导入或导出 MQTT Plus 配置 |

## 下载与快速开始

[GitHub Releases](https://github.com/zouri/mqtt-plus/releases) 提供以下安装包：

| 平台 | 安装包 |
| --- | --- |
| Windows x64 | NSIS 安装程序（`.exe`） |
| Linux x64 | Debian 包（`.deb`）和 AppImage |
| macOS | Intel x64 和 Apple Silicon arm64（`.dmg`） |

1. 从 Releases 下载并安装适合当前平台的软件包。
2. 启动 MQTT Plus，新建连接并填写 Broker 地址、端口，以及需要的认证或 TLS 设置。
3. 连接后添加订阅，选择 QoS 和载荷格式，即可查看消息流；接收消息后也可在“主题”页按层级浏览已发现的主题。
4. 使用底部发布编辑器发送消息；需要自定义解析时，为订阅绑定消息处理器。

MQTT Plus 不包含内置 Broker，开始前需要一个可以访问的 MQTT Broker。

### 收发第一条消息

在你有权限使用的测试 Broker 上，用同一个连接完成一次收发验证：

| 设置 | 示例 |
| --- | --- |
| 订阅主题 | `demo/my-device-001/#` |
| 订阅 QoS / 载荷格式 | `0` / `JSON` |
| 发布主题 | `demo/my-device-001/telemetry` |
| 发布 QoS / 载荷格式 | `0` / `JSON` |
| Retain | 关闭 |

先订阅，再在发布编辑器中发送以下模拟数据：

```json
{
  "deviceId": "my-device-001",
  "temperature": 23.7,
  "online": true
}
```

消息流中应出现收到的消息；在“主题”页展开 `demo → my-device-001 → telemetry`，即可看到主题和最新载荷预览。若启用了 MQTT 5 的 No Local 订阅选项，请先关闭它，或使用第二个连接发布。

示例主题和数据均为虚构。共享 Broker 上请替换成自己的唯一测试前缀，只发送非敏感数据，不要向他人的主题发布指令。

## 功能

- MQTT 5.0 和 MQTT 3.1.1，支持 TCP、TLS、WebSocket 和安全 WebSocket，支持用户名密码、服务端证书校验和客户端证书。
- 遗嘱消息及 MQTT 5 属性，包括会话过期、消息过期、内容类型、响应主题、关联数据和用户属性。
- 多连接管理；QoS 0/1/2 订阅与发布；支持 Retain、订阅暂停和消息筛选。
- 根据接收流量构建可展开的主题树，展示实时活动和最新载荷预览，并支持快速订阅主题或子树。
- Plaintext、JSON、Base64、Hex、CBOR、MsgPack 载荷编解码。
- 消息与运行日志分开保存到 SQLite，可分页查看、筛选和清理。
- 按收发方向和主题包含 / 排除规则控制消息采集，减少无关流量进入历史记录。
- 发布草稿、最近发布记录，以及从消息快速创建草稿。
- Lua 5.5 和 JavaScript 消息处理器，可按订阅绑定并保留版本记录。
- MQTT Plus 配置导入/导出，以及 MQTTX 连接配置导入。
- 英文和简体中文界面；支持系统、浅色和深色主题。

## 更多界面预览

### 订阅与消息流

通过订阅别名和颜色区分设备流量，在同一工作台查看 JSON 消息并准备发布指令。首页配图则展示“主题”视图：展开主题层级，选择节点检查最新消息、MQTT 5 属性和解析结果。

![MQTT Plus 订阅视图：设备分组、JSON 消息流与 QoS 发布编辑器](docs/images/mqtt-plus-messages.png)

### 消息处理器

把设备上报转换成更便于阅读的结果，在编辑器中验证脚本，再绑定到订阅；版本记录用于保留处理逻辑的演进。

![MQTT Plus 消息处理器：Lua 脚本编辑与验证](docs/images/mqtt-plus-processors.png)

### 个性化设置

选择中英文界面、浅色或深色主题、强调色和字体，并调整消息展开方式与自动跟随刷新率。

![MQTT Plus 设置：主题、字体、语言与工作台选项](docs/images/mqtt-plus-settings.png)

## 从源码构建

### 依赖

- CMake 3.29+
- 支持 C++20 的编译器
- Qt 6.11
- Ninja 或其他 CMake 生成器

Qt 需要包含 Concurrent、Core、Gui、Network、Qml、Quick、Quick Controls 2、Sql、Svg、Test、LinguistTools 和 WebSockets。首次配置时，CMake 会下载固定版本的 Lua、KSyntaxHighlighting 和 Extra CMake Modules；如果本机没有 Qt MQTT，还会下载并构建 Qt MQTT 6.11.1。

### 配置与编译

```bash
cmake -S . -B build/dev -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.11.x/toolchain
cmake --build build/dev --parallel
```

macOS 也可以使用仓库内的 preset：

```bash
cmake --preset qt6.11-debug -DCMAKE_PREFIX_PATH=/path/to/Qt/6.11.x/macos
cmake --build --preset qt6.11-debug
```

### 检查

```bash
cmake --build build/dev --target all_qmllint
ctest --test-dir build/dev --output-on-failure
```

使用 preset 构建时，将上述 `build/dev` 替换为 `build/qt6.11-debug`。

## 打包

打包脚本会执行 Release 构建、QML lint，并将产物写入 `dist/`。

```bash
# macOS: arm64 或 x86_64
./scripts/package-macos.sh /path/to/Qt/6.11.x/macos arm64

# Linux x64
./scripts/package-linux.sh /path/to/Qt/6.11.x/gcc_64
```

Windows 需要 NSIS，在 Developer PowerShell 中运行：

```powershell
.\scripts\package-windows.ps1 -QtPrefix C:/Qt/6.11.x/msvc2022_64
```

## 消息处理器

处理器绑定到订阅，在后台接收消息的解码结果；处理完成后，消息历史和界面中的解析结果会随之更新。入口函数固定为 `process(context)`：

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

`context` 包含 `topic`、`payload`、`receivedAt`、`format`、`decoded`、`decodeError` 和 `parameters`。处理器在受限运行时中执行，并受执行时间、输出大小和嵌套深度限制。

## 项目结构

```text
src/domain/       领域类型
src/usecases/     应用用例与流程编排
src/services/     MQTT、存储、编解码和处理器运行时
src/models/       Qt 列表模型
src/viewmodels/   QML ViewModel
src/app/          启动与对象装配
qml/components/   通用 QML 组件
qml/features/     功能页面
tests/            Qt Test 测试
docs/adr/         架构决策记录
```

## 常见问题

**主题树为什么是空的？**

主题树来自实际收到的消息，不是 Broker 全部主题的目录。先订阅有权限访问的主题过滤器（例如 `demo/my-device-001/#`），再让设备或另一个客户端发送消息。没有流量时，不会凭空发现主题。

**已经连接，为什么看不到消息？**

检查订阅是否成功、发布主题是否匹配、订阅是否暂停，以及消息筛选和采集规则是否排除了目标消息。连接或权限问题可结合运行日志排查。

**可以从 MQTTX 迁移吗？**

可以导入 MQTTX 连接配置。MQTT Plus 也支持自身配置的导入与导出；迁移后请核对 Broker 地址、认证和证书路径。

## 本地数据与隐私

- 会话和偏好设置由 `QSettings` 保存。
- 消息与日志保存在 `QStandardPaths::AppDataLocation/history.db`。
- 草稿和处理器保存在 `QStandardPaths::GenericConfigLocation/mqtt_plus/`。

会话密码保存在本机 `QSettings` 中，不使用系统凭据库。配置导出默认不包含密码和证书；选择导出敏感数据后，应将导出文件视为私密文件。

消息载荷、主题、日志和发布草稿也可能包含业务数据。分享截图或提交 Issue 前，请检查连接列表、Broker 地址、客户端 ID、主题、载荷、证书路径和状态栏；建议使用独立的演示配置与虚构数据，不要直接截取生产或私人测试环境。文档配图规范见 [截图与隐私检查](docs/images/README.md)。

## 下一步工作计划

- [ ] Broker 状态信息监控面板：汇总连接状态、运行时间、客户端与订阅数量、消息吞吐量及资源使用情况；在 Broker 提供 `$SYS` 主题时自动采集并可视化相关指标。

## 获取帮助与参与贡献

遇到问题或希望提出功能建议，请使用 [GitHub Issues](https://github.com/zouri/mqtt-plus/issues)。报告问题时建议附上操作系统、MQTT Plus 版本、复现步骤和相关日志；请先移除密码、证书和包含敏感数据的配置。

提交 PR 前请运行构建、`all_qmllint` 和完整测试。涉及界面或 MQTT 流程的变更，请在 PR 中说明手动验证步骤；界面变更请附截图。

项目由 [zouri](https://github.com/zouri) 维护，感谢 [所有贡献者](https://github.com/zouri/mqtt-plus/graphs/contributors)。

## 许可证

本项目基于 [MIT License](LICENSE) 发布。
