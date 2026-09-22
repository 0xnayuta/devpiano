# devpiano 系统架构与模块设计

> 用途：描述当前 devpiano 的代码架构、模块职责、数据模型与主运行链路。
> 更新时机：源码目录分层、核心模块职责、DSP/UI 机制或主数据流发生变化时。

---

## 1. 项目定位与架构宪法

`devpiano` 是一款以 JUCE 为框架、VST3 插件为核心扩展、内置自主研发全物理建模钢琴的现代 C++ 电脑键盘钢琴应用，聚焦软件键盘演奏与 MIDI 文件处理。

### 1.1 核心架构定位

> **devpiano is a dedicated piano performance host, not a general-purpose DAW.**  
> devpiano 是一个以钢琴演奏与表现力为中心的专属乐器工作站宿主，而非全功能通用数字音频工作站（DAW），更不是简单的技术 Demo。

### 1.2 十条架构宪法 (Architecture Principles)

作为指导 devpiano 长期工程演进、功能扩展与重构的最高准则，所有代码、PR 与重构设计均须受以下十条原则约束：

1. **固定拓扑原则 (Fixed Audio Topology)**：  
   音频拓扑严格固定为 `Performance Input -> Instrument -> Master -> Output` 单向管道。绝不引入任意 Patchbay 节点连线图、多轨 DAW 时间线或复杂矩阵路由网络，拒绝通用化导致的复杂度爆炸。
2. **乐器端点边界原则 (Instrument Endpoint Boundary)**：  
   内置全物理建模钢琴（`PianoSynthVoice`）与宿主外部 VST3 乐器在领域模型上共享统一的乐器端点职责契约；JUCE 底层 `juce::AudioProcessor` 仅作为 VST3 适配器的内部实现细节，绝不反向污染宿主核心事件模型。
3. **发音身份恒定原则 (Note-off Identity Preservation)**：  
   键盘映射（Layout）、键位分组（Group）或移调状态的动态切换，绝不可破坏或篡改已发出的 NoteOn。任何 NoteOff 发送时，必须 100% 使用 NoteOn 触发时锁定的发音身份（Pitch, Channel）快照，从数学与状态机上绝对杜绝悬挂音。
4. **修饰符瞬态原则 (Event Transformation Only)**：  
   演奏修饰键（如 Shift / Alt 等瞬态 Press 控制）仅在事件流变换（Event-time Transformation）阶段即时计算 NoteOn 力度或音调，严禁突变底层持久化配置（Settings Mutation），彻底杜绝瞬态操作污染全局配置。
5. **采样精度踏板时序原则 (Sample-Accurate Pedal Timing)**：  
   延音与切分踏板（Sync Pedal）的平滑连奏语义，必须在音频块（Audio Block）内部基于**采样点偏移（Sample Offset）与严格事件次序**确定性调度，严禁使用线程 Sleep、定时间隔或物理时钟延迟。
6. **视觉视图单一事实源 (Single Source of Truth)**：  
   QWERTY 键盘映射卡片与虚拟钢琴键盘均为实时性能映射看板（Performance Map），直接单向消费 `KeyboardMidiMapper` 暴露的 ViewModel，严禁在 UI 层自行维护或二次计算 MIDI 映射。
7. **零冗余基础设施原则 (Minimal Infrastructure)**：  
   未获得明确的钢琴演奏产品需求之前，严禁引入通用图引擎、视频编解码录制栈或复杂多进程 IPC 体系。坚持“Seam-first”演进策略——先划定清晰边界，再按需平滑迁移。
8. **轻量产品职责原则 (Focused Scope)**：  
   不承担屏幕捕获、视频容器（MP4）与编解码兼容的产品维护包袱，专注于高确定性的本地音频合成、WAV 离线渲染与标准 Type 0/1 MIDI 导出。
9. **实时音频无锁零分配铁律 (Realtime Safety)**：  
   音频回调（Audio Callback）路径严格遵守零堆内存分配（Zero-allocation）、无锁（Lock-free）铁律，所有运行时状态交换一律基于预分配与原子/轻量快照。
10. **离线实时执行同构原则 (Rendering Parity)**：  
    离线渲染管线（Offline Renderer）与实时音频引擎（Realtime Engine）必须共享完全一致的乐器参数、空间混响与演奏事件执行语义。

### 1.3 核心参考项目技术栈 (Reference Stack)

在系统演进中，devpiano 遵循“吸收设计思想，不继承产品复杂度”的原则，收敛参考以下 6 大核心开源项目与 1 个产品标杆：

| 参考项目 | 核心领域 | 吸收与借鉴点 | 坚决防范与边界 |
|---|---|---|---|
| **JUCE AudioPluginHost** | VST3 宿主基准 | 官方 VST3 插件生命周期、加载/卸载时序、崩溃安全扫描持久化（Crash-safe Scanner Persistence） | 防范过于原始的手写组件排版 |
| **Kushview Element** | 乐器端点抽象 | 乐器端点与音频/MIDI 边界解耦、统一音频块处理模型 | 坚决不搬入其复杂的任意节点 Patchbay 连线图与通用总线 |
| **Surge XT** | DSP 与 Voice 架构 | UI 与 DSP 参数解耦（Parameter Snapshot）、实时线程无锁平滑、Voice 分配与 MPE 思想 | 坚决不引入庞大复杂的合成器调制矩阵 |
| **Helio Sequencer** | 音乐时间线模型 | 轻量清晰的 MIDI 音符与事件数据组织形式、优雅现代的无缝音乐交互 | 坚决不向完整 DAW 编曲工作流膨胀 |
| **VMPK** | 物理键盘输入 | 物理按键扫描码（Raw Keycode）规范化、多国键盘物理布局与映射最佳实践 | 坚决不复制其繁杂的通用 MIDI 路由器数据模型 |
| **FigBug/Piano** | 物理建模测试 | 针对物理声学模型的确定性离线回归测试、长时间压力测试框架 | 仅参考工程与测试体系，坚决不摇摆当前 Modal 物理模型路线 |
| **Modartt Pianoteq** *(产品标杆)* | 产品形态与体验 | 全物理建模钢琴声学分区（琴盖、琴槌硬度曲线、共振峰、琴体漫射）、踏板联动、轻量绿色分发 | 商业产品，仅作为产品行为与听感终极对标物 |
---

## 2. 顶层目录职责

| 路径 | 职责 |
|---|---|
| `source/` | 项目主源码目录（C++20/23 编写）。所有业务逻辑、DSP、UI 与测试均位于此。 |
| `submodules/JUCE/` | JUCE 框架 Git 子模块，禁止直接修改。 |
| `source/UI/jive/` | 声明式 UI 基础设施（依 ADR-014 自 JIVE 核心最小闭包内化自主维护）。 |
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
  - **规模与职责**：`MainComponent.cpp` 保持轻量装配职责，主体仅负责顶层装配、`initialiseUi()` 的 JIVE 树构建与回调接线、UI 状态同步及音频设备生命周期管理；子面板访问器拆入 `MainComponentJiveAccessors.cpp`，具体业务流程已下沉至各 domain controller（`RecordingSessionController` / `PluginOperationController` / `SettingsWindowManager` / `AppStateBuilder`）。

---

### 3.2 Audio（音频引擎与合成器）

- **`source/Audio/AudioEngine.h/.cpp`**：
  - 拥有 `juce::MidiMessageCollector` 与实时音频输出链路；
  - 经 `InstrumentEndpoint` 解析发声实体：托管 VST3 实例就绪则驱动插件实例，否则驱动内置合成器；
  - 线程安全与音频鲁棒性：`masterGain` 采用 `std::atomic<float>`；具备 `25ms` audio warmup（静音过渡）与 `armPlaybackStartPreRoll`（消除 0s 音符冲突）。
- **`source/Audio/PianoSynthVoice.h` / `source/Audio/Piano88KeyTable.h`**：
  - **自主研发、纯 C++ 全物理建模钢琴音源**（Phase 12–32 成果，v1.1.0 核心发声引擎）；
  - **7 大声学子系统**：覆盖琴槌（Hammer）、琴弦（String）、琴桥（Bridge）、音板（Soundboard）、琴体（Cabinet）、空气（Air）与空间（Room）；
  - **88 键连续参数化模型**（`Piano88KeyTable.h`）：基于 Bensa & Steinway B 实测标定，连续插值琴弦刚度 $B$、击弦比 $d/L$、阻尼常数与单/双/三弦分区；
  - **琴槌非线性打击与毛毡老化**：三层毛毡动力学压实、动态接触时间 $T_c$、击弦点几何梳状陷波、3ms 起音高频裂音（HF Crack）与琴槌毛毡微老化穿透力调节（`feltAgeingAmount`）；
  - **琴弦非线性动力学与泛音抖动**：JOS PASP 刚性失谐、泛音刚度不谐和度抖动（`inharmonicityJitter` ±4.5%）、STFT 微初相矩阵、同音三弦 Mid-Side 差分展开与非对称拍频、低音纵波先驱声（$5100\text{ m/s}$）、泛音时间滞后膨胀绽放（Harmonic Blooming）与强击软饱和；
  - **共鸣与空间辐射**：长短琴桥交界补偿（G2/G#2）、16 峰正交云杉木物理音板模态、4.2kHz 云杉木高频截止、琴桥立体声空间辐射与动态声场空间漫射；
  - **微观机械动作拟真**：CC64 全局交感共鸣弦池、延音踏板下踏/抬起机械扫掠呼啸（Whoosh）与共鸣冲击（Resonance Shock，受 `pedalNoiseLevel` 调节）、未踩踏板单键开放弦交感、制音器落木闷击与琴键释放机械摩擦、离键速度动态 ADSR 阻尼缩放；
  - **硬实时性能保证**：Magic Circle 二阶递归振荡器，逐采样**零三角函数调用**，8 复音单核 CPU ≤ 0.7%，实时渲染路径零堆分配、零锁。
- **`source/Audio/PerspectiveProcessor.h`（空间声像视角处理器，Phase 31-A）**：
  - 纯数学立体声声像变换器，提供演奏者视角（Player，低音在左高音在右近场宽阔）与听众视角（Audience，声像镜像反转与中距声场凝聚）；
  - 负责双声道立体声与单声道平滑下混，保证单声道求和能量守恒。
- **`source/Audio/RoomReverbEngine.h`（轻量数学算法房间混响网络，Phase 31-B）**：
  - 内置纯算法立体声混响引擎，基于互质延时梳状滤波阵列与两级全通漫射矩阵（Schroeder-Moorer 架构）；
  - 内置 Studio（0.6s）、Chamber（1.5s）与 Concert Hall（2.4s）三大空间预设，平滑无级调节干湿比（`reverbWet`）。
- **`source/Audio/TemperamentEngine.h`（古典微调律制引擎，Phase 30）**：
  - 提供平均律（Equal）、1/4 中庸全音律（Meantone）、韦克迈斯特三律（Werckmeister III）、基恩伯格三律（Kirnberger III）与纯律（Just）六大微律音分偏移换算；
  - 支持 A4 基准基频换算（400.0 ~ 480.0 Hz，默认 440.0 Hz）。
- **`source/Audio/SineSynthVoice.h`**：
  - 内置正弦波合成器，供基准对比与测试使用。
- **`source/Audio/AudioDeviceDiagnostics.h`**：
  - 音频设备类型、采样率、缓冲区大小诊断日志输出。
- **`source/Audio/InstrumentEndpoint.h`（乐器端点概念层，Phase 34-E）**：
  - 宿主固定拓扑 `Performance Input -> Instrument -> Master -> Output` 中 “Instrument” 环节的薄抽象：内置全物理建模钢琴与托管 VST3 乐器共享同一端点职责，`juce::AudioProcessor` 仅作为 VST3 适配器实现细节；
  - `resolveInstrumentEndpoint()` 以无锁读取返回当前端点（种类、宿主实例、插件描述、就绪状态与通道几何），集中取代散落在设备准备、实时渲染与离线导出路径上重复的 `hasLoadedPlugin() + getInstance() + isPrepared()` 组合判断；
  - 实时侧由 `AudioEngine` 消费；离线侧由 `renderTakeThroughInstrumentEndpoint()` 提供同构路由（实例为空即内置端点），供 WAV 导出任务统一调用。

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
  - 崩溃安全扫描持久化（Phase 34-E）：增量持久化回调 `ScanIncrementalCallback`、dead-man's pedal 崩溃点识别与黑名单推迟；
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
  - 标准 MIDI 文件解析与统一导入：委托 `MidiTrackMergeEngine` 将多轨事件合并为单时间线 `RecordingTake`，智能提取调号与曲名元数据；单轨模式下智能选取音符最丰富的主音轨。
- **`source/Recording/MidiTrackMergeEngine.h/.cpp`**：
  - **多轨并轨合并引擎（Phase 26）**：纯静态算法引擎，负责将 `juce::MidiFile` 的所有独立音轨（Type 0/1）合并为单一连续时间线；
  - 支持智能通道映射策略（`passThrough` 保持原通道、`autoAssignIfSingleChannel` 单通道多轨自动分配 1-16 通道、`forceTrackToChannel` 强制轨索引取模分配）；
  - 精确解析曲目元数据（曲名、版权、拍号、调号、速度事件与 Tempo Map），并在同时间戳下按 Program Change → CC → Note Off → Note On 严格确定事件优先级。
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
  - **声明式设置面板**：使用 JIVE `juce::ValueTree` 声明 6 个设置卡片（音频设备、调号与通道跟随网格、键盘显示与语言、声学与调律 9 项物理控制、诊断日志、保存操作）；`juce::AudioDeviceSelectorComponent` 作为 Native 项无缝注入。

---

### 3.9 UI（声明式 UI 运行时、统一门面与原生组件）

- **`source/UI/ViewHost.h/.cpp`（统一 UI 宿主门面，Phase 28-A）**：
  - **架构界限与生命周期接管**：内部完整封装 `::jive::Interpreter` 与 `::jive::GuiItem`，析构与重载时自动调度 `safeCleanupJiveTree`，杜绝组件与样式表 UAF 风险；
  - **强类型组件访问**：提供 `host.find<T>(id)` 强类型查找、`setProperty`、`setText`、`setButtonLabel`、`setEnabled`、`setVisible`、`getSliderValue`、`setSliderValue` 与 `relayoutContainer`，业务代码完全告别底层 JIVE 裸指针；
  - **UI 线程断言**：在所有加载与重置入口注入 `JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED`。
- **`source/UI/jive/`（声明式 UI 核心与设计系统）**：
  - **`LayoutModel.h/.cpp`**：主窗口面板（Header, Plugin, Controls, KeyboardArea, StatusBar）ValueTree 工厂。
  - **`DesignTokens.h/.cpp`**：设计系统变量（颜色、字体、圆角、间距单一事实源，属于 `devpiano::ui::DesignTokens`）。
  - **`StyleCatalog.h/.cpp`**：全局样式管理器（读取编译期嵌入的 `style_sheets.json` 并动态注入树节点）。
  - **`JiveModalDialog.h/.cpp`**：**通用声明式模态弹窗系统**。提供 `launchSingleInput`、`launchConfirm`、`launchMetadataEdit` 与 `makeProgressLayout` 模板。
  - **`JiveUtils.h`**：ValueTree 快速构建与安全析构辅助工具。
- **`source/UI/jive/core/`（内生 UI 渲染与排版引擎，已实施 API Freeze）**：
  - FlexBox 与 CSS Grid 基础几何排版计算引擎、BoxModel、动态样式表与动画缓动内核。已彻底剥离死代码并封存为底层资产。
- **`source/UI/native/`（高性能原生组件）**：
  - **`CustomKeyboard.h/.cpp`**：88 键虚拟钢琴键盘（自绘内核，支持 Classic / Channel / Velocity 3 种着色模式与 DoReMi / FixedDo / NoteName 3 种音符标记，焦点绝不抢占，经 `KeyboardViewport` 注入 JIVE）。
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
- **`source/Diagnostics/DevPianoLogger.h/.cpp`**：**生产级 Dual-Sink 统一日志基础设施（Phase 33）**：
  - **文件持久化 Sink**：基于 `juce::FileLogger` 实现生产级落盘，写入系统标准 AppData 日志目录（Windows: `%APPDATA%\devpiano\devpiano.log`；Linux: `~/.config/devpiano/devpiano.log`），内置 512 KB 自动滚动限额，杜绝磁盘无限制膨胀；
  - **调试器 Sink**：平台输出重定向（Windows 路由至 `OutputDebugString`，Linux 路由至 `stderr`）；
  - **UI 诊断集成**：在设置面板诊断卡片动态展示当前日志物理路径，并提供“打开日志目录”（`openLogFolder`）原生交互；
  - **安全生命周期**：应用启动时注册为全局日志器（`juce::Logger::setCurrentLogger`），正常退出时安全重置并解除挂载。
- **`source/Diagnostics/MidiTrace.h/.cpp`**：MIDI 消息人类可读字符串格式化。
- **`source/Core/`**：
  - **`AppState.h`**：全应用运行时聚合快照视图，严格保持单向依赖与纯业务基础类型（零上层业务包含，前向声明 `ChannelMatrix` 并以 `std::shared_ptr` 管理快照，就地定义 `BuiltinTone` 枚举）；
  - **`KeyMapTypes.h`**：88 键虚拟映射基础模型；
  - **`MidiTypes.h`**：轻量级强类型封装。

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
    ├── 发声处理:
    │    ├── [已加载 VST3 插件] ──► AudioPluginInstance::processBlock()
    │    └── [未加载插件] ────────► PianoSynthVoice (物理建模) / SineSynthVoice
    │                                  ├── TemperamentEngine (古典微律调律与 A4 换算)
    │                                  ├── 7 大声学系统物理振动与微观机械瞬态
    │                                  └── PerspectiveProcessor (演奏者 / 听众视角成像)
    ├── RoomReverbEngine (后级算法立体声房间混响 Chamber / Hall / Studio)
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
    ├── RoomReverbEngine (后级算法立体声房间混响网络对齐，保证与实时声学一致)
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
