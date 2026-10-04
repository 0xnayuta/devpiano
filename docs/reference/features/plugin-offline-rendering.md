# VST3 插件离线渲染与 WAV 音频导出功能说明

> 用途：说明 devpiano 的非实时音频离线渲染管线（`RenderPipeline`）、独立离线 VST3 插件实例管理（`PluginOfflineRenderer`）、内置物理建模钢琴声学一致性导出（`WavFileExporter` 与 `RoomReverbEngine`）、后台多线程导出任务（`WavExportTask`）与 JIVE 声明式进度条交互。
> 当前状态：已接入独立非实时实例、事务 WAV 写出、非阻塞后台任务与协作取消/退出；共享声学参数，不承诺 VST3 实时/离线逐样本相同，完整事件闭包以 AUDIT-004 当前实施计划为准。
> 更新时机：离线渲染管线、插件状态快照、导出进度交互或音频格式与声学参数发生变化时。

---

## 1. 概述与设计定位

当用户录制了一段演奏或导入了 MIDI 文件后，需要将演奏内容导出为高质量的 `.wav` 音频文件分享或存档。
为了实现高保真、高稳定性与非阻塞的导出体验，devpiano 建立了专用的**非实时离线渲染子系统**：

1. **独立离线 VST3 实例**：离线渲染在独立的非实时插件实例中执行，与当前前台实时发声链路（`AudioDeviceManager`）及 Editor 窗口完全解耦，导出期间用户依然可以实时试听；
2. **公共离线渲染管线（`RenderPipeline`）**：集中处理事件时间戳缩放、排序、音频块切分与尾部 panic note-off 注入，为插件渲染与内置物理建模合成器消除重复代码；
3. **后台多线程与 JIVE 进度交互**：`WavExportTask` 在后台工作线程执行密集音频渲染，消息线程以 30 fps 平滑刷新基于 `JiveModalDialog` 驱动的暗黑主题进度条；
4. **事务写出与协作取消**：内置和插件路径均写入同目录自有临时文件，检查 writer/流状态并关闭 writer 后才替换目标；渲染中及提交前接受取消，普通失败/取消只清理临时文件，保留已有目标；
5. **优雅降级（Graceful Degradation）**：若插件不支持离线渲染或实例创建失败，系统自动安全降级至内置物理建模钢琴（`PianoSynthVoice`），保证导出永远可用。

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

实时与离线事件缩放采用同一采样点取整；有效长度覆盖最后事件采样点 `+1`。即使最后 NoteOff 等于 Take 长度，也在其采样点交付，再在完整事件边界收尾。内置与插件两条离线渲染路径均按块内采样偏移分段执行，同采样预设优先于 MIDI 音符应用新声学快照与 Master/Reverb，并共享同一最终持有者发音身份追踪（`PlaybackIdentityTracker`）。

---

## 3. 核心机制与关键技术细节

### 3.1 独立离线插件实例生命周期（`PluginOfflineRenderer`）

- **独立性**：基于当前已加载插件的 `PluginDescription`，通过 `formatManager.createPluginInstance()` 创建一个全新的离线实例；
- **无 Editor 开销**：离线实例不创建任何 UI 窗口，避免跨线程 GUI 句柄死锁；
- **状态快照**：若实时插件支持状态保存，通过 `instance->getStateInformation()` 抓取当前音色参数并注入离线实例；
- **受控生命周期**：`create` → 消息线程 `setNonRealtime(true)` / prepare / state restore → 后台逐块 process → 实际工作退出 → 所有者 releaseResources / delete。原生 VST3 的 setup 与 process 都消费 offline mode；直接调用渲染函数可复用准备好的实例，释放由其调用者负责。

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
7. **尾音衰减窗口**：离线渲染结束后自动追加固定 2.0 秒尾音衰减窗口（`wavTailSeconds = 2.0`），确保音板自然衰减与大空间混响尾音完整保留不被硬切。

---

## 4. 专项手工与边界测试清单

| 用例编号 | 测试场景 | 操作步骤与验证目标 | 状态 |
|---|---|---|:---:|
| **WAV-001** | 内置物理建模钢琴离线导出 | 在未加载插件下录制演奏并导出 WAV，导出的音频具有真实的物理建模钢琴音色 | [x] 已通过 |
| **WAV-002** | VST3 插件音色离线导出 | 加载 VST3 插件后录制演奏并导出 WAV，导出的音频为该 VST3 插件的真实音色 | [x] 已通过 |
| **WAV-003** | 导出期间实时弹奏解耦 | 导出长时间 WAV 期间，在前台按键盘弹奏，实时发声不受影响，导出音频中无键盘杂音 | [x] 已通过 |
| **WAV-004** | JIVE 进度条平滑刷新 | 导出过程中观察 JIVE 进度条从 0% 平滑推进至 100%，状态文本实时显示进度百分比 | [x] 已通过 |
| **WAV-005** | 中途协作取消与文件收尾 | Cancel 后显示取消中；等待正在执行的 block 返回再释放任务，目标目录无未完成输出，已有目标字节保留 | [x] Windows 原生慢插件与实际进度/退出消费者验证 |
| **WAV-006** | 目标路径无权限容错 | 导出至只读目录或非法路径，弹窗提示错误，Logger 记录日志，程序不崩溃 | [x] 已通过 |
| **WAV-007** | 空 Take 导出拦截 | 在无录制且无导入状态下，Export WAV 按钮自动保持 Disabled | [x] 已通过 |
| **WAV-008** | 物理声学与空间参数离线一致性 | 配置特定古典律制、听众视角与房间混响后导出 WAV，导出的音频与实时试听效果完全一致，无爆音、无尾音截断 | [x] 已通过 |
| **WAV-009** | 已有文件重复导出 | 连续导出不同采样率/内容到同一目标，重新读取新 header 与可听 payload，不追加旧 WAV | [x] Windows 文件消费者验证通过 |
| **WAV-010** | 覆盖失败保留原文件 | 已有目标下取消、拒绝参数或锁定目标；原字节不变，任务不得删除用户目标 | [x] Windows 隔离消费者验证通过 |

除手工场景外，离线渲染子系统由自动化单元测试全面覆盖：`PluginOfflineRendererTest`（离线插件实例创建、状态注入与 WAV 渲染）、`RenderPipelineTest`（事件缩放、稳定排序与 panic 注入）、`InstrumentEndpointTest`（端点同构路由解析）、`ExportFlowTest`（导出选项装配与异常边界防护）。
