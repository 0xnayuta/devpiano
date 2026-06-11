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
- Phase 6-1/6-2/6-5/6-6/6-7：演奏文件持久化、播放速度控制、MIDI 导入增强、Diagnostics、测试夹具库已完成。
- Phase 6-3（最近文件列表 + 拖拽打开）、Phase 6-4（基础 MIDI 编辑）暂缓，已完全替换为优先级更高的 Phase 6-8..6-11。

**当前阶段与状态：**
- Phase 6-8（自定义钢琴键盘）、Phase 6-9（16 通道 MIDI 矩阵）、Phase 6-10（扩展绑定系统）、Phase 6-11（GUI 设置补齐）为当前 P0 核心任务。
- Phase 5.8 MainComponent 瘦身已完成（1587→606 行），远低于 1200 行目标。
- 启动 / 音频重建早期首音音高异常已修复并通过人工验证；保留 `25ms` audio warmup。
- MIDI 导入播放首音无声已修复并完成人工回归。

**搁置项：**
- 外部 MIDI 硬件依赖验证（待硬件条件恢复）。

**测试基础设施已完成：**
- 代码格式配置（`.clang-format`，`./scripts/dev.sh format`）
- 静态分析配置（`.clang-tidy`，可选 `clang-tidy-21`）
- 自动化单元测试框架（`cmake -DBUILD_TESTS=ON` → `devpiano_tests`）
- 首批单元测试已就位（`KeyMapTypesTest`、`MidiFileImporterTest`，共 ~60 个 test case）

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

状态：进行中（6-1/6-2/6-5/6-6/6-7 已完成；6-8..6-11 为当前 P0 任务）。

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
  - 速度变更实时生效。每次启动默认 1.0x，不持久化。
  - `.devpiano` 和 `.mid` 共用同一回放链路。
- **Phase 6-6：Diagnostics 最小层** ✅
  - `source/Diagnostics/`（4 个文件）：`DebugLog.h/.cpp`、`MidiTrace.h/.cpp`。
  - 宏接口：`DP_LOG_INFO/WARN/ERROR`、`DP_DEBUG_LOG`、`DP_TRACE_MIDI`。
  - 已替换全部散落 `Logger::writeToLog` 调用。
- **Phase 6-7：MIDI / Performance 测试夹具库** ✅
  - `tests/fixtures/midi/`（7 个 MIDI fixture）。
  - `tests/fixtures/performance/simple-performance.json`。
- **Phase 6-5：MIDI 导入增强** ✅
  - 收集 CC（CC64 sustain）、pitch bend、program change 事件。
  - 导入后输出分类计数。fixture `sustain-pedal.mid` 已用于验证。

**暂缓 / 已替换：**
- Phase 6-3（最近文件列表 + 拖拽打开）：暂缓，`FileDragAndDropTarget` 实现已归入 Phase 7-3。
- Phase 6-4（基础 MIDI 编辑 delete notes）：暂缓，无近期计划。

**当前 P0 任务（Phase 6-8..6-11）：**

- **Phase 6-8：自定义钢琴键盘渲染与交互**
  - 基于 `juce::Graphics` 重写键盘 `Component`，替代 `MidiKeyboardComponent`。
  - 按键淡入淡出动画（timer 驱动的 alpha lerp）。
  - 每键自动着色模式：经典 / 通道 / 力度。
  - 每键颜色/标签自定义。
  - 音符显示模式：Do Re Mi / 固定 Do / 音符名称。
  - 点击键盘键弹出绑定编辑对话框。
  - 丢弃：DirectX 9、FreeType 字体、TGA/PNG 纹理贴图。
  - 设计原则：所有渲染使用 JUCE `Graphics` 原始绘制——不加载旧纹理、不构建独立渲染管线。

- **Phase 6-9：16 通道 MIDI 矩阵数据模型 + 设置 UI**
  - 数据结构：`std::array<PerChannelConfig, 16>`。
  - 每通道配置：velocity（0-127）、transpose（-48..48）、octave shift（-1/0/+1）、output channel（0-15）、program（0-127）、bank MSB（0-127）、sustain CC（0-127）、follow key（bool）。
  - 设置 UI：滚动列表或网格，每行一个通道。
  - 实时应用于 `MidiRouter` 路由逻辑。

- **Phase 6-10：扩展绑定系统（CC / program change / pitch bend / channel pressure）**
  - 扩展 `KeyActionType`，每个键支持绑定多种事件类型。
  - 每键 GUI 绑定编辑面板（非旧脚本编辑器）。
  - 保留 note on/off 自动 keyup 生成。
  - 设计原则：不做旧 256 个设置组、不做键组复制/粘贴/插入/清除 UI。

- **Phase 6-11：GUI 设置页面关键项补齐**
  - MIDI 输入重映射 UI（Settings → MIDI 页）。
  - 键淡入速度 slider（Settings → GUI 页）。
  - 自动颜色模式选择器。
  - 音符显示模式选择器。
  - 显示 MIDI / VSTi 乐器过滤复选框。
  - 可调整大小窗口 toggle。

### Phase 7：体验完善——离线渲染、拖放、语言、导出 UX

状态：未开始。Phase 7-1（VST3 离线渲染）为 P0，其余为 P1/P2。

**目标：** 填补剩余关键功能差距并完善用户操作体验。

- **Phase 7-1：VST3 插件离线渲染**（P0，从搁置恢复）
  - 非 UI 线程创建 `AudioPluginInstance` 副本。
  - 锁步或缓冲区副本处理，完成 VST3 音色下的 WAV 离线渲染。
  - 渲染进度反馈到 UI。
  - 原 Phase 3-2 搁置项恢复执行。

- **Phase 7-2：播放速度精确控制**（P1）
  - 替换步进按钮为 `juce::Slider`（0.5-2.0，0.01 步进）+ 数值标签。
  - 兼容现有 `RecordingEngine.playbackSpeedMultiplier` 实现。

- **Phase 7-3：拖放文件支持**（P1）
  - `FileDragAndDropTarget`：拖放 `.devpiano` / `.mid` / `.freepiano.layout` / `.vst3` 至窗口。
  - 替代 Phase 6-3 拖放部分。

- **Phase 7-4：运行时中英文语言切换**（P1）
  - JUCE `Translation` / `LookAndFeel` 机制。
  - 替换旧 `language_strdef.h` UTF-16 宏体系。
  - 设置 UI 中提供语言选择。

- **Phase 7-5：歌曲信息编辑对话框**（P1）
  - 编辑 `PerformanceFileMetadata`（标题/作者/备注）。
  - 简单的 Dialog 包装。

- **Phase 7-6：Export 进度对话框**（P1）
  - WAV/MIDI 导出时显示 `ProgressBar`。
  - 取消导出支持。

- **Phase 7-7：点击键盘键绑定编辑对话框**（P1）
  - 键盘键双击/右键弹出绑定编辑面板。
  - 结合 Phase 6-10 扩展绑定能力，提供 note/CC/program/pitch/bend 的 GUI 编辑。

- **Phase 7-8：全屏模式（F11 切换）**（P2）
  - `Desktop::setKioskModeComponent` 实现运行时切换。

- **Phase 7-9：MIDI 输入重映射 UI**（P2）
  - Settings → MIDI 页中完成通道重映射的 UI 控件。

### Phase 8：远期展望

状态：未开始，粗略规划。当前无具体时间表。

**可能包含：**
- 完整工程文件（`.devpiano-project`）：包含演奏数据 + layout preset + 插件状态 + 音频设备配置。
- 多轨数据模型：`RecordingTake` 引入 track 概念。
- Tempo map 支持：导入/编辑/保存 tempo 变化。
- Piano roll / 事件编辑器 UI。
- 量化 / snap-to-grid。
- 自动延音踏板（auto pedal）。
- 最近文件列表（最多 10 条）。


## 4. 当前近期重点

优先级从高到低：

1. **Phase 6-8..6-11（P0 核心补齐）** — 当前活跃
   - Phase 6-8：自定义钢琴键盘渲染与交互
   - Phase 6-9：16 通道 MIDI 矩阵数据模型 + 设置 UI
   - Phase 6-10：扩展绑定系统（CC / program change / pitch bend / channel pressure）
   - Phase 6-11：GUI 设置页面关键项补齐

2. **Phase 7-1：VST3 插件离线渲染**（从搁置恢复为 P0）
   - 完成 VST3 音色下的 WAV 离线渲染。
   - 渲染进度反馈。

3. **Phase 4 边界稳定**
   - 保持 Phase 4-4 边界：导入 playback take 禁止 MIDI 再导出；导入后允许 WAV 导出。
   - 继续搁置 Phase 4-6 merge-all。

4. **Phase 2 插件宿主持续稳定**
   - 低优先级持续观察退出阶段 Debug 告警。

5. **Phase 7-2..7-9（P1/P2 体验完善）** — Phase 7-1 完成后启动

6. **搁置项（待条件恢复）**
   - 外部 MIDI 硬件依赖验证（无硬件）。

## 5. 主要风险

| 风险 | 当前判断 | 应对方向 |
|---|---|---|
| 启动 / 音频重建早期首音音高异常 | 已缓解 | 已修正音频设备初始化顺序，并保留 `25ms` audio warmup；继续作为回归项观察。 |
| MIDI 导入播放首音无声 | 已修复 | 已增加 playback-start pre-roll / arming；后续触及 MIDI import / playback 启动链路时执行 Phase 4 §11.1 回归。 |
| 插件生命周期复杂 | 中 | 维护专项生命周期测试，重点覆盖 editor、卸载、重扫、退出。 |
| 自定义键盘 Component 复杂度 | 中 | Phase 6-8 是第一个替代 JUCE 内置 `MidiKeyboardComponent` 的定制渲染实现；增量迭代、先做基本功能后补充动画/着色。 |
| 16 通道 MIDI 矩阵设计变更 | 低中 | 数据模型 `std::array<PerChannelConfig, 16>` 是独立模块，不影响现有路由；UI 设计先做最小实验性版本。 |
| 外部 MIDI 硬件依赖 | 中 | 外部 MIDI 录制/回放/退出场景因无硬件暂缓；状态已记录至 [`known-issues.md`](../issues/known-issues.md)。 |
| 键盘映射边界多 | 低中 | 基础映射已全量验证；布局 preset 已补充专项回归清单。 |
| VST3 离线渲染新实现 | 中 | 需非 UI 线程创建 `AudioPluginInstance` 副本；先写 prototype 验证可执行性再集成至 UI。 |
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

## 8. 架构决策：明确不实现

以下为与 FreePiano 原项目对比后，基于"现代化、简洁化、工程化"原则明确裁定的功能。不做回填。

### 平台/OS 特定层

| 丢弃项 | 理由 |
|---|---|
| VST2 插件支持 | 已接受的工程决策；JUCE 仅 VST3，现代插件生态已迁移。 |
| 全局键盘钩子（`WH_KEYBOARD_LL`） | 侵入式、跨平台不可移植；应使用外置 MIDI 键盘覆盖后台演奏场景。 |
| Windows 键抑制 + Accessibility 键修改（StickyKeys/FilterKeys/ToggleKeys） | Windows 专属 hack，现代应用不作此类系统级修改。 |
| 窗口透明度（`SetLayeredWindowAttributes`） | 无跨平台等价物，装饰性功能。 |
| 旧 `.fpm` / `.lyt` 格式读取兼容 | FreePiano 专有格式，无公共标准；新 `.devpiano` JSON / `.mid` 为存储格式。 |

### 旧 FreePiano 特有架构

| 丢弃项 | 理由 |
|---|---|
| 256 个设置组 + 键组复制/粘贴/插入/清除 UI | LayoutPreset 系统是更干净的替代品。 |
| SM_INPUT_0..SM_INPUT_0F（64 个输入通道） | 旧多输入通道架构，在新模型下无对应。 |
| 原始键位文本编辑器（Settings → Keymap 页） | 由每键 GUI 绑定编辑面板替代（Phase 6-10 + 7-7）。 |
| 预览颜色（`preview_color` slider, %）| 语义不明，装饰性功能。 |
| 热键启用/禁用 toggle | 无全局钩子后此选项失效。 |

### 跨平台/低价值

| 丢弃项 | 理由 |
|---|---|
| MP4 视频导出 | 编码复杂度极高，窄用例。推荐 OBS 录制。 |
| 自动更新检查 | 粗糙的用户体验（HTTP GET + ShellExecute）。 |
| FreeType 字体渲染 + TGA/PNG 纹理贴图 | DirectX 9 附属物；JUCE `Graphics` 原生渲染替代。 |
| 独立 MIDI 输入半键盘视图 | 主虚拟键盘已通过 `MidiKeyboardState` 反映全部 MIDI 输入。 |
| 菜单栏（Win32 `HMENU`）迁移 | 当前组件式按钮 UI 是现代桌面应用的标准模式。 |
