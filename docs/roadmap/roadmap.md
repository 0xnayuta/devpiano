# devpiano Roadmap

> 用途：作为唯一的项目状态、阶段路线与近期重点来源。
> 更新时机：阶段目标变化、功能完成度变化、重大风险变化时。

## 1. 项目目标

将旧版 Windows FreePiano 重构为基于 JUCE 的现代 C++ 音频应用。

核心替代方向：

- 旧 WASAPI / ASIO / DSound 后端 -> JUCE `AudioDeviceManager`。
- 旧 VST 加载逻辑 -> JUCE `AudioPluginFormatManager` / `AudioPluginInstance`。
- 旧 Windows 键盘输入逻辑 -> JUCE `KeyListener` / `KeyPress` + 可配置 MIDI 映射。
- 旧 GDI / 原生控件 UI -> JUCE `Component` 树。
- 旧配置系统 -> `ApplicationProperties` / `ValueTree` / 项目内状态模型。

## 2. 当前状态摘要

**已完成阶段：**
- Phase 1-4（功能开发）：Phase 1-1-Phase 4 全部完成，核心功能已可用。
- Phase 5.1-5.7（架构收敛）：MainComponent 职责下沉，Phase 5-5..5-11 已完成。
- Phase 6-1/6-2/6-5/6-6/6-7/6-8/6-9/6-10（note editor）：演奏文件持久化、播放速度控制、MIDI 导入增强、Diagnostics、测试夹具库、自定义钢琴键盘、16 通道 MIDI 矩阵、note-only 绑定编辑器已完成。
- Phase 6-3（最近文件列表 + 拖拽打开）、Phase 6-4（基础 MIDI 编辑）暂缓，已完全替换为优先级更高的 Phase 6-8..6-11。

**当前阶段与状态：**
- Phase 6-8（自定义钢琴键盘）、Phase 6-9（16 通道 MIDI 矩阵）已完成。
- Phase 6-10（扩展绑定系统）：note-only 绑定编辑器已完成；CC / pitch bend / channel pressure 多事件类型扩展已搁置（低优先级，用户场景不足 10%）。
- Phase 6-11（GUI 设置补齐）：5 项简单控件已完成（colourMode/noteDisplay/fadeSpeed 选择器 + resizable toggle + instrumentFilter toggle）；ChannelMatrix UI 编辑器、MIDI 重映射 UI 待实现（中低优先级，进入 Phase 7 后按需追加）。
- 启动 / 音频重建早期首音音高异常已修复并通过人工验证；保留 `25ms` audio warmup。
- MIDI 导入播放首音无声已修复并完成人工回归。

**基础设施更新：**
- JUCE 子模块已升级至 `develop` 最新（3233cd13，JUCE 8.0.14）。弃用 API 已同步迁移（如 `Font` → `FontOptions`）。

**搁置项：**
- 外部 MIDI 硬件依赖验证（待硬件条件恢复）。

## 3. 阶段路线图

### Phase 1：工程骨架与最小演奏（Phase 1-1-Phase 1-2）

状态：已完成。

关键里程碑：
- JUCE GUI 程序可启动，音频设备可初始化。
- 电脑键盘可触发 note on / note off，虚拟钢琴键盘可联动显示。
- 程序可发声，来源为内置 fallback synth 或已加载插件。

### Phase 2：插件系统与键盘映射（Phase 2）

状态：已完成。

关键里程碑：
- VST3 插件扫描、加载、卸载、editor 窗口完整。
- 键盘映射系统可配置，支持默认布局和自定义 Preset。
- 插件扫描产品化增强（失败文件明细、多目录扫描、结果持久化、状态提示）。
- **Phase 2-5（已完成）：扫描 UX 增强** — 分片扫描进度反馈、扫描中状态区分、Enter 键加载、失败列表可发现性。

### Phase 3：UI 与高级功能（Phase 3）

状态：已完成。

关键里程碑：
- UI 拆分为头部、插件、参数、键盘等区域。
- 布局 Preset 系统（JSON 格式、自动发现、导入/保存/重命名/删除）。
- 录制/回放/MIDI 导出/WAV 离线渲染 MVP。

### Phase 4：MIDI 文件导入（Phase 4）

状态：已完成。

关键里程碑：
- MIDI 文件导入、自动选轨、回放、虚拟键盘可视化。
- 最近路径记忆、主窗口尺寸自适应与恢复。

功能与测试文档：[`../reference/features/midi-file-import.md`](../reference/features/midi-file-import.md)。


### Phase 5：架构收敛与 MainComponent 瘦身

状态：已完成（5.1-5.7 已完成，5.8a-5.8e 已完成，人工回归通过）。

**目标：** 将 `MainComponent.cpp` 从约 1587 行降至 1200 行以下，通过提取 helper 收敛职责。

**已完成（5.1-5.7）：**
- 录制会话状态结构化、导出流程统一、布局 CRUD 流程收敛。
- 设置窗口生命周期收敛、AppState 清理、ControlsPanel 按钮状态统一。
- MIDI 导入流程下沉。

**已完成（5.8a+5.8b+5.8c）：**
- 布局管理 handlers 提取到 `Layout/LayoutFlowSupport`（MainComponent 1587→1349 行，减少 238 行）。
- 录制/回放/MIDI 导入编排提取到 `Recording/RecordingSessionController`（MainComponent 1349→930 行，减少 419 行）。
- 插件操作提取到 `Plugin/PluginOperationController`（MainComponent 930→711 行，减少 219 行）。
- 设置窗口管理提取到 `Settings/SettingsWindowManager`（MainComponent 711→631 行，减少 80 行）。
- 状态快照构建提取到 `Core/AppStateBuilder`（MainComponent 631→606 行，减少 25 行；主要收益是边界收敛）。
- 累计减少 981 行，远低于 1200 行目标。

**后续 tech debt（低优先级，暂不执行）：**
- Phase 5.8f：AppStateBuilder 分层清理 — 已评估为低优先级 tech debt，暂不执行。详见 [`../archive/phase5-architecture-convergence.md`](../archive/phase5-architecture-convergence.md)。
- 5.8+ 后续机会（音频设备 helper、设置 flow helper、匿名 namespace 分散）— 已评估，不建议继续拆分，长期搁置到 MainComponent 再次膨胀。

5.8a-5.8e 已使 `MainComponent.cpp` 降至 606 行，已达成 1200 行以下目标。人工回归已通过，无明显回退。详细计划见 [`../archive/phase5-architecture-convergence.md`](../archive/phase5-architecture-convergence.md)。

详细完成记录见：[`../archive/phase5-architecture-convergence.md`](../archive/phase5-architecture-convergence.md)。

### Phase 6：功能补齐——钢琴键盘、MIDI 矩阵、绑定系统、GUI 设置
状态：Phase 6 主体已完成（6-1 ~ 6-11 中 5 控件完成）；**Phase 7 已启动**，Phase 7-1 为当前 P0。

**目标：** 填补与 FreePiano 差距中的 P0 核心功能——视觉化钢琴键盘、16 通道 MIDI 矩阵、扩展绑定类型、GUI 设置完整化。

**已完成：**

- **Phase 6-1：演奏文件保存/打开**（核心）✅
  - `RecordingTake` JSON 序列化（`.devpiano` 格式），包含 events、sampleRate、lengthSamples、元数据。
  - ControlsPanel Save/Open 按钮 + FileChooser 流程。
  - `RecordingSessionController` 接入 `PerformanceFile` API。

- **Phase 6-2：播放速度控制** ✅
  - ControlsPanel 增加速度显示（`1.00x`）和 `-`/`+` 按钮。
  - `RecordingEngine.playbackSpeedMultiplier` + `setPlaybackSpeedMultiplier()`。
  - `renderPlaybackBlock()` 使用 `combinedRatio` 实时缩放时间戳，不修改原始数据。

- **Phase 6-5：MIDI 导入增强** ✅
  - 扩展 `MidiFileImporter` 收集 CC（CC64 sustain 等）、pitch bend、program change 事件。
  - CC/pitch bend/program change 与 note 事件共用同一时间线。
  - 导入成功后输出 note-on/off 与 CC/pitch-bend/program-change 的分类计数。

- **Phase 6-6：Diagnostics 最小层** ✅
  - `source/Diagnostics/`（4 个文件）：`DebugLog.h/.cpp`、`MidiTrace.h/.cpp`。
  - 接口：`DP_LOG_INFO/WARN/ERROR`、`DP_DEBUG_LOG`、`DP_TRACE_MIDI`。
  - Debug-only 宏在 Release 下无输出或零副作用。

- **Phase 6-7：MIDI / Performance 测试夹具与最小回归样本库** ✅
  - `tests/fixtures/midi/`（7 个 MIDI fixture 文件）和 `tests/fixtures/performance/`。
  - 覆盖 MIDI 导入、MIDI roundtrip、错误处理、回放行为验证的统一输入基准。

- **Phase 6-8：自定义钢琴键盘** ✅
  - `CustomKeyboard` JUCE Component，支持 classic / rainbow / monochrome 着色模式。
  - 外部 MIDI / 电脑键盘 / 鼠标拖拽触发的 note 可视化，fade 动画。
  - 音符显示模式（Do Re Mi / 固定 Do / 音符名称）。
  - 双击键弹出绑定编辑对话框。

- **Phase 6-9：16 通道 MIDI 矩阵** ✅
  - `ChannelMatrix` 数据模型 + `MidiChannelMapper` 路由服务。
  - 三条路径（鼠标点击、电脑键盘、外部 MIDI）均通过矩阵路由。
  - 矩阵默认 inactive（`active=false`），完全向后兼容。

- **Phase 6-10：扩展绑定系统（note-only 绑定编辑器已完成）** ✅
  - `KeyBindingEditDialog` 每键 GUI 编辑面板（MIDI note/channel/velocity）。
  - CC / program change / pitch bend / channel pressure 多事件类型扩展 — 已搁置（低优先级，用户场景不足 10%）。

- **Phase 6-11：GUI 设置补齐（5 控件已完成，2 控件待定）**
  - `KeyboardDisplaySettings`（colourMode / noteDisplay / fadeSpeed）已持久化。
  - ✅ colourMode ComboBox、noteDisplay ComboBox、fadeSpeed Slider、resizable ToggleButton、instrumentFilter ToggleButton 已完成。
  - ChannelMatrix UI 编辑器、MIDI 重映射 UI — 中低优先级，进入 Phase 7 后按需追加。
  - Phase 6-10 多事件扩展（CC/pitch bend/channel pressure）— 低优先级，明确搁置，用户场景不足 10%。
  - Phase 6-3/6-4 维持暂缓。

### Phase 7：VST3 离线渲染与体验完善（当前阶段）

状态：Phase 7-1（VST3 离线渲染）P0 本轮启动；其余子项按优先级顺序推进。

**目标：** 打通 VST3 音色 WAV 导出闭环，补齐拖放、语言、播放速度精确控制等体验项。

**P0 — Phase 7-1：VST3 插件离线渲染**
- 非 UI 线程创建 `AudioPluginInstance` 副本实现 VST3 音色 WAV 导出。
- 以 `RecordingTake` 事件为输入，逐 block 渲染到音频 buffer。
- 输出到现有 WAV 写入流程（`RecordingExporter` / `WavAudioFormat`）。
- 关键风险：插件状态同步（preset/program）、MIDI-to-audio 精确同步、多线程安全。

**P1 — Phase 7-2：播放速度精确控制**
- 替换步进按钮为 `juce::Slider` + 数值标签。

**P1 — Phase 7-3：拖放文件支持**
- `FileDragAndDropTarget`：`.devpiano` / `.mid` / `.freepiano.layout` / `.vst3`。

**P1 — Phase 7-4：运行时中英文语言切换**
- JUCE `Translation` 机制，替换旧 `language_strdef.h` 体系。

**P1 — Phase 7-5：歌曲信息编辑对话框**
- 编辑 `PerformanceFileMetadata`。

1. **Phase 7-1：VST3 插件离线渲染（P0）** — 当前主动阶段。
   - 非 UI 线程创建 `AudioPluginInstance` 副本实现 VST3 音色 WAV 导出。
   - 以 `RecordingTake` 事件为输入，逐 block 渲染到音频 buffer。
   - 输出到现有 WAV 写入流程（`RecordingExporter` / `WavAudioFormat`）。
   - 启动前快速清扫（~5 min）：补 instrumentFilter toggle 的 show/hide 接入，完成 Phase 6-11 剩余简单项。

2. **Phase 7-2..7-7：体验完善（P1，按序推进）**
   - 播放速度精确控制、拖放文件支持、运行时语言切换、歌曲信息编辑、Export 进度对话框、全屏模式。

3. **Phase 6-11 遗留项（中低优先级，Phase 7 中按需追加）**
   - ChannelMatrix UI 编辑器、MIDI 输入重映射 UI — 遇到对应场景时切入。
   - Phase 6-10 多事件扩展（CC/pitch bend/channel pressure）— 明确搁置，不做计划。

4. **Phase 4 边界稳定**
   - 保持 Phase 4-4 边界：导入 playback take 禁止 MIDI 再导出；导入后允许 WAV 导出。
   - 继续搁置 Phase 4-6 merge-all。

5. **Phase 2 插件宿主持续稳定**
   - 低优先级持续观察退出阶段 Debug 告警。

6. **搁置项（待条件恢复）**
   - 外部 MIDI 硬件依赖验证。

## 5. 主要风险

| 风险 | 当前判断 | 应对方向 |
|---|---|---|
| 启动 / 音频重建早期首音音高异常 | 已缓解 | 已修正音频设备初始化顺序，并保留 `25ms` audio warmup；继续作为回归项观察。 |
| MIDI 导入播放首音无声 | 已修复 | 已增加 playback-start pre-roll / arming；后续触及 MIDI import / playback 启动链路时执行 Phase 4 §11.1 回归。 |
| 插件生命周期复杂 | 中 | 维护专项生命周期测试，重点覆盖 editor、卸载、重扫、退出。 |
| 外部 MIDI 硬件依赖 | 中 | 外部 MIDI 录制/回放/退出场景因无硬件暂缓；状态已记录至 [`known-issues.md`](../issues/known-issues.md)。 |
| 键盘映射边界多 | 低中 | 基础映射已全量验证；布局 preset 已补充专项回归清单。 |
| `MainComponent` 职责回流 | 低中 | 已通过 Phase 5.1-5.8e 架构收敛；布局、录制/回放/MIDI 导入、插件操作、设置窗口管理和状态快照构建已下沉到专门模块。 |
| 录制/回放实现风险 | 中 | MVP 主链路已接入；下一阶段优先收紧实时音频线程边界。 |
| 布局 Preset 实现风险 | 低 | 核心能力已完成并补充功能/测试文档。 |
| 文档状态漂移 | 中 | 本文件作为唯一 roadmap；当前任务只写入 [`current-iteration.md`](current-iteration.md)。 |

## 6. 完成标准参考

阶段性验收标准见：

- [`../reference/acceptance.md`](../reference/acceptance.md)

专项测试见：

- [`../reference/features/keyboard-mapping.md`](../reference/features/keyboard-mapping.md)
- [`../reference/features/layout-presets.md`](../reference/features/layout-presets.md)
- [`../reference/features/recording-playback.md`](../reference/features/recording-playback.md)
- [`../reference/features/midi-file-import.md`](../reference/features/midi-file-import.md)
- [`../reference/features/performance-persistence.md`](../reference/features/performance-persistence.md)
- [`../reference/features/plugin-hosting.md`](../reference/features/plugin-hosting.md)
- [`../reference/features/plugin-offline-rendering.md`](../reference/features/plugin-offline-rendering.md)
- [`../reference/features/fixture-inventory.md`](../reference/features/fixture-inventory.md)

## 7. 历史实现 Backlog

Phase 2-3 的详细实现计划与完成记录已归档至：

- [`../archive/phase2-3-implementation-backlog.md`](../archive/phase2-3-implementation-backlog.md)
