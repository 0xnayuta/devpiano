# VST3 插件离线渲染与 WAV 音频导出功能说明

> 用途：说明 devpiano 的非实时音频离线渲染管线（`RenderPipeline`）、独立离线 VST3 插件实例管理（`PluginOfflineRenderer`）、内置物理建模钢琴声学一致性导出（`WavFileExporter` 与 `RoomReverbEngine`）、后台多线程导出任务（`WavExportTask`）与 JIVE 声明式进度条交互。
> 当前状态：已全量接入独立非实时实例、事务 WAV 写出、非阻塞后台任务与协作取消/退出；内置音色达成 1:1 声学对齐，第三方 VST3 通过 Phase C 原生测试夹具验证，商业厂商插件与断电/声卡硬件边界单列。
> 更新时机：离线渲染管线、插件状态快照、分层实时契约、导出进度交互或音频格式与声学参数发生变化时。

---

## 1. 概述与设计定位

当用户录制了一段演奏或导入了 MIDI 文件后，需要将演奏内容导出为高质量的 `.wav` 音频文件分享或存档。
为了实现高保真、高稳定性与非阻塞的导出体验，devpiano 建立了专用的**非实时离线渲染子系统**：

1. **独立离线 VST3 实例**：离线渲染在独立的非实时插件实例中执行，与当前前台实时发声链路（`AudioDeviceManager`）及 Editor 窗口完全解耦，导出期间用户依然可以实时试听；
2. **公共离线渲染管线（`RenderPipeline`）**：集中处理事件时间戳缩放、排序、音频块切分与尾部 panic note-off 注入，为插件渲染与内置物理建模合成器消除重复代码；
3. **后台多线程与 JIVE 进度交互**：`WavExportTask` 在后台工作线程执行密集音频渲染，消息线程以 30 fps 平滑刷新基于 `JiveModalDialog` 驱动的暗黑主题进度条；
4. **事务写出与协作取消**：内置和插件路径均写入同目录自有临时文件，检查 writer/流状态并关闭 writer 后才替换目标；渲染中及提交前接受取消，普通失败/取消只清理临时文件，保留已有目标；
5. **端点降级边界**：当前没有插件或独立实例创建失败时，路由至 options 指定的内置音色并记录创建失败；不承诺保留原插件音色，也不保证无效文件、参数或 I/O 故障下导出成功。插件渲染本身失败返回失败，不在中途静默换音色。

---

## 2. 核心架构与主运行流程

```text
[用户点击 Export WAV] ──► ExportFlowSupport::buildWavExportOptions()
    │
    ▼
RecordingSessionController::handleExportWavClicked() ──► 弹出文件保存对话框 FileChooser
    │
    ▼
WavExportTask::startAsync() (现代化非阻塞异步工作线程启动)
    │
    ├── JiveModalDialog::makeProgressLayout() (弹出现代化 JIVE 进度浮层)
    ├── InstrumentEndpoint::renderTakeThroughInstrumentEndpoint() (同构乐器端点路由):
    │    ├── [VST3 插件端点] ──► PluginOfflineRenderer::renderTakeWithOfflinePlugin()
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
1. **帧精确预设切换（Frame-Accurate Preset Switching）**：在分段起点 `segAbsStart`，立即应用快照中的 `masterGain`、`roomReverb` 空间与干湿比、全局移调与通道 followKey 掩码，并向所有 16 个通道发送 CC67 柔音控制器；同一采样点处的预设变更优先于 MIDI 音符执行；
2. **非零 startSample 隔离机制**：
   - 内置合成器通过切片视图 `segmentAudioBuffer` 映射主缓冲区 `segStart` 偏移，以局部区间直接调用 `renderNextBlock(segmentAudioBuffer, midi, 0, segLen)`；
   - 离线 VST3 插件使用私有中转缓冲 `pluginBuffer`（尺寸 `segLen`，起始偏移始终为 0），由 `offlinePlugin.processBlock(pluginBuffer, midi)` 完成计算后，再按通道拓扑安全拷贝至主缓冲切片 `segmentOutputBuffer`，杜绝非零偏移污染第三方插件内部索引；
3. **Master 端处理拓扑**：在每个子分段内，立体声混响（`roomReverb.processStereo`）与增益（`applyGain(currentMasterGain)`）首先作用于分段音频；整块的所有分段计算完毕后，统一在 Master 端通过 `applyMasterSoftLimiter(outputBuffer, numSamples)` 执行防爆音软限制，最后写入临时 WAV 文件。
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
- **尾部防挂音注入**：在渲染结尾自动注入全通道 `allNotesOff` 与 `sustainOff`，消除由于 MIDI 数据不完整可能导致的尾部悬挂音。
- **拒绝保护**：最终事件或尾部不可表示时返回失败，已有目标字节保留，未创建的输出目录仍不存在。此检查不等于真实插件生命周期或超长渲染资源风险已全面验证。

### 3.3 后台任务与 JIVE 进度反馈（`WavExportTask`）

在 Phase 15-D 与 Phase 34-F 中，`WavExportTask` 实现了现代化重构与完全非阻塞异步化：
- **纯异步任务流（`startAsync`）**：在 Phase 34-F 中，彻底消除了历史遗留的主线程嵌套模态循环 `runDispatchLoopUntil(10)` 与 `Thread::sleep(10)`，改为基于 `startAsync(onComplete)` 的非阻塞异步任务模型；`CMakeLists.txt` 仅在 `devpiano_tests` 测试目标保留 `JUCE_MODAL_LOOPS_PERMITTED=1`，主应用 `devpiano` 目标不定义该宏；
- **同构乐器端点（`InstrumentEndpoint`）**：在 Phase 34-E 中引入 `renderTakeThroughInstrumentEndpoint()`，端点路由与实时发声完全一致，消除离线分支手写判断；
- **无锁进度传递**：后台线程通过 `std::atomic<double> currentProgress` 和 `std::atomic<bool> cancelRequested` 与主线程通信；
- **协作取消与退出**：Cancel / ESC / 窗口关闭只设置取消请求，显示“正在取消导出”；Timer 不因取消请求或提前 finished 标志释放任务，只在实际线程退出后收尾。主应用退出保持消息循环等待导出完成，直接析构与 `runSync()` 无限等待兜底，不调用有限超时的 `stopThread()` 强杀。
- **提交边界**：后台在块循环及 writer 关闭后的最终回调检查取消；提交前取消保留原目标并清理自有临时文件。已成功提交不能被之后的 UI 消息撤销。
- **验证范围**：真实原生 mode-aware VST3、event 阻塞超过旧强停窗口的取消、回调内自销毁、资源/临时文件和实际保存对话框/应用退出已在隔离 Windows 消费者验证，复建输入见 [Phase C 实施记录](../../roadmap/current-iteration.md#phase-c-实施记录与直接验证2026-10-03)。冷路径图形驱动资源与正式任务资源分开记录，不外推所有厂商插件、断电或完整实时/离线声学闭包。

### 3.4 内置合成器 1:1 声学一致性对齐（`WavExportOptions`）

`ExportFlowSupport::buildWavExportOptions()` 将当前声学参数快照注入 `WavExportOptions`，使内置钢琴的离线与实时处理使用相同参数和房间混响网络；插件的独立离线实例可能具有自身非实时行为，**不承诺输出样本逐比特一致**：

1. **基础发声与音色包络**：`masterGain`、`adsr`、`builtinTone`（`piano` 或 `sine`）、`pianoBrightness`、`pianoHammerHardness`、`pianoResonance`；
2. **微调律制与基准音高（Phase 30）**：`temperament`（6 大古典律制：`equal`、`just`、`pythagorean`、`meantone`、`werckmeister3`、`kirnberger3`）与 `referencePitchA4`（400.0 ~ 480.0 Hz，默认 440.0 Hz；内置实时与离线路径共用 `TemperamentEngine::clampReferencePitch()`）；
3. **立体声空间视角（Phase 31-A）**：`soundPerspective`（演奏者 `player` 与听众 `audience` 镜像与高频吸收）；
4. **琴盖物理开合（Phase 31-C）**：`lidPosition`（全开 `fullOpen`、半开 `halfStick`、闭盖 `closed` 传递函数）；
5. **空间房间混响（Phase 31-B）**：离线挂载独立的 `RoomReverbEngine` 实例，根据 `reverbSpace`（`chamber` / `concert_hall` / `studio`）与 `reverbWet` 对双声道音频流执行立体声混响浸润；
6. **微观机械动作拟真（Phase 32）**：`pedalNoiseLevel`（延音踏板扫掠声与共鸣冲击电平）与 `feltAgeingAmount`（琴槌毛毡微老化穿透力）；
7. **尾音窗口**：导出追加固定 2.0 秒；超出该窗口的声音会截断，不把该固定长度承诺成所有声学配置或厂商插件尾音均完整。

### 3.5 分层离线渲染契约与框架限制（Layered Offline Export & Framework Boundaries）

1. **内置物理建模钢琴导出链路**：
   - 实时发声与内置离线导出共用纯数学算法模型（`BuiltinSynthesiser`、`PianoSynthVoice`、`RoomReverbEngine`）；
   - 实时回调的零锁/零分配契约不外推到文件 writer、后台离线容器或 I/O；两路径共享采样级声学语义。Phase D/E 相同 Sine/ADSR 输入的实测差处于 16-bit WAV 量化范围，不把单次最大差固化为所有音源的通用数值门槛。
2. **第三方 VST3 插件离线导出链路**：
   - prepare 前声明 `setNonRealtime(true)`；是否启用更高品质、过采样或其他 offline 分支由插件决定，宿主不保证每个插件均有这些行为。
   - 依然受限于 JUCE VST3 适配器框架层约束：`processBlock()` 获取 `SpinLock processMutex`，MIDI 转换使用带 `CriticalSection` 的容器且单块事件数上限为 2048 条（`enum { maxNumEvents = 2048 }`）；
   - **不承诺逐样本比特相同**：由于第三方插件自身的内部过采样、内部线程调度、算法随机微失谐或非实时模式专用滤波，宿主仅保证输入 MIDI 与声学参数快照一致传递，不保证输出样本与实时监听逐比特相同。

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
| **WAV-008** | 物理声学与空间参数离线一致性 | 配置特定古典律制、听众视角与房间混响后导出 WAV，导出的音频与实时试听效果完全一致，无爆音、无尾音截断 | [x] 已通过 (内置引擎声学参数与分段 Gain/UnaCorda 精确验证；插件不承诺样本级完全相同) |
| **WAV-009** | 已有文件重复导出 | 连续导出不同采样率/内容到同一目标，重新读取新 header 与可听 payload，不追加旧 WAV | [x] 已通过 (Windows 生产文件消费者验证) |
| **WAV-010** | 覆盖失败保留原文件 | 已有目标下取消、拒绝参数或锁定目标；原字节不变，任务不得删除用户目标 | [x] 已通过 (Windows 隔离消费者验证) |

### 4.1 验收证据依据与未验证范围说明

1. **实施依据**：WAV-001～WAV-010 证据覆盖自动化单元测试与 Windows 隔离真实消费者验证（见 [Phase C](../../roadmap/current-iteration.md#phase-c-实施记录与直接验证2026-10-03) EVID-020/023/024 与 [Phase D/E](../../roadmap/current-iteration.md#phase-d-实施记录与直接验证2026-10-04) EVID-030/035/040），涵盖原生 mode-aware VST3 offline flag/processBlock 验证、原生慢插件事件阻塞取消、非阻塞 JIVE 进度与覆盖保护；
2. **严禁外推的未验证范围**：
   - **商业第三方插件离线渲染**：未在商业音源（如 Pianoteq, Kontakt）执行全量离线音质与稳定性测试；
   - **极端不可中断挂起**：若第三方插件单次 `processBlock` 内部彻底陷入死循环且永不返回，协作取消机制无法在不损坏 CRT 堆的前提下强行终止线程；
   - **断电与磁盘故障**：临时文件重命名依靠操作系统原子覆盖语义，未模拟写入中途系统突然断电或硬件扇区损坏。
