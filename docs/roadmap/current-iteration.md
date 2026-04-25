# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。  
> 读者：当前开发者、协作者、代码审查者。  
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前状态

当前活跃迭代：**MainComponent 职责收敛与键盘映射完善前置重构**。

上一轮已完成：`docs/` 最小重组。记录已归档到：[`../archive/docs-restructure-2026-04.md`](../archive/docs-restructure-2026-04.md)。

## 本轮目标

本轮以小步重构和验证收敛为主，不引入录制 / 回放等新主功能。

核心目标：

- 在不改变现有演奏、插件加载和设置恢复行为的前提下，继续削薄 `source/MainComponent.*`。
- 优先收敛插件扫描 / 加载 / 卸载 / 恢复流程的职责边界，为后续拆分 `PluginScanner`、插件实例生命周期宿主或 controller 做准备。
- 补齐默认键盘布局的未验证项，降低后续布局编辑 / Preset 功能的回归风险。
- 继续使用现有 `AppState` / UI state builder / Settings model 边界承载状态，不把新状态直接堆回 `MainComponent`。

## 本轮优先任务

### 1. 键盘映射默认布局回归

对应文档：[`../testing/keyboard-mapping.md`](../testing/keyboard-mapping.md)、[`../features/keyboard-mapping.md`](../features/keyboard-mapping.md)。

当前进展：

- [x] 已完成默认布局代码级 / 静态映射校验，并记录到 [`../testing/keyboard-mapping.md`](../testing/keyboard-mapping.md) 的 `Test Run 2026-04-24`。
- [x] 默认可见音区下，Q、A、Z、数字行已通过手工发声、高亮、note on/off、Shift / Caps Lock / 中文输入法回归。
- [x] 已定位并修复虚拟键盘翻页后 JUCE 内置 QWERTY 映射与项目映射叠加的问题。
- [x] 已完成 [`../testing/keyboard-mapping.md`](../testing/keyboard-mapping.md) 的 6.3 Windows 侧人工回归，翻页后所有行按键均无明显问题。

优先执行并更新以下未验证项：

- Q 行默认映射：`Q/W/E/R/T/Y/U/I/O/P`。
- Z 行默认映射：`Z/X/C/V/B/N/M`。
- 数字行默认映射：`1/2/3/4/5/6/7/8/9/0`。
- 多键组合中尚未完整验证的 Q 行组合。
- Shift / Caps Lock / 中文输入法场景下，默认布局是否仍稳定。
- fallback synth 与 VST3 插件两种发声模式下，键盘映射是否一致可用。

验收结果只更新测试文档；若发现功能缺陷，再进入代码修复。

### 2. 插件生命周期基线回归

对应文档：[`../testing/plugin-host-lifecycle.md`](../testing/plugin-host-lifecycle.md)、[`../features/plugin-hosting.md`](../features/plugin-hosting.md)。

当前进展：

- [x] 已完成基础插件生命周期测试：扫描后加载、连续加载 / 卸载、已加载状态下重扫，均未发现明显问题。
- [x] 已完成 plugin editor 生命周期测试：打开 / 关闭 editor、打开 editor 后卸载、打开 editor 后重扫，均未发现明显问题。
- [x] 已完成 `5.1` 音频设备设置切换路径验证：窗口可打开、保存、关闭，插件链路可继续发声；不同 `Audio device type` 下的 `buffer size` / `sample rate` 可调范围已确认为当前 Windows / JUCE 后端语义差异，不构成产品缺陷。
- [x] 已完成 `5.2` 连续执行 load / unload / scan，当前未发现明显问题。
- [x] 已完成 `6.1` 与 `6.2` 退出路径验证，当前未发现明显问题。
- [ ] `6.3` 因缺少外部 MIDI 设备暂未验证。

本轮已覆盖并记录以下高风险场景：

- 扫描后加载插件并试弹。
- 连续加载 / 卸载同一插件 3~5 次。
- 已加载插件状态下重新扫描。
- 打开 editor 后卸载插件。
- 打开 editor 后重新扫描。
- 打开 editor 或已加载插件后直接退出程序。
- 加载插件后切换音频设备设置。

本轮已基本建立插件生命周期基线；当前剩余重点转为：

- 在具备外部 MIDI 设备后补齐 `6.3` 退出场景。
- 若后续出现稳定复现的问题，再针对 `PluginHost` / editor window / audio device rebuild 路径做小步修复。

### 3. `MainComponent` 插件流程职责收敛

对应代码：`source/MainComponent.*`、`source/Plugin/PluginHost.*`、`source/Plugin/PluginFlowSupport.*`。

本轮建议只做低风险拆分：

- 先梳理插件相关方法的边界：扫描路径解析、启动恢复、加载、卸载、editor 窗口、音频设备重建。
- 优先抽离不直接依赖 UI 组件的插件流程 helper / controller。
- 暂时保留 `MainComponent` 对按钮回调、只读 UI 刷新、设置保存和 editor 窗口拥有权的协调职责。
- 避免一次性拆分 `PluginHost` 内部扫描与实例生命周期；先用生命周期测试结果决定是否需要拆。

完成标准：

- 现有插件扫描、选择、加载、卸载、editor 打开行为不回退。
- `MainComponent.cpp` 插件相关流程代码减少或边界更清晰。
- LSP diagnostics 无新增错误。
- WSL 构建通过。
- Windows MSVC 构建通过。

当前进展：

- [x] **Step 1**（第一层提取）：新增 `source/Plugin/PluginFlowSupport.h/.cpp`，将 `buildStartupPluginRestorePlan`、`withPluginRecoveryPathFallback`、`isUsablePluginScanPath`、`makePluginRecoverySettings` 从 `MainComponent` 提取为 `devpiano::plugin` 命名空间纯函数；`MainComponent` 仍保留启动恢复编排权，但决策逻辑已下沉。
- [x] **Step 2**（第二层提取）：
  - 新增 `restorePluginsAtPath(pluginHost, path, recovery, applySettingsCallback)` 到 `PluginFlowSupport`，将"scan + applySettings"序列封装为独立原子操作。
  - `restorePluginScanPathOnStartup` 和 `restoreLastPluginOnStartup` 改为接收 `const StartupPluginRestorePlan&`，直接使用 plan 中已解析的 `plan.recovery`（pluginSearchPath / lastPluginName），消除了启动路径中重复调用 `getPluginRecoverySettingsWithFallback()` 的冗余。
  - `scanPluginsAtPathAndApplyRecoveryState` 改用 `restorePluginsAtPath` 组合，代码更简洁。
  - `MainComponent` 仍持有 `applyPluginRecoverySettings` 调用权（需要 `appSettings`），所有 `PluginFlowSupport` 辅助函数均以 callback 形式注入此能力。

### 4. 录制 / 回放仅做模型预研，不接入主链路

对应后续方向：[`../roadmap/roadmap.md`](../roadmap/roadmap.md) 的 M6。

本轮不实现录制、回放、MIDI/WAV 导出。若有余力，仅允许做轻量设计草案：

- 现代演奏事件模型的职责边界。
- 键盘输入、外部 MIDI、插件发声与录制事件之间的关系。
- 与旧 `song.*` 的行为参考边界。

不得直接复制旧实现，也不要把未实现能力写成已完成。

## 本轮计划验证命令

代码修改后优先执行：

```bash
./scripts/dev.sh wsl-build
```

如修改影响 CMake / 编译数据库，先执行：

```bash
./scripts/dev.sh wsl-build --configure-only
```

涉及插件生命周期、Windows 行为或 MSVC 兼容性时执行：

```bash
./scripts/dev.sh win-build
```

## 后续候选文档任务

以下不是本轮代码重构的优先项，仅在需要时补充：

- 继续提炼 ADR：
  - JUCE 音频后端决策。
  - VST3-first 插件宿主决策。
- 按需补充轻量文档入口：
  - [`../index/glossary.md`](../index/glossary.md)：术语表。
  - [`../testing/known-issues.md`](../testing/known-issues.md)：已知问题与待验证风险。
  - [`../getting-started/overview.md`](../getting-started/overview.md)：项目概览。
  - [`../getting-started/current-status.md`](../getting-started/current-status.md)：当前状态摘要。
  - [`../development/troubleshooting.md`](../development/troubleshooting.md)：开发环境与构建问题排查。
  - [`../development/agent-collaboration.md`](../development/agent-collaboration.md)：从根目录 [`../../AGENTS.md`](../../AGENTS.md) 提炼的人类可读协作规则摘要。
- 后续如架构文档继续增长，再拆分音频链路、插件宿主、键盘映射、状态模型等专题。
