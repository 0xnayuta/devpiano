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

- **P1-A: Diagnostics 日志层迁移到 `juce::Logger` 内置级别**
  - 当前 `DebugLog.h` 定义了 `DP_LOG_INFO/WARN/ERROR` 等 4 个宏 + `logInfo/logWarn/logError` 自定义函数。
  - 改为：直接使用 `juce::Logger` 内置级别，移除自定义日志宏体系。
  - 需评估：DEBUG-only 编译开关（`DP_DEBUG_LOG`）的替代方案、`MidiTrace` 的特化需求可否用 `juce::Logger` level filtering 覆盖。
  - 文件：`source/Diagnostics/`（4 个文件）+ 所有调用点。
  - 估算：~1–2 小时。

- **P1-B: `WavExportOptions` 跨文件参数类型对齐**
  - 当前 `WavExportOptions` 定义在 `WavFileExporter.h`，`PluginOfflineRenderer` 和 `RecordingSessionController` 均引用。
  - 确认 `renderTakeWithOfflinePlugin` 和 `exportTakeAsWavFile` 两条导出路径的 writer 选项（sampleRate/bitsPerSample/channels）一致传递。
  - 提取 `WavExportOptions` 到独立公共头文件，消除跨模块依赖隐患。
  - 文件：`source/Export/WavExportOptions.h`（新建）、`source/Recording/WavFileExporter.h`、`source/Recording/PluginOfflineRenderer.h`。
  - 估算：~30 分钟。

### P2（较大重构或已评估过的低优先级 tech debt）

- **P2-A: `SettingsComponent` 手动回调迁移到 `ValueTree::Listener` 声明式绑定**
  - 当前每个 ComboBox/Slider 的 `onChange` 回调手工调用 `onDisplaySettingsChanged()` + `setDirty(true)`。
  - `juce::ValueTree::Listener` + `valueTreePropertyChanged()` 可实现自动响应，消除样板代码。
  - 难点：`SettingsModel` 含 `std::unordered_map<int, int>`、`ChannelMatrix` 等非标量字段，不能直接映射到 `ValueTree` property。
  - 建议等待设置页面扩展至 ~15 个控件以上再执行。
  - 文件：`source/Settings/SettingsComponent.h`、`source/Settings/SettingsModel.h`。
  - 估算：~3–4 小时。

- **P2-B: `MainComponent` 继续瘦身（Phase 5.8f 后续）**
  - Phase 5 结束时 `MainComponent.cpp` 降至 606 行，Phase 6/7 新增功能后当前 ~750 行。
  - `showSettingsDialog()` 中约 80 行的配置窗口装配 + 回调闭包可提取到 `SettingsWindowManager` 或新的 `SettingsDialogBuilder`。
  - Phase 5.8f 已评估为低优先级 tech debt，建议等待 MainComponent 再次膨胀至 900+ 行再切入。
  - 文件：`source/MainComponent.cpp`、`source/Settings/SettingsWindowManager.cpp`。
  - 估算：~2 小时。

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
