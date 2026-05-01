# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。  
> 读者：当前开发者、协作者、代码审查者。  
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前状态

当前活跃迭代：**架构健康迭代**（M8 收尾与 M9 启动之间的过渡轮，不编号为 M 阶段）。

M8 MIDI 文件导入与回放兼容性增强已收尾：M8-1 / M8-1b / M8-1c / M8-2 / M8-3 / M8-6 / M8-7 均已实现并通过 2026-05-01 人工验收；M8-5 merge-all 已搁置；M6-6e 作为后续 backlog。

本轮不是功能迭代，而是架构维护：在启动下一个能力里程碑（M9）之前，先巩固 `MainComponent` 职责边界、减少状态管理分散、防止架构回流膨胀。

---

**搁置说明**：M6 录制/回放稳定化和外部 MIDI 硬件依赖验证因当前无真实外部 MIDI 设备，暂无法测试。状态已记录至 `docs/testing/recording-playback.md`。这两个方向暂从本迭代移除，恢复时间取决于硬件条件。

## 本轮目标

巩固架构健康，为 M9 做准备。聚焦三个方向：

1. **MainComponent 职责继续收敛**：将仍留在 MainComponent 的流程性职责继续下沉到 helper。
2. **状态管理整理**：减少录制会话状态分散、清理 AppState 中的 UI 派生字段。
3. **ControlsPanel 按钮状态统一**：将分散的按钮状态更新收敛为单一状态源。

### 当前 MainComponent 分析（1555 行）

**已下沉到 helper 的职责**：
- RecordingFlowSupport：录制/回放命令判定与状态推进
- ExportFlowSupport：导出能力判定、默认文件名、WAV options 构建
- PluginFlowSupport：启动缓存恢复、扫描后 recovery 更新

**仍留在 MainComponent 的职责**：
- 录制/回放实际切换与状态缓存（`currentTake`、`currentRecordingState`、`currentTakeCanBeExported`）
- MIDI 导入完整用户流程（chooser、停止播放、导入、立即回放，80 行）
- 导出 MIDI/WAV 的 chooser、路径记忆、日志（重复模式明显）
- 布局文件 CRUD（save/import/rename/delete，共约 200 行）
- 设置窗口生命周期与 dirty 管理（约 100 行）
- 插件 editor window 生命周期管理
- 音频设备重建胶水逻辑

**潜在膨胀点**：
- `handleImportMidiClicked()`（80 行）：职责混合
- `handleRenameLayoutRequested()`（55 行）：异步 modal + 文件操作 + UI 同步
- `handleDeleteLayoutRequested()`（43 行）：确认 + 删除 + 回退 + UI 同步
- `showSettingsDialog()`（50 行）：Window 定义 + 关闭回调 + 内容创建
- `handleExportMidiClicked()` / `handleExportWavClicked()`：重复模式

## 本轮计划切片

### AH-1：录制会话状态结构化

- **目标**：将 `currentTake` + `currentRecordingState` + `currentTakeCanBeExported` 合并为单一 `RecordingSession` 结构体，减少状态分散。
- **修改范围**：`source/MainComponent.h`（新增 `RecordingSession` 或等价结构）、`source/MainComponent.cpp`（替换分散字段）。
- **不做**：不改变录制/回放行为；不改变 RecordingFlowSupport 接口；不改变 ControlsPanel 按钮逻辑。
- **完成标准**：录制相关状态从 3 个独立字段收敛为 1 个结构体；行为不回退。
- **风险**：低——纯内部重构，不改变外部接口。

### AH-2：导出流程统一

- **目标**：将 `handleExportMidiClicked()` 和 `handleExportWavClicked()` 中的重复模式（chooser 创建、路径更新、保存设置、导出调用、日志）统一为通用辅助层。
- **修改范围**：`source/Export/ExportFlowSupport.*`（新增通用导出编排 helper）、`source/MainComponent.cpp`（简化两个 handler）。
- **不做**：不改变 `MidiFileExporter` / `WavFileExporter` 核心实现；不改变 FileChooser 生命周期归属。
- **完成标准**：两个导出 handler 各减少 15+ 行重复代码；行为不回退。
- **风险**：低——已有 ExportFlowSupport 作为基础。

### AH-3：布局 CRUD 流程收敛

- **目标**：将 `handleSaveLayoutRequested()` / `handleImportLayoutRequested()` / `handleRenameLayoutRequested()` / `handleDeleteLayoutRequested()` 中的流程性逻辑抽到 `LayoutFlowSupport` 或等价 helper。
- **修改范围**：新增 `source/Layout/LayoutFlowSupport.*`（或扩展现有 `LayoutPreset.*`）、`source/MainComponent.cpp`（简化各 handler）。
- **不做**：不改变布局文件格式；不改变 ControlsPanel 布局相关回调接口；不引入新的 UI 组件。
- **完成标准**：布局相关 handler 从约 200 行收敛到约 80 行；MainComponent 只保留 chooser 生命周期和顶层编排。
- **风险**：低中——涉及文件操作和异步对话框，需仔细保持行为一致。

### AH-4：设置窗口生命周期收敛

- **目标**：将 `showSettingsDialog()` / `closeSettingsWindowAsync()` / `isSettingsWindowDirty()` / `saveAndCloseSettingsWindow()` 等设置窗口管理逻辑抽到独立 helper。
- **修改范围**：新增 `source/Settings/SettingsWindowFlowSupport.*`（或等价）、`source/MainComponent.cpp`（简化设置窗口相关方法）。
- **不做**：不改变 `SettingsComponent` 内部实现；不改变设置持久化逻辑。
- **完成标准**：设置窗口相关方法从约 100 行收敛到约 30 行；MainComponent 只保留窗口拥有权。
- **风险**：低——设置窗口生命周期相对独立。

### AH-5：AppState 清理（可选）

- **目标**：将 `PluginState` 中的 UI 派生字段（`pluginListText`、`availableFormatsDescription`）下沉到 `PluginPanelStateBuilder`，让 `AppState` 只保留业务事实字段。
- **修改范围**：`source/Core/AppState.h`、`source/UI/PluginPanelStateBuilder.*`。
- **不做**：不引入新的状态管理框架；不改变 AppState 快照机制。
- **完成标准**：`PluginState` 字段减少 2-3 个 UI 派生字段；builder 承担文案生成。
- **风险**：低——纯字段迁移。

## 推荐执行顺序

1. **AH-1**：最独立，风险最低，先建立状态结构化基础。
2. **AH-2**：导出统一，复用已有 ExportFlowSupport。
3. **AH-3**：布局 CRUD 收敛，涉及文件操作需仔细验证。
4. **AH-4**：设置窗口收敛，相对独立。
5. **AH-5**：AppState 清理，可选，视前 4 项进展决定是否执行。

## 收敛原则

- 不做大规模重写；每次只抽一个边界，保持可构建、可回归。
- `MainComponent` 保留：UI 组件拥有权、JUCE 生命周期入口、窗口生命周期、键盘焦点恢复、顶层装配。
- Helper 只承载纯流程/状态转换，不直接拥有 JUCE `Component`。
- 音频线程边界仍以 `AudioEngine` / `RecordingEngine` 为准；helper 不进入 audio callback。
- 每个切片完成后至少跑 `./scripts/dev.sh wsl-build --configure-only` 和 `./scripts/dev.sh win-build`。

## 完成标准

- [ ] AH-1 完成：录制会话状态从 3 个字段收敛为 1 个结构体。
- [ ] AH-2 完成：导出 handler 重复代码减少 15+ 行。
- [ ] AH-3 完成：布局 CRUD handler 从约 200 行收敛到约 80 行。
- [ ] AH-4 完成：设置窗口管理从约 100 行收敛到约 30 行。
- [ ] AH-5 完成（可选）：AppState PluginState 清理 UI 派生字段。
- [ ] 所有切片完成后，现有键盘演奏、插件加载、录制/回放/MIDI/WAV 导出、MIDI 导入、布局 preset 行为不回退。
- [ ] `MainComponent.cpp` 总行数从 1555 行下降到 1200 行以下。

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
- [x] M8-2 已实现并通过人工验收：导入 playback take 禁止再次导出 MIDI；多轨、非 960 PPQ、tempo meta event 和复杂 tempo map 限制均未发现明显问题。导入 MIDI 后允许导出 WAV 归入 M6-6e，暂时搁置。
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
