# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。  
> 读者：当前开发者、协作者、代码审查者。  
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前状态

当前活跃迭代：**M8 MIDI 文件导入与回放兼容性增强**。M6 录制 / 回放 / MIDI 导出 MVP（含 WAV 离线渲染 M6-6a/b/c/d）和 Section 5 MainComponent 录制/回放/导出状态流收敛（MC-1..MC-4）已完成。

上一轮已完成：MainComponent 插件流程初步收敛、键盘映射默认布局回归、布局 Preset 核心能力、录制 / 停止 / 回放 / MIDI 导出 / WAV 离线渲染（M6-6a/b/c/d）全部收尾，E.1–E.9 全部通过。

当前项目已经从"主链路打通 / MVP 恢复"进入 M8 MIDI 文件能力补齐阶段。

---

**搁置说明**：M6 录制/回放稳定化（见本轮 Section 1 最后 1 项边界检查）和外部 MIDI 硬件依赖验证（见 Section 2）因当前无真实外部 MIDI 设备，暂无法测试。已将状态记录至 `docs/testing/recording-playback.md`（包 C、7.1、已知限制）。这两个方向暂从本迭代移除，恢复时间取决于硬件条件。

## 本轮目标

本轮聚焦 M8 MIDI 文件导入 MVP 与外部 MIDI 兼容性增强。

已完成：

- [x] M8-1：Import MIDI 按钮、MIDI 文件导入为 `RecordingTake`、导入后回放、最近导入路径记忆。
- [x] M8-1b：自动选择含 note 最多的轨道（兼容性修正），2026-05-01 人工验收通过。
- [x] M8-7：主窗口尺寸自适应与恢复（UI polish），2026-05-01 人工验收通过。

M8-1b 人工验收记录（2026-05-01）：

- [x] track 0 只有 tempo/meta、track 1+ 有 note 的 MIDI 文件可自动选择有 note 的轨道并回放。
- [x] Logger 输出总轨数、每轨 note 事件数、选中的 track index、被忽略轨道数。
- [x] 所有轨道都没有 note 时安全返回失败并写 Logger，不崩溃。
- [x] 本程序导出的单轨 MIDI 文件仍正常导入回放。

M8-7 人工验收记录（2026-05-01）：

- [x] 首次启动或无保存状态时，窗口默认尺寸能完整呈现当前主要 UI 内容。
- [x] 用户手动调整窗口大小并关闭程序后，下次启动能恢复该窗口尺寸。
- [x] 保存的异常尺寸不会导致窗口不可见或小到不可操作。
- [x] 不高频写盘；窗口尺寸保存使用已有 settings 保存节流或关闭时保存。

剩余方向：

- M8-6：MIDI playback 虚拟键盘可视化（M8-later，可选）。
- M8-5：合并所有轨道 note 到单一 timeline（后续增强，暂不默认启用）。

已搁置（待硬件条件恢复）：

- M6 录制/回放稳定化最后 1 项边界检查。
- 外部 MIDI 硬件依赖验证（已记录至 `docs/testing/recording-playback.md`）。

## 本轮优先任务

### 3. WAV 离线渲染 MVP 设计

对应后续方向：[`roadmap.md`](roadmap.md) 的 M6-6。

对应文档：

- [`../features/M6-recording-playback.md`](../features/M6-recording-playback.md)
- [`../testing/acceptance.md`](../testing/acceptance.md)

**状态**：M6-6a/b/c/d 全部完成，E.1–E.9 全部通过。VST3 插件离线渲染（M6-6e）后置。

已完成范围：

- [x] 定义 `RecordingTake -> offline render loop -> WAV file` 的最小链路。
- [x] 第一切片支持 fallback synth 离线渲染（M6-6b）。
- [x] UI 接入 Export WAV 按钮 + FileChooser + 选项收集（M6-6c）。
- [x] 专项测试包 E（9 项）全部通过（M6-6d）。

后置（M6-6e）：

- [~] 第二切片：当前已加载 VST3 插件的离线渲染（暂不阻塞 MVP）。

明确不做：

- [x] 不做 MP4 / 视频导出。
- [x] 不做复杂编辑、钢琴卷帘、多轨工程、tempo map。
- [x] 不复刻旧 FreePiano `song.*` / `export.*` 的平台相关结构。

完成标准：

- [x] 形成可执行的 M6-6 实现切片。
- [x] 验收文档中 WAV 导出已标记为 `[x]` 完成。

### 4. 插件扫描产品化增强排期

对应文档：

- [`../features/M3-plugin-hosting.md`](../features/M3-plugin-hosting.md)
- [`../testing/plugin-host-lifecycle.md`](../testing/plugin-host-lifecycle.md)
- [`roadmap.md`](roadmap.md) 的 M2 / M3 后续增强。

本轮不优先实现大改，只做排期和小步准备。排期已落到 [`../features/M3-plugin-hosting.md`](../features/M3-plugin-hosting.md) 的“插件扫描产品化 backlog（排期）”：

- [x] M3-P1：记录扫描失败文件，而不只是失败数量。（已实现：失败路径写入 Logger，UI 摘要提示 see log）
- [x] M3-P2：明确多目录扫描的输入格式、UI 表达和持久化方式。（已实现：复用路径输入框，按 `FileSearchPath` 分隔，过滤无效目录并持久化规范化路径）
- [x] M3-P3：评估 `KnownPluginList` / 扫描结果持久化，降低启动恢复对同步扫描的依赖。（已实现：保存 `KnownPluginList` XML，启动优先恢复缓存，失败时回退重扫）
- [x] M3-P4：补充空状态、失败状态和恢复失败提示的 UI 需求。（已实现：状态文本区分未扫描、无目录、无插件、失败文件、缓存为空和加载失败）

完成标准：

- [x] 插件扫描增强进入 backlog，且不阻塞当前迭代目标。
- [x] M3-P1..P4 已完成实现并通过 2026-04-30 人工验证，未发现明显问题。

### 5. MainComponent 职责继续收敛

对应代码：

- `source/MainComponent.*`
- `source/Plugin/PluginFlowSupport.*`
- 后续计划新增 `source/Recording/RecordingFlowSupport.*` 或等价轻量 helper。
- 后续计划新增 `source/Export/ExportFlowSupport.*` 或继续放在 `source/Recording/*Exporter*` 周边的导出 helper。

重点：

- [x] 优先抽离录制 / 回放 / 导出状态流，而不是大规模重写 `MainComponent`。（MC-1..MC-4 全部完成）
- [x] 保留 `MainComponent` 对 UI 组件拥有权、窗口生命周期和顶层装配职责。（MC-1..MC-4 全部保持此约束）
- [x] 新增 WAV 导出时不得把完整离线渲染流程直接堆入 `MainComponent`。（已按 M6-6c 执行）

#### 5.1 收敛原则

- 不做大规模重写；每次只抽一个边界，保持可构建、可回归。
- `MainComponent` 保留：UI 组件拥有权、JUCE 生命周期入口、窗口生命周期、键盘焦点恢复、顶层装配。
- Helper 只承载纯流程/状态转换，不直接拥有 JUCE `Component`。
- 音频线程边界仍以 `AudioEngine` / `RecordingEngine` 为准；helper 不进入 audio callback。
- 每个切片完成后至少跑 `./scripts/dev.sh wsl-build --configure-only` 和 `./scripts/dev.sh win-build`。

#### 5.2 建议切片

1. **MC-1：RecordingFlowSupport 录制 / 回放 UI 流程 helper**（已实现）
   - 目标：抽离 `Record / Stop / Play` 的状态转换和按钮状态更新策略。
   - 候选职责：根据当前 `ControlsPanel::RecordingState`、`RecordingTake`、`RecordingEngine` 状态决定下一步动作。
   - 不做：不移动 audio callback 逻辑；不改变 `RecordingEngine` 数据模型；不改变现有按钮文案。
   - 完成标准：`MainComponent::handleRecordClicked()` / `handlePlayClicked()` / `handleStopClicked()` 变薄，行为不回退。
   - 状态：已新增 `source/Recording/RecordingFlowSupport.*`，承载 Record / Play / Stop intent 到 command / next-state 的纯流程决策。

2. **MC-2：ExportFlowSupport 导出选项与默认文件名 helper**（已实现）
   - 目标：抽离 MIDI / WAV 导出的默认文件名、空 take 判断、WAV options 构建。
   - 候选职责：生成 `recording_<ISO8601>.mid/.wav`、构建 `WavExportOptions`、统一导出日志前缀。
   - 不做：不把 `FileChooser` 生命周期移出 `MainComponent`；不改变 `MidiFileExporter` / `WavFileExporter` 核心实现。
   - 完成标准：`handleExportMidiClicked()` / `handleExportWavClicked()` 只负责 FileChooser 与调用导出函数。
   - 状态：已新增 `source/Export/ExportFlowSupport.*`，承载默认文件名、空 take 判断、WAV options 构建和导出日志前缀。

3. **MC-3：PluginFlowSupport 继续收敛 scan / restore / cache 流程**（已实现）
   - 目标：把启动缓存恢复、scan path normalise、KnownPluginList cache 更新等流程继续从 `MainComponent` 收到 `PluginFlowSupport`。
   - 候选职责：构建 restore plan、尝试缓存恢复、扫描并应用 recovery、更新 cache。
   - 不做：不移动 plugin editor window 拥有权；不改变 `PluginHost` 实例生命周期。
   - 完成标准：`restorePluginStateOnStartup()` 和 `scanPluginsAtPathAndApplyRecoveryState()` 只表达顶层时序。
   - 状态：`PluginFlowSupport` 已接管 cached plugin list 恢复、扫描后 recovery 更新和 `KnownPluginList` cache 写回。

4. **MC-4：ReadOnlyStateRefresh 边界命名清理**（已实现）
   - 目标：统一 `createRuntime*StateSnapshot()` / `applyReadOnlyUiState()` / `refreshReadOnlyUiState()` 的命名和调用时机。
   - 候选职责：明确哪些路径必须刷新只读 UI，哪些路径只需保存设置。
   - 不做：不引入复杂状态管理框架；不把 `AppState` 变成可变全局 store。
   - 完成标准：状态快照链路仍为 `SettingsModel + runtime -> AppStateBuilder -> PanelStateBuilder -> UI`，但调用点更少、更明确。
   - 状态：已将状态函数命名收敛为 `build*Snapshot()`、`renderReadOnlyUiState()`、`refreshReadOnlyUiStateFromCurrentSnapshot()` 和 `refreshMidiStatusFromCurrentSnapshot()`。

#### 5.3 推荐执行顺序

1. MC-2：导出 helper 最独立，风险最低。
2. MC-1：录制 / 回放 UI 流程 helper，覆盖当前 M6 主链路。
3. MC-3：插件流程继续收敛，依赖 M3-P1..P4 已稳定。
4. MC-4：最后做命名与刷新边界清理，避免过早抽象。

完成标准：

- [x] 新增流程有清晰边界。（RecordingFlowSupport、ExportFlowSupport、PluginFlowSupport 收敛、状态函数命名收敛）
- [x] 现有键盘演奏、插件加载、录制 / 回放 / MIDI / WAV 导出行为不回退。（2026-04-30 人工验证通过）
- [x] Section 5 计划已写入文档；MC-1..MC-4 全部完成，逐项勾选。

## 本轮计划验证命令

代码修改后优先执行：

```bash
./scripts/dev.sh wsl-build --configure-only
./scripts/dev.sh win-build
```

涉及环境或路径问题时执行：

```bash
./scripts/dev.sh self-check
```

## 后续候选文档任务

以下不是本轮代码实现的优先项，仅在需要时补充：

- 继续提炼 ADR：
  - JUCE 音频后端决策。
  - VST3-first 插件宿主决策。
  - WAV 离线渲染边界决策。
- 按需补充轻量文档入口：
  - [`../testing/known-issues.md`](../testing/known-issues.md)：已知问题与待验证风险。
  - [`../development/troubleshooting.md`](../development/troubleshooting.md)：开发环境与构建问题排查。
- 后续如架构文档继续增长，再拆分音频链路、插件宿主、录制 / 导出、状态模型等专题。

## MC-1..MC-4 人工回归记录（2026-04-30）

> Section 5 全部完成后整理。每项均已通过人工验证。

### MC-1：RecordingFlowSupport（录制 / 回放 UI 流程）

- `source/Recording/RecordingFlowSupport.h` / `.cpp` 新增
- 承载 Record / Play / Stop intent → command → next-state 的纯流程决策
- 关键文件：`source/Recording/RecordingFlowSupport.h`（`RecordingFlowState`/`Intent`/`Command`/`Status`、`chooseRecordingFlowCommand`、`getStateAfterCommand`、`shouldRestoreKeyboardFocus`）

人工回归（2026-04-30）：
- [x] 启动后点击 Record → 状态转为 recording
- [x] recording 中点击 Stop → 停止录制，回放按钮可点击
- [x] 有 take 时点击 Play → 开始回放
- [x] 回放中途点击 Stop → 停止回放
- [x] 点击 Export MIDI / Export WAV → 正常触发 FileChooser
- [x] 录制中关闭 plugin editor → 焦点正确恢复到键盘
- [x] 焦点切换后恢复键盘输入 → 无回归问题

### MC-2：ExportFlowSupport（导出选项与默认文件名）

- `source/Export/ExportFlowSupport.h` / `.cpp` 新增
- 承载默认文件名生成、空 take 判断、WAV options 构建、导出日志前缀
- 关键文件：`source/Export/ExportFlowSupport.h`（`makeDefaultRecordingExportFile`、`canExportTake`、`buildWavExportOptions`、`makeExportLogPrefix`）

人工回归（2026-04-30）：
- [x] 空 take 时 Export MIDI / Export WAV 按钮禁用
- [x] 有 take 时 Export MIDI → FileChooser 出现，默认名为 `recording_<ISO8601>.mid`
- [x] 有 take 时 Export WAV → FileChooser 出现，默认名为 `recording_<ISO8601>.wav`
- [x] WAV FileChooser 有选项面板（44100Hz / 16bit / stereo）
- [x] 导出完成后状态栏正确显示日志前缀
- [x] 使用 fallback synth 做离线渲染 → 正常生成 WAV

### MC-3：PluginFlowSupport（scan / restore / cache 收敛）

- `source/Plugin/PluginFlowSupport.h` / `.cpp` 更新
- 接管 cached plugin list 恢复、扫描后 recovery 更新和 `KnownPluginList` cache 写回
- 关键文件：`source/Plugin/PluginFlowSupport.h`（`tryRestoreCachedPluginList`、`scanPluginsAtPathAndUpdateRecovery`）

人工回归（2026-04-30）：
- [x] 首次扫描无效目录 → 状态文本显示"无可用目录"，Logger 有记录
- [x] 扫描含无效路径 → 过滤后正常扫描有效路径，失败路径写入 Logger
- [x] 扫描成功后已知插件列表已缓存 → 关闭应用，重启后启动阶段直接恢复缓存列表
- [x] 缓存恢复失败后回退重扫 → 状态文本显示"loaded cached list: N plugins"或回退重扫
- [x] 失败插件列表有内容时 → UI 显示"部分插件加载失败（见日志）"

### MC-4：状态刷新边界命名清理

- `MainComponent` 状态函数命名收敛
- 关键变更：`build*Snapshot()` / `renderReadOnlyUiState()` / `refreshReadOnlyUiStateFromCurrentSnapshot()` / `refreshMidiStatusFromCurrentSnapshot()`

人工回归（2026-04-30）：
- [x] 启动后插件列表 / MIDI 状态正常显示
- [x] 外部 MIDI 设备接入时 → Header MIDI 状态刷新（注：当前无外部 MIDI 设备，已记录至 `docs/testing/recording-playback.md`）
- [x] 键盘演奏时 → Header MIDI 状态实时更新
- [x] Scan / Load / Unload 插件后 → PluginPanel 状态更新
- [x] 打开 / 关闭插件 editor 后 → 状态仍正确刷新
