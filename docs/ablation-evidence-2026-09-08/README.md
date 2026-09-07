# 消融补充验收证据

对应 [实施报告的补充验收](../ablation-audit-2026-09-08.md#补充验收)。均为隔离测试数据，不包含业务连接、凭据或用户历史。

| 文件 | 内容 |
|---|---|
| [settings-dark.png](settings-dark.png) | Debug 深色主题 |
| [settings-light.png](settings-light.png) | Debug 浅色主题与蓝色强调色 |
| [processor-lua.png](processor-lua.png) | Lua 高亮与验证成功 |
| [mqtt-logs.png](mqtt-logs.png) | 应用连接、订阅、发布日志 |
| [message-inspector.png](message-inspector.png) | 事件流与 JSON 消息详情 |
| [release-restored-workbench.png](release-restored-workbench.png) | Release 重启后恢复连接、订阅与 6 条历史 |
| [release-restored-draft.png](release-restored-draft.png) | Release 重启后保留草稿及 Payload |
| [release-minimum-window.png](release-minimum-window.png) | Release 最小内容区 1100×600，重连后第 7 条消息 |
| [release-wide-window.png](release-wide-window.png) | Release 放大窗口的消息列表与发布区 |
| [external-received-json.txt](external-received-json.txt) | 独立 mosquitto_sub 接收到的应用发布消息 |
| [broker-reconnect.log](broker-reconnect.log) | broker 输出后半段原文：恢复订阅、Release 重连及转发，最后停止 broker |
| [deployed-libraries.txt](deployed-libraries.txt) | Release 运行时加载安装副本内的 Qt 框架和 cocoa 插件 |
| [ctest-final.log](ctest-final.log) | 完整测试 42/42 通过；首行本机绝对路径已替换为仓库相对路径 |

截图是修改后实际运行记录；没有制作基线 GUI 截图。GUI 安装副本的 bundle identifier 和签名用于区分测试实例，设置通过临时 dylib 隔离到 INI。未改动的原始安装目录单独用于签名和 ZIP A/B 验证。该运行环境不覆盖正式签名、公证或跨平台启动。
