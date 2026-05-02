# devpiano 当前架构

> 用途：描述当前 JUCE 重构版的代码结构、模块职责与主要运行链路。  
> 读者：需要理解代码组织、准备修改模块边界的开发者。  
> 更新时机：源码目录、核心模块职责或主数据流发生变化时。

## 1. 项目定位

`devpiano` 是将旧版 Windows FreePiano 重构为基于 JUCE 的现代 C++ 音频应用的项目。

当前主架构原则：

- 音频 / MIDI 后端使用 JUCE 抽象。
- 插件宿主使用 JUCE `AudioPluginFormatManager` / `AudioPluginInstance`。
- 电脑键盘输入通过 JUCE `KeyListener` / `KeyPress` 转换为 MIDI。
- UI 使用 JUCE `Component` 树。
- 旧源码只用于提炼行为，不直接复制平台相关实现。

## 2. 顶层目录职责

| 路径 | 职责 |
|---|---|
| `source/` | 当前 JUCE 主代码。新增和重构后的业务代码应放在这里。 |
| `freepiano-src/` | 旧 FreePiano 源码，仅作为迁移参考，不参与主构建。 |
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

当前状态（`MainComponent.cpp` 约 711 行，已低于 1200 行目标）：

- 已不再是纯单体 UI；插件区、参数区、头部状态区和键盘区已拆入 `source/UI/`。
- 插件流程、录制/回放状态流、导出选项、只读 UI 刷新边界已通过 Phase 5-1..5-7 完成收敛：
  - `RecordingFlowSupport`（`source/Recording/RecordingFlowSupport.*`）：录制 / 回放 UI 状态转换与顶层控制策略。
  - `ExportFlowSupport`（`source/Export/ExportFlowSupport.*`）：MIDI / WAV 导出的默认文件名、空 take 判断和导出选项构建。
  - `PluginFlowSupport`（`source/Plugin/PluginFlowSupport.*`）：扫描路径规范化、缓存恢复、启动恢复计划等插件流程已收敛。
  - 只读状态刷新边界：状态函数命名已收敛为 `build*Snapshot()`、`renderReadOnlyUiState()`、`refreshReadOnlyUiStateFromCurrentSnapshot()` 和 `refreshMidiStatusFromCurrentSnapshot()`。
  - MIDI 导入流程：导入路径推导、导入结果处理、替换 take 并自动播放已收敛为 3 个 helper。
- Phase 5.8a+5.8b+5.8c 已进一步下沉布局管理、录制/回放/MIDI 导入编排和插件操作：
  - `LayoutFlowSupport`（`source/Layout/LayoutFlowSupport.*`）：布局 preset CRUD、文件选择和确认流程。
  - `RecordingSessionController`（`source/Recording/RecordingSessionController.*`）：录制/回放/MIDI 导入/导出编排与会话状态。
  - `PluginOperationController`（`source/Plugin/PluginOperationController.*`）：插件扫描、加载/卸载、editor 和启动恢复编排。
- `MainComponent` 现在主要保留 UI 组件拥有权、JUCE 生命周期入口、音频设备重建胶水、设置窗口生命周期、键盘焦点恢复和顶层装配。

**仍留在 MainComponent 的职责**：
- 设置窗口生命周期与 dirty 管理（已局部收敛，仍由 MainComponent 拥有窗口）
- 音频设备重建胶水逻辑
- 顶层 UI 事件绑定、只读 UI 刷新和窗口焦点恢复

**Phase 5.8 剩余提取目标**：
- 设置窗口管理提取到 `Settings/SettingsWindowManager`（约 100 行）。
- 状态快照构建提取到 `Core/AppStateBuilder`（约 70 行）。

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
- `source/Core/AppStateBuilder.h`

职责：

- 定义平台无关的核心数据类型。
- 承载键盘布局、MIDI 轻量强类型、应用状态快照等模型。
- 区分持久化设置基线与运行时状态叠加。

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

### Midi

- `source/Midi/MidiRouter.h`
- `source/Midi/MidiRouter.cpp`

职责：

- 枚举并打开外部 MIDI 输入。
- 将外部 MIDI 消息送入 `MidiMessageCollector`。
- 支持附加消息回调用于 UI 活动反馈。

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

- 已具备最小可用插件宿主能力。
- 后续仍可继续拆分扫描职责与实例生命周期职责。

### Settings

- `source/Settings/SettingsModel.h`
- `source/Settings/SettingsStore.h`
- `source/Settings/SettingsStore.cpp`
- `source/Settings/SettingsComponent.h`

职责：

- 保存和恢复音频设备 XML、采样率、缓冲区、ADSR、主音量等基础设置。
- 保存插件恢复信息，例如最近扫描路径和上次插件名称。
- 保存键盘布局相关状态。
- 提供设置窗口中音频设备选择组件。

### UI

典型文件：

- `source/UI/HeaderPanel.*`
- `source/UI/PluginPanel.*`
- `source/UI/ControlsPanel.*`
- `source/UI/KeyboardPanel.*`
- `source/UI/PluginEditorWindow.*`
- `source/UI/*StateBuilder.*`

职责：

- 承载独立 UI 区域。
- 将只读展示状态从 `MainComponent` 中逐步抽离。
- 管理插件 editor 独立窗口托管。

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

### 外部 MIDI 链路

```text
MIDI Input Device
  -> MidiRouter
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
  -> HeaderPanelStateBuilder / PluginPanelStateBuilder
  -> HeaderPanel / PluginPanel
```

## 5. 旧代码边界

`freepiano-src/` 只用于理解旧行为，例如默认键位、录制回放功能边界、旧插件宿主预期等。

### 允许做的事情
- 阅读旧代码以理解历史行为
- 提取业务规则、消息语义、默认布局、功能边界
- 参考旧项目模块职责拆分方式
- 对照旧项目验证新实现行为是否一致

### 禁止做的事情
- 直接复制旧的 Windows 平台实现到新代码中
- 直接延续旧的宏式、全局函数式设计
- 重新引入以 `windows.h` 为中心的依赖链作为核心架构
- 将旧的音频后端、GUI、VST 实现原样照搬

### 核心原则

> 旧代码用于"提炼逻辑"，不是用于"直接复刻实现"。
