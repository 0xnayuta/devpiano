# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。  
> 读者：当前开发者、协作者、代码审查者。  
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前状态

当前活跃阶段：**Phase 5：架构收敛与 MainComponent 瘦身**

当前插入缺陷 **启动 / 音频重建早期首音音高异常** 已修复并通过人工验证；保留 `25ms` audio warmup。

插入缺陷 **MIDI 导入播放首音无声** 已通过 playback-start pre-roll / arming 修复并完成人工回归；后续触及 MIDI import / playback 启动链路时执行 Phase 4 §11.1 回归。

下一步可继续 Phase 5.8e 瘦身。

Phase 5.1-5.7 已完成（2026-05-01）：MainComponent 职责下沉，包括录制会话状态结构化、导出流程统一、布局 CRUD 收敛、设置窗口收敛、AppState 清理、ControlsPanel 按钮状态统一、MIDI 导入流程下沉。

当前 `MainComponent.cpp` 631 行（5.8a+5.8b+5.8c+5.8d 完成后），远低于 1200 行目标。

---

**搁置说明**：外部 MIDI 硬件依赖验证和 VST3 插件离线渲染（Phase 3-5）因当前条件暂无法测试，状态已记录至 `docs/testing/phase3-recording-playback.md`。

---

## 已完成插入缺陷：启动 / 音频重建早期首音音高异常

### 现象

- 程序启动后立即弹奏，首个音或启动后数秒内多个音会出现音高变化和异常音色。
- 等待数秒后再演奏，音高恢复稳定且正确。
- 无外部 MIDI 设备时仍复现；鼠标点击虚拟键盘、实体电脑键盘、内置 fallback synth、VST3 插件均受影响。
- 手动加载 / 卸载插件后也会再次出现。
- Windows Audio / DirectSound 均可复现；当前观察到 live device 为 `48000 Hz / 480 samples`。

### 根因判断与修复结论

外部 MIDI pitch wheel / controller 脏状态已基本排除。调查中先修正了 `MainComponent::initialiseAudioDevice()` 双阶段初始化：

1. `setAudioChannels(0, 2)` 已经会初始化音频设备、注册 audio callback 并触发 `prepareToPlay()`。
2. 随后如果存在保存的 audio device XML，又调用 `deviceManager.initialise(0, 2, xml, true)`，导致音频设备在 callback 已挂载后再次初始化。
3. 启动 / 重建早期可能短暂经过默认 sample rate / buffer，再切换到最终 live 配置。
4. 内置 synth 和 VST3 插件都依赖 prepare sample rate，因此同一生命周期问题可同时影响两条发声路径。

单独移除双初始化后异常更明显，说明旧双初始化并非根因，而是曾偶然提供额外预热时间。最终有效修复是在 `AudioEngine::prepareToPlay()` 后保留短暂 warmup：warmup 期间输出静音并清理 pending keyboard / MIDI state。

已人工验证 500ms、250ms、100ms、25ms 均稳定；正式值保留 `25ms`，约等于当前 `48000 Hz / 480 samples` 环境下 2-3 个 audio blocks。

修复原理：首音异常来自音频 prepare 后最早几个 blocks 的生命周期 transient，而不是具体输入设备或发声引擎。`25ms` warmup 让音频回调先跑过 2-3 个静音 blocks，并丢弃这段窗口内的 pending note / MIDI state，避免极早期输入被压缩到首个可听 block 或进入尚未稳定的 audio path。

详细问题记录与修复方案见：[`../testing/known-issues.md`](../testing/known-issues.md) §2。

### 已实施修复

1. **修正 `MainComponent::initialiseAudioDevice()` 双初始化**
   - 将保存的 XML 作为第三个参数传给 `setAudioChannels(0, 2, state)`。
   - 删除后续手动 `deviceManager.initialise(...)`。
2. **在 `AudioEngine` 中保留 `25ms` warmup**
   - `prepareToPlay()` 后设置 warmup block 数。
   - `getNextAudioBlock()` 在 warmup 期间输出静音，并清理 `MidiKeyboardState`、`MidiMessageCollector` 与 MIDI buffer。

### 已通过最小回归

- 无 VST：启动后立即点击虚拟键盘，首音稳定。
- 无 VST：启动后立即按实体键盘，首音稳定。
- 自动恢复 VST：启动后立即弹奏，首音稳定。
- 手动加载 VST 后立即弹奏，首音稳定。
- 手动卸载回内置 synth 后立即弹奏，首音稳定。
- Windows Audio 与 DirectSound 下均稳定。

---

## Phase 5.8 瘦身分析

### 当前 MainComponent.cpp 剩余关注点

当前 `MainComponent.cpp` 为 631 行；5.8a+5.8b+5.8c+5.8d 已将布局、录制/回放/MIDI 导入、插件操作和设置窗口管理下沉。剩余 5.8 关注点：

| 职责域 | 当前位置 | 后续处理 |
|---|---|---|
| 状态快照构建 | `buildRuntimeAudioStateSnapshot()`、`buildRuntimePluginStateSnapshot()`、`buildRuntimeInputStateSnapshot()`、`buildCurrentAppStateSnapshot()` | 5.8e：提取到 `Core/AppStateBuilder` |
| 音频设备生命周期胶水 | `initialiseAudioDevice()`、`prepareForAudioDeviceRebuild()`、`finishAudioDeviceRebuild()`、`runPluginActionWithAudioDeviceRebuild()` | 5.8+ 后续机会 |
| 匿名 namespace 工具函数 | `makeSafeUiText()`、`suppressImeForPeer()` 等 | 5.8+ 后续按域分散 |

### 提取优先级排序（历史分析，5.8a-5.8c 已完成）

按**自包含度 × 行数收益**排序：

1. **布局管理 handlers（~215 行）** — 已完成，提取到 `Layout/LayoutFlowSupport`。
2. **录制/回放/MIDI 导入 handlers（~229 行）** — 已完成，提取到 `Recording/RecordingSessionController`。
3. **插件操作（~175 行）** — 已完成，提取到 `Plugin/PluginOperationController`。
4. **设置窗口管理（~101 行）** — 依赖 `settingsWindow`、`deviceManager`、`appSettings`，自包含度高但行数较少。
5. **状态快照构建（~76 行）** — 纯读取函数，依赖多但无副作用，提取收益中等。

---

## Phase 5.8 任务计划

### 5.8a：布局管理 handlers 提取（已完成）

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

### 5.8b：录制/回放/MIDI 导入 handlers 提取（已完成）

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

### 5.8c：插件操作提取（已完成）

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

### 5.8d：设置窗口管理提取（已完成）

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

**设计模式**：`SettingsWindowManager` 持有设置窗口和 `SettingsComponent` 生命周期；`MainComponent` 在 `showSettingsDialog()` thin wrapper 中传入 parent、`AudioDeviceManager`、已保存音频设备状态、保存回调和关闭后恢复焦点回调。

**MainComponent 中保留**：
- `SettingsWindowManager` 成员
- `showSettingsDialog()` thin wrapper（传入 close callback）

**实际 MainComponent 减少**：80 行（711 → 631）。

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
| 5.8b 录制编排 | -419 ✅ | 930 行 |
| 5.8c 插件操作 | -219 ✅ | 711 行 |
| 5.8d 设置窗口 | -80 ✅ | 631 行 |
| 5.8e 状态快照 | -70 | 561 行 |

**5.8a+5.8b+5.8c+5.8d 完成后已降至 631 行，远低于 1200 行目标**。5.8e 为额外收敛。

---

## Phase 5.8+ 后续机会（5.8 完成后评估）

- **音频设备管理 helper**（~55 行）：`initialiseAudioDevice()`、`captureAudioDeviceState()`、`prepareForAudioDeviceRebuild()`、`finishAudioDeviceRebuild()`、`runPluginActionWithAudioDeviceRebuild()` 可提取为 `Audio/AudioDeviceLifecycleHelper`。
- **性能/插件恢复设置 helper**（~67 行）：`getPerformanceSettingsFromUi()`、`applyPerformanceSettingsToUi()`、`applyPerformanceSettingsToAudioEngine()` 等可提取为 `Settings/SettingsFlowSupport`。
- **匿名 namespace 工具函数**（~128 行）：`makeSafeUiText()`、`getLastMidiExportDirectory()`、`getLastMidiImportDirectory()`、`makeDefaultMidiExportFile()`、`suppressImeForPeer()` 可按域分散到对应模块。

---

## 完成标准

- [x] Phase 5.8a 完成：布局管理 handlers 提取到 `Layout/LayoutFlowSupport`，MainComponent 从 1587 行降至 1349 行（减少 238 行）。（2026-05-01）
- [x] Phase 5.8b 完成：录制/回放/MIDI 导入编排提取到 `Recording/RecordingSessionController`，MainComponent 从 1349 行降至 930 行（减少 419 行）。（2026-05-01）
- [x] Phase 5.8c 完成：插件操作提取到 `Plugin/PluginOperationController`，MainComponent 从 930 行降至 711 行（减少 219 行）。（2026-05-02）
- [x] Phase 5.8d 完成：设置窗口管理提取到 `Settings/SettingsWindowManager`，MainComponent 从 711 行降至 631 行（减少 80 行）。（2026-05-02）
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
