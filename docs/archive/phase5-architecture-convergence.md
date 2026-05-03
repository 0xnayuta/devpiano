# Phase 5 架构收敛完成记录

> 用途：记录 Phase 5.1-5.8 的详细完成情况，作为历史参考。  
> 读者：需要了解架构收敛历史细节的开发者。  
> 更新时机：本文件为归档文档，不再更新。当前信息请参考 `docs/roadmap/roadmap.md` 和 `docs/architecture/overview.md`。

## 背景

Phase 5 的目标是将 `MainComponent.cpp` 从约 1587 行降至 1200 行以下，通过提取 helper 收敛职责。

Phase 5.1-5.7 已于 2026-05-01 全部完成。Phase 5.8a-5.8e 已于 2026-05-02 全部完成，人工回归通过。最终 `MainComponent.cpp` 降至 606 行。

## Phase 5.1-5.7 完成记录

### 5.1：录制会话状态结构化（Phase 5-5）

- **目标**：将 `currentTake` + `currentRecordingState` + `currentTakeCanBeExported` 合并为单一 `RecordingSession` 结构体。
- **状态**：已完成（2026-05-01）。`RecordingSession` 结构体包含 `take`、`canExportMidi`、`state` 三个字段和便捷方法。

### 5.2：导出流程统一（Phase 5-6）

- **目标**：将 `handleExportMidiClicked()` 和 `handleExportWavClicked()` 中的重复模式统一为 `runExportRecordingFlow()`。
- **状态**：已完成（2026-05-01）。两个导出 handler 各缩减为约 10 行。

### 5.3：布局 CRUD 流程收敛（Phase 5-7）

- **目标**：将布局 handler 中的流程性逻辑抽到 `applyLayoutAndCommit()`、`runLayoutFileChooser()` 等 helper。
- **状态**：已完成（2026-05-01）。布局相关 handler 从约 200 行收敛到约 80 行。

### 5.4：设置窗口生命周期收敛（Phase 5-8）

- **目标**：将设置窗口管理逻辑抽到 `getSettingsContent()` 等 helper。
- **状态**：已完成（2026-05-01）。设置窗口相关方法从约 100 行收敛到约 30 行。

### 5.5：AppState 清理（Phase 5-9）

- **目标**：移除 `PluginState` / `RuntimePluginState` 中的 UI 派生字段。
- **状态**：已完成（2026-05-01）。移除 `pluginListText`、`availableFormatsDescription` 两个字段。

### 5.6：ControlsPanel 按钮状态统一（Phase 5-10）

- **目标**：将录制/回放/导入导出按钮的 enabled 状态收敛为单一 `RecordingControlsState`。
- **状态**：已完成（2026-05-01）。`syncRecordingSessionToUi()` 改为单一 `setRecordingControlsState()` 入口。

### 5.7：MIDI 导入流程下沉（Phase 5-11）

- **目标**：将 `handleImportMidiClicked()` 收敛为 FileChooser 生命周期 + 顶层编排。
- **状态**：已完成（2026-05-01）。抽出 `getLastMidiImportDirectory()`、`tryImportMidiFile()`、`replaceTakeAndStartPlayback()` 三个 helper。

## Phase 5.8 完成记录

### 5.8a：布局管理 handlers 提取（已完成）

- **目标**：将布局 CRUD + 文件对话框 + 确认对话框逻辑提取到 `Layout/LayoutFlowSupport`。
- **状态**：已完成（2026-05-01）。MainComponent 从 1587 行降至 1349 行（减少 238 行）。
- **提取内容**：`findUserLayoutFileById()`、`handleLayoutChanged()`、`handleSaveLayoutRequested()`、`handleResetLayoutToDefaultRequested()`、`handleImportLayoutRequested()`、`handleRenameLayoutRequested()`、`handleDeleteLayoutRequested()`、`applyLayoutAndCommit()`、`runLayoutFileChooser()`、`runLayoutRenameDialog()`、`runLayoutDeleteDialog()`。
- **设计模式**：namespace + 自由函数，通过回调或引用注入依赖。

### 5.8b：录制/回放/MIDI 导入 handlers 提取（已完成）

- **目标**：将录制编排逻辑提取到 `Recording/RecordingSessionController`。
- **状态**：已完成（2026-05-01）。MainComponent 从 1349 行降至 930 行（减少 419 行）。
- **提取内容**：`RecordingSession` 结构体、`handleRecordClicked()`、`handlePlayClicked()`、`handleStopClicked()`、`handleBackToStartClicked()`、`handleExportMidiClicked()`、`handleExportWavClicked()`、`handleImportMidiClicked()`、`tryImportMidiFile()`、`replaceTakeAndStartPlayback()`、`startInternalRecording()`、`stopInternalRecording()`、`startInternalPlayback()`、`stopInternalPlayback()`、`syncRecordingSessionToUi()`、`runExportRecordingFlow()`、`timerCallback()` 中的 playback-ended 处理。
- **设计模式**：`RecordingSessionController` 持有 `RecordingSession` 状态，通过构造注入依赖。

### 5.8c：插件操作提取（已完成）

- **目标**：将插件加载/卸载/editor/扫描编排逻辑提取到 `Plugin/PluginOperationController`。
- **状态**：已完成（2026-05-02）。MainComponent 从 930 行降至 711 行（减少 219 行）。
- **提取内容**：`restorePluginStateOnStartup()`、`restorePluginScanPathOnStartup()`、`restoreLastPluginOnStartup()`、`restorePluginByNameOnStartup()`、`resolvePluginScanPath()`、`loadPluginByNameAndCommitState()`、`loadSelectedPlugin()`、`unloadPluginAndCommitState()`、`unloadCurrentPlugin()`、`tryCreatePluginEditor()`、`handlePluginEditorWindowClosedAsync()`、`closePluginEditorWindow()`、`openPluginEditorWindow()`、`togglePluginEditor()`、`scanPluginsAtPathAndApplyRecoveryState()`、`scanPluginsAtPathAndCommitState()`、`scanPlugins()`。
- **设计模式**：`PluginOperationController` 持有 `pluginEditorWindow`，通过构造注入依赖。音频设备重建逻辑保留在 MainComponent。

### 5.8d：设置窗口管理提取（已完成）

- **目标**：将设置窗口生命周期管理提取到 `Settings/SettingsWindowManager`。
- **状态**：已完成（2026-05-02）。MainComponent 从 711 行降至 631 行（减少 80 行）。
- **提取内容**：`SettingsDialogWindow` 内部类、`showSettingsDialog()`、`isSettingsWindowDirty()`、`isSettingsWindowOpen()`、`closeSettingsWindowAsync()`、`closeSettingsWindow()`、`saveAndCloseSettingsWindow()`、`getSettingsContent()`。
- **设计模式**：`SettingsWindowManager` 持有设置窗口和 `SettingsComponent` 生命周期；`MainComponent` 通过 thin wrapper 传入依赖。

### 5.8e：状态快照构建提取（已完成）

- **目标**：将 `buildRuntime*Snapshot()` 和 `buildCurrentAppStateSnapshot()` 提取到 `Core/AppStateBuilder`。
- **状态**：已完成（2026-05-02）。MainComponent 从 631 行降至 606 行（减少 25 行）。
- **提取内容**：`buildRuntimeAudioStateSnapshot()`、`buildRuntimePluginStateSnapshot()`、`buildRuntimeInputStateSnapshot()`、`buildCurrentAppStateSnapshot()`。
- **设计模式**：纯函数/只读函数，无副作用，参数注入。

### 5.8f：AppStateBuilder 分层清理（低优先级 tech debt，暂不执行）

- **状态**：已评估，暂不执行。
- **内容**：将 `Core/AppStateBuilder.cpp` 中的 runtime snapshot 构建（依赖 `AudioDeviceManager`、`PluginHost`、`MidiRouter`、`KeyboardMidiMapper`）移到 `App/AppStateSnapshotBuilder`，恢复 `Core` 层纯状态职责。
- **不执行原因**：当前无行为问题，`Core` 层的跨模块依赖仅影响一个很小的 snapshot 组装点且只被 `MainComponent` 使用。进一步拆分需要引入新的间接层，收益不足以 justify 额外的维护成本。属于架构洁癖而非实际问题。
- **触发条件**：如果 `Core/AppStateBuilder` 的跨模块依赖开始被其他模块误用，或 `MainComponent` 再次膨胀到需要重新评估职责边界时，再执行此清理。

### 5.8+ 后续机会（已评估，不建议继续）

以下提取机会已评估，当前 `MainComponent.cpp` 606 行、职责边界清晰，进一步拆分的收益不足以 justify 额外间接层成本。**长期搁置到 MainComponent 再次膨胀时重新评估。**

- **音频设备管理 helper**（~55 行）：`initialiseAudioDevice()`、`captureAudioDeviceState()`、`prepareForAudioDeviceRebuild()`、`finishAudioDeviceRebuild()`、`runPluginActionWithAudioDeviceRebuild()` 可提取为 `Audio/AudioDeviceLifecycleHelper`。**不执行原因**：这些函数天然属于主窗口组件的音频生命周期入口，拆分后需要引入额外的回调/接口。
- **性能/插件恢复设置 helper**（~67 行）：`getPerformanceSettingsFromUi()`、`applyPerformanceSettingsToUi()`、`applyPerformanceSettingsToAudioEngine()` 等可提取为 `Settings/SettingsFlowSupport`。**不执行原因**：行数少、逻辑简单，拆分收益低。
- **匿名 namespace 工具函数**（~128 行）：`makeSafeUiText()`、`suppressImeForPeer()` 等可按域分散到对应模块。**不执行原因**：这些函数已被各模块使用，分散后会增加 include 依赖而不会改善可维护性。

### 执行顺序与依赖

```
5.8a（布局） ──┐
5.8b（录制） ──┼── 可并行，无相互依赖
5.8c（插件） ──┤
5.8d（设置） ──┤
5.8e（快照） ──┘
```

执行顺序：**5.8a → 5.8b → 5.8c → 5.8d → 5.8e**（全部已完成）

### 预期成果

| 提取项 | MainComponent 减少行数 | 累计减少 |
|---|---|---|
| 当前 | — | 1587 行 |
| 5.8a 布局管理 | -238 ✅ | 1349 行 |
| 5.8b 录制编排 | -419 ✅ | 930 行 |
| 5.8c 插件操作 | -219 ✅ | 711 行 |
| 5.8d 设置窗口 | -80 ✅ | 631 行 |
| 5.8e 状态快照 | -25 ✅ | 606 行 |

**5.8a-5.8e 完成后已降至 606 行，远低于 1200 行目标**。

## 完成标准（已完成项）

### Phase 5.1-5.7

- [x] Phase 5.1 完成：录制会话状态从 3 个字段收敛为 1 个结构体。（2026-05-01）
- [x] Phase 5.2 完成：导出 handler 重复代码减少 15+ 行。（2026-05-01）
- [x] Phase 5.3 完成：布局 CRUD handler 从约 200 行收敛到约 80 行。（2026-05-01）
- [x] Phase 5.4 完成：设置窗口管理从约 100 行收敛到约 30 行。（2026-05-01）
- [x] Phase 5.5 完成：按修正后的边界清理 AppState PluginState 中的 2 个 UI 派生字段。（2026-05-01）
- [x] Phase 5.6 完成：ControlsPanel 录制控制按钮状态统一为单一状态入口。（2026-05-01）
- [x] Phase 5.7 完成：`handleImportMidiClicked()` 从完整业务流程收敛为 FileChooser 生命周期 + 顶层编排。（2026-05-01）
- [x] Phase 5.1-5.7 人工回归：现有键盘演奏、插件加载、录制/回放/MIDI/WAV 导出、MIDI 导入、布局 preset 未发现明显回退。（2026-05-01）

### Phase 5.8

- [x] Phase 5.8a 完成：布局管理 handlers 提取到 `Layout/LayoutFlowSupport`，MainComponent 从 1587 行降至 1349 行（减少 238 行）。（2026-05-01）
- [x] Phase 5.8b 完成：录制/回放/MIDI 导入编排提取到 `Recording/RecordingSessionController`，MainComponent 从 1349 行降至 930 行（减少 419 行）。（2026-05-01）
- [x] Phase 5.8c 完成：插件操作提取到 `Plugin/PluginOperationController`，MainComponent 从 930 行降至 711 行（减少 219 行）。（2026-05-02）
- [x] Phase 5.8d 完成：设置窗口管理提取到 `Settings/SettingsWindowManager`，MainComponent 从 711 行降至 631 行（减少 80 行）。（2026-05-02）
- [x] Phase 5.8e 完成：状态快照构建提取到 `Core/AppStateBuilder`，MainComponent 从 631 行降至 606 行（减少 25 行）。（2026-05-02）
- [x] Phase 5.8 总验证：MainComponent.cpp ≤ 1200 行，WSL 构建通过，Windows MSVC 构建通过。
- [x] Phase 5.8 回归：键盘演奏、插件加载/卸载/editor、录制/回放/MIDI/WAV 导出、MIDI 导入、布局 preset、设置窗口未发现明显回退。（2026-05-02）
- [~] Phase 5.8f：AppStateBuilder 分层清理 — 已评估为低优先级 tech debt，暂不执行。

## 相关文档

- 项目路线图：`docs/roadmap/roadmap.md`
- 架构概览：`docs/architecture/overview.md`
