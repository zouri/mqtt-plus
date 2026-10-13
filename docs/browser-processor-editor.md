# Browser processor editor

## 中文使用说明

同一个正在运行的程序实例中，整个浏览器编辑器只有一个窗口可以编辑，不按脚本分别授予权限。第一个页面获得编辑权限，其余页面只读，仍可查看、复制与导出。正常关闭或刷新时尝试释放权限，其他可见页面随后自动取得权限；异常关闭则等待最多 30 秒的服务端租约过期。页面每 5 秒续约，客户端会在服务端到期前锁定，失联或后台暂停不能依靠旧权限继续修改。不同页面每次加载都生成新的窗口标识，复制标签页不会沿用编辑权限。没有强制接管按钮；已有草稿仍保留。

打开或切换到没有草稿的脚本时，仅加载正式版本或新脚本模板，不创建草稿。首次实际修改后才自动存储；在首次自动保存前撤销回原内容，也不会留下草稿。查看草稿列表不会创建记录，丢弃最后一份草稿后回到未修改内容，不再立即创建替代草稿。显式“新建分支”、导入草稿或保存新脚本仍会创建相应内容。校验未修改内容会使用临时快照，完成后在没有后续编辑的情况下清理；断连或清理失败时保留记录以免丢失内容。已有历史草稿不会被自动删除。

草稿同步、校验结果和基础版本分别显示，修改后旧校验结果标记为待重新校验。检测到断连或授权失效时出现持续提示与导出入口，代码及名称、描述、入口函数切为只读，文件和入口修改动作暂停；仍可查看、选择、复制、切换源码文件和导出。草稿列表显示本地时间、当前分支、文件数和基础版本，可先预览源码再恢复；丢弃分支需要确认。入口函数缺失时可点击诊断直接定位到入口函数字段。窄屏默认收起文件/入口和脚本信息面板，可用工具栏按钮展开，也可独立开启自动换行。

换行与窄屏面板按钮位于当前文件标题栏，不再单独占一行。校验按钮直接显示进行中、通过、失败或更改待校验状态，点击仍会校验当前内容。

程序内的脚本库只负责搜索、新建、删除、查看版本和订阅使用情况，源码统一通过“在浏览器中编辑”修改。编辑期间请保持程序运行；程序重启后从脚本库重新打开编辑器，旧授权链接不能重复使用。

浏览器顶部当前脚本名称旁的入口可直接切换脚本，支持按名称、描述或语言搜索。切换前只持久化当前草稿，不自动校验或发布版本。再次打开脚本时优先回到本标签页上次编辑的草稿；没有有效记录但只有一份草稿时自动恢复，多份且无法确定本标签页分支时才提示选择。“草稿”按钮始终可以打开分支列表，手动恢复、丢弃或新建分支。未保存的新脚本可从切换界面的“未保存脚本草稿”找回。取消选择不会改变当前脚本，刷新后仍恢复最后打开的草稿。校验或保存进行中不能切换；结果不确定的保存需先重试确认。

输入静置 1 秒、持续输入最长 5 秒后提交草稿。“草稿已存到本机”表示数据库已提交，不表示订阅已生效。校验通过并保存版本后才更新程序快照；保存过程中继续输入的内容保留为后续草稿，不会误标为已保存。复制标签页默认只读；条件写入和旧版本竞争时的分支保护仍保留，恢复列表只显示摘要，选中后再读取完整源码。

中文等输入法的组合输入期间暂停新的自动保存、校验和保存提交；选词结束后重新计时。已开始的保存仍只提交此前冻结的版本，不会把尚未确认的组合文字当作新版本。

“取消校验”可以取消排队或校验中的任务，草稿保留。已经进入提交阶段的保存不可取消。程序断开时页面保留内容，可以导出 JSON；未同步字符在关闭断连页面后不保证恢复。恢复、导入和另存的新脚本仍需校验才能生效。

旧 Qt 本地脚本编辑器及其语法高亮依赖已移除；发布消息等通用文本控件仍支持编辑、滚动和可选行号。运行安装包不需要 Node、网络或 CDN；构建需要 Node/npm 和包含 HttpServer 的 Qt 6.11。

本地验证和尚未完成的跨平台、输入法、压力性能验收见[验收记录](browser-editor-verification.md)。未完成的项目不是已经验证通过的能力。

The Processor Library now manages scripts without editing their source in QML. Select a processor and choose **Edit in Browser**, or choose a language and **New Processor**. Keep the desktop application running while editing. Reopen the editor from the desktop application after restarting it; do not bookmark authorization links.

The current processor control in the browser header opens a searchable picker for saved processors. Search matches names, descriptions, and languages. Switching persists the current draft without publishing a version, then resumes this tab's previously edited branch when it still exists. Otherwise, a single draft is restored automatically; multiple unfamiliar branches require a choice. The **Drafts** button always opens the branch chooser for manual recovery, deletion, or creating a new branch. **Unsaved processor drafts** returns to unfinished new processors. Cancelling keeps the current processor; refreshing resumes the last loaded draft. Switching is unavailable during validation/save or import; an uncertain save must be resolved before switching.

## Drafts and versions

One running application instance grants editing to exactly one browser document across all processors, not one window per script. Each document generates a fresh window ID (not stored in session storage), obtains a server lease, and renews it every five seconds while visible. Other documents are read-only and may view/copy/export. Closing or refreshing attempts to release the lease; if that request is lost, the thirty-second server deadline releases it. Clients lock before that deadline and recheck on returning from suspension. Waiting visible pages automatically acquire permission when available. There is no force-takeover action. This is scoped to the editor service of one app process, not an OS-wide lock across separate application instances.

Opening or switching to a processor without drafts loads its current saved package, or a new in-memory template, without writing a draft. Actual edits trigger persistence; reverting to the baseline before the first autosave does not create a record. Viewing the draft list and discarding the last draft do not create replacement records. Explicit branch creation, import, and saving a new processor remain intentional writes. Validation of unchanged content uses a temporary snapshot that is removed afterward only when its version and content remain unchanged; connection or cleanup failures retain it safely. Existing historical drafts are not automatically deleted.

The editor shows draft synchronization, runtime validation, and base version separately. Editing after validation marks the result as stale. A detected disconnection or expired session displays a persistent export banner and locks source and metadata as read-only, while disabling file/entry mutations and server-dependent actions. Viewing, selecting, copying, source-file selection, word wrap, and JSON export remain available. Editing resumes after a fresh authenticated request confirms connectivity; older in-flight responses cannot unlock a detected disconnection. Narrow screens start with file/entry and metadata panels collapsed; toolbar toggles reopen them, and word wrap can be enabled independently.

Word wrap and narrow-screen panel controls share the current file's title bar instead of a separate toolbar row. The validation action itself displays its current result and remains the action for validating the latest content.

Draft recovery shows local date/time, current branch, branch identifier, file count, base version, and description or entry details. Preview reads source without changing the editing target; restore fetches the latest draft again. Discarding a branch requires explicit confirmation. Missing entry-function diagnostics link directly to the entry-symbol field and retain the runtime's original message below a localized summary.

Typing persists a draft after one idle second, or every five seconds of continuous input. The footer acknowledges a draft only after storage succeeds. Drafts are local, are not encrypted, and never change active subscriptions until a validated save completes.

IME composition pauses new draft writes and validation/save submissions in both Monaco and metadata fields. Persistence timers restart when composition ends. A save already in flight still commits only its frozen snapshot, not provisional composition text.

The editor supports Lua and JavaScript packages with up to 64 files, 1 MiB per file, and 2 MiB of total source. Validation uses the application's actual runtime in a disposable process; browser JavaScript diagnostics do not define runtime compatibility. Samples are not executed in the editor.

Only the lease-owning document can modify drafts, submit validation/save, or cancel jobs. Existing branches and conditional-write protections remain supported; stale writes branch instead of overwriting a newer draft. A script save checks a persistent metadata/content version inside the SQLite transaction. A stale script base offers a diff or a new processor, never a force-save. Only the submitted draft version is cleared by saving; edits persisted during validation remain recoverable. Completion reconciles actual source content with the acknowledged draft, not just an input counter. Retrying an old save receipt never clears or rebases a later draft.

Per-processor branch preferences contain only draft identifiers in this tab's session storage, not source code. They survive a refresh and reset when a new launch ticket is exchanged. A missing remembered branch is not reused: recovery selects a sole remaining draft or asks when several remain. Automatic recovery reads the latest stored draft and does not force-overwrite another tab or bypass base-version checks.

The cancel control stops queued or validating jobs while keeping the draft. A save already committing cannot be cancelled. A visible tab renews/checks editing permission every five seconds, including while a job is active; requests time out after fifteen seconds. These are detection intervals, not guarantees of uninterrupted connectivity.

When the program disconnects, leave the tab open or export its current draft. Export/import uses `mqtt-plus.processor-draft/v1` JSON and imports as a new processor. Unsynced text is not guaranteed recoverable after a disconnected tab closes. Deleting a processor detaches its drafts, which can be recovered from **New Processor** as new scripts.

## Build and deployment

Install Node 22/npm and Qt 6.11 including `HttpServer`. CMake configuration runs frontend regression tests, builds `web/processor-editor`, and embeds its generated resources. Source, configuration, and lockfile changes trigger configuration again; changed dependency locks trigger `npm ci`. A failed test or build stops packaging instead of embedding stale assets. Installations need neither Node nor internet connectivity. CI installs both Node and Qt HTTP Server before building all platforms.

Generated Monaco JavaScript is excluded from Qt translation scanning. Update native translations with the `mqtt-plus_lupdate` target; browser labels use the application's selected language and their own English/Chinese strings. The obsolete native-editor translation entries are removed.

Bundled build resources under `/assets/` use long-lived immutable caching to avoid repeatedly downloading fonts and workers during navigation. The HTML entry point and all API responses, including errors and authorization exchanges, remain `no-store`; drafts, session tokens, and editor data are not placed in the HTTP cache.

Use the repository's normal configure, build, QML lint, and test commands. Browser and draft integration tests use temporary databases; the HTTP tests need loopback network permission. The test-only `test_browsereditorservice --serve-editor` mode starts a disposable local demo host without reading personal configuration.

The library schema is upgraded transactionally from version 1 to version 2. Version 2 adds processor change sequences and the save-request ledger. Previous applications reject the newer schema; back up the processor directory before downgrading. A separate `editor-drafts.db` uses schema version 1, rejects future versions, and stores draft branches. Quota checks and writes share an immediate transaction enforcing 100 drafts / 64 MiB of source. The library and draft databases are separate: a successful version commit can return a cleanup warning without rolling back the saved version. Draft count polling uses grouped SQL without decoding source packages.

## API conventions

All routes use `/api/v1`, JSON, and string identifiers. `/processors` and `/drafts?processorId=...` return summaries without source files; fetch `/processors/{id}` or `/drafts/{id}` for complete content. Draft updates/deletion require `If-Match: "<draftVersion>"`. Validate/save submits that exact version and returns `202` with `jobId`. Poll `/jobs/{id}`; `DELETE` cancels before commit. A save also requires a client-generated `requestId`.

`POST /editor/lease` with `windowId` acquires or renews the global editing lease and returns `editable`, `leaseMs`, and (only for the owner) `leaseToken`. `DELETE /editor/lease` conditionally releases it. All other non-GET routes require authenticated `X-Editor-Window` and `X-Editor-Lease` headers; missing, expired, foreign, and old-generation tokens return `423`. Storage writes recheck ownership on execution, and validated saves recheck before commit. Lease transfers serialize with already-admitted storage transactions, so a transfer cannot race a prior write. Existing Host/Origin and authorization checks still apply to lease operations.

`GET /jobs?requestId=...` finds an in-flight save for the current session. `GET /requests/{requestId}` retrieves its durable receipt after reconnecting. Check the receipt before retrying an uncertain save. Reusing an ID with different content is a conflict. Retained completed jobs are pruned at 32 records and no longer hold their input draft; save receipts remain in SQLite. `409` means a stale draft/base or conflicting request, `410` means a deleted processor, `413` means a quota/body limit, `422` means an invalid package or failed validation, and `429` means the job queue is busy.

## Security and limitations

The HTTP listener uses IPv4 loopback and a random port. A 60-second, one-use launch ticket is exchanged for a bearer session, then removed from the browser address. Tokens remain in per-tab session storage and expire when the application exits. Host and write-request Origin checks, body limits, connection limits, CSP, and a resource whitelist defend against unintended website access. They do not protect against malicious extensions or processes operating as the same local user.

Validation gets a fresh process, existing 50 ms runtime limits, and a five-second parent deadline. This isolates validator failure from the GUI, but is not a complete OS sandbox and does not move normal MQTT script execution out of process.

Implementation verification on macOS includes the full Qt suite, QML lint, TypeScript/Vite build, loopback authentication, stale saves, replay, draft restart recovery, deletion, invalid packages, and runaway validation. See the [verification record and release gates](browser-editor-verification.md). Release acceptance still requires Safari/Windows/Linux package checks, native input-method testing, and the specified high-traffic p95/p99 performance comparison. These are not claimed by the local tests.
