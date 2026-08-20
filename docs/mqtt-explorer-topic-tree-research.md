# MQTT Explorer 主题树设计调研

> 调研对象是 **MQTT Explorer**（用户原文中的 “mqtt export” 应为笔误）。结论基于官方仓库
> `thomasnordquist/MQTT-Explorer` 的 `master` 分支快照
> [`35f3197`](https://github.com/thomasnordquist/MQTT-Explorer/tree/35f31973c456a024a37450f5961e4610dc9a9ce0)
>（2026-01-30）以及[官方产品页](https://mqtt-explorer.com/)，并与本项目调研时的实现逐项对照。

> 实施状态（2026-08-20）：本报告识别出的 exact value/subtree activity 混用、10k 满额拒绝新 topic、
> 双角色节点主动作歧义和 payload 不可搜索问题已在随后修改中处理。第 6、7 节保留修改前的问题分析与决策依据。

## 结论摘要

MQTT Explorer 采用一棵**内存前缀树（trie）**：topic 的每个 `/` 分段是一条 `Edge`，节点同时保存
“该完整 topic 的当前消息”和子边。因此 `a/b` 与 `a/b/c` 可以同时存在，`b` 不必在“文件夹”和“叶子”
之间二选一。新消息先进入有上限的变更缓冲区，再按固定节拍、在 UI 空闲时批量合并；React 侧又做自适应
刷新节流、折叠分支按需挂载和分阶段子节点渲染。它不是虚拟树，也不把接收历史持久化为树的事实来源。

搜索不是在当前组件树上隐藏行，而是从完整连接树中找出匹配的**消息节点**，克隆这些节点并补齐祖先路径，
构造一棵独立过滤树；过滤期间的新消息也通过同一谓词进入过滤树。这个方案让搜索结果保留层级上下文，
代价是多一棵树、一次额外事件订阅和一次全量叶节点扫描。

## 1. 树模型与合并规则

### 1.1 节点、边与路径

- `TreeNode` 有 `edges`（按分段名 O(1) 查找）和 `edgeArray`（保持插入/展示顺序）；边保存名称、父节点和子节点。
  节点另有 `message`、`messageHistory`、累计 `messages`、`lastUpdate` 和 QML/React 无关的 `viewModel`。
  见 [`TreeNode.ts`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/backend/src/Model/TreeNode.ts#L5-L51) 与
  [`Edge.ts`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/backend/src/Model/Edge.ts#L5-L46)。
- 收到消息后直接执行 `topic.split('/')`，逐段建边，再把消息挂到最后一个节点；`path()` 沿父链取边名并以
  `/` 拼回。因此前导 `/`、连续 `//` 形成的空层级不会被主动规范化或折叠。
  见 [`TreeNodeFactory.ts`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/backend/src/Model/TreeNodeFactory.ts#L7-L44) 与
  [`TreeNode.ts#path`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/backend/src/Model/TreeNode.ts#L151-L169)。
- 合并以同名边递归下钻；不存在的边整枝挂入，抵达同一路径时替换 `message`，并把消息加入历史、累计计数。
  见 [`mergeEdges/updateWithNode`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/backend/src/Model/TreeNode.ts#L82-L105) 和
  [`setMessage/updateWithNode`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/backend/src/Model/TreeNode.ts#L143-L214)。

### 1.2 “既是叶子又是父节点”

这是模型原生支持的状态，而不是特殊分支：例如先收到 `a/b/c`，再收到 `a/b`，后一次合并会把消息写到
已有 `b` 节点，但保留其 `c` 子边。官方单测明确覆盖了这个顺序。
见 [`TreeNode.spec.ts`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/backend/src/Model/spec/TreeNode.spec.ts#L28-L47)。

UI 同样同时渲染展开箭头和当前值：箭头由 `edgeCount()>0` 决定，值由本节点非空消息决定。因此父 topic
可被选中、查看/发布，也可展开子树。
见 [`TreeNodeTitle.tsx`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/app/src/components/Tree/TreeNode/TreeNodeTitle.tsx#L29-L90)。

### 1.3 空载荷与移除

`hasMessage()` 要求 payload 存在且长度非零。任何空载荷更新（代码没有再检查 `retain`）都会让该节点不再算作
topic；若它也是叶节点，就从父节点移除，并可能向上递归清理空祖先；若仍有子节点，则保留为纯中间节点。
见 [`hasMessage/removeFromTreeIfEmpty`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/backend/src/Model/TreeNode.ts#L106-L128) 和
[`remove/update`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/backend/src/Model/TreeNode.ts#L189-L214)。

这与 MQTT 的 retained-delete 操作相契合，但也意味着**普通非 retained 的零长度业务消息也会从树中消失**。
递归删除功能则显式向本节点和所有有消息的后代发送 QoS 0、retain=true、空 payload，并以 20ms 间隔限速。
见 [`clearTopic.ts`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/app/src/actions/clearTopic.ts#L8-L52)。

## 2. 当前值、历史与聚合

- 节点的 `message` 永远是最后一次收到的消息；`retain`、QoS、messageId 和接收时间都是该最后消息的属性。
  **没有独立的“broker 中仍有 retained 值”状态**。后续非 retained 实时消息会覆盖 `message.retain`，所以界面上的
  Retained 标识描述的是最后一次投递，而不是对 broker retained store 的持续镜像。
  见 [`Message.ts`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/backend/src/Model/Message.ts#L5-L19) 与
  [`ValuePanel.tsx`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/app/src/components/Sidebar/ValueRenderer/ValuePanel.tsx#L37-L62)。
- 每个节点保留一个环形历史：最多 **100 条**且总计最多 **20,000 个 base64 字符长度**，先触及任一限制就淘汰最旧消息。
  `messages` 累计数不随历史淘汰而下降。见 [`TreeNode.ts`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/backend/src/Model/TreeNode.ts#L8-L17) 与
  [`RingBuffer.ts`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/backend/src/Model/RingBuffer.ts#L5-L88)。
- `leafMessageCount()` 实际是“本节点及后代从连接以来累计收到的消息数”，不是叶节点数；`childTopicCount()`
  是子树中当前具有非空消息的节点数（包括自己）。两者缓存于节点，在消息合并或边变化时失效。
  见 [`TreeNode.ts` 聚合方法](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/backend/src/Model/TreeNode.ts#L216-L248)。
- 折叠行显示上述“topic 数 + 累计消息数”，并在行尾预览当前值（最多 400 字符）；官方产品页也把
  “topic/activity 可视化、每 topic 历史、当前/上一消息 diff、数值绘图”列为核心能力。
  见 [`TreeNodeTitle.tsx`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/app/src/components/Tree/TreeNode/TreeNodeTitle.tsx#L29-L105) 与
  [官方功能列表](https://mqtt-explorer.com/)。

## 3. 排序、搜索和过滤

### 3.1 排序

同级节点支持四种顺序：

1. `none`：沿 `edgeArray` 的首次插入顺序；同 topic 后续更新不改变位置。
2. `abc`：对分段名执行 `localeCompare`。
3. `messages`：按 `leafMessageCount()` 降序，即按子树累计流量排序。
4. `topics`：按 `childTopicCount()` 降序，即按当前有值的 topic 数排序。

实现见 [`sortedNodes.tsx`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/app/src/sortedNodes.tsx#L5-L21)。后两种指标会随消息到达而变化，
所以节点可能在刷新时重排；代码没有稳定次排序键。

### 3.2 搜索树重建

搜索流程见 [`Settings.ts#filterTopics`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/app/src/actions/Settings.ts#L97-L145)：

1. 遍历完整连接树的 `childTopics()`，这里只返回当前有 payload 的节点。
2. 对完整路径做不区分大小写的子串匹配；否则尝试匹配 payload 的 Unicode 文本。
3. 对每个命中节点做“不带父子连接的克隆”，再按原路径补建祖先，最后合并成一棵新树。
4. 新树订阅同一连接事件，并用同一 `nodeFilter` 过滤后续消息；清空搜索时切回原连接树。
5. 按结果规模计算自动展开阈值，使小结果集更容易直接展开。

由此得到的行为边界：

- 匹配是普通子串，不支持 MQTT `+/#`、glob 或正则；父级名称能通过“完整路径”命中。
- 只克隆命中的消息节点及其祖先上下文，不会因为父节点命中而自动带出所有后代。
- 克隆保留该节点当前值、历史、累计计数和更新时间，但只保留匹配分支。
- payload 搜索使用解码后的 Unicode 原文，不搜索格式化 JSON、hex 展示或自定义 decoder 的展示文本。
- 代码把 payload 转成小写后，却用原始 `filterStr` 比较；因此带大写字符的查询可能无法按预期匹配 payload，
  而路径搜索没有这个问题。这是从实现直接推导出的边界，不是文档承诺。

## 4. 更新与性能策略

消息进入 UI 树之前有两级节流：

1. **模型层批处理**：事件回调只把消息压入 `ChangeBuffer`。缓冲估算上限约 100MB（payload 的 base64 长度 +
   每条 24 字节估算开销）；满后新消息不再入队。每 300ms 检查一次，在 `requestIdleCallback` 中一次性取出并
   顺序合并全部消息；`applyChangesHasCompleted` 防止重叠批次。见
   [`ChangeBuffer.ts`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/backend/src/Model/ChangeBuffer.ts#L8-L43) 与
   [`Tree.ts`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/backend/src/Model/Tree.ts#L18-L93)。
2. **视图层自适应刷新**：树记录最近渲染耗时的 10 秒移动平均，下一次刷新至少间隔 300ms，慢渲染时扩大到
   预测耗时的 7 倍，并再次借助 idle callback 更新 React 状态。见
   [`Tree/index.tsx`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/app/src/components/Tree/index.tsx#L12-L163)。

暂停只停止“合并”，不停止接收；消息继续堆在同一有限缓冲中，恢复后批量应用。官方 release notes 也明确称其为
“Buffer changes while in pause mode”。见 [`Tree.ts#pause/resume`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/backend/src/Model/Tree.ts#L67-L93)、
[`Tree actions`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/app/src/actions/Tree.ts#L91-L120) 和
[v0.2.6 release notes](https://github.com/thomasnordquist/MQTT-Explorer/releases/tag/v0.2.6)。

树 UI 并非虚拟列表。它依赖折叠分支不挂载后代，以及展开时先渲染 10 个直接子节点，再在 idle callback 中将
配额提高到至少 25、之后约 1.5 倍递增。见
[`TreeNodeSubnodes.tsx`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/app/src/components/Tree/TreeNode/TreeNodeSubnodes.tsx#L17-L66)。
这能降低突发首次渲染成本，但巨大且完全展开的树最终仍会创建全部对应 React 节点。

## 5. 树上的主要交互

- 桌面端点击一行会同时选择并展开/折叠；移动端点击行只选择，展开按钮单独操作。选择节点会同步发布表单的 topic。
  可选“鼠标悬停选择”。见 [`TreeNode/index.tsx`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/app/src/components/Tree/TreeNode/index.tsx#L49-L105) 与
  [`Tree actions`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/app/src/actions/Tree.ts#L14-L51)。
- 方向键按当前排序在“已挂载/可见”节点间导航：左右用于折叠、展开或移到相邻可见节点，上下遍历树序。
  见 [`visibleTreeTraversal.ts`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/app/src/actions/visibleTreeTraversal.ts#L8-L143)。
- 新近更新节点可闪烁提示，但只处理视口内、最近 3 秒、非选中且不是刚切换选择的行，避免无意义动画开销。
  见 [`useAnimationToIndicateTopicUpdate.tsx`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/app/src/components/Tree/TreeNode/effects/useAnimationToIndicateTopicUpdate.tsx#L4-L38)。
- 自动展开以“直接子边数 <= 设置阈值”为判据，而不是按深度或子树总规模；根始终展开。
  见 [`useIsAllowedToAutoExpandState.tsx`](https://github.com/thomasnordquist/MQTT-Explorer/blob/35f31973c456a024a37450f5961e4610dc9a9ce0/app/src/components/Tree/TreeNode/effects/useIsAllowedToAutoExpandState.tsx#L4-L16)。

## 6. 与本项目修改前实现对比

本项目当前实现位于 [`TopicTreeModel`](../src/models/topictreemodel.h)、
[`TopicObservation`](../src/domain/topicobservation.h) 和
[`TopicTreePanel.qml`](../qml/features/topics/TopicTreePanel.qml)。两边都以 `/` 分段构建前缀树，都保留空层级，
也都允许 `isTopic && hasChildren`，但它们实际上服务于不同的事实模型：MQTT Explorer 偏向“本次连接的当前值 +
短历史”，本项目是“SQLite 收件历史中观察到过的 topic 索引”。

| 维度 | MQTT Explorer | 本项目 | 影响 |
|---|---|---|---|
| 精确 topic 的值 | `TreeNode.message/history` 只属于该节点的**精确完整路径** | 一条 observation 的 `latestHistoryId/latestPayloadPreview/lastSeenMs` 会写到终点及所有祖先 | 祖先若同时也是精确 topic，会被更新更晚的子孙值覆盖，无法区分“`a/b` 的值”和“`a/b` 子树最近活动” |
| 父/叶双重角色 | 同一行同时显示自己的值和展开器；桌面点击会选择精确 topic 并切换展开 | 模型能表达双角色，但 `primaryAction()` 先判断 `hasChildren`，所以双角色行主点击只展开/折叠；精确过滤只能进菜单 | 用户看到该行有 preview，却不能用主要动作打开这个精确 topic，行为与显示语义不一致 |
| retained/空载荷 | 最新消息携带 `retain` 和长度；任意零长度 payload 都被当作无当前值，空叶被裁剪 | `TopicObservation` 只有 topic、historyId、时间和 preview，没有 retain/retain-known/payload-size | 本项目不能表达 retained tombstone；同时它本来就是持久化 observed-history tree，不应直接照抄 MQTT Explorer 的“任意空载荷删叶”规则 |
| 生命周期 | 连接内内存树，断开后重建；无新消息的旧 topic 会一直留到连接结束或空载荷删除 | 启动/切 session 从 SQLite 每 topic 最新收件记录恢复，实时 observation 继续追加 | 本项目显示的是“曾观察到”，不是 broker 当前 namespace 或 retained store 镜像；UI 文案 “observed topics” 与此一致 |
| 容量 | 单 topic 历史限 100 条/20k base64 字符；待合并队列约 100MB，满时丢新消息 | 最多 10,000 个精确 topic、50,000 个树节点；没有淘汰，达到上限后所有新 topic 都拒绝，已有 topic仍可更新 | 实时运行达到 10k 后，更晚出现的新 topic 永久不可见，直到切 session/显式 reset；reset 才从库中重新选最近 10,000 个 |
| 更新批次 | 300ms 模型批合并 + 自适应 UI 节流 | 应用层单次 100ms `QTimer` 合并 observation；结构变化 reset 可见行，纯值变化定点 `dataChanged` | 本项目的 Qt model 更新路径更直接，但结构高频变化仍可能频繁 reset |
| 可见列表 | React 递归树；折叠不挂载，展开分阶段渲染，最终不虚拟化 | 扁平 `m_visibleRows` + QML `ListView.reuseItems` | 本项目在大量可见行上的视图虚拟化更合适 |
| 排序 | 插入序、locale 字母序、子树累计消息数、子树 topic 数可选 | 子节点用 `QMap`，固定按 segment 字典序 | 本项目顺序稳定，但不能按活跃度/规模探索 |
| 搜索 | 路径或 payload 子串；克隆匹配 exact-topic 并补祖先，过滤期间继续接收 | 在原树递归匹配 segment/fullTopic，生成扁平可见行；自动展开命中路径，不改变原展开集 | 本项目不复制树、内存更省，但不搜索 payload；两者都保留祖先上下文 |

### 6.1 关键语义差异：exact value 与 subtree activity

MQTT Explorer 只在合并抵达**精确路径终点**时调用 `setMessage()`；祖先的 `lastUpdate` 会因合并而刷新，但祖先
不会借用子孙的 message。这样节点天然有两组互不混淆的信息：自己的 message，以及由子树汇总的 activity/count。

本项目在 [`applyObservations()`](../src/models/topictreemodel.cpp) 中收集整条 `pathNodes`，随后把同一 observation 的
`latestHistoryId`、`lastSeenMs`、`latestPayloadPreview` 写入每个路径节点；测试
[`newestObservationUpdatesLeafAndAncestors()`](../tests/test_topictreemodel.cpp) 也把该传播行为固定为契约。这让 branch
活动摘要很便宜，但对双角色节点存在实质歧义：

```text
a/b       payload = "parent"
a/b/c     payload = "child"，稍后到达
```

此时 `a/b.isTopic == true`，但其 preview/historyId 指向 `a/b/c`。点击或菜单若把 historyId 当作 `a/b` 的精确消息，
会打开错误 topic 的消息详情。

建议把节点状态拆成两组，避免继续复用一个字段表达两件事：

- `exactLatestHistoryId / exactLastSeenMs / exactPayloadPreview`：只在终点节点更新，仅供精确 topic 的值和详情动作使用。
- `subtreeLatestHistoryId / subtreeLastSeenMs`（preview 通常不必传播）：沿祖先更新，仅供活动点、子树排序和摘要使用。

UI 上 branch-only 节点显示子树活动；双角色节点显示自己的 exact preview，同时仍可用 activity indicator 表示后代更新。

### 6.2 关键语义差异：retained tombstone 与 observed-history tree

本项目的持久层 `mqtt_messages` 已保存 `retain`、`retain_known`、`payload_size`，但
[`loadLatestIncomingTopics()`](../src/services/storage/historystore.cpp) 投影到 `TopicObservation` 时丢掉了这些字段。
所以主题树目前无法区分：

- retained 空 payload（可解释为删除 broker retained value）；
- 非 retained 空 payload（仍是一条合法且应留在历史中的 observation）；
- payload 因策略未保存/preview 为空；
- 真正的零字节 payload。

MQTT Explorer 的做法是简单但有损的：不检查 retain，零长度一律清当前值并裁剪空叶。本项目既然能跨重启恢复，
更稳妥的定位是明确保持 **observed-history tree**，不要暗示它是 broker 当前 retained 树。若产品需要 retained 视图，
应先让 observation 带上 `retainKnown`、`retain`、`payloadSize`，再另建 exact retained-state 语义：只有已知 retained 的
零长度消息产生 tombstone；普通零长度消息仍作为历史 observation 存在。历史发现状态和 broker retained 当前状态
不应共用一个 `isTopic` 布尔值。

### 6.3 容量与交互差异

`kMaximumTopicCount = 10'000` 保护了内存，但 [`applyObservations()`](../src/models/topictreemodel.cpp) 在满额后只会
`continue`，没有淘汰最旧 exact topic。已有 topic 永远占槽，新 topic 即便比树内全部节点更新也不会进入；当前测试
只验证 reset 时“最新 10,000 胜出”，没有覆盖“达到上限后的第 10,001 个实时新 topic”。这与 SQLite 查询的
“最近 10,000 个 topic”窗口语义不一致。

交互上，[`primaryAction()`](../qml/features/topics/TopicTreePanel.qml) 对 `hasChildren` 的优先级高于 `isTopic`。因此
双角色节点点击只展开，精确 topic filter 需要右键/更多菜单；而单纯叶节点点击会直接过滤。这种随是否拥有子节点而
改变主动作的设计会在树成长后悄然改变用户习惯。

## 7. 优先级建议

### P0：先修正数据语义

1. **拆分 exact value 与 subtree activity。** 终点才更新 exact 字段，祖先只更新 activity 字段；迁移 QML 的
   preview、详情 historyId、recentlyActive 分别读取正确角色，并加入“同一节点既是 topic 又是 branch”的回归测试。
2. **明确树的产品定义并补齐 observation 契约。** 当前 UI 应明确是 observed-history tree。若要支持 retained 当前态，
   给 `TopicObservation` 增加 `retainKnown/retain/payloadSize`，把 retained-state 独立建模；不要复制 MQTT Explorer
   “任意零长度 payload 都删叶”的有损规则。

### P1：修复满额和双角色交互

3. **让 10k 限制成为滚动窗口而非永久拒绝。** 最小方案是在首次因新 topic 截断后调度一次有节流的数据库 reset，
   重新选最新 10,000；更完整的方案是按 exact topic 最近活动做 LRU/最小堆淘汰，并递归裁剪无用祖先。新增测试应
   证明达到 10k 后到达的更新 topic 能进入，最旧 topic 被替换，已有 topic 更新不额外占槽。
4. **把展开和精确 topic 动作分开。** chevron 只负责展开；行主体在 `isTopic` 时选择/过滤精确 topic，branch-only
   行主体才可展开。双角色节点同时保留“this topic”和“this subtree”菜单动作，键盘 Enter/Space 也遵循同一规则。

### P2：再补探索能力与压力验证

5. 在 exact/subtree 指标拆分后，再考虑按子树活跃度或 topic 数排序，避免现在用传播 preview 冒充活动聚合。
6. 增加深路径、连续空层级、结构高频变化、搜索中持续到达新 topic、50k node 上限和超大同级列表的性能测试；
   特别观察结构变化触发 `beginResetModel()` 时的选择、滚动位置和展开状态。

## 资料范围与可信度

本报告只使用 MQTT Explorer 官方站点、官方 GitHub 仓库源代码、仓库测试和官方 release notes；没有采用第三方
评测或社区帖子。源代码链接固定到上述 commit，避免 `master` 后续变化导致结论与引用错位。产品页描述的是
用户可见能力，涉及精确语义与边界时均以源码为准。
