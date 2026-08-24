# devpiano 系统架构与模块设计

> 用途：描述当前 devpiano 的代码架构、模块职责、数据模型与主运行链路。
> 更新时机：源码目录分层、核心模块职责、DSP/UI 机制或主数据流发生变化时。

---

## 1. 项目定位与核心原则

`devpiano` 是一款以 JUCE 为框架、VST3 插件为核心音源、内置自主研发物理建模钢琴的现代 C++ 电脑键盘钢琴应用，聚焦软件键盘演奏与 MIDI 文件处理。

### 核心架构原则

1. **音频 / MIDI 后端**：统一使用 JUCE `AudioDeviceManager` 替代旧原生 WASAPI / ASIO / DirectSound 后端。
2. **插件宿主**：使用 JUCE `AudioPluginFormatManager` / `AudioPluginInstance` 作为 VST3 插件宿主抽象，实现生命周期安全隔离。
3. **内置自主物理建模音源**：以零外部采样依赖、纯 C++ 算法驱动、覆盖 7 大声学系统的**全物理建模钢琴（`PianoSynthVoice`）**作为默认发声来源，支持与正弦波（`SineSynthVoice`）平滑切换。
4. **键盘演奏输入**：使用 JUCE `KeyListener` / `KeyPress` 捕获键盘事件，基于稳定 key code 经 `KeyboardMidiMapper` 转换为标准 MIDI 消息。
5. **声明式 UI 与样式解耦**：全应用主界面、设置面板与交互弹窗全面统一至 **JIVE 声明式 UI 框架**（`juce::ValueTree` + JSON 样式表 + Flex/Grid 布局），消灭传统手写像素排版。
6. **单一事实源与静态资产内嵌**：设计 Token（`design_tokens.json`）、JIVE 样式表（`style_sheets.json`）与中文语言包（`zh_CN.loc`）由 CMake `juce_add_binary_data` 构建期二进制静态内嵌，确保单文件绿色分发零外部文件依赖。

---

## 2. 顶层目录职责

| 路径 | 职责 |
|---|---|
| `source/` | 项目主源码目录（C++20/23 编写）。所有业务逻辑、DSP、UI 与测试均位于此。 |
| `submodules/JUCE/` | JUCE 框架 Git 子模块，禁止直接修改。 |
| `submodules/JIVE/` | JIVE 声明式 UI 框架 Git 子模块（MIT），禁止直接修改。 |
| `submodules/melatonin_inspector/` | 运行时 Component 检查器 Git 子模块（MIT），禁止直接修改。 |
| `docs/` | 项目文档体系（按 reference / guides / decisions / roadmap / issues / audit / archive 组织）。 |
| `scripts/` | WSL 开发、格式化、静态检查、单元测试与 Windows 镜像同步脚本。 |
| `tools/` | Windows 侧 PowerShell 同步与 MSVC 构建工具。 |
| `build-wsl-clang/` | WSL 本地 Debug 构建目录与 clangd / LSP 编译数据库（`compile_commands.json`）来源。 |

---

## 3. source/ 模块分层与职责

```text
source/
├── Main.cpp / MainComponent.*     # 应用生命周期与主装配层
├── Audio/                         # 音频引擎与物理建模 / 正弦合成器
├── Input/                         # 电脑键盘事件捕获与 MIDI 映射
├── Midi/                          # 16 通道 MIDI 矩阵与通道路由
├── Plugin/                        # VST3 插件扫描、加载、生命周期与 Editor 托管
├── Recording/                     # 演奏录制、回放、MIDI 导入导出与公共渲染管线
├── Export/                        # WAV 导出后台任务与选项构建
├── Layout/                        # Performance Preset 预设数据模型与 CRUD 编排
├── Settings/                      # 设置模型、持久化存储、窗口管理与 JIVE 设置布局
├── UI/                            # JIVE 声明式布局模型、设计 Token、弹窗体系与 Native 组件
├── Locale/                        # 多语言管理器与内嵌语言包
├── Diagnostics/                   # 结构化日志、MidiTrace 与调试输出
└── Core/                          # 纯核心数据结构与轻量强类型定义
```

---

### 3.1 应用入口与主装配层

- **`source/Main.cpp`**：
  - `juce::JUCEApplication` 派生类入口；
  - 创建主桌面窗口，管理应用启动、单实例约束与正常退出序列。
  - **UI 树解析**：通过 `jive::Interpreter` 解释 `LayoutModel` 声明的主窗口 ValueTree。`MainComponent::resized()` 保持声明式（更新 JIVE root 尺寸并刷新状态文本截断，由 FlexBox 自动计算全局排版）。
  - **规模与职责**：`MainComponent.cpp` 当前约 1310 行，主体为装配、`initialiseUi()` 的 JIVE 树构建与回调接线、UI 状态同步及音频设备生命周期管理；子面板访问器拆入 `MainComponentJiveAccessors.cpp`，具体业务流程已下沉至各 domain controller（`RecordingSessionController` / `PluginOperationController` / `SettingsWindowManager` / `AppStateBuilder`）。

---

### 3.2 Audio（音频引擎与合成器）

- **`source/Audio/AudioEngine.h/.cpp`**：
  - 拥有 `juce::MidiMessageCollector` 与实时音频输出链路；
  - 管理发声实体切换：优先驱动已加载 VST3 插件；无插件时驱动内置合成器；
  - 线程安全与音频鲁棒性：`masterGain` 采用 `std::atomic<float>`；具备 `25ms` audio warmup（静音过渡）与 `armPlaybackStartPreRoll`（消除 0s 音符冲突）。
- **`source/Audio/PianoSynthVoice.h` / `source/Audio/Piano88KeyTable.h`**：
  - **自主拥有、纯 C++ 全物理建模钢琴音源**（Phase 12–24 成果，v1.0.0 核心发声引擎）；
  - **7 大声学子系统**：覆盖琴槌（Hammer）、琴弦（String）、琴桥（Bridge）、音板（Soundboard）、琴体（Cabinet）、空气（Air）与空间（Room）；
  - **88 键连续参数化模型**（`Piano88KeyTable.h`）：基于 Bensa & Steinway B 实测标定，连续插值琴弦刚度 $B$、击弦比 $d/L$、阻尼常数与单/双/三弦分区；
  - **琴槌非线性打击**：三层毛毡动力学压实、动态接触时间 $T_c$、击弦点几何梳状陷波与 3ms 起音高频裂音（HF Crack）；
  - **琴弦非线性动力学**：JOS PASP 刚性失谐、STFT 微初相矩阵、同音三弦 Mid-Side 差分展开与非对称拍频、低音纵波先驱声（$5100\text{ m/s}$）、泛音时间滞后膨胀绽放（Harmonic Blooming）与强击软饱和；
  - **共鸣与空间辐射**：长短琴桥交界补偿（G2/G#2）、16 峰正交云杉木物理音板模态、4.2kHz 云杉木高频截止、琴桥立体声空间辐射与动态声场空间漫射；
  - **机械与踏板交感**：CC64 全局交感共鸣弦池、未踩踏板单键开放弦交感、琴盖开合度传递函数（Full/Half/Closed）与制音器落弦闷击（Damper Felt Fall）；
  - **硬实时性能保证**：Magic Circle 二阶递归振荡器，逐采样**零三角函数调用**，8 复音单核 CPU ≤ 0.7%，实时渲染路径零堆分配、零锁。
- **`source/Audio/SineSynthVoice.h`**：
  - 内置正弦波合成器，供基准对比与测试使用。
- **`source/Audio/AudioDeviceDiagnostics.h`**：
  - 音频设备类型、采样率、缓冲区大小诊断日志输出。

---

### 3.3 Input（电脑键盘输入）

- **`source/Input/KeyboardMidiMapper.h/.cpp`**：
  - 将 `juce::KeyPress` 映射为 `juce::MidiMessage`（noteOn / noteOff）；
  - 主路径采用稳定 key code（`normaliseAlphaNumericKeyCode`），避免字符输入法与 CapsLock 状态干扰；
  - 维护 held key 跟踪表，确保 note on/off 严格成对，焦点丢失时自动发送 panic 清理。

---

### 3.4 Midi（16 通道矩阵与路由）

- **`source/Midi/ChannelMatrix.h`**：
  - 16 通道独立矩阵配置：每通道半音移调（`transpose`）、八度偏移（`octaveShift`）、力度覆盖（`velocity`）、音色（`program`）、音色库（`bankMSB`）、延音踏板控制器（`sustainCC`）与按键跟随（`followKey`）。
- **`source/Midi/MidiChannelMapper.h/.cpp`**：
  - 高效内联变换：执行通道重定向与矩阵参数应用；
  - 支持全局调号（`keySignature`，-7..+7 半音）与 MIDI 移调开关。

---

### 3.5 Plugin（VST3 插件宿主）

- **`source/Plugin/PluginHost.h/.cpp`**：
  - 管理 JUCE `AudioPluginFormatManager`，注册 VST3 格式；
  - 维护 `juce::KnownPluginList`，支持 XML 格式导入导出与启动缓存恢复；
  - 插件异步分片扫描（Chunked Scan Session），实时进度与失败文件追踪；
  - 插件实例创建、prepare、processBlock、release 与卸载。
- **`source/Plugin/PluginOperationController.h/.cpp`**：
  - 编排插件扫描、异步加载/卸载、Editor 窗口创建以及启动恢复任务，防止状态机并发冲突。
- **`source/Plugin/PluginFlowSupport.h/.cpp`**：
  - 纯函数集合：扫描路径校验规范化、XML 缓存恢复计划推导。

---

### 3.6 Recording & Export（录制、回放与导出）

- **`source/Recording/RecordingEngine.h/.cpp`**：
  - 实时音频线程无锁采集（`recordMidiBufferBlock`），预分配事件队列（容量溢出计数防护）；
  - `sampleRate` + `lengthSamples` + `events` 组成的 `RecordingTake` 数据结构；
  - 播放状态机管理：播放速度实时倍率（0.5x–2.0x，原子变速重校准）、Back 从头回放、All-notes-off 保护。
- **`source/Recording/RecordingSessionController.h/.cpp`**：
  - 会话控制器：统一调度录制、回放、`.devpiano` 文件保存/打开、MIDI 导入与 WAV 导出流程。
- **`source/Recording/RenderPipeline.h/.cpp`**：
  - 共享离线渲染管线：统一负责事件时间戳换算、时间线缩放、事件排序与尾部 panic note-off 注入，为 `WavFileExporter` 与 `PluginOfflineRenderer` 消除重复逻辑。
- **`source/Recording/PerformanceFile.h/.cpp`**：
  - `.devpiano` 原生演奏文件持久化（v2 JSON 格式 + Base64 编码 + 元数据），通过 `juce::TemporaryFile` 实现原子写入。
- **`source/Recording/MidiFileImporter.h/.cpp`**：
  - 标准 MIDI 文件解析，智能选择 Note 密度最高的单轨，提取 Note、CC64 延音、PitchBend 与 ProgramChange。
- **`source/Recording/MidiFileExporter.h/.cpp`**：
  - 将录制 Take 导出为标准 Type 1 MIDI 文件（960 PPQ）。
- **`source/Recording/PluginOfflineRenderer.h/.cpp`**：
  - 独立创建非实时离线 VST3 实例，无 Editor 依赖渲染，异常时安全降级至 fallback synth。
- **`source/Export/WavExportTask.h/.cpp`**：
  - 后台工作线程 WAV 导出，通过 `JiveModalDialog::makeProgressLayout` 提供现代暗黑进度条浮层，支持随时取消并自动清理残留文件。
- **`source/Export/ExportFlowSupport.h/.cpp`**：
  - 纯函数集合：默认导出文件名推导、导出选项构建与空 Take 校验。

---

### 3.7 Layout（预设系统）

- **`source/Layout/PerformancePreset.h/.cpp`**：
  - Performance Preset 数据模型（键位绑定、ChannelMatrix、调号、键盘渲染设置、128 项逐键标签/颜色）与 `.devpiano.preset` JSON 序列化。
- **`source/Layout/PresetFlowSupport.h/.cpp`**：
  - 预设发现、新建（Save As New）、导入、重命名、删除与 F1-F12 快捷键切换，支持录制时注入 `presetChange` 事件并在回放时自动切调。

---

### 3.8 Settings（设置与状态模型）

- **`source/Settings/SettingsModel.h`**：
  - 强类型设置数据模型：音频设备 XML、采样率、缓冲大小、ADSR、物理建模参数、插件路径、语言、最近文件列表等。
- **`source/Settings/SettingsStore.h/.cpp`**：
  - 基于 `juce::ApplicationProperties` 与 XML 的设置存取。
- **`source/Settings/SettingsWindowManager.h/.cpp`**：
  - 管理独立设置窗口的生命周期、保存、dirty 标记与关闭。
- **`source/Settings/AppStateBuilder.h/.cpp`**：
  - 将持久化设置与运行时动态状态合并为完整的 `AppState` 快照。
- **`source/Settings/jive/SettingsLayoutModel.h/.cpp`**：
  - **声明式设置面板**：使用 JIVE `juce::ValueTree` 声明设置界面；16 通道跟随开关采用 JIVE CSS Grid（8 列 × 2 行）排版；`juce::AudioDeviceSelectorComponent` 作为 Native 项无缝注入。

---

### 3.9 UI（声明式 UI、设计系统与原生组件）

- **`source/UI/jive/`（声明式 UI 核心）**：
  - **`LayoutModel.h/.cpp`**：主窗口面板（Header, Plugin, Controls, KeyboardArea, StatusBar）ValueTree 工厂。
  - **`DesignTokens.h/.cpp`**：设计系统变量（颜色、字体、圆角、间距单一事实源，读取编译期嵌入的 `design_tokens.json`）。
  - **`StyleCatalog.h/.cpp`**：全局样式管理器（读取编译期嵌入的 `style_sheets.json` 并动态注入树节点）。
  - **`JiveModalDialog.h/.cpp`**：**通用声明式模态弹窗系统**。提供 `launchSingleInput`、`launchConfirm`、`launchMetadataEdit` 与 `makeProgressLayout` 模板，全面取代手写坐标弹窗。
  - **`JiveUtils.h`**：ValueTree 快速构建与安全析构辅助工具。
- **`source/UI/native/`（高性能原生组件）**：
  - **`CustomKeyboard.h/.cpp`**：88 键虚拟钢琴键盘（自绘内核，支持 Classic / Channel / Velocity 3 种着色模式与 DoReMi / FixedDo / NoteName 3 种音符标记，经 `KeyboardViewport` 注入 JIVE）。
  - **`AdsrCurveComponent.h/.cpp`**：实时交互式 ADSR 包络曲线组件。
  - **`StatusBarMidiDot.h`**：MIDI 活动呼吸指示灯。
- **`source/UI/`（弹窗接入与样式）**：
  - **`PresetDialogs.cpp`** / **`PerformanceMetadataDialog.cpp`**：预设与元数据编辑弹窗（全面转接 `JiveModalDialog`）。
  - **`KeyBindingEditDialog.h/.cpp`**：逐键绑定与调色板编辑弹窗（转接 `JiveModalDialog`）。
  - **`DevPianoLookAndFeel.h/.cpp`**：JUCE 原生控件的暗黑扁平主题定制。
  - **`PluginEditorWindow.h/.cpp`**：独立宿主窗口，托管 VST3 插件原生 UI。

---

### 3.10 Locale（多语言与静态资产管理）

- **`source/Locale/LocaleManager.h`**：
  - 语言管理与运行时即时切换（英文 / 简体中文），优先读取编译期嵌入的 `BinaryData::zh_CN_loc`。
- **`CMakeLists.txt` 构建期资产嵌入**：
  ```cmake
  juce_add_binary_data(devpiano_binary_data SOURCES
      source/Locale/zh_CN.loc
      source/UI/jive/design_tokens.json
      source/UI/jive/style_sheets.json
  )
  ```

---

### 3.11 Diagnostics & Core

- **`source/Diagnostics/Log.h`**：统一日志宏（`DP_LOG_INFO/WARN/ERROR`、`DP_DEBUG_LOG`、`DP_TRACE_MIDI`），在 Release 构建下零副作用。
- **`source/Diagnostics/DevPianoLogger.h/.cpp`**：`juce::Logger` 实现，Windows 路由至 `OutputDebugString`，Linux 路由至 `stderr`。
- **`source/Diagnostics/MidiTrace.h/.cpp`**：MIDI 消息人类可读字符串格式化。
- **`source/Core/`**：`AppState.h`（全应用快照）、`KeyMapTypes.h`（键位模型）、`MidiTypes.h`（轻量强类型）。

---

## 4. 主运行链路与数据流

### 4.1 电脑键盘演奏链路

```text
[电脑键盘按键] (JUCE KeyPress / KeyListener)
    │
    ▼
KeyboardMidiMapper (根据当前 KeyboardLayout / key code 转换为 MIDI 消息)
    │
    ▼
AudioEngine::MidiMessageCollector (收集并排队 MIDI 消息)
    │
    ▼
AudioEngine::getNextAudioBlock() (音频回调线程)
    ├── MidiKeyboardState (更新键盘状态，驱动虚拟键盘高亮)
    ├── RecordingEngine::recordMidiBufferBlock() (若录制中，原子写入 take)
    └── 发声处理:
         ├── [已加载 VST3 插件] ──► AudioPluginInstance::processBlock()
         └── [未加载插件] ────────► PianoSynthVoice (增强模态物理建模合成) / SineSynthVoice
    │
    ▼
Master Gain (std::atomic<float> 主音量调节)
    │
    ▼
JUCE AudioDeviceManager ──► [音频硬件输出]
```

---

### 4.2 插件扫描与加载链路

```text
用户触发扫描 ──► PluginOperationController::scanVst3Plugins()
                     │
                     ▼
                 PluginHost::beginVst3ScanSession() (消息线程分片扫描)
                     ├── 异步遍历路径 ──► 更新 KnownPluginList ──► 写入 XML 缓存
                     └── 失败项记录至 lastScanFailedFiles ──► 状态栏提示 (see log)
                     │
用户选择插件 ──► PluginOperationController::loadPluginByName()
                     │
                     ├── 关闭已有 Editor 窗口 ──► 释放旧实例 releaseResources()
                     ├── AudioPluginFormatManager::createPluginInstance()
                     ├── PluginHost::prepareToPlay(sampleRate, blockSize)
                     └── AudioEngine 切换至插件发声路径
```

---

### 4.3 演奏录制、回放与离线导出链路

```text
[录制]
用户点击 Record ──► RecordingEngine::startRecording() ──► 预分配容量
音频回调实时采样 ──► recordMidiBufferBlock() ──► 填充 RecordingTake.events
用户点击 Stop   ──► RecordingEngine::stopRecording() ──► 产出不可变 Take

[持久化]
用户点击 Save   ──► PerformanceFile::saveToFile() (JSON 序列化 + TemporaryFile 原子保存)
用户点击 Open   ──► PerformanceFile::loadFromFile() ──► 恢复 Take ──► 自动开始回放

[离线导出 WAV]
用户点击 Export WAV ──► WavExportTask (后台独立线程启动)
    │
    ├── JiveModalDialog::makeProgressLayout (弹出声明式暗黑进度条)
    ├── RenderPipeline (统一时间戳缩放、排序与 panic 注入)
    ├── 发声渲染:
    │    ├── [有插件] ──► PluginOfflineRenderer (独立离线实例非实时渲染)
    │    └── [无插件] ──► fallback synth (离线 PianoSynthVoice 模态渲染)
    └── 写入 WAV 文件 ──► 导出完成自动关闭弹窗 / 取消时清理残留文件
```

---

### 4.4 UI 声明式渲染与状态同步链路

```text
SettingsModel + Runtime Audio/Plugin State
    │
    ▼
AppStateBuilder::buildSnapshot() (组装单一事实源 AppState)
    │
    ▼
PluginPanelStateBuilder / MainComponent JIVE Accessors
    │
    ▼
jive::Interpreter 解释 LayoutModel / SettingsLayoutModel 声明树
    │
    ├── StyleCatalog 动态合并 design_tokens.json 与 style_sheets.json
    └── Native 组件注入工厂 (CustomKeyboard / AdsrCurve / AudioDeviceSelector)
```
