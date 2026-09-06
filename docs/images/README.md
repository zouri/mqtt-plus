# 文档截图与隐私检查 / Screenshot Privacy Checklist

README 中的配图应展示真实应用界面，但不能暴露个人或业务测试数据。当前四张 PNG 更新于 2026-09-07，来自 0.5.0 版本的真实 Qt Quick 界面。

Documentation screenshots must show the real application without exposing personal or business test data. The four current PNGs were updated on 2026-09-07 using the actual Qt Quick interface from version 0.5.0.

## 本次截图环境 / Capture Environment

- 基于当前构建的 QML 资源和应用服务，由临时截图程序注入虚构消息，再通过 `QQuickWindow::grabWindow()` 抓取实际渲染画面；未重新绘制或用 AI 生成界面。A temporary capture harness used the current build's QML resources and application services, injected fictional messages, and captured actual rendering with `QQuickWindow::grabWindow()`, without redrawing or AI generation.
- 使用 Qt offscreen 平台与软件渲染，窗口为 1480 × 900，输出为 2220 × 1350 PNG；不包含操作系统标题栏。Captured with Qt's offscreen platform and software renderer at a 1480 × 900 window size, producing 2220 × 1350 PNGs without OS window decorations.
- 配置、消息库、草稿和处理器存储重定向到独立临时目录，自动更新检查关闭。Settings, message history, drafts, and processors were redirected to an isolated temporary directory; automatic update checks were disabled.
- 仅展示 `localhost:1883` 与 `localhost:8083` 演示连接，均未连接；消息为模拟输入，不代表实时 Broker 流量。Only disconnected demo connections at `localhost:1883` and `localhost:8083` are shown. Messages are synthetic inputs, not live broker traffic.
- 演示主题为 `demo/office/…` 与 `demo/warehouse/…`，设备编号、温湿度、灯光和电量数据均为虚构。Topics use `demo/office/…` and `demo/warehouse/…`; device IDs, temperature, humidity, lighting, and battery data are fictional.

## 拍摄规范 / Capture Rules

- 使用独立演示配置，不修改或清空日常使用的连接、历史和草稿。Use a separate demo configuration; do not modify or clear everyday connections, history, or drafts.
- 使用本地测试 Broker 或允许公开演示的公共 Broker。不要连接私人环境来制作截图。Use a local test broker or a public broker suitable for demonstrations, not a private environment.
- 主题使用 `demo/my-device-001/telemetry` 等虚构名称；载荷只放模拟温度、状态等非敏感数据。Use fictional topics and non-sensitive simulated payloads.
- 逐项检查连接名称、域名、IP、账号、客户端 ID、主题、消息、脚本、日志、草稿、文件路径、状态栏和弹窗。Inspect connection names, domains, IPs, accounts, client IDs, topics, messages, scripts, logs, drafts, file paths, status bars, and dialogs.
- 不得包含密码、令牌、私钥、证书内容、真实设备标识或业务域名。Never include passwords, tokens, private keys, certificate contents, real device identifiers, or business domains.
- 保存后重新打开最终图片，检查整个画面及图片元数据，而不只是主内容区域。Reopen the final image and inspect the entire frame and metadata, not only the main content area.
- 不用 AI 生成的界面代替产品截图。更新截图时同步检查中英文 README 的说明与图片链接。Do not substitute AI-generated interfaces for product screenshots. Keep both README captions and image links in sync.

## 当前配图 / Current Images

| 文件 / File | 内容 / Content | 隐私检查重点 / Privacy Focus |
| --- | --- | --- |
| `mqtt-plus-workbench.png` | 主题树、消息详情、MQTT 5 属性与发布编辑器 / Topic tree, message details, MQTT 5 properties, publishing | localhost、虚构主题与载荷、底部状态栏 / localhost, fictional topics and payloads, status bar |
| `mqtt-plus-messages.png` | 订阅分组、JSON 消息流与发布编辑器 / Subscription groups, JSON messages, publishing | 演示订阅别名与虚构设备消息 / Demo subscription aliases and fictional device messages |
| `mqtt-plus-processors.png` | Lua / JavaScript 处理器库及已验证的 Lua 脚本 / Lua and JavaScript library with a validated Lua script | 名称、描述和脚本中无真实业务数据 / No real business data in names, descriptions, or scripts |
| `mqtt-plus-settings.png` | 外观与工作台设置 / Appearance and workbench settings | 无账号、私人路径或连接信息 / No accounts, private paths, or connection data |

删除当前截图中的敏感内容不会移除 Git 历史里的旧文件。若未来发现已提交的凭据，应先撤销或轮换凭据，再单独安排历史清理。

Removing sensitive content from a current screenshot does not remove older files from Git history. If committed credentials are discovered, revoke or rotate them first, then handle history cleanup separately.
