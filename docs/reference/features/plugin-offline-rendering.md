# VST3 插件离线渲染与 WAV 音频导出功能说明

> 用途：说明 devpiano 的非实时音频离线渲染管线（`RenderPipeline`）、独立离线 VST3 插件实例管理（`PluginOfflineRenderer`）、内置物理建模钢琴声学一致性导出（`WavFileExporter` 与 `RoomReverbEngine`）、后台多线程导出任务（`WavExportTask`）与 JIVE 声明式进度条交互。
> 验证边界：独立非实时实例、事务 WAV 写出、纯异步后台任务与协作取消；内置音色达成参数级快照对齐，尾音窗口固定 2.0 秒（超窗截断），第三方 VST3 经原生夹具验证，商业插件与硬件边界单列。项目状态以 [roadmap](../../roadmap/roadmap.md) 为准。
> 更新时机：离线渲染管线、插件状态快照、分层实时契约、导出进度交互或音频格式与声学参数发生变化时。

---

## 1. 概述与设计定位

当用户录制了一段演奏或导入了 MIDI 文件后，需要将演奏内容导出为高质量的 `.wav` 音频文件分享或存档。
为了实现高保真、高稳定性与非阻塞的导出体验，devpiano 建立了专用的**非实时离线渲染子系统**：

1. **独立离线 VST3 实例**：离线渲染在独立的非实时插件实例中执行，与当前前台实时发声链路（`AudioDeviceManager`）及 Editor 窗口完全解耦，导出期间用户依然可以实时试听；
2. **公共离线渲染管线（`RenderPipeline`）**：集中处理事件时间戳缩放、排序、音频块切分与尾部 panic note-off 注入，为插件渲染与内置物理建模合成器消除重复代码；
3. **后台多线程与 JIVE 进度交互**：`WavExportTask` 在后台工作线程执行密集音频渲染，消息线程以 30 fps 平滑刷新基于 `JiveModalDialog` 驱动的暗黑主题进度条；
4. **事务写出与协作取消**：内置和插件路径均写入同目录自有临时文件，检查 writer/流状态并关闭 writer 后才替换目标；渲染中及提交前接受取消，普通失败/取消只清理临时文件，保留已有目标；
5. **端点与降级边界**：单层没有可用插件或独立实例创建失败时，沿用 options 指定的内置音色降级。双层固定 Piano + 当前单 VST3；options 或任一 Take 快照需要插件层而无可用独立实例时明确失败并保护原目标，不能静默只导出钢琴。渲染中失败不换音色。

---

## 2. 核心架构与主运行流程

```text
[用户点击 Export WAV] ──► devpiano::exporting::buildWavExportOptions()
    │
    ▼
RecordingSessionController::handleExportWavClicked() ──► 弹出文件保存对话框 FileChooser
    │
    ▼
WavExportTask::startAsync() (现代化非阻塞异步工作线程启动)
    │
    ├── JiveModalDialog::makeProgressLayout() (弹出现代化 JIVE 进度浮层)
    ├── devpiano::exporting::renderTakeThroughInstrumentEndpoint() (乐器端点路由):
    │    ├── [VST3 插件端点] ──► renderTakeWithOfflinePlugin()
    │    │                         ├── 独立创建 AudioPluginInstance
    │    │                         ├── setNonRealtime(true) → prepareToPlay(sampleRate, blockSize)
    │    │                         └── 实际 worker 退出后由任务 releaseResources() 一次并安全析构
    │    │
    │    └── [内置乐器端点] ──► WavFileExporter (BuiltinSynthesiser 持有通道 CC67；PianoSynthVoice / Sine + RoomReverbEngine)
    ├── RenderPipeline (统一调度事件时间戳缩放、按 samplePosition 排序并分块送入 processBlock)
    ├── juce::WavAudioFormat 写入同目录自有临时文件 (16-bit / 24-bit / 32-bit float, 双声道 Stereo)
    └── 关闭 writer 后事务替换目标；普通失败/协作取消仅清理临时文件
```

实时与离线事件缩放采用同一采样点取整；有效长度覆盖最后事件采样点 `+1`。即使最后 NoteOff 等于 Take 长度，也在其采样点交付，再在完整事件边界收尾。

内置（`WavFileExporter`）与插件（`PluginOfflineRenderer`）两条离线渲染路径均在每个音频块（`blockSize`）内按 `presetChange` 事件采样偏移执行**分段切分（Sub-segmentation）**：
1. **采样精确参数切换**：分段起点提交声学/层快照，同采样预设先于 MIDI。音符直接消费 Take 中的最终音高/通道，不重新执行矩阵、区域或移调；NoteOff 使用 FIFO 锁定的原身份。双层固定 Piano，不被快照 `builtinTone=sine` 改为 Sine；单层保留原端点选择。
2. **非零 startSample 隔离机制**：
   - 内置合成器通过切片视图 `segmentAudioBuffer` 映射主缓冲区 `segStart` 偏移，以局部区间直接调用 `renderNextBlock(segmentAudioBuffer, midi, 0, segLen)`；
   - 离线 VST3 插件使用私有中转缓冲 `pluginBuffer`（尺寸 `segLen`，起始偏移始终为 0），由 `offlinePlugin.processBlock(pluginBuffer, midi)` 完成计算后，再按通道拓扑安全拷贝至主缓冲切片 `segmentOutputBuffer`，杜绝非零偏移污染第三方插件内部索引；
3. **固定双层与 Master 拓扑**：两层使用独立 MIDI/音频缓冲，第三方插件对 MIDI 的修改不污染 Piano，也不回灌主输入。Piano 通过有界采样 delay 与插件报告延迟对齐，层增益在公共 Reverb 前应用；混音后只执行一次公共 Reverb、Master 与软限制，再写入临时 WAV。禁用层清理原端点及 delay，启用恢复逐通道踏板但不重发旧起音；零延迟旁路继续推进 ring 历史，避免旧声音复活。

---

## 3. 核心机制与关键技术细节

### 3.1 独立离线插件实例生命周期与线程所有权（`PluginOfflineRenderer`）

- **独立性与无 UI 隔离**：基于当前已加载插件的 `PluginDescription`，通过 `formatManager.createPluginInstance()` 创建一个全新的离线实例；离线实例不创建任何 UI 窗口，避免跨线程 GUI 句柄死锁与消息循环争用；
- **缓存与状态线程所有权（State & Cache Thread Ownership）**：
   - 实时插件状态快照（`getStateInformation()`）与离线实例状态注入（`setStateInformation()`）属于**消息线程专有操作**，在后台工作线程启动前完成；音频回调与后台渲染线程严禁跨线程访问状态序列化；
   - 消息线程在停 callback 窗口创建独立实例，配置总线，再 `setNonRealtime(true)`、设 rate/block、prepare/reset 和恢复状态；随后 worker 逐块处理，实际退出后才 release/销毁一次。
- **慢插件加载与协作取消限制（Slow Plugin & Cancellation Boundaries）**：
   - 导出任务的取消（`requestCancellation()`）完全基于原子标志 `cancelRequested` 协作推进，在每个音频块循环边界与 writer 关闭后进行检查；
   - 若劣质第三方插件在单次 `processBlock()` 内部发生永久死锁或极其缓慢的计算，后台线程无法安全强杀（Win32 `TerminateThread` 会破坏互斥量与 CRT 堆，已被彻底移除），任务将协作等待该调用返回；
   - 应用退出时，`MainComponent` 保持消息循环等待导出线程正常退出，`WavExportTask` 析构与 `runSync()` 采用无限等待兜底，确保第三方实例资源释放时宿主堆完全稳定。

### 3.2 共享渲染管线（`RenderPipeline`）

`source/Recording/RenderPipeline.cpp` 通过 `prepareRenderTimeline()` 返回完整时间线或失败；内置和插件路径均在创建目录、临时文件及 writer 前完成数值准入：
- **采样率自适应换算**：有限、支持范围的录制/目标采样率按比例换算；检查长度和事件时间戳的整数转换，不把饱和值当作有效时间线；
- **稳定排序与结束点**：事件按 `samplePosition` 非递减稳定排序，同采样顺序不变；检查最后事件 `+1`、固定 2.0 秒尾部及总长度加法。块游标推进到实际 `blockEnd`，不在最后短块之后再加完整块长导致溢出；
- **尾部防挂音注入**：在渲染结尾注入全通道 CC64/66/67 Off、All Sound Off 与 All Notes Off，清理持有音和三踏板状态。
- **拒绝保护**：最终事件或尾部不可表示时返回失败，已有目标字节保留，未创建的输出目录仍不存在。此检查不等于真实插件生命周期或超长渲染资源风险已全面验证。

### 3.3 后台任务与 JIVE 进度反馈（`WavExportTask`）

`WavExportTask` 采用完全非阻塞异步任务模型：
- **纯异步任务流（`startAsync`）**：消除主线程嵌套模态循环与休眠，基于 `startAsync(onComplete)` 异步派发；`CMakeLists.txt` 仅在 `devpiano_tests` 测试目标保留 `JUCE_MODAL_LOOPS_PERMITTED=1`，主应用 `devpiano` 目标不定义该宏；
- **乐器端点路由**：`renderTakeThroughInstrumentEndpoint()` 根据 options 与整个 Take 的层需求选择内置或独立插件复合路径，先检查双层插件依赖，再创建输出。层配比与采样边界由有效快照决定。
- **无锁进度传递**：后台线程通过 `std::atomic<double> currentProgress` 和 `std::atomic<bool> cancelRequested` 与主线程通信；
- **协作取消与退出**：Cancel / ESC / 窗口关闭只设置取消请求，显示“正在取消导出”；Timer 不因取消请求或提前 finished 标志释放任务，只在实际线程退出后收尾。主应用退出保持消息循环等待导出完成，直接析构与 `runSync()` 无限等待兜底，不调用有限超时的 `stopThread()` 强杀。
- **提交边界**：后台在块循环及 writer 关闭后的最终回调检查取消；提交前取消保留原目标并清理自有临时文件。已成功提交不能被之后的 UI 消息撤销。
- **验证范围**：真实原生 mode-aware VST3、event 阻塞超过旧强停窗口的取消、回调内自销毁、资源/临时文件和实际保存对话框/应用退出已在隔离 Windows 消费者验证，复建输入见 [Phase C 实施记录](../../archive/audit-004-code-quality-fix-phases.md#phase-c-实施记录与直接验证2026-10-03)。冷路径图形驱动资源与正式任务资源分开记录，不外推所有厂商插件、断电或完整实时/离线声学闭包。
- **窗口尺寸与操作区**：`ProgressContentWrapper` 通过 `ViewHost::fitToContent()` 测量内容，使用 `JiveModalDialog::launchWindow()` 在原生标题栏模式确定后定尺和居中；进度模板共用 [底部操作区规则](declarative-ui-and-theming.md#33-内容定尺与统一底部操作区)。本次只改变布局，不将进度窗口改成通用确认弹窗，不提前关闭尚未完成的取消任务。

### 3.4 内置合成器声学参数快照对齐与导出契约（`WavExportOptions`）

`buildWavExportOptions()` 注入当前有效参数；Take 快照在其采样点替换参数。两条离线路径先提交初始 reverbSpace/reverbWet 再 `prepare()`，避免起音沿用 `RoomReverbEngine` 的内部默认 wet。实际 Piano 整段实时/WAV 对照覆盖非零 wet、同采样快照/NoteOn 与后续原身份释放；不外推第三方声音逐比特相同。参数范围如下：

1. **基础发声与音色包络**：`masterGain`、`adsr`、`builtinTone`（`piano` 或 `sine`）、`pianoBrightness`、`pianoHammerHardness`、`pianoResonance`；
2. **微调律制与基准音高**：`temperament`（6 大古典律制：`equal`、`just`、`pythagorean`、`meantone`、`werckmeister3`、`kirnberger3`）与 `referencePitchA4`（400.0 ~ 480.0 Hz，默认 440.0 Hz；内置实时与离线路径共用 `TemperamentEngine::clampReferencePitch()`）；
3. **立体声空间视角**：`soundPerspective`（演奏者 `player` 与听众 `audience` 镜像与高频吸收）；
4. **琴盖物理开合**：`lidPosition`（全开 `fullOpen`、半开 `halfStick`、闭盖 `closed` 传递函数）；
5. **空间房间混响**：离线挂载独立的 `RoomReverbEngine` 实例，根据 C++ 枚举 `reverbSpace`（`ReverbSpace::chamber` / `concertHall` / `studio`，对应持久化标识 `chamber` / `concert_hall` / `studio`，旧别名 `hall` 不再映射并回退 `chamber`）与 `reverbWet` 对双声道音频流执行立体声混响浸润；
6. **微观机械动作拟真**：`pedalNoiseLevel`（延音踏板扫掠声与共鸣冲击电平）与 `feltAgeingAmount`（琴槌毛毡微老化穿透力）；
   - **Piano 调律与被动共鸣**：`stretchTuningEnabled`（默认 true）、`duplexResonance`（`[0,1]`，默认 0.15）贯通初始参数及后续 Take 快照；仅作用 Piano，不改变 Sine 或插件 MIDI/厂商调律。
   - **固定双层**：`layers` 保存模式、两层开关及 `[0,1]` 增益；需要对齐时 Piano 使用 prepare 阶段分配的采样延迟缓冲，报告延迟非法或超出有界容量时明确失败/故障，不在实时回调重新分配。
7. **尾音窗口**：导出追加固定 2.0 秒（`wavTailSeconds = 2.0`）；超出该窗口的声音会截断，明确不把该固定长度承诺成所有声学配置、长延音或厂商插件尾音均完整。

### 3.5 分层离线渲染契约与框架限制（Layered Offline Export & Framework Boundaries）

1. **内置物理建模钢琴导出链路**：
   - 实时发声与内置离线导出共用纯数学算法模型（`BuiltinSynthesiser`、`PianoSynthVoice`、`RoomReverbEngine`）；
   - 实时回调零锁/零分配不外推到后台容器、writer 或 I/O。相同参数、分块与来源下生产 Piano 快照对照差异限于 PCM 量化；第三方插件内部行为与未测硬件分别登记，不忽略为量化。
2. **第三方 VST3 插件离线导出链路**：
   - prepare 前声明 `setNonRealtime(true)`；是否启用更高品质、过采样或其他 offline 分支由插件决定，宿主不保证每个插件均有这些行为。
   - 依然受限于 JUCE VST3 适配器框架层约束：`processBlock()` 获取 `SpinLock processMutex`，MIDI 转换使用带 `CriticalSection` 的容器且单块事件数上限为 2048 条（`enum { maxNumEvents = 2048 }`）；
   - **输出边界**：插件可选择过采样、随机失谐、线程或 offline 分支。宿主保持最终 MIDI 身份、逐通道控制器及一次公共 Master/混响，不向插件写入 Piano 逐键拉伸或物理参数，不承诺输出逐比特一致。

---

## 4. 专项手工与边界测试清单

| 用例编号 | 测试场景 | 操作步骤与验证目标 | 状态 |
|---|---|---|:---:|
| **WAV-001** | 内置物理建模钢琴离线导出 | 在未加载插件下录制演奏并导出 WAV，导出的音频具有真实的物理建模钢琴音色 | [x] 已通过 (内置引擎与自动化双向对照验证) |
| **WAV-002** | VST3 插件音色离线导出 | 加载 VST3 插件后录制演奏并导出 WAV，导出的音频为该 VST3 插件的真实音色 | [x] 已通过 (Phase C/D/E 原生 mode-aware VST3 夹具验证；未外推商业厂商插件) |
| **WAV-003** | 导出期间实时弹奏解耦 | 导出长时间 WAV 期间，在前台按键盘弹奏，实时发声不受影响，导出音频中无键盘杂音 | [x] 已通过 |
| **WAV-004** | JIVE 进度条平滑刷新 | 导出过程中观察 JIVE 进度条从 0% 平滑推进至 100%，状态文本实时显示进度百分比 | [x] 已通过 |
| **WAV-005** | 中途协作取消与文件收尾 | Cancel 后显示取消中；等待正在执行的 block 返回再释放任务，目标目录无未完成输出，已有目标字节保留 | [x] 已通过 (Windows 原生慢插件事件阻塞与句柄/临时文件验证，EVID-023) |
| **WAV-006** | 目标路径无权限容错 | 导出至只读目录或非法路径，弹窗提示错误，Logger 记录日志，程序不崩溃 | [x] 已通过 |
| **WAV-007** | 空 Take 导出拦截 | 在无录制且无导入状态下，Export WAV 按钮自动保持 Disabled | [x] 已通过 |
| **WAV-008** | 物理声学与空间参数离线一致性 | 配置特定古典律制、听众视角与房间混响后导出 WAV，导出的音频与实时参数快照对齐，无爆音，尾音收敛于固定 2.0 秒窗口（超窗截断） | [x] 已通过 (内置引擎声学参数与分段 Gain/UnaCorda 精确验证；不承诺样本逐 bit 相同或超窗尾音完整) |
| **WAV-009** | 已有文件重复导出 | 连续导出不同采样率/内容到同一目标，重新读取新 header 与可听 payload，不追加旧 WAV | [x] 已通过 (Windows 生产文件消费者验证) |
| **WAV-010** | 覆盖失败保留原文件 | 已有目标下取消、拒绝参数或锁定目标；原字节不变，任务不得删除用户目标 | [x] 已通过 (Windows 隔离消费者验证) |

### 4.1 验收证据依据与未验证范围说明

1. **实施依据**：WAV-001～WAV-010 证据覆盖自动化单元测试与 Windows 隔离真实消费者验证（见 [Phase C](../../archive/audit-004-code-quality-fix-phases.md#phase-c-实施记录与直接验证2026-10-03) EVID-020/023/024 与 [Phase D/E](../../archive/audit-004-code-quality-fix-phases.md#phase-d-实施记录与直接验证2026-10-04) EVID-030/035/040），涵盖原生 mode-aware VST3 offline flag/processBlock 验证、原生慢插件事件阻塞取消、非阻塞 JIVE 进度与覆盖保护；测试中字段转发/getter 复制不作为 DSP/逐 bit 行为保证；
2. **严禁外推的未验证范围**：
   - **商业第三方插件离线渲染**：未在商业音源（如 Pianoteq, Kontakt）执行全量离线音质与稳定性测试；
   - **极端不可中断挂起**：若第三方插件单次 `processBlock` 内部彻底陷入死循环且永不返回，协作取消机制无法在不损坏 CRT 堆的前提下强行终止线程；
   - **断电与磁盘故障**：临时文件重命名依靠操作系统原子覆盖语义，未模拟写入中途系统突然断电或硬件扇区损坏。
