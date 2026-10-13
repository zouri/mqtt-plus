# 浏览器编辑器验收记录 / Browser editor verification

## 唯一可编辑窗口 / Single Editable Window

2026-10-13：同一应用编辑器服务全局只有一个可编辑页面，不按脚本分别授权。页面身份每次加载重新生成，复制授权会话不复制编辑权限。后端 30 秒单调时钟租约，前端可见页面每 5 秒续约并在服务端到期前 3 秒锁定；pagehide 尝试释放，释放丢失由到期处理。后台页面长期不续约也会失去权限，返回时先确认权限再编辑。没有强制接管入口，不删除历史草稿。

新增 HTTP 回归覆盖无权限写入、同一授权的两个窗口、不同授权/语言的窗口、只读 GET、创建/更新/删除/保存阻止、非持有者释放阻止及正常释放后新窗口取得权限。可控时钟测试覆盖续约、异常失联到期、旧权限令牌失效、旧令牌不能释放新权限、失效写入不执行。延迟真实校验进程测试覆盖“校验中释放权限，随后保存提交返回 423 且正式脚本未创建、草稿保留”。前端权限回归、类型检查及生产构建通过；共 18 项前端测试、45 个 Qt 测试程序及 QML lint 通过。Monaco 大包体提示仍保留。

内置浏览器使用 `test_browsereditorservice --serve-editor-pair` 和同一临时数据库服务的 Lua / JavaScript 两个不同授权入口：第一个窗口可编辑，第二个窗口字段只读、保存/校验禁用并显示提示；关闭第一个未修改窗口后，第二个自动获得权限并可修改名称。截图 `images/mqtt-plus-editor-single-window.jpg` 仅含内置 JavaScript 模板与默认演示名称，无需脱敏。两个窗口均未读取日常数据。

浏览器测试不冒充系统级复制标签页、关闭崩溃、后台长期节流、IME、BFCache 或跨浏览器安装包验收；相关服务端到期和复制会话边界由协议/时钟回归覆盖。同一编辑器服务的唯一权限不等于多个桌面应用进程之间的全局互斥。

## 断连只读 / Read-only on disconnect

2026-10-13：检测到网络失联或授权失效后，Monaco 源码、名称、描述和入口函数只读，文件增删改、语言/入口选择及服务提交动作禁用。查看、复制、切换已有源码文件、换行和 JSON 导出仍可使用。连接确认使用请求发起时的连接代次，断连前发出的旧成功响应不会解除锁定；同一服务的新认证请求成功后恢复编辑。保留原有 10 秒心跳与 15 秒请求超时，锁定发生在检测到失联后，不声称瞬时感知程序退出。

前端 17 项、Qt 45 项、Debug 构建及 QML lint 通过。内置浏览器和临时数据库宿主实测：暂停演示服务等待心跳超时，确认三个元数据文本框只读、所有文件修改和选择控件禁用；尝试输入后源码模型/可见代码及名称未改变，导出按钮仍可用。Monaco 内部输入缓冲仍能接收模拟按键，但提示只读且没有写入源码，不能将该缓冲误认为模型内容。恢复同一演示进程后，心跳隐藏断连提示、字段解锁，名称修改和源码输入再次生效。未执行真实用户草稿清理或修改。

截图 `images/mqtt-plus-editor-disconnected-readonly.jpg` 使用内置 Lua 模板及虚构 `Demo readonly` / `Synthetic disconnect check` 信息，已重新打开并检查元数据，无需脱敏。系统输入法、剪贴板复制、导出下载、授权失效和多请求竞争的跨浏览器端到端流程仍需人工验收；这里没有宣布这些额外流程已通过。

## 按需创建草稿 / Lazy draft creation

2026-10-13 修复“打开即创建草稿”：正式脚本无草稿时只读取当前版本，新脚本模板只放在内存；首次内容变化后才持久化。回归测试先复现了两条打开路径都会发出 `POST /drafts`，修复后断言现有脚本仅 `GET /processors/{id}`、新模板不发请求。新增显式分支、撤销到原内容及丢弃后重载基线的测试，前端共 16 项通过。Debug 构建、45 个 Qt 测试和 QML lint 通过；既有 Monaco 大包体提示保留。

使用临时数据库宿主及 Codex 内置浏览器实测：首次打开和手动草稿列表均为零记录；未修改内容校验后临时快照被清理，列表仍为空；首次修改名称后出现一份草稿；保存版本后刷新读取正式版本、草稿列表为空；显式新建分支仍生成草稿。截图 `images/mqtt-plus-editor-no-draft.jpg` 只含内置模板和默认新脚本名称，已重新打开并检查画面/元数据，无需脱敏。没有读取或删除日常历史草稿。

浏览器原生丢弃确认、校验清理时的网络故障及跨平台完整流程仍需人工验证；丢弃后不重建的读取/存储决策由前端回归测试覆盖，不冒充系统确认框端到端验收。

## 本地验证 / Local verification

日期：2026-10-13（Asia/Shanghai）。环境：macOS arm64、Qt 6.11.1、Debug、本机临时数据库；浏览器验证使用 Playwright Chromium。测试不读取日常连接、历史或草稿。Date: 2026-10-13, Asia/Shanghai; macOS arm64, Qt 6.11.1, Debug, temporary databases, Playwright Chromium. Normal user data is not used.

| 项目 / Check | 结果 / Result |
| --- | --- |
| Debug 构建 / Debug build | 通过 / Passed |
| CTest | 45 个测试程序通过 / 45 test executables passed |
| QML lint | 通过 / Passed |
| 原生翻译 / Native translations | 更新无解析错误，701 项完成、0 项未完成；旧编辑器过期文案已清理 / Clean update, 701 finished, 0 unfinished; obsolete editor labels removed |
| 前端类型检查、生产构建 / Frontend type check and production build | 通过；Monaco 大包体提示保留，不等于运行失败 / Passed; Monaco chunk-size warning remains |
| 前端保存状态回归 / Frontend save-state regression | 9 项通过：原有 7 项及单草稿自动恢复、本标签页分支优先与失效记录回退 / 9 passed, including automatic recovery and tab-local branch preference |
| 草稿存储 / Draft storage | 重启恢复、乱序写入拒绝、元数据竞争、幂等、保存期间输入、删除保留源码、未来 schema 拒绝、100 草稿上限 / Restart recovery, stale writes, metadata conflicts, replay, newer input, deletion source preservation, future-schema rejection, 100-draft quota |
| HTTP 与校验进程 / HTTP and validator | 鉴权、Host/Origin、死循环、异常输出、崩溃、5 秒硬超时、取消 / Auth, Host/Origin, runaway scripts, malformed output, crash, hard deadline, cancellation |
| 管理页面 / Management view | 使用实际 QML 的只读页面渲染测试和隔离截图 / Actual read-only QML rendering test and isolated screenshot |
| 浏览器交互 / Browser interaction | 修改虚构名称、保存版本、继续修改描述、刷新恢复源码与元数据、停止服务后的断连提示；最终构建验证取消校验和按钮显示 / Synthetic edits, version save, newer draft and refresh recovery, disconnect notice; final build validation cancellation and control visibility |

重现基础验证 / Reproduce the core checks:

```sh
cmake --preset qt6.11-debug
cmake --build --preset qt6.11-debug
cmake --build --preset qt6.11-debug --target all_qmllint
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qt6.11-debug --output-on-failure
cd web/processor-editor
npm test
npm run build
```

HTTP 测试需要本机回环网络权限。`test_browsereditorservice --serve-editor` 可提供临时演示入口，退出后删除数据库。只读管理页面可用 `MQTT_PLUS_TEST_CAPTURE=docs/images/mqtt-plus-processor-management.png` 配合 `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software` 运行 `test_processorsviewmodel` 重新截图。发布前检查[截图隐私规范](images/README.md)。Do not publish browser traces: they may contain disposable authorization values.

浏览器流程中的已删除草稿查询 `404` 和主动停止服务后的连接拒绝是预期故障分支，不应与前端运行异常混淆。断连验证不等于跨平台离线安装包验收。The deleted-draft lookup and connection refusals after stopping the demo host were expected branches, not unhandled frontend exceptions.

## 离线与输入验证 / Offline and composition checks

2026-10-13 补充验证：Playwright Chromium 的浏览器上下文只放行演示服务的精确本机 origin，其余请求全部拒绝。重新加载后 Lua / JavaScript 编辑器资源和工作线程可用，中文名称与源码保存、校验、版本保存、Cmd+S、修改后刷新恢复、版本化 JSON 导出及导入后重新校验通过。中文插入的 Cmd+Z / Cmd+Shift+Z 撤销重做通过；自动化插入的文本和换行是两个撤销分组，需要分别撤销，不能把第一次只移除换行误判为无法撤销中文。未修改操作系统网络设置。The browser context permitted only the exact loopback origin; all other requests were blocked, without changing OS network settings.

名称字段通过 DOM composition/input 事件模拟，Monaco 通过 Chromium 输入协议 `Input.imeSetComposition` / `Input.insertText` 验证。修复前，等待 1.3 秒即可复现临时拼音被持久化；修复后，等待 1.5 秒数据库仍保留此前内容，结束组合输入后最终中文正常提交。These are browser-level composition checks, not proof of native pinyin candidate-window or platform IME behavior. 系统输入法选词、候选翻页及跨浏览器行为仍需人工实测。

浏览器流程由临时数据库测试宿主提供，使用与应用相同的 HTTP 服务与内嵌静态资源，但不是从安装后的完整桌面 GUI 发起。因此不能据此宣布各平台安装包离线流程全部通过。The isolated test host does not replace installed desktop GUI acceptance.

## macOS 安装包 / macOS package

本机已构建 Release arm64 DMG：`dist/mqtt-plus-0.5.3-macos-arm64.dmg`。验证包 SHA-256：`0d2ae922fe3c848fa9cae6212705689035983f08ac019bf2c42cdd389bc1ab63`。该文件为本次验证产物，不是已通过全部发布验收的版本。

- DMG 校验和及只读挂载通过；挂载应用的 `codesign --verify --deep --strict` 通过，签名为本机 ad-hoc，不等于公证或 Gatekeeper 验收。
- 使用挂载包内的真实 `--processor-validator`，移除 Qt / DYLD 搜索路径覆盖并将 PATH 限制为系统目录；Lua、JavaScript 各自有效与无效输入的 4 项长度前缀 CBOR 检查通过。该进程不初始化正常用户 GUI 或数据库，也不依赖 Node。
- 主程序引用包内 Qt 框架；HttpServer、Qml 与 Sql 已随包部署。最终 SQL 插件目录只有 SQLite，无 Mimer、ODBC 或 PostgreSQL 插件。
- 部署工具扫描本机无用 Mimer 插件时报告缺失外部库，但最终排除该插件；这条部署工具诊断不能当作缺失编辑器运行资源，也不能隐瞒为完全无告警构建。

**发布阻塞：本机 QtMqtt 框架的 Mach-O 最低系统版本为 26.0，而应用 Info.plist 和构建目标为 13.0。** 链接器也明确告警。不能宣称此包可运行于 macOS 13–15；需要将 QtMqtt 按项目最低系统版本重新构建，或另行明确批准提高产品最低系统要求后再验收。未擅自修改本机 Qt 安装或提高产品要求。

复现挂载包校验器检查 / Reproduce packaged validator checks:

```sh
build/qt6.11-debug/test_browsereditorservice --verify-packaged-validator \
  "$MOUNT_POINT/mqtt-plus.app/Contents/MacOS/mqtt-plus"
```

The macOS DMG and packaged validator checks are partial acceptance only. The local QtMqtt deployment target mismatch remains a release blocker; installed Safari/Chrome workflows, notarization, and older-system compatibility have not been established.

## 脚本切换 / Processor switching

2026-10-13 使用隔离 Chromium 和临时数据库验证新版浏览器入口：空列表、按名称和语言搜索、Lua / JavaScript 往返切换、切换前草稿落库、恢复旧分支、刷新保持最后打开的脚本、取消不改变选择、找回未保存的新脚本、未确认保存阻止切换、校验进行中禁用切换。7 项前端测试、Debug 构建、45 个 CTest 和 QML lint 通过。

桌面 1440 x 900 和移动 390 x 844 的编辑器及选择对话框检查通过。Monaco 在视口变更后异步更新尺寸，检查在尺寸稳定后执行，未发现持续横向溢出。截图使用虚构 Demo 脚本，见[隐私说明](images/README.md)。The initial immediate-after-resize overflow check was repeated after the editor settled; it did not indicate persistent layout overflow.

该入口复用已有鉴权的 `/processors` 与草稿 API，不新增网络权限，不执行用户脚本，不改变订阅绑定或当前正式版本。首次演示数据没有提供完整模板而被创建为默认名称，修正测试夹具后重测全部切换流程通过。The picker only changes the editing target, not the active message-processing version.

上述 DMG 校验记录对应输入法修复阶段的包，不包含之后新增的脚本切换入口；当前 Debug 构建已包含该入口。不要把旧验证包当作当前工作区的最新发布包。

### 自动恢复调整 / Automatic recovery refinement

2026-10-13 后续验证：单草稿首次打开无弹窗；多份草稿时优先使用本标签页上次的分支；跨 Lua / JavaScript 往返及刷新后保持分支；陌生多分支仍提示选择；“草稿”按钮即使只有一份也保留手动选择、新建和丢弃入口；已记住分支被删除后自动恢复唯一剩余分支。新授权清除旧会话的分支偏好。9 项前端测试、Debug 构建、45 个 CTest 和 QML lint 通过。Automatic recovery does not change conditional-write or processor-version conflict rules.

最初过密的自动化切换请求触发了每秒 30 次限流，导致字体及工作线程重复请求失败；不得将该次警告当作无错误验证。内置 `/assets/` 构建资源现使用长期 immutable 缓存，页面和 API 仍为 `no-store`；HTTP 回归测试检查字体缓存、HTML/API 不缓存及未知资源不缓存。保留原有限流，过密 API 请求仍可能返回 `429`。随后重新授权并在缓存服务上复测跨语言切换和刷新恢复，最终浏览器日志为 0 错误、0 告警。Static caching changes neither authentication nor the request-rate limit.

## 发布前待验收 / Release gates

以下项目没有在本机测试中冒充完成；CI 配置不代表已经执行成功。The items below are not established by local checks or merely configuring CI.

- macOS Safari/Chrome、Windows Edge、Linux Chrome/Firefox：离线安装包打开、刷新、恢复、保存、导入导出、程序退出后断连。Verify these workflows on installed offline packages in each browser/platform.
- 中文输入法组合输入、撤销重做、搜索替换、Ctrl/Cmd+S、浏览器系统快捷键冲突。Verify IME composition and native keyboard behavior manually.
- 真实网络故障时保留页面内容、下载 JSON、重新授权后恢复；验证提交后的结果不可取消。Verify disconnection, JSON recovery, and commit/cancel boundaries on packaged builds.
- 固定 100 脚本、接近源码包上限、持续 MQTT 输入的迁移前后性能对比。记录构建、硬件、流量、包大小、采样方法和原始指标；GUI 探针 p99 < 100 ms、最大 < 250 ms，浏览器输入到绘制 p95 < 50 ms。Use the same workload and instrumentation for both builds; do not substitute idle unit-test timing for these measurements.

正常 MQTT 消息脚本仍在原运行时执行，独立校验进程不是完整安全沙箱。草稿数据库未加密；关闭断连页面后的未同步字符不保证恢复。Normal message execution is unchanged, validation is not a complete sandbox, drafts are unencrypted, and unsynced text may be lost when a disconnected tab closes.
