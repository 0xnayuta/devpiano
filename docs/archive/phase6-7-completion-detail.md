# Phase 6-7 完成明细（历史归档）

> 用途：记录 Phase 6 和 Phase 7 各子项的详细完成描述，供需要查阅实现细节时参考。
> 当前路线图 `roadmap.md` 中已压缩为里程碑摘要。

---

## Phase 6：功能补齐——钢琴键盘、MIDI 矩阵、绑定系统、GUI 设置

**状态：** Phase 6 主体已完成（6-1 ~ 6-11 中 5 控件完成）；Phase 7 已启动，Phase 7-1 为当时的 P0。

**目标：** 填补与 FreePiano 差距中的 P0 核心功能——视觉化钢琴键盘、16 通道 MIDI 矩阵、扩展绑定类型、GUI 设置完整化。

### 已完成

#### Phase 6-1：演奏文件保存/打开（核心）✅
- `RecordingTake` JSON 序列化（`.devpiano` 格式），包含 events、sampleRate、lengthSamples、元数据。
- ControlsPanel Save/Open 按钮 + FileChooser 流程。
- `RecordingSessionController` 接入 `PerformanceFile` API。

#### Phase 6-2：播放速度控制 ✅
- ControlsPanel 增加速度显示（`1.00x`）和 `-`/`+` 按钮。
- `RecordingEngine.playbackSpeedMultiplier` + `setPlaybackSpeedMultiplier()`。
- `renderPlaybackBlock()` 使用 `combinedRatio` 实时缩放时间戳，不修改原始数据。

#### Phase 6-5：MIDI 导入增强 ✅
- 扩展 `MidiFileImporter` 收集 CC（CC64 sustain 等）、pitch bend、program change 事件。
- CC/pitch bend/program change 与 note 事件共用同一时间线。
- 导入成功后输出 note-on/off 与 CC/pitch-bend/program-change 的分类计数。

#### Phase 6-6：Diagnostics 最小层 ✅
- `source/Diagnostics/`（4 个文件）：`DebugLog.h/.cpp`、`MidiTrace.h/.cpp`。
- 接口：`DP_LOG_INFO/WARN/ERROR`、`DP_DEBUG_LOG`、`DP_TRACE_MIDI`。
- Debug-only 宏在 Release 下无输出或零副作用。

#### Phase 6-7：MIDI / Performance 测试夹具与最小回归样本库 ✅
- `tests/fixtures/midi/`（7 个 MIDI fixture 文件）和 `tests/fixtures/performance/`。
- 覆盖 MIDI 导入、MIDI roundtrip、错误处理、回放行为验证的统一输入基准。

#### Phase 6-8：自定义钢琴键盘 ✅
- `CustomKeyboard` JUCE Component，支持 classic / channel / velocity 着色模式。
- 外部 MIDI / 电脑键盘 / 鼠标拖拽触发的 note 可视化，fade 动画。
- 音符显示模式（Do Re Mi / 固定 Do / 音符名称）。
- 双击键弹出绑定编辑对话框。

#### Phase 6-9：16 通道 MIDI 矩阵 ✅
- `ChannelMatrix` 数据模型 + `MidiChannelMapper` 路由服务。
- 三条路径（鼠标点击、电脑键盘、外部 MIDI）均通过矩阵路由。
- 矩阵默认 inactive（`active=false`），完全向后兼容。

#### Phase 6-10：扩展绑定系统（note-only 绑定编辑器已完成）✅
- `KeyBindingEditDialog` 每键 GUI 编辑面板（MIDI note/channel/velocity）。
- CC / program change / pitch bend / channel pressure 多事件类型扩展 — 已取消（用户场景不足 10%，`MidiChannelMapper` 可在插件层处理，无需键盘绑定干预，详见 §已取消功能）。

#### Phase 6-11：GUI 设置补齐（5 控件已完成，2 控件待定）
- `KeyboardDisplaySettings`（colourMode / noteDisplay / fadeSpeed）已持久化。
- ✅ colourMode ComboBox、noteDisplay ComboBox、fadeSpeed Slider、resizable ToggleButton、instrumentFilter ToggleButton 已完成。
- ChannelMatrix UI 编辑器、MIDI 重映射 UI — 已取消（无真实用户场景，详见 §已取消功能）。
- Phase 6-10 多事件扩展 — 已取消（同上）。
- Phase 6-4 基础 MIDI 编辑 — 已取消（详见 §已取消功能）。
- Phase 6-3（最近文件列表）— 已升级为 P0-C 纳入当前 Backlog；拖拽打开已在 Phase 7-3 完成。

---

## Phase 7：VST3 离线渲染与体验完善

**状态：** 全部子项已完成（Phase 7-1 / 7-2 / 7-3 / 7-4 / 7-6 / 快速清扫）。Phase 7-5（Metadata 编辑）经评估无必要，Phase 7-7（全屏模式）不实现。

### 已完成

#### Phase 7 启动前快速清扫 ✅
- 补 `instrumentFilter` toggle 的 show/hide 接入。

#### Phase 7-1：VST3 插件离线渲染 ✅
- message thread 上创建 `AudioPluginInstance` 副本、后台线程渲染。
- 以 `RecordingTake` 事件为输入，逐 block 渲染到音频 buffer。
- 输出到现有 WAV 写入流程（`WavFileExporter` / `WavAudioFormat`）。
- 官方 git tag `v0.1.0`。

#### Phase 7-2：播放速度精确控制 ✅
- 替换步进按钮为 `juce::Slider` + 数值标签。
- 跨线程安全：`std::atomic<double>` / `std::atomic<std::int64_t>`。

#### Phase 7-3：拖放文件支持 ✅
- `FileDragAndDropTarget`：`.devpiano` / `.mid` / `.freepiano.layout` / `.vst3`。
- 蓝色 3px 边框视觉反馈。

#### Phase 7-4：运行时中英文语言切换 ✅
- JUCE `Translation` 机制，替换旧 `language_strdef.h` 体系。
- 嵌入式中文 locale 表（`source/Locale/zh_CN.loc.h`）。
- Settings 页面 Language ComboBox。
- 切换即时生效，所有面板 `refreshTexts()` 联动。

#### Phase 7-6：Export 进度对话框 ✅
- `WavExportTask`：`ThreadWithProgressWindow` + `ProgressBar` + 取消支持。
- message thread 上创建 `AudioPluginInstance` 副本，后台线程渲染。
- 取消时自动清理不完整 WAV 文件。

### 已评估并关闭

#### Phase 7-5：Metadata 编辑对话框 — 明确搁置
- `PerformanceFileMetadata` struct + JSON 序列化基础设施已就位。
- UI 编辑对话框不开发现阶段无价值。

#### Phase 7-7：全屏模式 — 不实现
- `resizable` toggle + OS 最大化窗口可替代。
- 无 kiosk mode 边界问题。

---

## 已取消功能

以下功能在 Phase 7 完成后经评估决定取消，不再列入计划。

### Phase 6-4 基础 MIDI 编辑（delete notes）— 取消
- 允许从 `RecordingTake` 中删除单条 note 的功能。
- **取消理由**：当前重录成本低，delete note 需设计选中/编辑模式和撤销机制，价值不足以打断架构优化。

### ChannelMatrix UI 编辑器 — 取消
- 以网格/表格 UI 编辑 16 通道 MIDI 矩阵的配置。
- **取消理由**：矩阵默认 inactive（完全透传），绝大多数用户不受影响。只有需要复杂多通道路由的 power user 才有意义，当前无真实用户场景。

### MIDI 输入重映射 UI — 取消
- 重新映射外部 MIDI 控制器通道的 UI。
- **取消理由**：完全绑定在外部 MIDI 硬件路径上。无硬件则无法开发/验证。与"外部 MIDI 硬件依赖验证"合并为同一阻塞项。

### Phase 6-10 多事件扩展（CC/pitch bend/channel pressure）— 取消
- 将键盘绑定从仅支持 MIDI note 扩展到 CC / pitch bend / channel pressure 事件类型。
- **取消理由**：用户场景不足 10%。键盘映射 CC/bend/pressure 在旧 FreePiano 也几乎无人使用，且 `MidiChannelMapper` 层可以在插件层处理这些事件，无需键盘绑定干预。
