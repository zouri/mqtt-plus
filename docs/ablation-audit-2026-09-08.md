# 瘦身与消融实验审核

日期：2026-09-08。基线：`d2cad315421744fc1be0da5f8421257a82a3af73`。

本文保留初始审核记录；实施结果与补充验收见后文。

最新状态：第一轮低风险消融已完成本机 GUI、真实本机 MQTT、Release 构建/安装、重启恢复及 Release ZIP A/B 验证，详见末尾“补充验收”。Linux/Windows 实机与正式发布包验收仍未完成；本报告与证据随本轮改动一并归档到 Git。

范围：C++、QML、资源引用、构建和打包。初始审核采用只读扫描与临时目录中的对照实验。

## 优先发现

### A1：QML 扫描范围包含生成目录（P2，已做局部消融）

- 位置：`CMakeLists.txt:335`、`:459`。app target 创建于仓库根，部署流程触发导入扫描。
- 本地 Qt `Qt6QmlMacros.cmake:4599` 读取 target SOURCE_DIR，`:4613` 将其作为 scanner 的 rootPath。生成的 `build/qt6.11-debug/.qt/qml_imports/mqtt_plus_app_conf.rsp` 确认为整个仓库。
- 基线配置耗时 166.8 秒；观察到 scanner 子进程运行超过两分钟，并报告 `build/fetchcontent/ksyntaxhighlighting-src/autotests/input/highlight.qml` 和 ECM 测试 fixture 的解析错误。
- 控制变量实验：复制 rsp，仅将 rootPath 改为仓库的 `qml/`，并将输出路径移到 `/tmp`。两次扫描耗时分别为 0.20、0.19 秒。
- 基线结果 94 条记录，收窄后 39 条。按 NAME 去重比较，收窄结果没有新增名称；基线额外包含 QtQuick3D、QtMultimedia、QtTest、KDE 模块及旧 `features/scripts` 等名称。
- 负对照：保持原 rootPath 并追加 `-exclude build -exclude dist`，仍耗时 159.07 秒，仍扫描到第三方 fixture。这个相对目录参数写法没有解决问题，不能直接作为修复提交。
- 结论：扫描确实纳入了应用源码范围之外的导入；收窄范围值得优先修复。166.8 秒是完整配置时间，不应与独立 scanner 时间计算精确加速比；本轮也没有证明最终安装包减少多少字节。
- 落地门槛：使用受支持的扫描排除方式或调整 target 所属源码目录，验证完整配置、构建、部署与各平台启动。不要修改 Qt 安装文件，不要直接关闭 import scan，也不要直接编辑生成 rsp 作为正式修复。

### A2：未使用 QML 组件仍被编译打包（P3，引用审核通过）

- `qml/components/AppDivider.qml:6` 与 `AppSectionHeader.qml:7` 合计 54 行、1,029 字节源文件。
- 应用源码和测试中没有组件名消费者；入口仅从 `src/app/main.cpp:167` 加载 Main，未发现动态 createComponent/createQmlObject 入口。
- `CMakeLists.txt:351`、`:364` 仍通过 glob 收入全部 QML。
- 最小消融：单独删除这两个组件，然后 configure/build/qmllint，并启动遍历页面。收益是减少维护表面和生成单元，不能将源文件大小当作二进制或内存收益。

### A3：无消费者的属性和 C++ 接口（P3，引用审核通过）

- `qml/AppUi.qml:189` 的 themeModeMetaByMode 仅被 `:212` 的 themeModeMeta() 读取，该函数没有调用者。
- 同文件的 spaceXs、spaceLg、spaceXl、space2xl、radiusPill、text3xl 无消费者；另有 `EventStreamView.qml:36` compactHeader、`WorkbenchView.qml:30` connectionPaneEdgeColor、`SessionOverviewPanel.qml:27` clientIdText。
- `src/viewmodels/draftsviewmodel.h:44` / `.cpp:62` 的 selectFilteredDraftAt() 无调用，实际 UI 使用 selectDraftById()；同类的 editorDeleteSucceeded 信号仅声明和 emit，没有 handler。
- `src/services/parsing/messageparseworker.h:52` 的 tasksDropped，以及 `historywriterworker.h:73` 的 messagesDropped/parseResultsDropped 无订阅者。统计消费者使用 queueStateChanged 和计数 getter。
- 最小消融：QML 定义、草稿 API、worker 信号分成三个独立实验。删 worker 信号时必须保留计数器、锁、合并通知标记及 queueStateChanged，不能删除整个 notifyDropped()。
- 验收：主题与窗口宽度切换、筛选后草稿选择/删除、worker 背压统计仍正确；构建、lint、完整测试通过。未实际删除验证，不标为“已验证可删”。

### A4：构建依赖暴露范围偏大（P3，需独立配置实验）

- 初始假设：`CMakeLists.txt:9` 的 Qt Test 可按 BUILD_TESTING 条件查找。**实施时已否决**：当前 KSyntaxHighlighting 自身无条件 REQUIRED Test，单改顶层无效，详见实施结果。
- `CMakeLists.txt:331` 将 KF6SyntaxHighlighting PUBLIC 暴露给 Core，但唯一生产使用者 codesyntaxhighlighter 已属于 APP_ONLY。可从 Core 移除该依赖，保留 app 与 highlighter 测试的直接依赖，再跑全量验证。
- 通用测试 helper 的宽链接列表和重复编译 payloadcodec 属于后续构建图优化，不建议为瘦身删除测试。

## 需要测量的候选

| 候选 | 证据 | 消融与验收 |
|---|---|---|
| 双 Inspector 常驻 | `qml/features/workbench/SessionMessagePanel.qml:178`、`:232` 直接实例化两套 MessageInspector；`:184` 的 opened 不依赖页面 active | 先实验按需加载或收紧活跃条件；测对象数、RSS、离页后详情转换次数；检查异步解析、最新 topic 回显、弹窗关闭、Use as draft |
| subscriptionFormats 镜像状态 | `src/domain/sessionruntime.h:20` 重复维护订阅格式；`src/usecases/eventhistoryservice.cpp:725` 读取形成上下文 | 改为从 subscriptions 构建格式表；验证增删改、导入、复制、重启恢复、重叠通配符；先确认测试直接注入 map 的语义 |
| 四个 current*Rate 接口 | `src/viewmodels/workbenchviewmodel.h:130`，实现约 42 行，仅测试调用；UI 使用已有速率 properties | 改测真实公开属性后移除重复入口；保留空会话、时间窗过期断言及刷新定时器，不以删测试换绿灯 |
| 全量语法资源 | `CMakeLists.txt:108` 启用 QRC_SYNTAX，而 highlighter 仅选 JavaScript/Lua；现有资源对象文件约 1.6 MiB | 研究语法依赖闭包后打包，不能只保留两个 XML；对象文件大小不等于最终收益，需 Release 安装包 A/B |

## 明确保留

- 全部 27 个 SVG 均有引用，包含 actionId 动态映射；不按静态路径搜索误删。
- TopicTreePanel 的轮询补足 delegate 离屏/回收后的选中 topic 更新，不能因已有 delegate handler 就删除。
- UpdateService 有 FakeUpdateService 测试替身；ProcessorRuntimeAdapter 有 JS、Lua 和 fake，是有效隔离，不是多余抽象。
- messageDetailsChanged 有 QML handler；模型结构通知、跨线程上下文、压力计数和队列状态通知应保留。
- 其他页面已通过 Loader 管理生命周期，非活动列表已有 null model 与 delegate reuse，不必整体重写。

## 验证基线

| 命令 | 结果 |
|---|---|
| `rtk cmake --preset qt6.11-debug` | 通过；配置 166.8 秒，存在第三方 QML fixture 扫描错误输出 |
| `rtk cmake --build --preset qt6.11-debug -j 4` | 通过；第三方 QT_DEPRECATED_WARNINGS_SINCE 重定义警告 |
| `rtk cmake --build --preset qt6.11-debug --target all_qmllint -j 4` | 通过 |
| `rtk proxy ctest --test-dir build/qt6.11-debug --output-on-failure` | 42/42 通过，24.07 秒 |

Qt review 辅助脚本完成扫描：C++ 403 条、QML 647 条原始诊断。这些是未经逐条确认的规则命中，不是 1,050 个确定缺陷，也不是本轮删除依据；正式 qmllint 通过。

未执行：源码删除后的构建、GUI 手动回归、真实 broker 流程、RSS/帧耗时测量、当前版本 Release 包 A/B。不能把基线测试通过当作候选消融通过。

## 磁盘与实施顺序

构建目录和历史发布产物的磁盘占用不能代表当前发布包体积。包体收益以同工具链的 Release 安装包 A/B 对比为准，详见补充验收。

建议顺序：先修扫描范围，再分别验证两个死组件、死属性、草稿 API、worker 信号，最后缩窄依赖。每次只改变一个候选组，记录测试和可比较的指标；Inspector、镜像状态和语法资源另做测量，不与低风险删除混成一个大提交。

## 实施结果

### 已落地

1. 将 app target 及其 QML、翻译、图标配置移入 `qml/CMakeLists.txt`，让 Qt 原生配置期和构建期扫描都以 `qml/` 为根；未修改 Qt 工具、未关闭导入扫描。显式保留 qrc 别名和可执行文件输出目录，Windows 打包图标变量继续传回顶层。
2. 删除 AppDivider、AppSectionHeader 两个无消费者组件。
3. 删除 9 个无消费者属性及 themeModeMeta 死链。lupdate 核对后只保留对应 4 条翻译删除，撤销其余无关位置刷新及历史翻译漂移。
4. 删除 selectFilteredDraftAt 和 editorDeleteSucceeded；保留按 ID 选择、删除后选中状态处理。
5. 删除三个无人订阅的 dropped 信号与仅为 emit 服务的局部快照；保留锁、累计计数、通知合并标记及 queueStateChanged。
6. 从 Core 的 PUBLIC 依赖中移除 KF6SyntaxHighlighting；保留应用和语法高亮测试的依赖。
7. 更新应用 target 的架构边界测试；增强 parser/history writer 测试，验证启动前的丢弃仍发出队列状态通知，后续再次丢弃也能通知且累计计数正确。

### 消融结果与更正

- 初始完整配置 166.8 秒；正式修改后多次配置约 2.6–3.7 秒。配置期和构建期扫描结果均为 39 条，生成 rsp 的 rootPath 为 `qml/`，不再混入 QtQuick3D、QtMultimedia 或 KDE fixture 导入。
- 单独删除两个组件、再删除属性，两组均通过 app 构建和 all_qmllint。
- 草稿相关及架构测试 5/5 通过；worker 专项测试 2/2 通过；全部改动后的完整测试 42/42 通过（23.20 秒）。测试目标没有删除。
- 完整构建和 all_qmllint 通过；翻译生成 707 条，全部已完成。
- 尝试 `BUILD_TESTING=OFF` 且禁止查找 Qt6Test 时，配置在第三方 `ksyntaxhighlighting-src/CMakeLists.txt:42` 失败：其 `find_package` 无条件 REQUIRED Test。因此撤回顶层条件化实验，保留现有 Qt Test 要求。恢复 Qt Test 后，独立 Release / BUILD_TESTING=OFF 配置通过；没有在该目录完成 Release 编译或包体对比。
- `-exclude` 的相对和绝对路径实验均未解决递归扫描问题。Qt 6.11.1 源码只跳过当前迭代目录，没有阻止 QDirIterator 继续遍历子目录，因此正式实现采用 target 源码目录调整，而非该选项。[Qt scanner 源码](https://raw.githubusercontent.com/qt/qtdeclarative/v6.11.1/tools/qmlimportscanner/main.cpp)

### 首轮部署与验证限制（补充验收前的记录）

- macOS Debug 安装到 `/tmp/mqtts-ablation-install` 完成，签名严格验证通过。部署工具曾报告缺失 libmimerapi；项目已有的安装后清理移除了 qsqlmimer，最终 sqldrivers 中仅有 qsqlite。此结果不等于跨平台发布包验证。
- 开发构建 offscreen 启动未报告 QML 类型或资源加载错误，但合成消息流程因测试数据目录数据库无法打开而中止，没有宣称流程通过。
- 安装包仅带 cocoa 平台插件，不支持 offscreen；该 headless 启动尝试未成功。没有为测试向发布包额外塞入 offscreen 插件。
- 首轮尚未完成页面遍历、截图、真实 broker 或安装包 GUI 启动验证；本机验证在补充验收中完成。Linux/Windows 尚未实机验证。
- Inspector、subscriptionFormats、四个 current*Rate 接口和语法资源裁剪仍保留；它们属于报告中的另行测量组。

## 补充验收

日期：2026-09-08。范围仍为上述第一轮消融；本次只新增验证记录和截图，没有继续修改生产代码或开展第二轮候选优化。

### 环境与证据边界

- macOS Apple Silicon，Qt 6.11.1。工作台、处理器库、草稿库、日志和设置均实际进入并检查。
- Mosquitto 2.1.2 仅监听 `127.0.0.1:18884`；所有消息均为 `ablation/20260908/` 下的合成数据。客户端 ID 为 `mqttplus-ablation-qa`，没有使用现有业务连接。
- GUI 使用 Debug 和 Release 安装目录的隔离测试副本，更改副本的 bundle identifier 并重新 ad-hoc 签名；应用业务源码和 QML 未改动。数据位于 `/tmp/mqtts-ablation-qa`。
- macOS 原生 QSettings 不随 `CFFIXED_USER_HOME` 一起隔离，因此额外通过临时 dylib 将其构造定向到该目录下的 INI 设置；这只隔离设置存储，不替换 MQTT、解析或持久化实现。该 dylib 未加入安装产物。Release 的 dyld 日志确认 QtCore、QtGui、QtQml 和 cocoa 插件从安装副本内部加载。
- 原始 Release 安装产物另行通过 `codesign --verify --deep --strict`。GUI 结果属于上述隔离副本，不能等同于正式 Developer ID 签名、公证、Gatekeeper 或干净机器验证。

### GUI 与真实 MQTT 验收

| 验收项 | 结果与证据 |
|---|---|
| 连接与订阅 | GUI 保存本机连接并显示“已连接”；broker 记录 MQTT 5 CONNECT/CONNACK、`ablation/20260908/#` 的 SUBSCRIBE/SUBACK |
| 外部发布 → 应用接收 | `mosquitto_pub` 发送合成 JSON；主题树、最新消息和事件流均显示对应 topic、49 B Payload 及接收时间 |
| 应用发布 → 独立客户端接收 | GUI 发布 `{"source":"mqtt-plus-ui","seq":2}`；独立 `mosquitto_sub` 收到完全相同的 topic 和 Payload；应用显示发送记录及订阅回环接收记录 |
| 暂停/恢复订阅 | 点击“暂停全部订阅”后 broker 收到 UNSUBSCRIBE；暂停期间发布的负对照没有转发给应用且没有入库，消息数保持 5；恢复后重新 SUBSCRIBE，收到 `received-after-resume`，消息数增至 6 |
| 消息详情 | 最新主题详情和双击事件流打开的消息检查器均正常；Plaintext → JSON 格式化显示正确，关闭按钮有效，“用作草稿”正确填入发布编辑器 |
| 草稿选择与删除 | 从发布编辑器保存 `QA Keep`，复制并保存 `QA Delete`；筛选 Delete 后按名称选择，删除确认指向 QA Delete；删除后无匹配项、编辑器清空且删除按钮禁用；清除筛选后 QA Keep 仍可选择且内容正确 |
| 主题和编辑器 | 深色/浅色以及薄荷绿/蓝色切换正常；设置布局、图标及 Lua 语法高亮显示正常，默认 Lua 处理器的“验证”返回“已验证 · 当前内容可用” |
| 窗口尺寸 | Debug 普通/放大窗口，以及 Release 1100×600 最小内容区与放大窗口均实际检查；工作台、草稿字段和操作按钮可用，截图包含窗口标题栏 |
| 重启与持久化 | Debug 正常断开并退出后，Release 副本恢复同一测试连接、订阅、6 条历史以及 QA Keep 的名称、topic、Payload；QA Delete 未恢复 |
| Release 重连 | Release 点击连接后 broker 重新记录 CONNECT 和 SUBSCRIBE；外部新消息 `{"release":true,"seq":3}` 到达，历史总数增至 7，最后正常断开并退出 |

最终 SQLite 按 topic 统计：inbound 1、outbound 4、resumed 1、release 1，paused 0。outbound 包含一次 Plaintext 和一次 JSON 的 GUI 发布及各自回环。

截图和文本证据见 [补充验收证据目录](ablation-evidence-2026-09-08/README.md)。截图是本次修改后验收记录，不是修改前后视觉差异对比。

### Release 构建、安装与体积对比

当前版本命令（`QT_PREFIX` 指向本机 Qt 6.11.1 macOS 安装目录）：

```bash
rtk cmake -S . -B build/ablation-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
  -DCMAKE_PREFIX_PATH="${QT_PREFIX:?Set QT_PREFIX to the Qt installation directory}"
rtk cmake --build build/ablation-release -j 4
rtk cmake --install build/ablation-release --prefix /private/tmp/mqtts-ablation-release-install
rtk proxy codesign --verify --deep --strict /private/tmp/mqtts-ablation-release-install/mqtt_plus_app.app
```

全部通过。部署仍出现既有的 libmimerapi 缺失输出，安装后既有清理逻辑移除了 qsqlmimer，最终 SQLite 驱动可用，真实收发和重启后的历史查询通过。

基线由 `git archive d2cad31` 导出到 `/private/tmp/mqtts-ablation-baseline`，复制同版本 FetchContent 源缓存，使用同一 Qt、Ninja、Release 和 `BUILD_TESTING=OFF` 配置、构建、安装。双方未修改的安装目录均严格验签通过，再用相同的 `ditto -c -k --sequesterRsrc --keepParent` 命令生成 ZIP。

| 指标 | 基线 d2cad31 | 当前工作区 | 差值 |
|---|---:|---:|---:|
| app 可执行文件字节数（含签名） | 7,796,224 | 7,778,672 | −17,552 |
| 安装目录分配空间（`du -sk`，KiB） | 148,312 | 148,296 | −16 |
| 安装目录 ZIP 字节数 | 55,424,398 | 55,418,915 | −5,483（约 −0.01%） |

结论：本次 Release 包体收益很小，不能宣称显著缩包。ZIP 包含签名、构建及压缩元数据；这是一轮同工具链实测，不将每一字节都归因于死代码删除，也不将 ZIP 结果当作正式 DMG 的大小。扫描范围修复与维护面减少仍是主要收益。

初次沿用 `/tmp/mqtts-ablation-no-tests` 构建时，moc 生成的相对包含路径受 `/tmp` → `/private/tmp` 路径及既有生成缓存影响而失效；切换为仓库内新的 `build/ablation-release` 后完整构建通过，未修改生产代码或 Qt 文件。独立导出基线配置本次为 7.1 秒，源码/生成目录内容与最初 166.8 秒场景不同，不据此重新计算扫描加速比。

### 最终状态

- 本次再次执行完整 CTest：**42/42 通过，12.74 秒**；`all_qmllint` 和 `git diff --check` 通过。
- Debug/Release GUI 进程均正常退出，本机 Mosquitto 已停止。
- 本机第一轮功能验收已补齐。仍未完成 Linux/Windows 实机、macOS 正式 DMG/签名公证/干净机器验收。
- Inspector 生命周期、subscriptionFormats 镜像状态、current*Rate 接口和语法资源裁剪仍未实施，RSS/帧耗时也未测量，继续作为独立后续候选。
