# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向：架构优化

Phase 7 核心功能已完成。当前 Backlog 见下方，阶段完成详情见 [`roadmap.md`](roadmap.md) §阶段路线图。

## 架构优化 Backlog

Phase 7 核心功能完成后识别的架构优化项。按优先级排列，为当前主要推进方向。

### P0（安全，快速）

- ~~**P0-C: 最近文件列表 UI**~~ [已完成]
  - `juce::RecentlyOpenedFilesList` 内置持久化，只需在 ControlsPanel 或 File 菜单添加一个"最近文件"子菜单/按钮。
  - 连接打开 Performance 和打开 MIDI 两条文件路径。
  - 无需额外数据模型，JUCE 自动保存到 `ApplicationProperties`。
  - 文件：`source/UI/ControlsPanel.cpp`、`source/Recording/RecordingSessionController.cpp`。
  - 提交：`0454b75` (feat) / `3b7e16a` (fix)。

- ~~**P0-B: `PluginOfflineRenderer` 生命周期注释补充**~~ [已完成]
  - 当前 `createOfflinePluginInstance` 注释只写了 "Must be called on the message thread"，未说明完整 3 阶段生命周期。
  - 改为：**message thread 创建实例 → move 到 `WavExportTask` → 后台线程 `renderTakeWithOfflinePlugin`**。
  - 文件：`source/Recording/PluginOfflineRenderer.h`。
  - 提交：`0046435`。

- ~~**P0-A: `PerformanceFile` MIDI 消息序列化改用 `MemoryBlock::toBase64Encoding()`**~~ [已完成]
  - 当前 `midiMessageToVar`/`varToMidiMessage` 手工将 `MidiMessage` 原始字节复制为 `juce::var` 整数数组。
  - 改用 `MemoryBlock::toBase64Encoding()` / `fromBase64Encoding()`，JSON 体积更小。
  - 删除旧 int-array 编解码 ~35 行。
  - 文件：`source/Recording/PerformanceFile.cpp`。
  - 提交：`d40845c`。 (refactor)

### P1（有明确价值，需中等投入）

- ~~**P1-A: Diagnostics 日志层迁移到 `juce::Logger` + `DevPianoLogger` 子类**~~ [已完成]
  - 当前 `DebugLog.h` 定义了 `DP_LOG_INFO/WARN/ERROR` 等 4 个宏 + `logInfo/logWarn/logError` 自定义函数。
  - 改为：删除 `DebugLog.h/.cpp`（~130 行），创建 `Log.h`（5 个薄宏直接调 `juce::Logger::writeToLog`）+ `DevPianoLogger` 子类。
  - 移除 DEBUG 双发问题（旧系统 Debug 下 DBG() + writeToLog 双发）。
  - `MidiTrace.h/.cpp` 不变（纯格式化工具函数，无日志依赖）。
  - 文件：`source/Diagnostics/`（Log.h、DevPianoLogger.h/.cpp，删除 DebugLog.h/.cpp）+ 所有调用点。
  - 提交：`c38e320`。 (refactor)

- ~~**P1-B: `WavExportOptions` 跨文件参数类型对齐**~~ [已完成]
  - 当前 `WavExportOptions` 定义在 `WavFileExporter.h`，`PluginOfflineRenderer` 和 `RecordingSessionController` 均引用。
  - 提取到独立 `Export/WavExportOptions.h`，消除跨模块依赖。`PluginOfflineRenderer.h` 前向声明 → `#include`。
  - 文件：`source/Export/WavExportOptions.h`（新建）、`source/Recording/WavFileExporter.h`、`source/Recording/PluginOfflineRenderer.h`、`source/Export/ExportFlowSupport.h`、`source/Export/WavExportTask.h`。
  - 提交：`4456f43`。 (refactor)

### P2（较大重构或已评估过的低优先级 tech debt）

- ~~**P2-A: `SettingsComponent` 手动回调迁移到 `ValueTree::Listener` 声明式绑定**~~ [已完成]
  - 每个 ComboBox/Slider/Toggle 的 `onChange` 回调替换为 `editingState.setProperty()`，集中式 `valueTreePropertyChanged` 统一处理 model 回写 + setDirty + 回调触发。
  - 两个 ToggleButton 使用 Value binding 消除回调。
  - 顺便修复 `fadeSpeedSlider` 遗漏 `setDirty(true)` 的 bug。
  - 文件：`source/Settings/SettingsComponent.h`。
  - 提交：`af5644a`。 (refactor)

- ~~**P2-B: `MainComponent` 继续瘦身（Phase 5.8f 后续）**~~ [已完成]
  - `showSettingsDialog()` body（~47 行）提取到 `SettingsWindowManager::showFor(MainComponent&)`，遵循现有友元类模式。
  - `MainComponent.cpp`: 812 → 765 行（-47）。
  - 文件：`source/MainComponent.cpp/.h`、`source/Settings/SettingsWindowManager.cpp/.h`。
  - 提交：`4b4e1af`。 (refactor)

## 本轮决策

- **Phase 7-5（Metadata 编辑对话框）** — 明确搁置。`PerformanceFileMetadata` struct + JSON 序列化基础设施已就位，UI 编辑对话框不开发现阶段无价值。
- **Phase 7-7（全屏模式）** — 不实现。`resizable` toggle + OS 最大化窗口可替代，且无 kiosk mode 边界问题。

## 已修复缺陷（回归提醒）

各缺陷的完整修复记录与回归条件见 [`../issues/known-issues.md`](../issues/known-issues.md) §2：

- 启动早期首音音高异常（保留 `25ms` audio warmup）。
- MIDI 导入播放首音无声（playback-start pre-roll / arming）。
- 辅助窗口键盘焦点冲突（restoreKeyboardFocus guard）。
- Phase 6-2 播放速度控制（公式修正 + position 重校准 + atomic 化）。


## 验证命令

代码修改后优先执行：

```bash
./scripts/dev.sh wsl-build --configure-only
./scripts/dev.sh win-build
```

涉及环境或路径问题时执行：

```bash
./scripts/dev.sh self-check
```

## 相关文档

- 项目路线图：[`roadmap.md`](roadmap.md)
- 架构概览：[`../reference/architecture.md`](../reference/architecture.md)
- Phase 5 完成记录：[`../archive/phase5-architecture-convergence.md`](../archive/phase5-architecture-convergence.md)
- Phase 6-7 完成记录：[`../archive/phase6-7-completion-detail.md`](../archive/phase6-7-completion-detail.md)
