# devpiano 当前架构

> 用途：描述当前 JUCE 重构版的代码结构、模块职责与主要运行链路。
> 更新时机：源码目录、核心模块职责或主数据流发生变化时。

## 1. 项目定位

`devpiano` 是一款基于 JUCE 的现代 C++ 电脑键盘钢琴应用。

当前主架构原则：

- 音频 / MIDI 后端使用 JUCE 抽象。
- 插件宿主使用 JUCE `AudioPluginFormatManager` / `AudioPluginInstance`。
- 电脑键盘输入通过 JUCE `KeyListener` / `KeyPress` 转换为 MIDI。
- UI 使用 JUCE `Component` 树。
- 架构设计以 JUCE 标准 API 和抽象为核心，避免平台耦合。

## 2. 顶层目录职责

| 路径 | 职责 |
|---|---|
| `source/` | 当前 JUCE 主代码。新增和重构后的业务代码应放在这里。 |
| `JUCE/` | JUCE 子模块，禁止修改。 |
| `docs/` | 项目文档。 |
| `scripts/` | WSL / Windows 镜像同步 / 构建验证脚本。 |
| `build-wsl-clang/` | WSL 本地构建目录与 clangd 编译数据库来源。 |

## 3. source 模块分层

### 应用入口

- `source/Main.cpp`
  - JUCEApplication 启动入口。
  - 创建主窗口并承载 `MainComponent`。

### 主装配层

- `source/MainComponent.h`
- `source/MainComponent.cpp`

职责：

- 装配 UI 子组件。
- 初始化音频设备、MIDI 路由、插件宿主和设置。
- 协调键盘输入、插件操作、状态保存与只读 UI 刷新。

当前状态（`MainComponent.cpp` 体量已远低于原始单体 1587 行，且持续下降）：

- 已不再是纯单体 UI；插件区、参数区、头部状态区和键盘区已拆入 `source/UI/`。
- 插件流程、录制/回放状态流、导出选项、只读 UI 刷新边界已通过 Phase 5 完成收敛：
  - `RecordingFlowSupport`（`source/Recording/RecordingFlowSupport.*`）：录制 / 回放 UI 状态转换与顶层控制策略。
  - `ExportFlowSupport`（`source/Export/ExportFlowSupport.*`）：MIDI / WAV 导出的默认文件名、空 take 判断和导出选项构建。
  - `PluginFlowSupport`（`source/Plugin/PluginFlowSupport.*`）：扫描路径规范化、缓存恢复、启动恢复计划等插件流程已收敛。
  - 只读状态刷新边界：状态函数命名已收敛为 `build*Snapshot()`、`renderReadOnlyUiState()`、`refreshReadOnlyUiStateFromCurrentSnapshot()` 和 `refreshMidiStatusFromCurrentSnapshot()`。
  - MIDI 导入流程：导入路径推导、导入结果处理、替换 take 并自动播放已收敛为 3 个 helper。
- Phase 5 进一步下沉布局管理、录制/回放/MIDI 导入编排、插件操作、设置窗口管理和状态快照构建：
  - `PresetFlowSupport`（`source/Layout/PresetFlowSupport.*`）：Performance Preset CRUD、文件选择、commit 与录制集成。
  - `RecordingSessionController`（`source/Recording/RecordingSessionController.*`）：录制/回放/MIDI 导入/导出编排与会话状态。
  - `PluginOperationController`（`source/Plugin/PluginOperationController.*`）：插件扫描、加载/卸载、editor 和启动恢复编排。
  - `SettingsWindowManager`（`source/Settings/SettingsWindowManager.*`）：设置窗口、`SettingsComponent`、dirty/save/close 生命周期。
  - `AppStateBuilder`（`source/Settings/AppStateBuilder.*`）：持久化状态基线与 runtime audio/plugin/input snapshot 组装。
- 架构优化 Backlog（7 项 P0/P1/P2）进一步收敛：最近文件列表 UI、PluginOfflineRenderer 生命周期注释、Base64 序列化、Diagnostics 日志层迁移、WavExportOptions 独立头文件、ValueTree::Listener 与 MainComponent 瘦身。
- `MainComponent` 现在主要保留 UI 组件拥有权、JUCE 生命周期入口、音频设备重建胶水、键盘焦点恢复和顶层装配。

边界纪律与收敛原则：

- `MainComponent` 保留主 UI 组件拥有权、JUCE 生命周期入口、键盘焦点恢复和顶层装配。
- 流程 controller / manager 可以按职责拥有短生命周期对象（如 `FileChooser`、插件 editor window、设置窗口），但必须通过清晰回调边界与 `MainComponent` 交互。
- Helper / controller 不进入 audio callback；音频线程边界仍以 `AudioEngine` / `RecordingEngine` 为准。
- 不做大规模重写；每次只抽一个边界，保持可构建、可回归。
- 每个切片完成后至少跑 `./scripts/dev.sh wsl-build --configure-only` 和 `./scripts/dev.sh win-build`。
- 每个切片必须保持键盘演奏、插件加载、录制 / 回放、MIDI / WAV 导出行为不回退。

### Core

- `source/Core/KeyMapTypes.h`
- `source/Core/MidiTypes.h`
- `source/Core/AppState.h`

职责：

- 定义核心数据类型：键盘布局、MIDI 轻量强类型、应用状态快照。
- 依赖 JUCE 工具类型（`juce::String`、`juce::Array`），无渲染/GUI 依赖。
- 区分持久化设置基线与运行时状态叠加。

### Midi

- `source/Midi/MidiChannelMapper.h`
- `source/Midi/MidiChannelMapper.cpp`
- `source/Midi/ChannelMatrix.h`

职责：

- 16 通道 MIDI 矩阵配置与路由。
- 每通道半音移调、八度偏移、力度覆盖、音色/音色库选择、延音 CC。
- `applyMatrixToNoteOn/Off` / `makeProgramChange` 内联变换。

### Audio

- `source/Audio/AudioEngine.h`
- `source/Audio/AudioEngine.cpp`

职责：

- 汇总 `MidiMessageCollector` 中的 MIDI 消息。
- 优先驱动已加载插件实例的 `processBlock`。
- 未加载插件时使用内置 fallback synth 保持最小可发声能力。
- 与 JUCE 音频回调链路协作输出音频。

### Input

- `source/Input/KeyboardMidiMapper.h`
- `source/Input/KeyboardMidiMapper.cpp`

职责：

- 将电脑键盘按下 / 松开转换为 MIDI note on / note off。
- 基于 `KeyboardLayout` 进行映射。
- 当前主路径优先使用稳定 key code，减少对字符输入和输入法状态的依赖。


### Plugin

- `source/Plugin/PluginHost.h`
- `source/Plugin/PluginHost.cpp`

职责：

- 注册 JUCE 插件格式。
- 扫描 VST3 插件目录。
- 管理 `KnownPluginList`。
- 创建、prepare、release、卸载当前 `AudioPluginInstance`。
- 提供插件 editor 创建能力。

当前状态：

- 已具备完整插件宿主能力：扫描、缓存恢复、加载 / 卸载、editor 与启动恢复编排已收敛为 `PluginFlowSupport`（`source/Plugin/PluginFlowSupport.*`：扫描路径规范化、缓存恢复、启动恢复计划）与 `PluginOperationController`（`source/Plugin/PluginOperationController.*`：扫描、加载 / 卸载、editor 和启动恢复编排）。`PluginHost` 保留格式注册、扫描执行、`KnownPluginList` 与实例生命周期等核心状态。
- 扫描失败文件与最近扫描摘要（`lastScanFailedFiles` / `lastScanSummary`）由 `PluginHost` 维护，供状态条展示。

### Recording

- `source/Recording/RecordingEngine.*` — 录制引擎：实时线程 MIDI 采集（`recordEvent`）、dropped-event 计数、`hasTake` / `stopRecording` 生命周期与 take 生成。
- `source/Recording/RecordingSessionController.*` — 录制 / 回放 / MIDI 导入 / 导出编排与会话状态（Phase 5 自 MainComponent 下沉）。
- `source/Recording/RenderPipeline.*` — 共享离线渲染管线：事件时间戳缩放 / 排序、scaled take length 计算与 panic MIDI 注入，`WavFileExporter` 与 `PluginOfflineRenderer` 共用（AUDIT-REC-007）。
- `source/Recording/PerformanceFile.*` — `.devpiano` 文件持久化：JSON 保存 / 加载、原子临时文件写入、metadata 读取与公共 `parsePerformanceFileRoot` 解析（AUDIT-SEC-004、QUAL-008）。
- `source/Recording/WavFileExporter.*` — WAV 文件导出（实时回调渲染路径）。
- `source/Recording/PluginOfflineRenderer.*` — 插件离线渲染（非实时渲染路径）。
- `source/Recording/MidiFileImporter.*` — MIDI 文件导入：格式校验、track 选择与事件解析。
- `source/Recording/MidiFileExporter.*` — MIDI 导出（录制 take → `.mid`）。
- `source/Recording/RecordingFlowSupport.*` — 录制 / 回放 UI 状态转换与顶层控制策略。

### Export

- `source/Export/WavExportTask.*` — 后台线程 WAV 导出任务：进度对话框、取消、异常捕获与残留文件清理、结果日志（ERR-009/012/015）。
- `source/Export/WavExportOptions.h` — 导出选项（采样率 / 通道数 / 块大小）。
- `source/Export/ExportFlowSupport.*` — 导出默认文件名、空 take 判断与导出选项构建。

### Layout

- `source/Layout/PerformancePreset.*` — PerformancePreset 数据模型（键盘布局、ChannelMatrix、键盘显示、移调、自定义标签 / 颜色）+ JSON 序列化（格式版本 1）。
- `source/Layout/PresetFlowSupport.*` — Performance Preset CRUD、文件选择、commit 与录制集成。

### Diagnostics

- `source/Diagnostics/Log.h` — 日志宏（`DP_LOG_*`），全部路由到 `juce::Logger::writeToLog`，替代旧 DebugLog.h 宏体系。
- `source/Diagnostics/DevPianoLogger.*` — `juce::Logger` 子类：Windows `OutputDebugString` / Linux stderr 输出。
- `source/Diagnostics/MidiTrace.*` — MIDI 消息可读描述（note on/off、CC、pitch bend、program change 等）。

### Settings

- `source/Settings/SettingsModel.h`
- `source/Settings/SettingsStore.h`
- `source/Settings/SettingsStore.cpp`
- `source/Settings/SettingsComponent.h`
- `source/Settings/SettingsWindowManager.h`
- `source/Settings/SettingsWindowManager.cpp`
- `source/Settings/AppStateBuilder.h`
- `source/Settings/AppStateBuilder.cpp`

职责：

- 保存和恢复音频设备 XML、采样率、缓冲区、ADSR、主音量等基础设置。
- 保存插件恢复信息，例如最近扫描路径和上次插件名称。
- 保存键盘布局相关状态。
- 提供设置窗口中音频设备选择组件。
- `AppStateBuilder` 组装持久化基线 + runtime snapshot 为完整 `AppState`。


### UI

UI 采用 JIVE 声明式布局：`source/UI/jive/LayoutModel.cpp` 以 ValueTree 声明全部面板
（header / plugin-panel / controls-panel / keyboard-area / status-bar），
`MainComponent` 通过 `jive::Interpreter` 一次解释整棵布局树，FlexBox 负责全部定位与缩放。
样式由 `StyleCatalog` 在解释前将 `style_sheets.json` 规则合并进每个节点的 `style`
属性（jive::Object）；`MainComponentJiveAccessors.cpp`（#include 进 MainComponent.cpp）
提供面板访问器（getCustomKeyboard、setControlsValues、updatePluginPanelState 等）。

典型文件：

- `source/UI/jive/LayoutModel.*` — 面板 ValueTree 工厂
- `source/UI/jive/StyleCatalog.*` — 全局样式注入
- `source/UI/jive/DesignTokens.*` — 设计 token（配色/字号/尺寸）
- `source/UI/jive/style_sheets.json` — 全局样式规则
- `source/UI/native/` — 注入 JIVE 的原生组件（AdsrCurve、StatusBarMidiDot、KeyboardViewport）
- `source/UI/CustomKeyboard.*` — 键盘组件（经 KeyboardViewport 注入 JIVE）
- `source/UI/PluginEditorWindow.*`
- `source/UI/PluginPanelStateBuilder.*`
- `source/UI/KeyboardTypes.h` / `source/UI/PluginTypes.h` / `source/UI/RecordingTypes.h`

职责：

- 承载独立 UI 区域（组件工厂注入 + 访问器接线）。
- 将只读展示状态从 `MainComponent` 中逐步抽离。
- 管理插件 editor 独立窗口托管。
- `KeyboardTypes.h` 定义键盘渲染枚举、`KeyboardSettings` 和音符名 helper。

### Locale 与静态资产管理

静态资产（JIVE 样式表、设计 Token、中文语言包）由 CMake 的 `juce_add_binary_data(DevPianoBinaryData)` 在构建期统一打包为二进制静态库，保证程序在独立运行、脱离源码仓库（如发布至纯净机器或移动到任意目录）时永远 100% 正确加载基准样式与文本。

- `source/Locale/LocaleManager.h` — 语言激活、语言枚举、代码转换与展示名 helper；优先读取 `BinaryData::zh_CN_loc`，支持外部 `.loc` 文件覆盖。
- `source/Locale/zh_CN.loc` — 中文语言包（JUCE `LocalisedStrings` 格式，由 `DevPianoBinaryData` 编译期嵌入）。
- `source/UI/jive/design_tokens.json` — 设计 Token（配色/字号/尺寸，单一事实源，由 `DevPianoBinaryData` 编译期嵌入）。
- `source/UI/jive/style_sheets.json` — JIVE 全局样式规则（由 `DevPianoBinaryData` 编译期嵌入）。
## 4. 主运行链路

### 电脑键盘演奏链路

```text
JUCE KeyPress / KeyListener
  -> KeyboardMidiMapper
  -> juce::MidiMessage
  -> MidiMessageCollector
  -> AudioEngine
  -> PluginHost / fallback synth
  -> AudioDeviceManager
  -> Audio Output
```


### 插件链路

```text
Plugin scan path
  -> PluginHost scan
  -> KnownPluginList
  -> selected PluginDescription
  -> AudioPluginInstance
  -> prepareToPlay / processBlock / releaseResources
```

### 状态展示链路

```text
SettingsModel + runtime state
  -> AppStateBuilder / MainComponent snapshot
  -> PluginPanelStateBuilder
  -> MainComponent::updatePluginPanelState -> JIVE item state / components
```

## 5. 项目起源

devpiano 源于对旧版 Windows FreePiano 的现代化重构。Phase 1-9 中，所有有价值的 FreePiano 功能（键盘映射、插件宿主、录制回放、MIDI 导入导出、WAV 渲染、逐键个性化、调号系统、Performance Preset）已用 JUCE 架构完整重建。旧参考源码（`freepiano-src/`）已移除。
