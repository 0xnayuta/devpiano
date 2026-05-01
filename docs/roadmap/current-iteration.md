# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。  
> 读者：当前开发者、协作者、代码审查者。  
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前状态

当前活跃阶段：**Phase 5：架构收敛与 MainComponent 瘦身**

Phase 5.1-5.7 已完成（2026-05-01）：MainComponent 职责下沉，包括录制会话状态结构化、导出流程统一、布局 CRUD 收敛、设置窗口收敛、AppState 清理、ControlsPanel 按钮状态统一、MIDI 导入流程下沉。

当前 `MainComponent.cpp` 约 1587 行，目标降至 1200 行以下。

---

**搁置说明**：外部 MIDI 硬件依赖验证和 VST3 插件离线渲染（Phase 3-5）因当前条件暂无法测试，状态已记录至 `docs/testing/phase3-recording-playback.md`。

## Phase 5.1-5.7 完成记录

### 5.1：录制会话状态结构化（原 AH-1）

- **目标**：将 `currentTake` + `currentRecordingState` + `currentTakeCanBeExported` 合并为单一 `RecordingSession` 结构体。
- **状态**：已完成（2026-05-01）。`RecordingSession` 结构体包含 `take`、`canExportMidi`、`state` 三个字段和便捷方法。

### 5.2：导出流程统一（原 AH-2）

- **目标**：将 `handleExportMidiClicked()` 和 `handleExportWavClicked()` 中的重复模式统一为 `runExportRecordingFlow()`。
- **状态**：已完成（2026-05-01）。两个导出 handler 各缩减为约 10 行。

### 5.3：布局 CRUD 流程收敛（原 AH-3）

- **目标**：将布局 handler 中的流程性逻辑抽到 `applyLayoutAndCommit()`、`runLayoutFileChooser()` 等 helper。
- **状态**：已完成（2026-05-01）。布局相关 handler 从约 200 行收敛到约 80 行。

### 5.4：设置窗口生命周期收敛（原 AH-4）

- **目标**：将设置窗口管理逻辑抽到 `getSettingsContent()` 等 helper。
- **状态**：已完成（2026-05-01）。设置窗口相关方法从约 100 行收敛到约 30 行。

### 5.5：AppState 清理（原 AH-5）

- **目标**：移除 `PluginState` / `RuntimePluginState` 中的 UI 派生字段。
- **状态**：已完成（2026-05-01）。移除 `pluginListText`、`availableFormatsDescription` 两个字段。

### 5.6：ControlsPanel 按钮状态统一（原 AH-6）

- **目标**：将录制/回放/导入导出按钮的 enabled 状态收敛为单一 `RecordingControlsState`。
- **状态**：已完成（2026-05-01）。`syncRecordingSessionToUi()` 改为单一 `setRecordingControlsState()` 入口。

### 5.7：MIDI 导入流程下沉（原 AH-7）

- **目标**：将 `handleImportMidiClicked()` 收敛为 FileChooser 生命周期 + 顶层编排。
- **状态**：已完成（2026-05-01）。抽出 `getLastMidiImportDirectory()`、`tryImportMidiFile()`、`replaceTakeAndStartPlayback()` 三个 helper。

## 当前 MainComponent 分析（约 1587 行）

**已下沉到 helper 的职责**：
- RecordingFlowSupport：录制/回放命令判定与状态推进
- ExportFlowSupport：导出能力判定、默认文件名、WAV options 构建
- PluginFlowSupport：启动缓存恢复、扫描后 recovery 更新
- MIDI 导入流程：导入路径推导、导入结果处理、替换 take 并自动播放

**仍留在 MainComponent 的职责**：
- 录制/回放实际切换与状态缓存（已收敛为 `RecordingSession`，但启动/停止/回放编排仍在 MainComponent）
- MIDI 导入 FileChooser 生命周期与顶层编排（已收敛为约 50 行）
- 导出 MIDI/WAV 的 chooser 生命周期、路径记忆与日志（已通过 `runExportRecordingFlow()` 收敛）
- 布局文件 CRUD 顶层编排（已通过 helper 收敛，仍保留 FileChooser / modal 生命周期）
- 设置窗口生命周期与 dirty 管理（已局部收敛，仍由 MainComponent 拥有窗口）
- 插件 editor window 生命周期管理
- 音频设备重建胶水逻辑

**Phase 5.8+ 待规划提取目标**：
- 设置窗口管理提取（约 100 行）
- 布局管理继续收敛（约 100 行）
- 插件操作提取（约 130 行）
- 录制/回放编排提取（约 100 行）
- 插件 editor window 生命周期提取（约 50 行）

## 收敛原则

- 不做大规模重写；每次只抽一个边界，保持可构建、可回归。
- `MainComponent` 保留：UI 组件拥有权、JUCE 生命周期入口、窗口生命周期、键盘焦点恢复、顶层装配。
- Helper 只承载纯流程/状态转换，不直接拥有 JUCE `Component`。
- 音频线程边界仍以 `AudioEngine` / `RecordingEngine` 为准；helper 不进入 audio callback。
- 每个切片完成后至少跑 `./scripts/dev.sh wsl-build --configure-only` 和 `./scripts/dev.sh win-build`。

## 完成标准

- [x] Phase 5.1 完成：录制会话状态从 3 个字段收敛为 1 个结构体。（2026-05-01）
- [x] Phase 5.2 完成：导出 handler 重复代码减少 15+ 行。（2026-05-01）
- [x] Phase 5.3 完成：布局 CRUD handler 从约 200 行收敛到约 80 行。（2026-05-01）
- [x] Phase 5.4 完成：设置窗口管理从约 100 行收敛到约 30 行。（2026-05-01）
- [x] Phase 5.5 完成：按修正后的边界清理 AppState PluginState 中的 2 个 UI 派生字段。（2026-05-01）
- [x] Phase 5.6 完成：ControlsPanel 录制控制按钮状态统一为单一状态入口。（2026-05-01）
- [x] Phase 5.7 完成：`handleImportMidiClicked()` 从完整业务流程收敛为 FileChooser 生命周期 + 顶层编排。（2026-05-01）
- [x] Phase 5.1-5.7 人工回归：现有键盘演奏、插件加载、录制/回放/MIDI/WAV 导出、MIDI 导入、布局 preset 未发现明显回退。（2026-05-01）
- [~] Phase 5.8+ 待规划：MainComponent.cpp 继续收敛，目标从约 1587 行降至 1200 行以下。

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
