# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。  
> 读者：当前开发者、协作者、代码审查者。  
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前状态

当前活跃阶段：**Phase 5：架构收敛与 MainComponent 瘦身**

Phase 5.1-5.7 已完成（2026-05-01）：MainComponent 职责下沉，包括录制会话状态结构化、导出流程统一、布局 CRUD 收敛、设置窗口收敛、AppState 清理、ControlsPanel 按钮状态统一、MIDI 导入流程下沉。

当前 `MainComponent.cpp` 1349 行（5.8a 完成后），目标降至 1200 行以下。

---

**搁置说明**：外部 MIDI 硬件依赖验证和 VST3 插件离线渲染（Phase 3-5）因当前条件暂无法测试，状态已记录至 `docs/testing/phase3-recording-playback.md`。

## Phase 5.8 瘦身分析

### 当前 MainComponent.cpp 行数分布

| 职责域 | 行范围 | 约行数 | 占比 |
|---|---|---|---|
| 匿名 namespace（工具函数） | 19-146 | 128 | 8% |
| 构造/析构/初始化 | 148-251 | 104 | 7% |
| 音频回调 + Timer + UI 基础 | 322-404 | 83 | 5% |
| 性能/插件恢复设置 helper | 426-492 | 67 | 4% |
| **布局管理 handlers** | 494-708 | **215** | **14%** |
| UI 同步 + 设置持久化 | 710-818 | 109 | 7% |
| **设置窗口管理** | 819-919 | **101** | **6%** |
| **状态快照构建 + UI 刷新** | 921-996 | **76** | **5%** |
| **插件启动恢复** | 998-1052 | **55** | **3%** |
| 运行时音频配置 helper | 1054-1088 | 35 | 2% |
| **录制会话同步 + 导出** | 1090-1146 | **57** | **4%** |
| **录制/回放内部操作** | 1148-1236 | **89** | **6%** |
| **插件加载/卸载/editor/扫描** | 1238-1357 | **120** | **8%** |
| **录制/回放/MIDI 导入 handlers** | 1359-1587 | **229** | **14%** |

### 提取优先级排序

按**自包含度 × 行数收益**排序：

1. **布局管理 handlers（~215 行）** — 最自包含，仅依赖 `keyboardMidiMapper`、`appSettings`、`controlsPanel`、`audioEngine`，无音频线程交互。
2. **录制/回放/MIDI 导入 handlers（~229 行）** — 依赖 `recordingSession`、`recordingEngine`、`audioEngine`、`appSettings`，但逻辑清晰，已有 `RecordingFlowSupport` 基础。
3. **插件操作（~175 行）** — 依赖 `pluginHost`、`pluginPanel`、`pluginEditorWindow`、`appSettings`，已有 `PluginFlowSupport` 基础。
4. **设置窗口管理（~101 行）** — 依赖 `settingsWindow`、`deviceManager`、`appSettings`，自包含度高但行数较少。
5. **状态快照构建（~76 行）** — 纯读取函数，依赖多但无副作用，提取收益中等。

---

## Phase 5.8 任务计划

### 5.8a：布局管理 handlers 提取

**目标**：将布局 CRUD + 文件对话框 + 确认对话框逻辑提取到 `Layout/LayoutFlowSupport`。

**提取内容**（约 215 行 → 新文件约 180 行，MainComponent 减少约 180 行）：

从 `MainComponent.cpp` 提取到 `Layout/LayoutFlowSupport.h/.cpp`：
- `findUserLayoutFileById()` — 匿名 namespace 中的工具函数
- `handleLayoutChanged()` → `LayoutFlowSupport::handleLayoutChanged()`
- `handleSaveLayoutRequested()` → `LayoutFlowSupport::handleSaveLayoutRequested()`
- `handleResetLayoutToDefaultRequested()` → `LayoutFlowSupport::handleResetLayoutToDefaultRequested()`
- `handleImportLayoutRequested()` → `LayoutFlowSupport::handleImportLayoutRequested()`
- `handleRenameLayoutRequested()` → `LayoutFlowSupport::handleRenameLayoutRequested()`
- `handleDeleteLayoutRequested()` → `LayoutFlowSupport::handleDeleteLayoutRequested()`
- `applyLayoutAndCommit()` → `LayoutFlowSupport::applyLayoutAndCommit()`
- `runLayoutFileChooser()` → `LayoutFlowSupport::runLayoutFileChooser()`
- `runLayoutRenameDialog()` → `LayoutFlowSupport::runLayoutRenameDialog()`
- `runLayoutDeleteDialog()` → `LayoutFlowSupport::runLayoutDeleteDialog()`

**设计模式**：参考 `RecordingFlowSupport` / `PluginFlowSupport`，使用 namespace + 自由函数，通过回调或引用注入依赖（`KeyboardMidiMapper&`、`SettingsModel&`、`ControlsPanel&`、`AudioEngine&`）。

**MainComponent 中保留**：thin wrapper 调用 `LayoutFlowSupport` 函数，或直接在 `initialiseUi()` 中将 lambda 绑定到 `LayoutFlowSupport` 函数。

**预期 MainComponent 减少**：~180 行。

---

### 5.8b：录制/回放/MIDI 导入 handlers 提取

**目标**：将录制编排逻辑提取到 `Recording/RecordingSessionController`。

**提取内容**（约 229 行 → 新文件约 200 行，MainComponent 减少约 200 行）：

从 `MainComponent.cpp` 提取到 `Recording/RecordingSessionController.h/.cpp`：
- `RecordingSession` 结构体（从 MainComponent.h 移出）
- `handleRecordClicked()` → controller method
- `handlePlayClicked()` → controller method
- `handleStopClicked()` → controller method
- `handleBackToStartClicked()` → controller method
- `handleExportMidiClicked()` → controller method
- `handleExportWavClicked()` → controller method
- `handleImportMidiClicked()` → controller method
- `tryImportMidiFile()` → controller method
- `replaceTakeAndStartPlayback()` → controller method
- `startInternalRecording()` → controller method
- `stopInternalRecording()` → controller method
- `startInternalPlayback()` → controller method
- `stopInternalPlayback()` → controller method
- `syncRecordingSessionToUi()` → controller method
- `runExportRecordingFlow()` → controller method
- `timerCallback()` 中的 playback-ended 处理 → controller method

**设计模式**：`RecordingSessionController` 持有 `RecordingSession` 状态，通过构造注入 `RecordingEngine&`、`AudioEngine&`、`SettingsModel&`、`ControlsPanel&`。导出文件对话框由 controller 管理（chooser 成员移入）。

**MainComponent 中保留**：
- `RecordingSessionController` 成员
- `initialiseUi()` 中的 lambda 绑定
- `timerCallback()` 调用 controller 的 playback-ended check

**预期 MainComponent 减少**：~200 行。

---

### 5.8c：插件操作提取

**目标**：将插件加载/卸载/editor/扫描编排逻辑提取到 `Plugin/PluginOperationController`。

**提取内容**（约 175 行 → 新文件约 150 行，MainComponent 减少约 150 行）：

从 `MainComponent.cpp` 提取到 `Plugin/PluginOperationController.h/.cpp`：
- `restorePluginStateOnStartup()` → controller method
- `restorePluginScanPathOnStartup()` → controller method
- `restoreLastPluginOnStartup()` → controller method
- `restorePluginByNameOnStartup()` → controller method
- `resolvePluginScanPath()` → controller method
- `loadPluginByNameAndCommitState()` → controller method
- `loadSelectedPlugin()` → controller method
- `unloadPluginAndCommitState()` → controller method
- `unloadCurrentPlugin()` → controller method
- `tryCreatePluginEditor()` → controller method
- `handlePluginEditorWindowClosedAsync()` → controller method
- `closePluginEditorWindow()` → controller method
- `openPluginEditorWindow()` → controller method
- `togglePluginEditor()` → controller method
- `scanPluginsAtPathAndApplyRecoveryState()` → controller method
- `scanPluginsAtPathAndCommitState()` → controller method
- `scanPlugins()` → controller method

**设计模式**：`PluginOperationController` 持有 `pluginEditorWindow`，通过构造注入 `PluginHost&`、`SettingsModel&`、`PluginPanel&`。音频设备重建逻辑（`runPluginActionWithAudioDeviceRebuild`）保留在 MainComponent 或提取为独立 helper。

**MainComponent 中保留**：
- `PluginOperationController` 成员
- `initialiseUi()` 中的 lambda 绑定
- `runPluginActionWithAudioDeviceRebuild()`（与 AudioAppComponent 生命周期耦合）

**预期 MainComponent 减少**：~150 行。

---

### 5.8d：设置窗口管理提取

**目标**：将设置窗口生命周期管理提取到 `Settings/SettingsWindowManager`。

**提取内容**（约 101 行 → 新文件约 80 行，MainComponent 减少约 80 行）：

从 `MainComponent.cpp` 提取到 `Settings/SettingsWindowManager.h/.cpp`：
- `SettingsDialogWindow` 内部类 → 独立类
- `showSettingsDialog()` → manager method
- `isSettingsWindowDirty()` → manager method
- `isSettingsWindowOpen()` → manager method
- `closeSettingsWindowAsync()` → manager method
- `closeSettingsWindow()` → manager method
- `saveAndCloseSettingsWindow()` → manager method
- `getSettingsContent()` → manager method

**设计模式**：`SettingsWindowManager` 持有 `settingsWindow`，通过构造注入 `juce::AudioDeviceManager&`、`SettingsModel&`。关闭回调通过 `std::function` 注入。

**MainComponent 中保留**：
- `SettingsWindowManager` 成员
- `showSettingsDialog()` thin wrapper（传入 close callback）

**预期 MainComponent 减少**：~80 行。

---

### 5.8e：状态快照构建提取

**目标**：将 `buildRuntime*Snapshot()` 和 `buildCurrentAppStateSnapshot()` 提取到 `Core/AppStateBuilder`（已有头文件，扩展实现）。

**提取内容**（约 76 行 → 扩展现有文件，MainComponent 减少约 70 行）：

从 `MainComponent.cpp` 移到 `Core/AppStateBuilder.h/.cpp`：
- `buildRuntimeAudioStateSnapshot()` → 自由函数，接收 `AudioDeviceDiagnostics` 参数
- `buildRuntimePluginStateSnapshot()` → 自由函数，接收 `PluginHost&` + editor 状态
- `buildRuntimeInputStateSnapshot()` → 自由函数，接收 `KeyboardMidiMapper&` + MIDI 状态
- `buildCurrentAppStateSnapshot()` → 组合函数

**设计模式**：纯函数，无副作用，参数注入。`renderReadOnlyUiState()` 和 `refreshReadOnlyUiStateFromCurrentSnapshot()` 保留在 MainComponent（依赖 UI 组件引用）。

**预期 MainComponent 减少**：~70 行。

---

## Phase 5.8 执行顺序与依赖

```
5.8a（布局） ──┐
5.8b（录制） ──┼── 可并行，无相互依赖
5.8c（插件） ──┤
5.8d（设置） ──┤
5.8e（快照） ──┘
```

建议执行顺序：**5.8a → 5.8b → 5.8c → 5.8d → 5.8e**

理由：
- 5.8a 最自包含，风险最低，先做建立模式。
- 5.8b 行数最多，收益最大。
- 5.8c 依赖 5.8b 中 `runPluginActionWithAudioDeviceRebuild` 的归属决策。
- 5.8d 和 5.8e 行数较少，收尾。

---

## Phase 5.8 预期成果

| 提取项 | MainComponent 减少行数 | 累计减少 |
|---|---|---|
| 当前 | — | 1587 行 |
| 5.8a 布局管理 | -238 ✅ | 1349 行 |
| 5.8b 录制编排 | -200 | 1149 行 |
| 5.8c 插件操作 | -150 | 999 行 |
| 5.8d 设置窗口 | -80 | 919 行 |
| 5.8e 状态快照 | -70 | 849 行 |

**5.8a 完成后已降至 1349 行。5.8b 完成后即可达到 1200 行目标**。5.8c-5.8e 为额外收敛。

---

## Phase 5.8+ 后续机会（5.8 完成后评估）

- **音频设备管理 helper**（~55 行）：`initialiseAudioDevice()`、`captureAudioDeviceState()`、`prepareForAudioDeviceRebuild()`、`finishAudioDeviceRebuild()`、`runPluginActionWithAudioDeviceRebuild()` 可提取为 `Audio/AudioDeviceLifecycleHelper`。
- **性能/插件恢复设置 helper**（~67 行）：`getPerformanceSettingsFromUi()`、`applyPerformanceSettingsToUi()`、`applyPerformanceSettingsToAudioEngine()` 等可提取为 `Settings/SettingsFlowSupport`。
- **匿名 namespace 工具函数**（~128 行）：`makeSafeUiText()`、`getLastMidiExportDirectory()`、`getLastMidiImportDirectory()`、`makeDefaultMidiExportFile()`、`suppressImeForPeer()` 可按域分散到对应模块。

---

## 完成标准

- [x] Phase 5.8a 完成：布局管理 handlers 提取到 `Layout/LayoutFlowSupport`，MainComponent 从 1587 行降至 1349 行（减少 238 行）。（2026-05-01）
- [ ] Phase 5.8b 完成：录制编排提取到 `Recording/RecordingSessionController`，MainComponent 减少 ~200 行。
- [ ] Phase 5.8c 完成：插件操作提取到 `Plugin/PluginOperationController`，MainComponent 减少 ~150 行。
- [ ] Phase 5.8d 完成：设置窗口管理提取到 `Settings/SettingsWindowManager`，MainComponent 减少 ~80 行。
- [ ] Phase 5.8e 完成：状态快照构建提取到 `Core/AppStateBuilder`，MainComponent 减少 ~70 行。
- [ ] Phase 5.8 总验证：MainComponent.cpp ≤ 1200 行，WSL 构建通过，Windows MSVC 构建通过。
- [ ] Phase 5.8 回归：键盘演奏、插件加载/卸载/editor、录制/回放/MIDI/WAV 导出、MIDI 导入、布局 preset、设置窗口未发现明显回退。

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

## 相关文档

- 项目路线图：`docs/roadmap/roadmap.md`
- 架构概览：`docs/architecture/overview.md`
- Phase 5.1-5.7 完成记录：`docs/archive/phase5-architecture-convergence.md`
