# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。  
> 读者：当前开发者、协作者、代码审查者。  
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前状态

当前活跃迭代：**AH-7：MIDI 导入流程下沉**（架构健康迭代的后续小切片，M8 收尾与 M9 启动之间的过渡轮，不编号为 M 阶段）。

M8 MIDI 文件导入与回放兼容性增强已收尾：M8-1 / M8-1b / M8-1c / M8-2 / M8-3 / M8-6 / M8-7 均已实现并通过 2026-05-01 人工验收；M8-5 merge-all 已搁置；M6-6e 作为后续 backlog。

上一轮架构健康迭代 AH-1..AH-6 已完成并通过构建验证与人工回归，已收尾。本轮继续做一个小范围架构切片：只分析并规划 `handleImportMidiClicked()` 的流程下沉，暂不修改源码。

---

**搁置说明**：M6 录制/回放稳定化和外部 MIDI 硬件依赖验证因当前无真实外部 MIDI 设备，暂无法测试。状态已记录至 `docs/testing/recording-playback.md`。这两个方向暂从本迭代移除，恢复时间取决于硬件条件。

## 已完成架构健康迭代目标（AH-1..AH-6）

上一轮已完成以下三个方向：

1. **MainComponent 职责继续收敛**：将仍留在 MainComponent 的流程性职责继续下沉到 helper。
2. **状态管理整理**：减少录制会话状态分散、清理 AppState 中的 UI 派生字段。
3. **ControlsPanel 按钮状态统一**：将分散的按钮状态更新收敛为单一状态源。

### 当前 MainComponent 分析（约 1568 行）

**已下沉到 helper 的职责**：
- RecordingFlowSupport：录制/回放命令判定与状态推进
- ExportFlowSupport：导出能力判定、默认文件名、WAV options 构建
- PluginFlowSupport：启动缓存恢复、扫描后 recovery 更新

**仍留在 MainComponent 的职责**：
- 录制/回放实际切换与状态缓存（已收敛为 `RecordingSession`，但启动/停止/回放编排仍在 MainComponent）
- MIDI 导入完整用户流程（chooser、停止播放、导入、立即回放，约 80 行）
- 导出 MIDI/WAV 的 chooser 生命周期、路径记忆与日志（已通过 `runExportRecordingFlow()` 收敛，仍由 MainComponent 顶层编排）
- 布局文件 CRUD 顶层编排（save/import/rename/delete 已通过 helper 收敛，仍保留 FileChooser / modal 生命周期）
- 设置窗口生命周期与 dirty 管理（已局部收敛，仍由 MainComponent 拥有窗口）
- 插件 editor window 生命周期管理
- 音频设备重建胶水逻辑

**潜在膨胀点**：
- `handleImportMidiClicked()`：chooser + 导入 + take 替换 + 立即回放仍可单独下沉
- 插件 editor window 生命周期：可作为较小后续切片
- 音频设备重建胶水：与插件加载/卸载/恢复相关，风险较高，应独立规划

## 已完成计划切片

### AH-1：录制会话状态结构化（已完成）

- **目标**：将 `currentTake` + `currentRecordingState` + `currentTakeCanBeExported` 合并为单一 `RecordingSession` 结构体，减少状态分散。
- **修改范围**：`source/MainComponent.h`（新增 `RecordingSession` 结构体）、`source/MainComponent.cpp`（替换分散字段，新增 `syncRecordingSessionToUi()`）。
- **不做**：不改变录制/回放行为；不改变 RecordingFlowSupport 接口；不改变 ControlsPanel 按钮逻辑。
- **完成标准**：录制相关状态从 3 个独立字段收敛为 1 个结构体；行为不回退。
- **风险**：低——纯内部重构，不改变外部接口。
- **状态**：已完成（2026-05-01）。`RecordingSession` 结构体包含 `take`、`canExportMidi`、`state` 三个字段和 `hasTake()`、`isRecording()`、`isPlaying()`、`isIdle()` 便捷方法；`syncRecordingSessionToUi()` 统一同步到 ControlsPanel；WSL configure 和 Windows MSVC build 均通过。

### AH-2：导出流程统一（已完成）

- **目标**：将 `handleExportMidiClicked()` 和 `handleExportWavClicked()` 中的重复模式（chooser 创建、路径更新、保存设置、导出调用、日志）统一为通用辅助层。
- **修改范围**：`source/MainComponent.h`（新增 `runExportRecordingFlow()` 声明和 `ExportFileType` 前向声明）、`source/MainComponent.cpp`（新增 `runExportRecordingFlow()` 实现，简化两个 handler）。
- **不做**：不改变 `MidiFileExporter` / `WavFileExporter` 核心实现；不改变 FileChooser 生命周期归属。
- **完成标准**：两个导出 handler 各减少 15+ 行重复代码；行为不回退。
- **风险**：低——已有 ExportFlowSupport 作为基础。
- **状态**：已完成（2026-05-01）。`runExportRecordingFlow()` 统一了 guard clause、FileChooser 创建、async launch、cancel handling、path save、export call、logging、chooser reset；两个 handler 各缩减为约 10 行；WSL configure 和 Windows MSVC build 均通过。

### AH-3：布局 CRUD 流程收敛（已完成）

- **目标**：将 `handleSaveLayoutRequested()` / `handleImportLayoutRequested()` / `handleRenameLayoutRequested()` / `handleDeleteLayoutRequested()` 中的流程性逻辑抽到 `LayoutFlowSupport` 或等价 helper。
- **修改范围**：`source/MainComponent.h`（新增 `applyLayoutAndCommit()`、`runLayoutFileChooser()`、`runLayoutRenameDialog()`、`runLayoutDeleteDialog()` 声明）、`source/MainComponent.cpp`（新增 4 个 helper 实现，简化 5 个布局 handler）。
- **不做**：不改变布局文件格式；不改变 ControlsPanel 布局相关回调接口；不引入新的 UI 组件。
- **完成标准**：布局相关 handler 从约 200 行收敛到约 80 行；MainComponent 只保留 chooser 生命周期和顶层编排。
- **风险**：低中——涉及文件操作和异步对话框，需仔细保持行为一致。
- **状态**：已完成（2026-05-01）。`applyLayoutAndCommit()` 统一了 allNotesOff + setLayout + applyInputMapping + syncUi + save + restoreFocus 模式；`runLayoutFileChooser()` 统一了 FileChooser 创建和 async launch；`runLayoutRenameDialog()` 和 `runLayoutDeleteDialog()` 分别封装了 rename 和 delete 对话框逻辑；WSL configure 和 Windows MSVC build 均通过（无警告）。

### AH-4：设置窗口生命周期收敛（已完成）

- **目标**：将 `showSettingsDialog()` / `closeSettingsWindowAsync()` / `isSettingsWindowDirty()` / `saveAndCloseSettingsWindow()` 等设置窗口管理逻辑抽到独立 helper。
- **修改范围**：`source/MainComponent.h`（新增 `getSettingsContent()` 声明）、`source/MainComponent.cpp`（新增 `getSettingsContent()` 实现，简化 `isSettingsWindowDirty()`）。
- **不做**：不改变 `SettingsComponent` 内部实现；不改变设置持久化逻辑。
- **完成标准**：设置窗口相关方法从约 100 行收敛到约 30 行；MainComponent 只保留窗口拥有权。
- **风险**：低——设置窗口生命周期相对独立。
- **状态**：已完成（2026-05-01）。`getSettingsContent()` 统一了 `settingsWindow` 空检查和 `dynamic_cast` 模式；`isSettingsWindowDirty()` 从 7 行缩减为 4 行；WSL configure 和 Windows MSVC build 均通过。

### AH-5：AppState 清理（已完成）

- **可行性结论**：方向可行；按“展示文案下沉、事实字段保留”的限定边界执行。
- **问题修正**：
  - `PluginState` 中明确可移除的 UI 派生字段只有 2 个：`pluginListText`、`availableFormatsDescription`，不是“2-3 个”。
  - 实际影响范围不止 `source/Core/AppState.h` 和 `source/UI/PluginPanelStateBuilder.*`，还会涉及 `source/Core/AppStateBuilder.h`、`source/MainComponent.cpp` 的 runtime plugin snapshot 构建，以及可能涉及 `source/UI/PluginPanelStateBuilder.h`。
  - `availableFormatsDescription` 当前来自 `PluginHost::getAvailableFormatsDescription()`，不能只凭 `PluginState` 中的 `supportsVst3` 无损还原完整文案；若要完全下沉到 builder，需要明确 builder 是否允许直接依赖 `PluginHost` 或新增更事实化的格式字段。
- **目标**：仅移除 `PluginState` / `RuntimePluginState` 中的 `pluginListText`、`availableFormatsDescription` 两个展示文案字段；保留 `availablePluginNames`、`lastScanSummary`、`lastLoadError`、`supportsVst3`、`currentPluginName` 等业务/运行时事实字段。
- **推荐实现边界**：
  - 让 `PluginPanelStateBuilder` 基于 `PluginHost` 构建 `PluginPanel::State` 中的 `pluginListText` 和 `availableFormatsDescription` 展示文案。
  - `PluginPanel` 不直接依赖 `PluginHost`；依赖只停留在 state builder 层。
  - `AppState` 继续承担运行时事实快照；不要把插件扫描、加载、editor、prepared 等生命周期事实字段移出 `AppState`。
  - 接受插件面板状态构建不再完全由纯 `AppState` 快照还原；这是本切片为清理 UI 派生字段付出的边界代价。
- **不做**：不引入新的状态管理框架；不重写 AppState 快照机制；不迁移插件扫描、加载、editor 生命周期字段；不改变 `PluginPanel::State` 的 UI 展示字段。
- **完成标准**：`PluginState` 和 `RuntimePluginState` 移除上述 2 个 UI 文案字段；插件列表文本和格式描述仍能与当前行为等价显示；插件扫描、启动恢复、加载失败提示、editor 状态不回退。
- **风险**：中低——字段本身是 UI 派生值，但会触碰 AppStateBuilder、MainComponent snapshot 与 PluginPanelStateBuilder 的边界；需要小步修改并跑 WSL configure + Windows MSVC build。
- **状态**：已完成（2026-05-01）。`PluginState` / `RuntimePluginState` 已移除 `pluginListText`、`availableFormatsDescription`；`PluginPanelStateBuilder` 通过 `PluginHost` 构建这两个展示文案；`PluginPanel` 未直接依赖 `PluginHost`；WSL configure 和 Windows MSVC build 均通过。

### AH-6：ControlsPanel 按钮状态统一（已完成）

- **目标**：将 `ControlsPanel` 中录制/回放/导入导出按钮的 enabled 状态与状态文案收敛为单一 `State` / `ActionState` 输入，减少 `setRecordingState()`、`setHasTake()`、`setCanExportTake()` 多入口分别触发 UI 更新带来的分散状态。
- **修改范围**：`source/UI/ControlsPanel.h`、`source/UI/ControlsPanel.cpp`、`source/MainComponent.cpp` 中 `syncRecordingSessionToUi()` 调用点。
- **推荐实现边界**：
  - 在 `ControlsPanel` 内新增轻量状态结构，集中包含 `recordingState`、`hasTake`、`canExportMidiTake`、`canExportWavTake`。
  - 提供单一入口（例如 `updateRecordingControlsState(...)`）统一刷新录制、播放、停止、Back、Import MIDI、Export MIDI/WAV 按钮状态和状态标签。
  - 保留 `ControlsPanel` 内部按钮 enabled 推导逻辑，不把按钮细节泄漏到 `MainComponent`。
- **不做**：不改变录制/回放流程命令；不改 `RecordingFlowSupport`；不调整布局按钮逻辑；不改变按钮文案和现有交互行为。
- **完成标准**：`syncRecordingSessionToUi()` 只通过一个入口同步 ControlsPanel 录制控制状态；按钮 enabled 规则与现状等价；录制、播放、停止、Back、导入 MIDI、导出 MIDI/WAV 行为不回退。
- **风险**：低中——主要是 UI 状态入口收敛，但需覆盖 idle / recording / playing、空 take、导入 MIDI 可导出 WAV 但不可再导出 MIDI 等组合。
- **状态**：已完成（2026-05-01）。`ControlsPanel::RecordingControlsState` 统一了承载 `recordingState`、`hasTake`、`canExportMidiTake`、`canExportWavTake`；`syncRecordingSessionToUi()` 改为单一 `setRecordingControlsState()` 入口；旧的 `setRecordingState()` / `setHasTake()` / `setCanExportTake()` 分散入口已移除；人工回归后补充修正：playing 期间 Import MIDI 禁用，导入 MIDI 停止后允许 Export WAV、继续禁止 Export MIDI；WSL configure 和 Windows MSVC build 均通过；修正后人工验证未发现明显问题。

## AH-7：MIDI 导入流程下沉（规划中）

### 当前代码分析

- 入口：`source/MainComponent.cpp:1468` 的 `handleImportMidiClicked()` 仍承担完整用户流程。
- 关联状态：`source/MainComponent.h:63` 的 `RecordingSession` 保存当前 take、`canExportMidi` 和 `ControlsPanel::RecordingState`。
- UI 同步：`source/MainComponent.cpp:1078` 的 `syncRecordingSessionToUi()` 把 `RecordingSession` 同步到 `ControlsPanel`。
- 导入核心：`source/Recording/MidiFileImporter.*` 已是独立转换层，只负责 `.mid` → `RecordingTake`，不应再扩大职责。
- 持久化字段：`SettingsModel::lastMidiImportPath` 和 `SettingsStore` 负责最近导入路径读写。

### 当前耦合点

- FileChooser 起始目录直接从 `appSettings.lastMidiImportPath` 推导。
- FileChooser async callback 中同时处理：取消、路径记忆、导入、失败日志、播放中二次 stop、替换 take、UI 同步、自动播放、焦点恢复。
- `recordingSession` 与 `RecordingEngine` 的状态切换由 MainComponent 直接编排。
- 导入成功后 MIDI 导出禁用、WAV 导出可用的边界通过 `recordingSession.canExportMidi = false` 和 `syncRecordingSessionToUi()` 表达。

### 推荐目标

- 将 `handleImportMidiClicked()` 从“完整业务流程”收敛为“FileChooser 生命周期 + 顶层编排”。
- 抽出导入路径、导入结果处理、session 替换/自动播放的局部 helper，降低 MainComponent 尾部复杂度。
- 保持 `MidiFileImporter` 纯净，不把 UI、settings、播放状态写入 importer。

### 推荐切片

1. **AH-7-1：导入路径 helper**
   - 抽出最近导入路径到 FileChooser 起始目录的推导逻辑。
   - 抽出成功选择文件后的 `lastMidiImportPath` 更新和 `saveSettingsSoon()` 调用。
   - 风险低；行为应完全等价。
2. **AH-7-2：导入结果处理 helper**
   - 抽出“调用 `importMidiFile()`、处理空/失败 take、记录失败日志”的流程。
   - 建议返回轻量结果对象或 `std::optional<RecordingTake>`，避免 helper 直接操作 UI。
   - 风险低中；需保持失败/空文件路径不改变 session。
3. **AH-7-3：替换 take 并自动播放 helper**
   - 抽出成功导入后的 `recordingSession.take` 替换、`canExportMidi=false`、状态同步、`startInternalPlayback()` 和播放状态同步。
   - 风险中；需重点保持按钮状态、自动播放、Export MIDI/WAV 边界。

### 建议保留在 MainComponent 的职责

- `std::unique_ptr<juce::FileChooser>` 拥有权和 `launchAsync()` 生命周期。
- `recordingSession` 的最终状态写入和 `syncRecordingSessionToUi()` 调用。
- `startInternalPlayback()` / `stopInternalPlayback()` 这类依赖 `RecordingEngine` 与 audio all-notes-off 的顶层编排。
- `restoreKeyboardFocus()` 与具体 UI 收尾。

### 不做

- 不改变 MIDI importer 行为、轨道选择策略、tempo/PPQ 处理。
- 不改变导入后立即回放行为。
- 不改变导入 MIDI 后禁止 Export MIDI、允许 Export WAV 的边界。
- 不引入完整 song/project 模型。
- 不移动 FileChooser 拥有权到非 UI helper。

### 风险与验证点

- 风险：异步 FileChooser lambda 捕获与 chooser reset 时机；导入失败/取消路径；播放中/停止后状态一致性；导入后按钮状态边界。
- 验证：取消导入不改状态；无效/空/no-note 文件安全返回；导入成功自动播放；播放中 Import MIDI 仍禁用；Stop 后 Export MIDI 禁用、Export WAV 可用；正常录制 Stop 后 MIDI/WAV 均可导出。

### 当前状态

- **状态**：规划完成，尚未修改源码。
- **下一步**：执行 AH-7-1 / AH-7-2 小切片；每个切片后运行 `./scripts/dev.sh wsl-build --configure-only` 和 `./scripts/dev.sh win-build`。

## AH-1..AH-6 推荐执行顺序（历史）

1. **AH-1**：最独立，风险最低，先建立状态结构化基础。
2. **AH-2**：导出统一，复用已有 ExportFlowSupport。
3. **AH-3**：布局 CRUD 收敛，涉及文件操作需仔细验证。
4. **AH-4**：设置窗口收敛，相对独立。
5. **AH-5**：AppState 清理，可选，视前 4 项进展决定是否执行。
6. **AH-6**：ControlsPanel 按钮状态统一，作为本轮剩余架构健康切片。

## 收敛原则

- 不做大规模重写；每次只抽一个边界，保持可构建、可回归。
- `MainComponent` 保留：UI 组件拥有权、JUCE 生命周期入口、窗口生命周期、键盘焦点恢复、顶层装配。
- Helper 只承载纯流程/状态转换，不直接拥有 JUCE `Component`。
- 音频线程边界仍以 `AudioEngine` / `RecordingEngine` 为准；helper 不进入 audio callback。
- 每个切片完成后至少跑 `./scripts/dev.sh wsl-build --configure-only` 和 `./scripts/dev.sh win-build`。

## 完成标准

- [x] AH-1 完成：录制会话状态从 3 个字段收敛为 1 个结构体。（2026-05-01）
- [x] AH-2 完成：导出 handler 重复代码减少 15+ 行。（2026-05-01）
- [x] AH-3 完成：布局 CRUD handler 从约 200 行收敛到约 80 行。（2026-05-01）
- [x] AH-4 完成：设置窗口管理从约 100 行收敛到约 30 行。（2026-05-01）
- [x] AH-5 完成：按修正后的边界清理 AppState PluginState 中的 2 个 UI 派生字段。（2026-05-01）
- [x] AH-6 完成：ControlsPanel 录制控制按钮状态统一为单一状态入口。（2026-05-01）
- [x] AH-1..AH-5 完成后人工回归：现有键盘演奏、插件加载、录制/回放/MIDI/WAV 导出、MIDI 导入、布局 preset 未发现明显回退。（2026-05-01）
- [x] AH-6 补充人工回归：导入 MIDI 播放中 Import MIDI 禁用；Stop 后 Export MIDI 禁用、Export WAV 可用；正常录制 Stop 后 Export MIDI / Export WAV 均可用，未发现明显问题。（2026-05-01）
- [x] 本轮架构健康迭代可收尾：AH-1..AH-6 已完成，构建验证与人工回归均无明显问题。（2026-05-01）
- [~] `MainComponent.cpp` 的继续收敛不作为本轮阻塞项，后续单独规划更大切片。

## 其他可选后续切片（不纳入 AH-7 完成条件）

- 插件 editor window 生命周期收敛：较小、独立，但收益低于 MIDI 导入流程下沉。
- 音频设备重建胶水收敛：收益高但风险较高，建议等插件生命周期测试更充分后再做。

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

---

## M8 历史完成记录

> 以下为 M8 收尾状态快照，当前 M8 任务已全部完成。

### M8 收尾审查结论（2026-05-01）

- [x] M8-1 / M8-1b / M8-1c / M8-2 / M8-3 / M8-6 / M8-7 已实现并通过人工验收。
- [x] M8-2 已实现并通过人工验收：导入 playback take 禁止再次导出 MIDI，但允许导出 WAV；多轨、非 960 PPQ、tempo meta event 和复杂 tempo map 限制均未发现明显问题。VST3 插件音色离线渲染仍归 M6-6e，暂时搁置。
- [x] M8-3 已实现并通过人工验收：最近导入/导出路径记忆已接入；播放中点击 `Back` 可从当前 take 开头重新播放。
- [~] M8-5 已搁置：仍保持 note-rich 单轨导入，不合并所有轨道；这是当前推荐模式。

### MC-1..MC-4 历史完成记录（2026-04-30）

- [x] MC-1：RecordingFlowSupport 录制/回放 UI 流程 helper。
- [x] MC-2：ExportFlowSupport 导出选项与默认文件名 helper。
- [x] MC-3：PluginFlowSupport 继续收敛 scan/restore/cache 流程。
- [x] MC-4：ReadOnlyStateRefresh 边界命名清理。

### M6/M7 历史完成记录

- [x] M6-6a/b/c/d：WAV 离线渲染 MVP，E.1–E.9 全部通过。
- [x] M3-P1..P4：插件扫描产品化增强，2026-04-30 人工验证通过。
- [x] M7：布局 Preset 核心能力已完成。
