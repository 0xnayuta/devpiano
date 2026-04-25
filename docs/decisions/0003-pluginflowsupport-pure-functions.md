# ADR 0003: `PluginFlowSupport` 保持纯函数约束

## 状态

已采用。

## 背景

`source/Plugin/PluginFlowSupport` 是 `MainComponent` 插件流程职责收敛的第一步成果，将"构建启动恢复计划"、"扫描路径可用性判断"、"路径 fallback"、"scan + apply 原子操作"等纯决策逻辑从 `MainComponent` 提取为 `devpiano::plugin` 命名空间下的自由函数（free functions）。

这是一个有明确边界意识的提取：**所有函数都是纯函数（输入 → 输出，无副作用），不持有任何成员变量，依赖通过参数显式传递**。

## 决策

`PluginFlowSupport` 必须保持为纯函数命名空间，不持成员变量。

具体约束：

- 所有函数签名接收所需的全部数据作为参数，不隐式依赖外部状态。
- 若需要与 `PluginHost`、`AppSettings`、`MainComponent` 等对象交互，必须通过 callback 或参数显式注入，不在内部持有指针或引用。
- 若将来需要管理多个依赖项，优先将其建模为**明确注入的策略类**（如 `PluginStartupPolicy`），而不是往 `PluginFlowSupport` 里加成员变量。
- 若将来逻辑继续膨胀，提升为独立的协作类（如 `PluginStartupManager`），但 `PluginFlowSupport` 本身保持 stateless。

添加成员变量须经过 architecture review。

## 原因

保持 `PluginFlowSupport` 为纯函数集合有以下优势：

- **数据流完全显式**：调用方清楚看到什么输入产生什么输出，不需要追踪隐式状态。
- **可测试性强**：每个函数都可以用裸参数直接调用，不需要 mock 任何对象。
- **不会成为第二个 `MainComponent`**：`MainComponent` 膨胀的根本原因是它同时持有太多依赖项并直接操作它们。纯函数命名空间天然避免了这个问题。
- **边界清晰**：`PluginFlowSupport` 的使用者（目前是 `MainComponent`）知道这些函数不会偷偷修改任何内部状态。

若违反此约束，`PluginFlowSupport` 会变成一个隐式耦合的"助手对象"，调用方不知道它背后碰了哪些状态，测试困难，且等于把 `MainComponent` 的耦合问题换了个地方重新引入。

## 影响

正面影响：

- `PluginFlowSupport` 的测试始终是纯单元测试，不需要集成环境。
- 后续任何开发者都知道这个模块的边界：只做数据变换，不做状态管理。
- 即使将来提升为独立类，迁移路径也是显式且低风险的。

代价：

- 需要 discipline 约束：明确禁止往 `PluginFlowSupport` 里加成员变量，改设计时需要先 review 此 ADR。

## 当前实践

`PluginFlowSupport` 当前包含以下纯函数：

- `makePluginRecoverySettings(pluginSearchPath, lastPluginName)` → 构建恢复视图
- `withPluginRecoveryPathFallback(recovery, defaultPath)` → 路径空时 fallback
- `isUsablePluginScanPath(path)` → 路径是否有效
- `buildStartupPluginRestorePlan(persistedRecovery, defaultPath)` → 构建启动恢复计划
- `restorePluginsAtPath(pluginHost, path, recovery, applySettingsCallback)` → scan + apply 原子操作

所有涉及副作用的操作（`PluginHost.scanVst3Plugins`、设置保存、UI 刷新）均通过参数或 callback 注入，不由 `PluginFlowSupport` 内部发起。