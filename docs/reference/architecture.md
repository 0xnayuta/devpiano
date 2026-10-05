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
   不承担屏幕捕获、视频容器（MP4）与编解码维护，专注本地合成、WAV 渲染及 MIDI Type 0/1 导入、Type 1 导出。
9. **实时音频无锁零分配铁律 (Realtime Safety)**：
   产品自有实时发声与调度回调保持零堆分配、零锁，状态通过预分配队列/原子快照交换。第三方 JUCE VST3 适配器的 SpinLock、CriticalSection 与单块 2048 消息上限单列；插件内部二进制行为另属不可控层。后台文件写出不承担实时回调的零分配承诺，详见 known-issues。
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
├── Audio/                         # 音频引擎、物理建模、节拍器与正弦合成器
├── Input/                         # 电脑键盘事件、击键间隔动态力度与 MIDI 映射
├── Midi/                          # 16 通道 MIDI 矩阵与通道路由
├── Plugin/                        # VST3 插件扫描、加载、生命周期与 Editor 托管
├── Recording/                     # 演奏录制回放、Take-relative A-B 循环 / Seek、MIDI I/O 与公共渲染管线
├── Export/                        # WAV 导出后台任务与选项构建
├── Layout/                        # Performance Preset 预设数据模型与 CRUD 编排
├── Settings/                      # 设置模型、持久化存储、窗口管理与 JIVE 设置布局
├── UI/                            # 内生声明式布局、设计 Token、弹窗、时间轴与和弦 HUD
├── Locale/                        # 多语言管理器与内嵌语言包
├── Diagnostics/                   # 结构化日志、MidiTrace 与调试输出
└── Core/                          # 纯核心数据结构与轻量强类型定义
```

---

### 3.1 应用入口与主装配层

- **`source/Main.cpp`**：
  - `juce::JUCEApplication` 派生类入口；
  - 创建主桌面窗口，管理应用启动、单实例约束与正常退出序列；
  - **纯净跨平台生命周期（Phase 34-F）**：彻底拔除 Win32 原生 `WNDPROC` Hook、`AttachThreadInput` 与 `<windows.h>` 平台特化，全平台统一基于 JUCE 9 原生 `DocumentWindow::activeWindowStatusChanged()` 配合 `callAsync` 延后分发 `restoreKeyboardFocus()`，窗口前台化使用 `toFront(true)` 与 `juce::Process::makeForegroundProcess()`，顶层跨平台纯度达到 100%；
  - **UI 树解析**：`initialiseUi()` 创建 `LayoutModel` 主窗口 ValueTree 并交由 `ViewHost` 封装的 `jive::Interpreter` 解释；`MainComponent::resized()` 更新宿主尺寸和状态文本截断，交由 FlexBox 计算全局排版。
  - **规模与职责**：`MainComponent.cpp` 保持轻量装配职责，主体仅负责顶层装配、`initialiseUi()` 的 JIVE 树构建与回调接线、UI 状态同步及音频设备生命周期管理；子面板访问器拆入 `MainComponentJiveAccessors.cpp`，具体业务流程已下沉至各 domain controller（`RecordingSessionController` / `PluginOperationController` / `SettingsWindowManager` / `AppStateBuilder`）。

---

### 3.2 Audio（音频引擎与合成器）

- **`source/Audio/AudioEngine.h/.cpp`**：
  - 拥有 `juce::MidiMessageCollector` 与实时音频输出链路；
  - 经 `InstrumentEndpoint` 解析发声实体：托管 VST3 实例就绪则驱动插件实例，否则驱动内置合成器；
  - 线程安全与音频鲁棒性：`masterGain` 采用 `std::atomic<float>`；具备 `25ms` audio warmup（静音过渡）与 `armPlaybackStartPreRoll`（消除 0s 音符冲突）。
  - 变速、Seek 与 Stop 的待提交命令仅在块入口消费；内置音色重建复用明确停 callback 的守卫，启动/再次启动不并发写活动声部或房间混响。
  - `MetronomeProcessor` 在音频块内维护完整节拍时段；预备拍起点由音频拥有者返回块内偏移，捕获排除目标下拍之前的输入。节拍混入仍位于乐器/混响之后、Master Gain / limiter 之前；消息线程只观察进度与会话转换，不触发第二次启动。
  - **预分配无锁状态交换与视觉分发**：键盘物理输入经有界 SPSC 队列（`RealtimeQueue`）注入音频块，音频回调仅更新原子音符位图；消息线程通过 `dispatchPendingDisplayEvents()` 统一刷新 `juce::MidiKeyboardState` 并驱动 UI 定时器，杜绝音频线程调用 UI/Timer 接口。
  - **异常几何防御**：超协商尺寸或通道数的音频块在块首直接静音并累计原子故障计数（`pluginBufferResizeCount`），回调内不执行堆重分配，由消息线程定时器消费告警。
- **`source/Audio/BuiltinSynthesiser.h/.cpp`**：
  - 实时和内置 WAV 共用乐器拥有者：逐通道 CC67 连续值在新 Piano 声部起音前提交；重复起音/释放按真实发音通道筛选，不把机械聚合监听当发音身份。头文件不包含重型 Piano 实现，具体发声初始化与 dispatch 放在 `.cpp`。
  - 纯音频所有、完全无锁的发声调度器，重写了底层语音分配与踏板调度，移除原生 JUCE 内部锁；支持零长度块原地消费同采样 MIDI 控制事件。
- **`source/Audio/PianoSynthVoice.h` / `source/Audio/Piano88KeyTable.h`**：
  - **自主研发、纯 C++ 全物理建模钢琴音源**（Phase 12–32 成果，v1.1.0 核心发声引擎）；
  - **7 大声学子系统**：覆盖琴槌（Hammer）、琴弦（String）、琴桥（Bridge）、音板（Soundboard）、琴体（Cabinet）、空气（Air）与空间（Room）；
  - **88 键连续参数化模型**（`Piano88KeyTable.h`）：基于 Bensa & Steinway B 实测标定，连续插值琴弦刚度 $B$、击弦比 $d/L$、阻尼常数与单/双/三弦分区；
  - **琴槌非线性打击与毛毡老化**：三层毛毡动力学压实、动态接触时间 $T_c$、击弦点几何梳状陷波、3ms 起音高频裂音（HF Crack）与琴槌毛毡微老化穿透力调节（`feltAgeingAmount`）；
  - **琴弦非线性动力学与泛音抖动**：JOS PASP 刚性失谐、泛音刚度不谐和度抖动（`inharmonicityJitter` ±4.5%）、STFT 微初相矩阵、同音三弦 Mid-Side 差分展开与非对称拍频、低音纵波先驱声（$5100\text{ m/s}$）、泛音时间滞后膨胀绽放（Harmonic Blooming）与强击软饱和；
  - **共鸣与空间辐射**：长短琴桥交界补偿（G2/G#2）、16 峰正交云杉木物理音板模态、4.2kHz 云杉木高频截止、琴桥立体声空间辐射与动态声场空间漫射；
  - **微观机械动作拟真**：CC64 全局交感共鸣弦池、延音踏板下踏/抬起机械扫掠呼啸（Whoosh）与共鸣冲击（Resonance Shock，受 `pedalNoiseLevel` 调节）、未踩踏板单键开放弦交感、制音器落木闷击与琴键释放机械摩擦、离键速度动态 ADSR 阻尼缩放；
  - **物理发声核心与实时契约**：Magic Circle 二阶递归振荡器与节拍器系数预计算达成全回调零三角函数调用，8 复音单核 CPU $\le 0.7\%$；产品自有发声路径零堆分配、零锁、零 `std::sin`。第三方 VST3 插件适配器的框架锁与扩容限制见 [`../issues/known-issues.md`](../issues/known-issues.md)。
- **`source/Audio/PerspectiveProcessor.h`（空间声像视角处理器，Phase 31-A）**：
  - 纯数学立体声声像变换器，提供演奏者视角（Player，低音在左高音在右近场宽阔）与听众视角（Audience，声像镜像反转与中距声场凝聚）；
  - 负责双声道立体声与单声道平滑下混，保证单声道求和能量守恒。
- **`source/Audio/RoomReverbEngine.h`（轻量数学算法房间混响网络，Phase 31-B）**：
  - 内置纯算法立体声混响引擎：每声道 8 组梳状滤波器与 4 级全通漫射滤波器构成 Schroeder-Moorer 网络；
  - 内置 Studio（0.6s）、Chamber（1.5s）与 Concert Hall（2.4s）三大空间预设，平滑无级调节干湿比（`reverbWet`）。
- **`source/Audio/TemperamentEngine.h`（古典微调律制引擎，Phase 30）**：
  - 提供平均律（Equal）、1/4 中庸全音律（Meantone）、毕达哥拉斯律（Pythagorean）、韦克迈斯特三律（Werckmeister III）、基恩伯格三律（Kirnberger III）与纯律（Just）六大微律音分偏移换算；
  - A4 基准基频范围为 400.0 ~ 480.0 Hz（默认 440.0 Hz）；引擎、设置滑块、预设、Take 声学快照与内置离线渲染统一消费 `TemperamentEngine::clampReferencePitch()`。
- **`source/Audio/SineSynthVoice.h`**：
  - 内置正弦波合成器，供基准对比与测试使用。
- **`source/Audio/AudioDeviceDiagnostics.h`**：
  - 音频设备类型、采样率、缓冲区大小诊断日志输出。
- **`source/Audio/InstrumentEndpoint.h`（乐器端点概念层，Phase 34-E）**：
  - 宿主固定拓扑 `Performance Input -> Instrument -> Master -> Output` 中 “Instrument” 环节的薄抽象：内置全物理建模钢琴与托管 VST3 乐器共享同一端点职责，`juce::AudioProcessor` 仅作为 VST3 适配器实现细节；
  - `resolveInstrumentEndpoint()` 以无锁读取返回当前端点（种类、宿主实例、插件描述、就绪状态与通道几何），集中取代散落在设备准备、实时渲染与离线导出路径上重复的 `hasLoadedPlugin() + getInstance() + isPrepared()` 组合判断；
  - 实时侧由 `AudioEngine` 消费；离线侧由 `renderTakeThroughInstrumentEndpoint()` 提供同构路由（实例为空即内置端点），供 WAV 导出任务统一调用。
- **`source/Audio/SyncPedalProcessor.h`（切分延音踏板处理器，Phase 34-C）**：
  - 钢琴演奏学“切分踏板（Legato / Sync Pedal）”纯算法调度器，消除按住空格键连奏时的断音空洞；
  - 在音频块（Audio Block）内部以采样精确（Sample-Accurate）偏移与严格事件顺序执行调度：$$\text{CC64}(0) \longrightarrow \text{NoteOn}(\text{newNote}) \longrightarrow \text{CC64}(127)$$
  - 实时音频路径严格无锁（Lock-free）、零堆内存分配（Zero-allocation），杜绝任何线程 Sleep 或物理时钟延迟。

- **`source/Audio/MetronomeProcessor.h` / `source/Core/MetronomeModel.h`（Phase 35-A）**：
  - `MetronomeProcessor::processAndMix()` 在实时回调中合成无外部采样依赖的强弱拍；`TimeSignature` 支持 2/4、3/4、4/4、6/8，BPM 限幅 40–280，`TapTempoCalculator` 使用最近至多 3 个间隔的均值并在超时后重置；
  - 音量、拍号、开关与预备拍小节数由 `SettingsModel` 持久化；控制器停 callback 后预分配/arm，音频线程在完整一/两小节后的下拍启动，UI 轮询或后续用户命令只接管已经开始的会话。
---

### 3.3 Input（电脑键盘输入）

- **`source/Input/KeyboardMidiMapper.h/.cpp`**：
  - 将 `juce::KeyPress` 映射为 `juce::MidiMessage`（noteOn / noteOff）；
  - 主路径采用稳定 key code（`normaliseAlphaNumericKeyCode`），避免字符输入法与 CapsLock 状态干扰；
  - **发音身份恒定与防悬挂快照（Phase 34-B）**：`HeldKeyIdentity` 在按键按下时锁定经 Group、modifier、全局移调与通道矩阵路由后的最终输出音高/通道及 NoteOff 力度；释放时直接按快照发送 NoteOff，不重新使用当前映射配置。布局替换保留仍按住的记录，直至物理释放或明确 Panic 清理；
  - 同一最终输出身份有多个物理键持有时，最后释放才关音；支持的序列化 trigger 只有 `keyDown`，`keyUp`/未知值准入拒绝，缺省字段保留原默认。
  - **Layout Group 键位分组（Phase 34-B）**：支持单预设内 4 组轻量键位分组（`KeyGroup`），反引号键（`）或 UI 按钮秒级切换；
  - **瞬态修饰键变换（Phase 34-D）**：捕获 Shift / Alt 键，按住期间由 `PerformanceModifierState` 执行力度拉满（Velocity Boost）与八度平移（+8va）纯事件流变换，松开自动回弹，基线配置 100% 零突变；
  - **双演奏看板单一事实源快照**：`createQwertySnapshot()` 生成 `QwertyViewModel`，同时包含 ANSI 网格和按最终输出音高索引的 `pianoKeys`。Group、modifier、触键曲线、矩阵和 followKey 在映射层投影；两个 UI 不自行反查原始绑定或重新计算 MIDI 映射。输入身份与最终输出身份分开保存，鼠标按配置输入执行一次矩阵变换，释放使用起音锁定的最终身份。
  - **击键间隔动态力度（Phase 35-B）**：`TypingCadenceEstimator` 在消息线程按击键时间间隔计算可选动态力度，`VelocityHumanizer` 施加有界确定性哈希扰动；静音绑定优先于 Shift、力度微扰及矩阵固定力度，非静音绑定才接受 Shift 拉满，不修改持久化键位。

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
  - 按 `PluginDescription` identifier 创建/恢复实例，显示名只展示；单文件重复探测仍返回 description 并更新 metadata，不把 `KnownPluginList::addType()` 返回 false 当成无类型。
- **`source/Plugin/PluginOperationController.h/.cpp`**：
  - 编排插件扫描、加载/卸载、Editor 窗口及启动恢复；包括增量重扫在内的实例变更先关 Editor、停 callback，再操作宿主。设备重启后发布最新只读 UI 状态。
- **`source/Plugin/PluginFlowSupport.h/.cpp`**：
  - 纯函数集合：扫描路径校验规范化、XML 缓存恢复计划推导。

---

### 3.6 Recording & Export（录制、回放与导出）

- **`source/Recording/RecordingEngine.h/.cpp`**：
  - 实时音频线程无锁采集（`recordMidiBufferBlock`），预分配事件队列（容量溢出计数防护）；
  - `sampleRate` + `lengthSamples` + `events` 组成的 `RecordingTake` 数据结构；
  - 播放状态机管理：0.5x–2.0x 变速、Seek 与 Stop 经有界原子邮箱发布，由 `AudioEngine` 的单一块入口调用 `applyPendingTransportCommands()`；纯变速保留下一未渲染事件游标，避免缩放取整重播旧 NoteOn。
  - 回放 NoteOn 以预分配 FIFO 保存最终输出身份；Off 匹配原身份并按最终输出持有数释放，映射变化与设备 prepare 不清空同代身份。录制在暂停/停止的停机边界终结已录身份/踏板，实际最终松键在其采样点闭合重叠起音。
  - **预设永久身份与快照同构**：Take 内嵌 RecordedPreset 表，事件以 Take-local 槽位引用永久 UUID/声学快照；实时与 WAV 按采样偏移执行，同采样预设先于音符。UI 通知合并更新最新视图，不依赖目录或回写音频参数。
  - `AbLoopEngine` 保存 Take-relative A/B；Seek/回跳先清音再恢复 program、bank、CC 与 pitch 状态，不重发历史 NoteOn。播放/暂停游标与固定录制 Take 域在设备 prepare 时重基准；实时长度包含最后事件，音频交付边界收尾后才通知 UI。
  - 结构提交前验证采样率、非负有序事件与最坏支持倍率的整数范围，保留现有合成时间域；文件的物理采样率准入与通用数值安全检查分开。
- **`source/Recording/RecordingSessionController.h/.cpp`**：
  - 会话控制器：统一调度录制、回放、`.devpiano` 文件保存/打开、MIDI 导入与 WAV 导出流程。
  - 编排 Take-relative Seek、A/B、暂停恢复及时间轴快照；结构清除/启动/录制停止和暂停快照复用停机守卫，异步导出退出先请求取消，实际工作完成后才销毁所有者。
- **`source/Recording/RenderPipeline.h/.cpp`**：
  - `prepareRenderTimeline()` 统一检查时间戳缩放、最终事件 `+1`、快照有效性与尾部加法；成功才返回稳定排序事件、内嵌快照表和完整长度。内置/插件 WAV 在打开输出前使用该结果，分段按采样点应用声学快照，保留同采样语义顺序及 panic 注入。
- **`source/Recording/TimelineValidation.h`**：
  - 分离文件支持采样率与通用时间域数值安全，复用有界整数转换、缩放、加法及最坏倍率长度检查；不依赖 UI 或插件适配器。
- **`source/Recording/PerformanceFile.h/.cpp`**：
  - `.devpiano` 原生演奏文件持久化（v3 JSON + JUCE 专有长度前缀二进制编码 + 内嵌快照表）；32 MiB 文件预算、1 MiB 单帧预算及完整读取/帧形状/数值准入后稳定规范化乱序时间线，保存仍通过 `juce::TemporaryFile` 事务替换。旧数字格式事件显式拒绝，不静默猜测映射。
  - 会话通过 `RecordingSession` 将 Take、元数据与原生文件绑定整体提交；新录制/MIDI 导入解除旧绑定，成功 Save As 重新绑定，generation 阻止跨 Take 的延迟信息/文件结果。
- **`source/Recording/MidiFileImporter.h/.cpp`**：
  - 标准 MIDI 文件解析与统一导入：委托 `MidiTrackMergeEngine` 将 Type 0/1 各音轨的 MIDI 播放事件合并为单一 `RecordingTake` 时间线，支持通道映射并提取全局元数据；不提供选轨模式。
  - JUCE 解析前核验全部声明轨、chunk/VLQ/事件及固定 meta 的长度与指数；仅完整结构后的尾字节宽容，必要时剔除已验证扩展块。拒绝不提交新 Take。
- **`source/Recording/MidiTrackMergeEngine.h/.cpp`**：
  - **多轨并轨合并引擎（Phase 26）**：纯静态算法引擎，负责将 `juce::MidiFile` 的所有独立音轨（Type 0/1）合并为单一连续时间线；
  - 支持智能通道映射策略（`passThrough` 保持原通道、`autoAssignIfSingleChannel` 单通道多轨自动分配 1-16 通道、`forceTrackToChannel` 强制轨索引取模分配）；
  - 精确解析曲目元数据（曲名、版权、拍号、调号、速度事件与 Tempo Map），并在同时间戳下按 Program Change → CC → Note Off → Note On 严格确定事件优先级。
- **`source/Recording/MidiFileExporter.h/.cpp`**：
  - 标准 Type 1、默认 960 PPQ，单轨演奏流和 tick 0 的 120 BPM；不合成标题/拍号/调号 meta，不输出 presetChange 或 SysEx。检查写入并关闭同目录临时流后事务替换目标。
  - 在 writer 前检查支持采样率、合法 PPQ、JUCE `int` tick 和 SMF VLQ delta 的可表示范围，拒绝不破坏原目标。
- **`source/Recording/PluginOfflineRenderer.h/.cpp`**：
  - 独立创建 VST3 实例，在 prepare 前 `setNonRealtime(true)`；渲染函数不接管实例所有权或重复 release，直接消费者负责释放，`WavExportTask` 在工作退出后统一释放一次。
- **`source/Export/WavExportTask.h/.cpp`**：
  - `startAsync(onComplete)` 非阻塞运行并显示取消中状态，Timer 只在实际工作线程退出后关闭进度窗和回调；正常应用退出异步等待，直接析构/runSync 以无限协作等待兜底，不使用有限超时强杀。内置/插件 WAV 的事务输出与原目标保留约束不变。
- **`source/Export/ExportFlowSupport.h/.cpp`**：
  - 纯函数集合：默认导出文件名推导、导出选项构建与空 Take 校验。

---

### 3.7 Layout（预设系统）

- **`source/Layout/PerformancePreset.h/.cpp`**：
  - 预设 v2 UUID 数据模型、键位/矩阵、声学、显示与逐键配置；JSON 含调号字段，但普通激活不覆写应用全局调号。另存为生成新 UUID，重命名/自动保存保持身份；旧唯一名称安全迁移，多义拒绝。
- **`source/Layout/PresetFlowSupport.h/.cpp`**：
  - 预设发现、新建（Save As New）、导入、重命名、删除与 F1-F12 快捷键切换，支持录制时注入 `presetChange` 事件并在回放时自动切调。
  - 启动恢复与选择使用统一激活提交；文件身份由入口显式提供，不从内置布局 ID 推断。重命名区分规范化同路径与独立已有目标，后者先确认；目标提交失败时回滚暂存源文件。

---

### 3.8 Settings（设置与状态模型）

- **`source/Settings/SettingsModel.h`**：
  - 强类型设置数据模型：音频设备 XML、采样率、缓冲大小、ADSR、物理建模参数、插件路径、语言、最近文件列表等。
  - 防抖快照完整复制节拍器、预备拍、击键动态/人性化字段；音频设备与插件缓存 XML 仍独立克隆。
- **`source/Settings/SettingsStore.h/.cpp`**：
  - 基于 `juce::ApplicationProperties` 与 XML 的设置存取。
  - 成功同步 `save()` 撤销此 store 较旧的待写快照；失败同步保存保留原待写任务，后续 `scheduleSave()` 仍可提交。timer 将 payload 移入局部所有权后保存，避免保存时清理自身 optional。
- **`source/Settings/SettingsWindowManager.h/.cpp`**：
  - 管理独立设置窗口的生命周期、保存、dirty 标记与关闭。
- **`source/Settings/AppStateBuilder.h/.cpp`**：
  - 将持久化设置与运行时动态状态合并为完整的 `AppState` 快照。
- **`source/Settings/jive/SettingsLayoutModel.h/.cpp`**：
  - **声明式设置面板**：JIVE `juce::ValueTree` 声明 6 个设置卡片（音频设备、调号与通道跟随网格、键盘显示与语言、声学与调律物理控制、诊断日志、保存操作）；音频设备类型、输出、通道、采样率与缓冲大小由 JIVE ComboBox/Button 构建，并根据设备可用性更新。节拍器与击键动态参数虽由 `SettingsModel` 持久化，目前设置卡片中没有相应音量/击键动态控件。

---

### 3.9 UI（声明式 UI 运行时、统一门面与原生组件）

- **`source/UI/ViewHost.h/.cpp`（统一 UI 宿主门面，Phase 28-A）**：
  - **架构界限与生命周期接管**：内部完整封装 `::jive::Interpreter` 与 `::jive::GuiItem`，析构与重载时自动调度 `safeCleanupJiveTree`，杜绝组件与样式表 UAF 风险；
  - **强类型组件访问**：提供 `host.find<T>(id)` 强类型查找、`setProperty`、`setText`、`setButtonLabel`、`setEnabled`、`setVisible`、`getSliderValue`、`setSliderValue` 与 `relayoutContainer`；样式热重载通过 `refreshStyles()` 更新当前树，不向业务公开 `GuiItem` 或可突变的根树逃逸接口；
  - **内容测量边界**：`fitToContent(width)` 在 UI 线程封装声明式内容的宽度约束、实际排版边界和高度校准；弹窗不向业务暴露 raw GuiItem 或重复维护字体测量逻辑。
  - **UI 线程断言**：在所有加载与重置入口注入 `JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED`。
- **`source/UI/jive/`（声明式 UI 核心与设计系统）**：
  - **`LayoutModel.h/.cpp`**：主窗口面板（Header, Plugin, Controls, QwertyCard, KeyboardArea, StatusBar）ValueTree 工厂，声明式嵌入 5 行 ANSI 物理键盘网格卡片（`makeQwertyCardTree()`）。
  - **`DesignTokens.h/.cpp`**：设计系统变量（颜色、字体、圆角、间距单一事实源，属于 `devpiano::jive::DesignTokens`）。
  - **`StyleCatalog.h/.cpp`**：全局样式管理器（读取编译期嵌入的 `style_sheets.json` 并动态注入树节点）。
  - **`JiveModalDialog.h/.cpp`**：通用声明式模态弹窗系统。提供输入、确认、元数据与进度模板；共用按内容定尺的根布局及操作区，原生标题栏确定后通过 `launchWindow()` 设置准确内容尺寸并居中。初始化与确认使用 `onInitHost` / `onConfirmHost`，取消使用 `onCancel`；WAV 独立进度包装层复用尺寸规则但保留协作取消生命周期。
  - **`JiveUtils.h`**：ValueTree 快速构建与安全析构辅助工具。
- **`source/UI/jive/core/`（内生 UI 渲染与排版引擎，已实施 API Freeze）**：
  - FlexBox 与 CSS Grid 基础几何排版计算引擎、BoxModel、动态样式表与动画缓动内核。已彻底剥离死代码并封存为底层资产。
- **`source/UI/`（高性能原生组件及交互）**：
  - **`source/UI/CustomKeyboard.h/.cpp`**：88 键虚拟钢琴键盘，直接消费 `QwertyViewModel::pianoKeys`。绑定标签与配置输入索引独立保存于投影，几何重建重新消费它们；实际发音通道仅更新着色，不反向改变鼠标路由。支持 Classic / Channel / Velocity / Harmony 着色、DoReMi / FixedDo / NoteName 标注，局部脏矩形剪裁，经 `KeyboardViewport` 注入声明式布局。
  - **`source/UI/QwertyComponent.h/.cpp`**：5 行 ANSI 物理键盘映射看板，消费 `QwertyViewModel` 并呈现 12-TET 色彩与和弦 HUD；卡片标题另有 `qwerty-chord-badge` 标签。
  - **`source/UI/native/AdsrCurveComponent.h/.cpp`**：实时交互式 ADSR 包络曲线组件。
  - **`source/UI/native/TimelineBar.h/.cpp`**：Take-relative 播放时间/总时长、点击/拖动 Seek、A/B 标记和清除循环。
  - **`source/UI/native/StatusBarMidiDot.h`**：MIDI 活动呼吸指示灯。
- **`source/UI/`（弹窗接入与样式）**：
  - **`source/UI/jive/JiveModalDialog.h/.cpp`**：统一 JIVE 模态对话框入口，提供单行输入、确认、元数据编辑与进度浮层模板。
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

- **`source/Diagnostics/Log.h`**：`DP_LOG_INFO/WARN/ERROR` 在 Debug/Release 均启用；仅 `DP_DEBUG_LOG` / `DP_TRACE_MIDI` 在 Release 编译移除且不求值参数。文件和 debugger 日志均属于非实时工作。
- **`source/Diagnostics/DevPianoLogger.h/.cpp`**：**生产级 Dual-Sink 统一日志基础设施（Phase 33）**：
  - **文件持久化 Sink**：写入系统标准 AppData 日志目录（Windows: `%APPDATA%\DevPiano\devpiano.log`；Linux: `~/.config/DevPiano/devpiano.log`）。活动文件与固定备份 `devpiano.old.log` **合计 512 KiB**，各分配 256 KiB；启动裁剪和每次会话内写入均守预算，超长消息按 UTF-8 码点边界截减。打开、裁剪、写入或轮转失败停用文件 sink，并通过 `hasFileError()` / `getLastError()` 及 debugger 保留原因，不继续越额写入；
  - **调试器 Sink**：始终接收完整消息（Windows: `OutputDebugString`；Linux: `stderr`），不受文件限幅或文件 sink 故障影响。日志写入在非实时线程串行化；禁止放进音频回调；
  - **故障预算边界**：旧文件被外部锁定等原因导致无法裁剪时，保留原字节并停用文件 sink；不承诺把不可写文件强制缩小。持续预算约束与故障不再追加分别验收。
  - **UI 诊断集成**：在设置面板诊断卡片动态展示当前日志物理路径，并提供“打开日志目录”（`openLogFolder`）原生交互；
  - **安全生命周期**：应用启动时注册为全局日志器（`juce::Logger::setCurrentLogger`），正常退出时安全重置并解除挂载。
- **`source/Diagnostics/MidiTrace.h/.cpp`**：MIDI 消息人类可读字符串格式化；NoteOn 与 NoteOff 直接显示原始整数力度 0..127，零力度 NoteOn 仍按 MIDI/JUCE 语义报告为 NoteOff。
- **`source/Core/`**：
  - **`AppState.h`**：全应用运行时聚合快照视图，严格保持单向依赖与纯业务基础类型（零上层业务包含，前向声明 `ChannelMatrix` 并以 `std::shared_ptr` 管理快照，就地定义 `BuiltinTone` 枚举）；
  - **`KeyMapTypes.h`**：88 键虚拟映射基础模型、`KeyGroup`（4 组轻量分组）、`HeldKeyIdentity`（发音身份快照）、`SustainPolicy`（切分踏板策略）与 `PerformanceModifierState`（瞬态事件流变换）；
  - **`QwertyModel.h`**：ANSI 5 行网格与双演奏看板只读投影；`QwertyKeyVisualState` 和 `PianoKeyVisualState` 区分矩阵输入、最终输出及配置输入音符索引，后者由 `hasBinding` 区分已配置绑定与未绑定琴键的候选输入；
  - **`MetronomeModel.h`**：拍号、录音预备拍选项与 Tap Tempo 的滚动间隔计算。
  - **`MusicTheory.h`（Phase 34-A / 35-C）**：12-TET 和声色环、文字高对比度算法，以及根据按下音高类集合识别和弦、转位与 Slash Chords 的 `detectChord()`；
  - **`MidiTypes.h`**：轻量级强类型封装。

---

## 4. 主运行链路与数据流

### 4.1 电脑键盘演奏链路

```text
[电脑键盘按键] (JUCE KeyPress / KeyListener)
    │
    ├── 0. 瞬态修饰键: PerformanceModifierState (Shift: 力度拉满 / Alt: 高八度)
    ├── 1. 键位分组: KeyGroup (当前 Group A~D 的音高 / 通道偏移)
    ├── 2. 动态力度: TypingCadenceEstimator + VelocityHumanizer (事件时变换，Shift 优先)
    ├── 3. 发音身份快照: HeldKeyIdentity (松开时用按下时的音高 / 通道)
    └── 4. 踏板策略: SustainPolicy (Space 键 Normal / Sync)
    │
    ▼
KeyboardMidiMapper (生成 MIDI 消息；供消息线程生成 QwertyViewModel)
    │
    ▼
AudioEngine::liveMidiQueue (经 MidiKeyboardState 消息线程 Listener 注入有界 SPSC 输入)
    │
    ▼
AudioEngine::getNextAudioBlock() (音频回调线程)
    ├── 收集有界 SPSC 物理输入，原子更新视觉位图 (MidiKeyboardState 移至消息线程 dispatchPendingDisplayEvents)
    ├── SyncPedalProcessor (块内按采样点排序 CC64(0) -> NoteOn -> CC64(127))
    ├── RecordingEngine::recordMidiBufferBlock() (录制时写入预分配事件队列)
    ├── RecordingEngine::renderPlaybackBlock() (回放、Seek、A-B 循环边界)
    ├── 发声处理 (resolveInstrumentEndpoint() 无锁选择):
    │    ├── [已就绪 VST3 插件] ──► AudioPluginInstance::processBlock()
    │    └── [内置乐器端点] ─────► PianoSynthVoice / SineSynthVoice
    │                                  ├── TemperamentEngine (古典微律与 A4 换算)
    │                                  ├── 7 大声学系统与机械瞬态
    │                                  └── PerspectiveProcessor (演奏者 / 听众视角)
    ├── RoomReverbEngine (后级房间混响)
    ├── MetronomeProcessor::processAndMix() (采样级强弱拍；UI 读取原子节拍序号)
    └── Master Gain + limiter ──► JUCE AudioDeviceManager ──► [音频输出]
```

---

### 4.2 插件扫描与加载链路

```text
用户触发扫描 ──► PluginOperationController::scanPlugins()
                     │
                     ▼
                 PluginHost::beginVst3ScanSession() (消息线程分片扫描)
                     ├── 异步遍历路径 ──► 更新 KnownPluginList ──► 写入 XML 缓存
                     └── 失败项记录至 lastScanFailedFiles ──► 状态栏提示 (see log)
                     │
用户选择 description 身份 ──► PluginOperationController::loadSelectedPlugin()
                     │
                     ├── 关闭 Editor、停止 callback ──► PluginHost::loadPluginByIdentifier()
                     ├── AudioPluginFormatManager::createPluginInstance()
                     ├── PluginHost::prepareToPlay(sampleRate, blockSize)
                     └── AudioEngine 切换至插件发声路径
```

---

### 4.3 演奏录制、回放与离线导出链路

```text
[录制]
用户点击 Record ──► RecordingSessionController (可选节拍器预备拍)
    └── RecordingEngine::startRecording() (预分配事件容量)
音频回调实时采样 ──► recordMidiBufferBlock() ──► 填充 RecordingTake.events
用户点击 Stop   ──► RecordingEngine::stopRecording() ──► 产出 Take 快照

[持久化]
用户点击 Save   ──► PerformanceFile::saveToFile() (JSON 序列化 + TemporaryFile 原子保存)
用户点击 Open   ──► PerformanceFile::loadFromFile() ──► 恢复 Take ──► 自动开始回放

[回放与跟练]
TimelineBar ──► RecordingSessionController (Take-relative Seek / A/B 标记)
    └── RecordingEngine + AbLoopEngine ──► AudioEngine::getNextAudioBlock()
         └── 跳转/回跳注销旧发音，恢复目标通道状态后消费目的事件；暂停/恢复保持 Take 时间轴

[离线导出 WAV]
用户点击 Export WAV ──► WavExportTask::startAsync() (现代化非阻塞异步任务启动)
    │
    ├── JiveModalDialog::makeProgressLayout (弹出声明式暗黑进度条浮层)
    ├── RenderPipeline (统一时间戳缩放、排序与 panic 注入)
    ├── renderTakeThroughInstrumentEndpoint() (同构乐器端点路由):
    │    ├── [有插件] ──► PluginOfflineRenderer (独立离线实例非实时渲染)
    │    └── [无插件] ──► 内置 PianoSynthVoice 渲染
    ├── RoomReverbEngine (后级算法立体声房间混响网络对齐，保证与实时声学一致)
    └── WAV writer 关闭后事务替换目标；失败/提交前取消只清理任务临时文件，实际 worker 退出后通知完成
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
ViewHost 封装 jive::Interpreter 解释 LayoutModel / SettingsLayoutModel 声明树
    │
    ├── StyleCatalog 应用编译期嵌入的 design_tokens.json / style_sheets.json
    ├── Native 组件工厂 (CustomKeyboard / QwertyComponent / TimelineBar / AdsrCurve)
    └── QwertyViewModel (KeyboardMidiMapper 提供按键与和弦快照)
```
