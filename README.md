# MQTT Plus

[![Build and package](https://github.com/zouri/mqtt-plus/actions/workflows/build-packages.yml/badge.svg)](https://github.com/zouri/mqtt-plus/actions/workflows/build-packages.yml)

MQTT Plus 是一个使用 Qt Quick 构建的跨平台桌面 MQTT 客户端。

[下载最新版本](https://github.com/zouri/mqtt-plus/releases/latest) · [提交问题](https://github.com/zouri/mqtt-plus/issues)

## 功能

- MQTT 5.0 和 MQTT 3.1.1，支持 TCP、TLS、用户名密码、服务端证书校验和客户端证书。
- 多连接管理；QoS 0/1/2 订阅与发布；支持 Retain、订阅暂停和消息筛选。
- Plaintext、JSON、Base64、Hex、CBOR、MsgPack 载荷编解码。
- 消息与运行日志分开保存到 SQLite，可分页查看、筛选和清理。
- 发布草稿、最近发布记录，以及从消息快速创建草稿。
- Lua 5.5 和 JavaScript 消息处理器，可按订阅绑定并保留版本记录。
- MQTT Plus 配置导入/导出，以及 MQTTX 连接配置导入。
- 英文和简体中文界面；支持系统、浅色和深色主题。

## 下载

[GitHub Releases](https://github.com/zouri/mqtt-plus/releases) 提供以下安装包：

| 平台 | 安装包 |
| --- | --- |
| Windows x64 | NSIS 安装程序（`.exe`） |
| Linux x64 | Debian 包（`.deb`）和 AppImage |
| macOS | Intel x64 和 Apple Silicon arm64（`.dmg`） |

## 从源码构建

### 依赖

- CMake 3.29+
- 支持 C++20 的编译器
- Qt 6.11
- Ninja 或其他 CMake 生成器

Qt 需要包含 Concurrent、Core、Gui、Network、Qml、Quick、Quick Controls 2、Sql、Svg、Test 和 LinguistTools。首次配置时，CMake 会下载固定版本的 Lua、KSyntaxHighlighting 和 Extra CMake Modules；如果本机没有 Qt MQTT，还会下载并构建 Qt MQTT 6.11.1。

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

处理器绑定到订阅，在消息写入历史和界面前接收解码结果。入口函数固定为 `process(context)`：

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

## 本地数据

- 会话和偏好设置由 `QSettings` 保存。
- 消息与日志保存在 `QStandardPaths::AppDataLocation/history.db`。
- 草稿和处理器保存在 `QStandardPaths::GenericConfigLocation/mqtt_plus/`。

会话密码保存在本机 `QSettings` 中，不使用系统凭据库。配置导出默认不包含密码和证书；选择导出敏感数据后，应将导出文件视为私密文件。

## 参与贡献

提交 PR 前请运行构建、`all_qmllint` 和完整测试。涉及界面或 MQTT 流程的变更，请在 PR 中说明手动验证步骤；界面变更请附截图。

## 许可证

本项目基于 [MIT License](LICENSE) 发布。
