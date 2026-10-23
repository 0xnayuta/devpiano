# R0–R11 内置钢琴音源结构整理归档

> 阶段：R0–R11 内置钢琴音源结构整理。状态：完成并验收；R5/R6 所有权迁移经用户试听通过，R7 sustain 初始化修复完成，R9 主弦提取验证完成，R10 封装完成，R11 收口完成。
> 归档日期：2026-10-23。来源：`refactor/physical-piano-components` 分支归档前的 `docs/roadmap/current-iteration.md`（归档基线提交 `1182836`）。
> 归档范围：R0 基线、十项线索复核、组件与状态表、R1/R2/R3 提取、R4/R5/R6/R6-F1 踏板所有权迁移、R7 sustain 修复、R8 主弦边界设计、R9 主弦提取、R10 接口封装、R11 最终验收与恢复快照。
> 历史边界：以下保留 R0–R11 各轮的原记录。R11 原文中的“未提交/未跟踪”描述是当时快照的历史状态，后续提交与 PR 交付不回头改写该记录；最终源码与验证状态以提交历史和现行架构/功能契约为准。R5/R7 是已批准行为变化，不能按“纯重构”理解。
> 替代关系：项目状态和长期顺序见 [roadmap](../roadmap/roadmap.md)；当前候选阶段见 [当前迭代](../roadmap/current-iteration.md)；现行组件边界见 [架构](../reference/architecture.md)，声学行为见 [内置音源](../reference/features/builtin-piano-synthesis.md)。证据位于仓库外 `G:\source\projects\devpiano-evidence\`，本页不复制大体积音频。

---

## 0. R0 当前任务入口：规划冻结、实现复核与重构基线（2026-10-22）

**状态**：R0–R4 已执行并封存；R5 采用修订方案 A 的实现及客观验证已完成；R6 的定向收口与同一生产路径 R3/R5 对照已完成；R6-F1 修正材料经用户重新试听，未听出明显差异或截断，R5 所有权迁移的试听验收通过（不扩大为整个音源音色认证）；R7 已修复新分配声部的 note-driven sustain 初始化；R8 完成主弦核心边界设计；R9 已按方案 B 完成主弦组件提取及 R7 最终音频/生命周期等价验证；R10 完成 Voice 内部状态封装与消费者边界收口；R11 完成本轮物理钢琴音源重构的最终验收与交付收口。Phase 40+ 仍暂缓。本节是当前唯一直接任务入口，第 1 节以下为 Phase 39 历史记录，不再作为当前执行指令。

### 0.1 本轮边界

- 目标：降低 `PianoSynthVoice` 及相关组件的职责集中、状态耦合和维护成本；分清单音状态、整琴状态、宿主公共状态。
- 保持已有演奏身份、实时安全、实时/离线一致性与文件保护；不引入兼容双路径或为未来功能预建框架。
- R1/R2/R3/R9/R10 的组件提取与封装未修改生产 DSP 公式、参数默认值、声部数量或计算顺序；R5 的整琴踏板/尾音所有权迁移和 R7 的新声部 sustain 初始化是已批准行为变化，见 §0.18。
- Phase 40–42 及后续尚未实施的声学能力全部暂缓，恢复须由用户明确决定；Phase 39 的未完成项不因本轮自动通过，也不作为结构整理的前置条件。
- 唯一任务入口为本页；证据在 `G:\source\projects\devpiano-evidence\r0-baseline-20261022\`（仓库外）。

### 0.2 组件与状态表

| 组件 | 当前所有者 / 实例 | 输入、输出、直接调用者 | 初始化、推进、停止、重置 | 参数生效 / 回调 / 准备分配 | 测试是否直接依赖内部字段 |
|---|---|---|---|---|---|
| `PianoSynthVoice`（R0 时 `PianoSynthVoice.h` 2322 行；R1 后 2131 行；R2 后 2077 行；R3 后 1461 行） | 单音声部；每个 `BuiltinSynthesiser` 8 个（实时与离线各一套） | 输入 NoteOn/Off、CC64/67、参数；输出 stereo float；调用者 `BuiltinSynthesiser::renderVoices` | `startNote` 初始化分音/瞬态/ADSR/共鸣；`renderNextBlock` 逐样本推进；`stopNote` 进入 ADSR 释放并触发 damper；包络与 damper 结束清音；硬停/采样率变更重置 | setter 可在任意时刻写；全部在音频回调内执行；构造/`setCurrentPlaybackSampleRate`/`startNote` 更新系数；无堆分配 | **是**：`PianoSynthVoiceTest` 直接读 `numActivePartials`/`partials`，直接写 `hammerContactEngine`；多套测试依赖 `isVoiceActive`/`getCurrentlyPlayingNote` |
| `BuiltinSynthesiser` | 整琴调度器；`AudioEngine` 的 `synth`/`sineSynth`，离线 `pianoSynth`/`sineSynth` 各一个 | 输入 `MidiBuffer` 或直接 note/controller；输出 `renderNextBlock`；调用者 `AudioEngine`、`WavFileExporter`、`PluginOfflineRenderer` | 构造设最小细分；采样率变更 `allNotesOff`；渲染按事件切块；noteOn/off、CC64/66/67、allNotesOff；`clearVoices` 清容器 | 持有 16 通道踏板状态；实时回调；voice/容器在 `rebuildSynth`/离线初始化分配，渲染循环不分配 | 测试检查 voice 数、踏板标志与 `isVoiceActive`；不访问容器私有 |
| `AudioEngine` | 宿主/整琴运行状态；一个实例 | 输入键盘、录音回放、性能输入、参数 setter；输出 `AudioBuffer`；调用者设备回调和测试 | `prepareToPlay` 分配/准备；`getNextAudioBlock` 消费事件、分段、应用 snapshot；`releaseResources` 停音、清 Room | 参数为原子待提交值，块首合并应用；Room/Master/limiter 在回调；准备阶段分配 MIDI buffer/延迟/插件 buffer | `AudioEngineTest` 依赖 voice 索引、`isVoiceActive` 与 8 声部行为 |
| `Piano88KeyTable` / `PianoTuning` / `TemperamentEngine` / `HammerVoicing` / `PianoRegisterVoicing` | 静态共享只读数据/纯函数；无实例状态 | `getNoteParams`、`getPianoStretchRatios`、`getFrequency`、`sampleHammerVoicing`、`pianoRegisterLoudnessGain`；由 `PianoSynthVoice` 构造/`startNote` 调用 | 函数局部 static 首次初始化；之后只读；无停止/重置 | `startNote` 读取；音频回调只读；首次表初始化在 voice 构造（引擎构造/离线准备） | `PianoSynthVoiceTest`、`PianoTuningAndDuplexTest` 直接断言曲线/参数值 |
| `PerspectiveProcessor` | 每个 `PianoSynthVoice` 一份，不是整琴共享 | 输入 stereo L/R，输出 stereo；调用者 `PianoSynthVoice::renderNextBlock` | `prepare` 设采样率并清 filter；`setPerspective` 立即/平滑切换；voice 结束时 reset | 每样本处理；回调内；无堆分配 | `PerspectiveProcessorTest` 独立测试；`PianoSynthVoiceTest` 通过音频验证 |
| `RoomReverbEngine` | 整琴共享总线；`AudioEngine` 一份，`WavFileExporter`/`PluginOfflineRenderer` 各一份 | 输入 stereo block，输出 stereo；调用者 `AudioEngine`/两个 WAV 消费者 | `prepare` 按采样率重分配 comb/allpass 并 reset；`processStereo` 推进；`reset` 清 buffer；`setSpace` 改反馈不清尾 | 每 block 回调；`prepare` 分配 `std::vector`；wet 平滑；target/current 均 `<=1e-4` 才旁路 | `RoomReverbEngineTest` 独立；`PluginOfflineRendererTest` 通过输出验证 |
| `AcousticSnapshot` / `RenderPipeline` / `WavFileExporter` / `PluginOfflineRenderer` | 值快照与离线渲染管线；每个离线任务私有 | 输入 Settings/Take preset/options；输出 WAV；调用者 `RecordingSessionController`/`WavExportTask`/`PluginOfflineRenderer` | `prepareRenderTimeline` 缩放/排序；每个 preset 边界应用 snapshot；逐 block 渲染/write | 离线准备与 IO 线程；snapshot 会对每个 voice 重复调用 setter；WAV 16/24 bit | `ExportFlowTest`、`PluginOfflineRendererTest`、`RenderPipelineTest` |

**状态归属结论（R0 历史快照；当前最终所有权以 §0.18 和架构文档为准）**：单音状态（分音振荡器、ADSR、hammer/damper/pedal 瞬态、Duplex、12 音级交感池、16 峰音板、琴盖、视角）在 R0 时都在 `PianoSynthVoice` 内；`BuiltinSynthesiser` 持有声部容器和 16 通道踏板布尔；`AudioEngine` 持有 Room、Master、层路由、活动快照和传输状态；静态表只读。R5 后整琴机械踏板 owner 已移出 voice0；R0 时第 0 声部只是当时的机械踏板声承载体。

**参数生效时机**：`setPianoParameters`、`setHammerVoicing`、`setFeltAgeingAmount`、`setPianoTuning` 的力度/音色部分主要在下一次 `startNote` 读取（`setPianoTuning` 同时更新当前 voice 的 sympathetic 系数）；`setTemperament`/`setReferencePitchA4` 立即更新 sympathetic 系数但不重置已起音分音；`setLidPosition`/`setSoundPerspective` 对当前声音立即生效（视角按 30 ms 平滑）；`setPedalNoiseLevel` 在下一次踏板边沿触发时读取。`AudioEngine` 的 setter 先发布原子待提交值，音频线程在块首合并；离线路径在每个 preset 边界应用完整 snapshot。

### 0.3 十项线索复核

| # | 结论 | 依据 |
|---|---|---|
| 1 | **已确认的代码事实**：`SympatheticResonancePool` 是每个 `PianoSynthVoice` 的成员；只有 12 个音级计数和本声部 `noteOnKey/noteOffKey`，没有跨声部输入或真实 88 弦开放键位图。 | `PianoVoiceResonance.h:18-137`（R1 后）；`noteOnKey`/`noteOffKey` 只由本 voice 的 `startNote`/`stopNote` 调用 |
| 2 | **已确认的代码事实 + 运行观察（R0 拓扑）**：R0 时第 0 声部承载全局 `pedalTransient`/shock，空闲 `isPlayingChannel` 仍为 true；8 声部诊断中新增音符偷走 voice 0（`steal-voice0-target,0,67,1`）。R5 owner 迁移后 voice 0 不再承担全局踏板，R7 同一诊断的目标是空闲 voice 7（`steal-voice0-target,7,67,*`）；名称保留但已不是 steal voice 0。R7 `0 -> 1` 只指 R7 修复前/后，R0 目标本来就是 1；比较对象见 §0.15。 | R0：`r0-baseline-20261022/run1/lifecycle-diagnostics.txt`；R5：`r5-pedal-owner-20261022/run-r5-final/lifecycle-diagnostics.txt`；R7：`r7-note-sustain-initialization-20261022/r0-before|r0-after/lifecycle-diagnostics.txt`；`BuiltinSynthesiser.cpp::findVoiceToSteal` |
| 3 | **已确认的代码事实 + 运行观察（区分拓扑）**：R7 前 `noteOn` 只设置 JUCE voice sustain 标志，不更新 note-driven `sympatheticPool`。R0 诊断的新 voice 为索引 1、pool false（`new-voice-after-cc64,1,60,0`）；R5 移除 voice0 踏板占用后为索引 0、pool false（`new-voice-after-cc64,0,60,0`）；R7 修复后为索引 0、pool true（`...,0,60,1`）。 | `BuiltinSynthesiser.cpp::noteOn`；R7 `r7-production-fix.patch`；`r5-pedal-owner-20261022/comparison-to-r0.json`；§0.15 |
| 4 | **已确认的代码事实**：Duplex 有独立存活分支，琴盖/视角状态在 Duplex 尾音期间保留；但 body resonator 在 ADSR/damper 结束时 reset，非踏板 voice 的 sympathetic pool 同时 reset，`isVoiceActive` 不包含 sympathetic 能量；空音符路径只在 R0 的 voice 0 pedal/shock 活跃时处理；R5 后由独立 owner 持有。 | `PianoSynthVoice.h:770-1000`；`allPartialsSilent`/`clearCurrentNote` 条件 |
| 5 | **已确认的代码事实**：琴盖反射抽头固定为 141/344/507 采样点，只有低通系数按采样率更新；延迟时间随采样率变化。 | `PianoSynthVoice.h:1956-2049`（R1 后） |
| 6 | **已确认的代码事实**：`applyAcousticSnapshotToBuiltin` 对每个 voice 无条件调用全部 setter；`setPianoTuning`、`setTemperament`、`setReferencePitchA4` 各自调用 `sympatheticPool.updateCoefficients`，同时变更时可重复遍历 12 个谐振器；缓存命中时提前返回。 | `RenderPipeline.cpp:116-171`；`PianoSynthVoice.h:326-380`；`PianoVoiceResonance.h:51-75` |
| 7 | **已确认的代码事实**：R0 时 `PianoSynthVoice.h` 2322 行（R1 后 2131 行、R2 后 2077 行）且没有 `private:` 区段；15 个翻译单元包含该头；测试直接访问内部字段/写 `hammerContactEngine`。 | `PianoSynthVoice.h:236,246,1094-1097`；`PianoSynthVoiceTest.cpp:722,1352-1390`；`grep` 15 个包含者 |
| 8 | **已确认的代码事实**：生产没有使用 `juce::Reverb`；唯一实现是自定义 `RoomReverbEngine`。旁路条件是 target/current wet 均 `<=1e-4`；wet 平滑系数 0.998；干湿为 `dry=1-0.5*wet`、`wetGain=2.8*wet`（注释称 constant-power，但公式不是）；`setSpace` 不清尾；尾音由共享总线独立推进。 | `RoomReverbEngine.h:49-176`；`grep juce::Reverb` 仅命中 JUCE 源码 |
| 9 | **已确认的代码事实 + 运行观察**：实时 `AudioEngine::rebuildSynth` 固定 8 个 Piano voice，离线默认 8；`BuiltinSynthesiser` 本身无硬上限。R0 时第 0 声部会占用一个 slot，且可被偷；R5 后全局踏板声不再占用 voice slot。9 音序列可正常渲染（`seq-steal-nine-notes`），但选择哪个 voice 由分配器决定；该序列不构成偷声部门槛结论。 | `AudioEngine.cpp:607-618`；`RenderPipeline.h:51-57`；诊断文件 |
| 10 | **已确认的代码事实**：`PianoNoteParams::isBassBridge` 只写不读，`decayDampingC` 只写不读，`detuneCents` 只写不读；`PianoSynthVoice` 的 `partialCountForNote`、`decaySecondsForNote`、`decayDampingCForNote`、`inharmonicityBForNote`、`fastDecayRatioForNote`、`slowWeightForNote`、`beatingDetuneRatioForNote`、`beatingPartialCountForNote`、`regionForNote`、`strikingPositionRatioForNote`、`bodyWet`、`partialFastDecaySeconds`、`bodyWetRatio` 无生产消费者；`RoomReverbEngine::sr`、`PerspectiveProcessor::sr` 只赋值不读取。 | `Piano88KeyTable.h:15,22,24,104,172,183`；`PianoSynthVoice.h:191,911-967`；全仓 `grep` 无消费者（测试使用 `partialDecaySeconds`/`beatingFrequency`/`resonatorCount`，不计为生产消费者） |

### 0.4 基线

- 协议：48 kHz、24-bit、双声道、block 512、2.0 s WAV 尾音；8 个单音（21/36/48/60/72/84/96/108，覆盖低/中/高及 36/48/72/96 分区）和 4 个短序列（CC64 后新声部、9 音偷声部、Panic 后重用、同键重复），另 1 个 Room wet 0.35 集成用例；每个用例全新引擎/voice，`triggerCounter` 从 0 开始。
- 两次独立进程（run1/run2）共 13 个 WAV 和 13 个 raw float 的 SHA-256 全部一致；全部有限；主基线 Room 关闭。WAV 经 `exportTakeAsWavFile`（24-bit PCM、Master/limiter 后），raw `.f32` 经 `initialiseOfflinePianoSynth` + `BuiltinSynthesiser::renderNextBlock`（Room/Master/limiter 前）。因此不宣称内部浮点逐 bit 一致。
- 现有 `Export: piano snapshot onset and release parity` 回归 29/29 通过；直接复用，未重复建立等价实时/离线用例。
- 既有证据核对：`phase39/task39-3` 与 `issue2-register-20261020` 的最近 48 kHz 渲染集使用旧 `+6/+5/+2/-1/-2 dB` 音区曲线（提交 `2e747da` 前后），而 HEAD `09a07b6` 已改为当前 `+10/+8/+1/0/0 dB` 曲线；Task 39-3 文档引用的 `build-win-msvc\phase39-task3-baseline-20261019` 目录在当前镜像不存在。因此旧证据不能直接作为当前生产基线，本轮新基线是必要的。
- 证据：`G:\source\projects\devpiano-evidence\r0-baseline-20261022\README.md`、`evidence-manifest.json`、`run1/`、`run2/`、`probe/`、`default-tests.log`、`ctest.log`、`integration-parity.log`。

### 0.5 后续工作包（R0 方案；R1 仅完成 A0）

**A. 保持声音与状态行为的结构拆分（低风险优先）**

1. **A0 / R1（已完成）**：把 `SympatheticResonancePool` 原样移到独立头文件（保留 `sympatheticPool` 成员名、类型、算法、调用顺序），`PianoSynthVoice.h` 只保留组合；基线 13 个 WAV/raw 哈希逐 case 相同。理由：状态自包含但并非“无外部依赖”，它依赖调律/数学底层；本次提取保留了这些依赖边界。
2. **A1 / R2（已完成）**：提取 `startNote` 的只读起音计算到 `computePianoNoteSetup` / `PianoNoteSetup`，不改变浮点表达式和状态安装顺序。
3. **A2 / R3（已完成）**：把 hammer/damper/pedal 三个瞬态引擎成组移出到 `PianoMechanicalTransients.h`；它们依赖 `peakLevelAtFullVelocity`、查表正弦和有界三角。保持成员名、成员相对顺序、字段和调用顺序；旧 `PianoSynthVoice::HammerTransient` 测试消费者同轮改为命名空间类型，不保留别名。
4. **A3（移至 B 类候选，未实施）**：参数快照按变化位只调用相关 setter 会改变调用副作用，先按 B 类审查，不预认定为 A 类。
5. **暂时保留在一起**：`Partial` 数组与 `renderNextBlock` 内的振荡器循环、`startNote` 的力度→硬度→接触时间→权重/衰减顺序、`BodyResonator` 16 峰组；这些共享每样本顺序和相位状态，当前拆开只会增加跨对象访问。
6. **状态迁移**：可考虑迁到整琴层的是全局踏板机械声/冲击（当前 voice 0）和真正的共享音板/交感总线；迁移会改变分配、通道、停止与尾音语义，归 C 类，不在 A 类实施。

**B. 满足明确条件才成立的实现等价变更**

- 参数快照按变化位提交并只调用相关 setter：先审查 active ADSR/视角/调律系数等副作用，再证明逐样本等价和基线哈希不变；未实施。
- 删除无消费者字段/辅助函数（第 10 项）并在测试中改用行为断言；条件：全仓消费者核验、构建/测试通过、基线哈希不变。
- 合并调律 setter 为一次 `applyTuning`；条件：同时改多个调律参数时输出逐样本等价，12 谐振器只重算一次。
- 去掉 `currentPlayingMidiNote` 副本，改用 JUCE 基类 `getCurrentlyPlayingNote()`；条件：独立 voice 测试与所有 NoteOff/偷声部路径通过。
- 提取头文件仅当编译时间/包含面有实测收益；不以文件数量为目标。`RoomReverbEngine` 不替换为 `juce::Reverb`，除非有明确声音/尾音等价证据（见 C）。

**C. 会改变声音、控制或尾音行为的修正（须单独批准）**

- 为 voice 0 保留全局机械声 slot，或把踏板机械声/冲击移到整琴层，避免特定分配下偷声部切断踏板尾音。
- 在 `noteOn` 分配新 voice 时同步 `sympatheticPool.setPedalDown`（或由整琴层统一驱动），修复 CC64 后新声部共鸣状态不一致。
- 明确 body/sympathetic 的尾音存活条件，避免 ADSR/damper 结束即 reset，或在 `isVoiceActive` 中纳入真实能量；这会改变尾音，须新基线 + 试听。
- 琴盖延迟改为按时间（采样率归一化）定义；会改变 44.1/48/96 kHz 的相对时间。
- Room 干湿/旁路语义修正（若确认 `1-0.5*wet`、`2.8*wet` 非预期）；须单独测量/试听。

### 0.6 R0 验证记录

- Windows Debug MSVC 构建通过；Windows CTest 1/1 通过（`devpiano_tests`，323315 断言通过 / 0 失败）；R0 基线测试 27 通过 / 0 失败（run1/run2 各一次）；`Export: piano snapshot onset and release parity` 29/29。
- 未执行 Release、CPU SLA、物理声卡、商业插件或全量 tidy；不将本轮 Debug 结果外推。
- R0 生产源码未修改；临时测试 `source/tests/R0BaselineRenderTest.cpp` 与 CMake 一行仅在基线执行期间存在，证据归档后移除（`probe/` 保留精确源码）。

### 0.7 R1 提取记录（2026-10-22，已完成）

- **文件划分**：新增 `source/Audio/PianoDspMath.h`（`kPi/kTwoPi/kHalfPi/kInvTwoPi/kInvHalfPi`、`SinCosResult`、`boundedSinCos`、`boundedSin`、`boundedCos`，从 `PianoSynthVoice.h` 原样搬移）；新增 `source/Audio/PianoVoiceResonance.h`（`devpiano::audio::SympatheticResonancePool`，从 `PianoSynthVoice.h` 原样搬移）；`PianoSynthVoice.h` 仅增加两个 include 并把嵌套定义改为成员 `devpiano::audio::SympatheticResonancePool sympatheticPool;`；`CMakeLists.txt` 只在 app/tests 的 `target_sources` 中登记两个新头文件。
- **依赖方向**：`PianoSynthVoice -> PianoVoiceResonance -> PianoDspMath + PianoTuning + TemperamentEngine`。`PianoVoiceResonance.h` 自身包含所需头文件；不包含 `PianoSynthVoice.h`，不依赖调用方全局别名或 include 顺序。`PianoDspMath.h` 可独立包含。
- **所有权**：`sympatheticPool` 仍是每个 `PianoSynthVoice` 一份；没有改成整琴共享，没有修改 voice 0、声部分配、尾音或 CC64 同步语义。
- **行为**：共鸣频率、12 个音级计数、shock、门控、增益、衰减和调用时机未改；`PianoSynthVoice.h` 的搬移文本已与 HEAD 原块逐行核对一致（去缩进后完全相等）。
- **验证**：Windows Debug app/tests 构建通过；默认 CTest 1/1 通过（323315 断言）；全量 `clang-tidy --all` 通过；R0 探针 13 个 case 在 R1 下 run1/run2 与 R0 `run1` 逐 case 对照：WAV SHA-256、raw `.f32` SHA-256、声道/采样率/位深/帧数全部相同；生命周期诊断输出相同。
- **证据**：`G:\source\projects\devpiano-evidence\r1-extract-sympathetic-20261022\`（`README.md`、`comparison-to-r0.json`、`run1/`、`run2/`、`probe/`、`ctest-with-probe.log`、最终清理后的 `ctest-final.log`）。探针源码 SHA-256 与 R0 归档 `probe/R0BaselineRenderTest.cpp` 一致，未改渲染语义。
- **后续**：R3 已完成；任何 B/C 类变更待用户明确指令。

### 0.8 R2 提取记录（2026-10-22，已完成）

- **文件划分**：新增 `source/Audio/PianoNoteSetup.h`（`PianoNoteSetup` + `computePianoNoteSetup`）；`PianoSynthVoice.h` 的 `startNote` 只负责守卫、调用计算、安装输出状态和原有子组件触发；`PianoDspMath.h` 接管原先位于 `PianoSynthVoice` 的 `deterministicNoteJitter`；`FeltAgeingTest` 仅更新该辅助函数的符号引用。
- **精确边界**：从 `getNoteParams` 后的逐键/老化/调律/击弦计算，到 `effectiveInharmonicityB`、`baseFrequency`、`alpha1` 等供分音准入使用的参数。输出为 17 个实际消费的标量：`firstPartialFrequency`、`effectiveInharmonicityB`、`baseFrequency`、`keyPos`、`clampedVelocity`、`velocityLevel`、`unaCordaMu`、`hammerHardness`、`contactHardness`、`effectiveBrightness`、`hammerVoicingRolloffDelta`、`effectiveStrikePos`、`effectiveTc`、`jitterPhase`、`baseDecaySeconds`、`k1`、`alpha1`。没有大型通用音源快照。
- **Voice 保留**：`currentPlayingMidiNote`、`noteOutputGain`、`triggerCounter` 的存储与递增、`duplexPool.prepare`、`numActivePartials` 初值和分音准入循环、权重累计/同音弦功率归一化、`Partial` 状态安装、所有子组件 prepare/trigger/reset、`sympatheticPool.noteOnKey`、ADSR 启动、`renderNextBlock`/`stopNote`/`controllerMoved`。
- **triggerCounter 与扰动**：Voice 在采样率 guard 之后、调用纯计算之前执行一次 `++triggerCounter`，把推进后的值作为 `triggerSequence` 显式传入；计算函数内不再含 `++counter`、全局 RNG、时间/地址种子或静态可变状态。无效采样率仍在递增前返回。整数乘法、`uint32_t` 转换、三次 `hashJitter` 调用顺序保持原样。
- **行为与验证**：R0 探针 13 个 case 的 WAV/raw `.f32` SHA-256、声道/采样率/位深/帧数与 R0 完全相同；生命周期诊断相同。Windows Debug app/tests 构建、最终清理后默认 CTest、相关 `PianoSynthVoice`/`FeltAgeing`/`UnaCordaAcoustics` 回归通过。
- **证据**：`G:\source\projects\devpiano-evidence\r2-note-setup-20261022\`（`README.md`、`comparison-to-r0.json`、`run1/`、`run2/`、`run-final/`、`probe/`、构建/CTest/定向回归日志）。


### 0.9 R3 提取记录（2026-10-22，已完成）

- **文件划分**：新增 `source/Audio/PianoMechanicalTransients.h`（585 行；`HammerTransient`、`DamperTransient`、`PedalTransient` 三个独立类型，原样搬移）；新增 `source/Audio/PianoSineLookupTable.h`（60 行；原 `SineLookupTable` + 单一 `gSineLookupTable`）；新增 `source/Audio/PianoSourceLevels.h`（13 行；`nominalPeakLevelAtFullVelocity`、`globalSourceGain`、`peakLevelAtFullVelocity` 单一编译期定义）；`PianoSynthVoice.h` 由 2077 行降至 1461 行，只保留成员组合并移除旧嵌套定义。
- **依赖方向**：`PianoSynthVoice -> PianoMechanicalTransients -> PianoDspMath + PianoSineLookupTable + PianoSourceLevels`。`PianoMechanicalTransients.h` 不包含 `PianoSynthVoice.h`，不依赖调用方别名或 include 顺序；三个瞬态结构保持相互独立，不引入基类或聚合引擎。
- **原样边界**：三个类型的字段、默认值、常量、公式、RNG 初始化、滤波系数、`peakLevelAtFullVelocity` 使用和 `prepare/trigger/process/reset/isActive` 调用位置未改；`HammerTransient`、`DamperTransient`、`PedalTransient` 成员名及相对声明顺序保持。`PianoSynthVoiceTest` 仅把 `PianoSynthVoice::HammerTransient` 更新为 `devpiano::audio::HammerTransient`，输入与断言未改。
- **共享来源**：正弦表尺寸 4096、`float` 元素、双精度/单精度 `lookupNorm` 插值、构造公式和 `inline const` 初始化方式保持；`SineSynthVoice.h` 的 `detail::SineLookupTable` 是既有独立实现，未合并。源级增益仍由主弦预算与 hammer/damper 瞬态共享，数值仍为 `0.95f * 0.49f`。
- **验证**：Windows Debug app/tests 构建通过；最终清理临时探针后默认 CTest 1/1 通过。R0 探针 13 个 case 与 R0 `run1` 逐项对照：WAV SHA-256、raw `.f32` SHA-256、声道/采样率/位深/帧数和 cases.csv 全部相同；两项生命周期诊断相同。机械瞬态独立包含/反向依赖/重复定义检查通过；相关 `PianoSynthVoice` 10205、`DamperRelease` 13、`PedalAcoustics` 1112、`MechanicalAcoustics` 19、`FeltAgeing` 713 断言通过，0 失败；格式检查与全量 `clang-tidy --all` 均退出 0。
- **证据**：`G:\source\projects\devpiano-evidence\r3-mechanical-transients-20261022\`（`start-state.txt`、`r3-delta.md`、`source-before/`、`comparison-to-r0.json`、`run-final/`、`probe/`、构建/CTest/定向回归日志）。

### 0.10 R4 整琴踏板声学所有权设计（2026-10-22，完成设计，未实施）

**状态**：R4 只完成所有权设计、事件/音频契约和迁移前行为取证；生产 DSP、事件调度、状态所有权、声部数、参数提交和文件格式均未修改。R5 是否实施须由用户明确批准。

**起点**：HEAD 仍为 `09a07b6bf090675f319981e75c8057ea3a022601`；工作树包含 R0–R3 的全部 tracked/untracked 修改。R3 源码可由其 manifest、`source-before`/`source-after` 和补丁重建；R4 起始状态另见证据目录 `r4-pedal-ownership-20261022/start-state.txt`。

**流程偏离记录**：R3 报告中使用了 WSL 完整 `wsl-build` 和 `dev.sh test`。当前 `AGENTS.md`/`docs/guides/wsl-windows-msvc-workflow.md` 只允许 WSL 编辑和 `wsl-build --configure-only`；完整构建与软件测试必须在 Windows Debug 镜像树执行。R4 未重复 WSL 构建/测试，也未重跑 R3 验证。

#### 推荐方案 A 与必要备选 B

- **A（R5 已批准）**：在 `BuiltinSynthesiser` 内增加一个 `PianoPedalAcoustics` 实例，拥有 `PedalTransient`、一个只接收 shock 的 `SympatheticResonancePool`（不调用 `noteOnKey/noteOffKey`、不接收 raw note 输入）、一个 `PianoLidAcoustics` 和一个 `PerspectiveProcessor`；`PianoSynthVoice` 的音符驱动 `sympatheticPool` 保持每个 voice 一份。这样可把机械瞬态、sympathetic shock、琴盖/视角状态和 voice 0 特例一起移出 Voice。代价是新增一个独立的 12 模态 shock 状态，并产生可控的 shock 差异。
- **B（必要备选）**：第一步只把 `PedalTransient` 与琴盖/视角路径移到 `PianoPedalAcoustics`，保留 `PianoSynthVoice` 的 sympathetic shock 和 voice 0 特例。实现较窄，但不能完整解除 voice 0 职责。

推荐 A：当前 `sympatheticPool.triggerShock` 与 note-driven resonance 共用 `s1/s2`、`shockSamplesRemaining`、`openNoteCount` 和同一个 `process()`；直接把 shock 状态搬到 owner 会改变 note-driven 池的递推状态，而把整个池搬走又会违反“暂不升级为全琴共享共鸣”。A 的最小可行拆分是保留每个 voice 的 note-driven 池，另建一个 **shock-only** 的同一类型池，只用于踏板冲击；它不是全琴开放弦/音符共鸣模型，也不产生重复冲击，因为旧 voice 0 的 `triggerShock` 会同时移除。

#### 当前事实与状态迁移

| 状态/职责 | 当前所有者 | R5 推荐所有者 | 关键约束 |
|---|---|---|---|
| `PedalTransient`（whoosh、内部低频 shock、RNG、滤波、whoosh/shock 计数） | 每个 `PianoSynthVoice` 一份；只有 voice 0 执行 `trigger/getNextSample` 和输出 | `PianoPedalAcoustics::mechanical` | 其他 voice 的实例没有机械输出；R5 删除 per-voice 机械实例 |
| `sympatheticPool` 的 note-driven 状态（`openNoteCount`、`c1/c2/g/s1/s2`、`pedalDown`） | 每个 `PianoSynthVoice` 一份 | 保持每个 Voice 一份 | 不升级为整琴共享开放弦模型；`noteOnKey/noteOffKey` 和 `process(rawMono * noteOutputGain)` 保持 |
| `sympatheticPool` 的 shock（`shockSamplesRemaining` 与对 `s1` 的脉冲） | voice 0 的 `sympatheticPool`，与 note-driven 状态共用数组 | `PianoPedalAcoustics::shockTail`（shock-only 的 `SympatheticResonancePool`） | 旧 voice 池不再 `triggerShock`；新池不接收 noteOn/OffKey，不共享 note 状态 |
| `pianoPedalNoiseLevel` | 每个 Voice 保存，只有 voice 0 读取 | `PianoPedalAcoustics` 单一值 | 快照/实时参数仍按现有块首提交 |
| `pedalDown` / `lastControllerValue` | 分散在 Voice/PedalTransient | `BuiltinSynthesiser` 的逐通道原始 CC 历史与聚合 `anySustainDown`；owner 只接收聚合边沿 | 不建立两份可各自修改的通道真值；逐通道 sustain/sostenuto 原状态保留 |
| `isPedalTransientActive` / `isSympatheticShockActive` | `PianoSynthVoice` 方法 | owner/`BuiltinSynthesiser` 查询接口 | 测试改为 owner 行为；不保留旧嵌套别名 |
| `LidPosition` / `LidAcoustics` / `PerspectiveProcessor` | `PianoSynthVoice` 内 per-voice；踏板借用 voice 0 的实例 | `PianoPedalAcoustics` 的独立实例；算法原样复用 | 抽取 `PianoLidAcoustics.h`；固定抽头 141/344/507、低通和视角公式不变 |
| `voiceIndex == 0` 的 active/channel/render 特例 | `PianoSynthVoice` | 删除踏板/shock 相关条件 | 保留 voice index 机制用于既有声部分配/测试，不把机械尾音算作 voice 活跃 |

#### 事件契约

| 事件 | 逐通道演奏状态 | 机械声 / shock | 尾音与历史 | Voice 分配 |
|---|---|---|---|---|
| CC64 down，聚合 false→true | 对应通道 sustain=true；记录该通道 raw value | 触发一次 down `PedalTransient`；A 同时 `triggerShock` | 开启/重启 owner 尾音；保存 lastCC | 无 |
| CC64 up，聚合 true→false | 对应通道 sustain=false | 触发一次 up `PedalTransient`；不触发 shock | 尾音继续按状态机 drain；保存 lastCC | 无 |
| 同值重复 / 仍在同一侧 | 更新该通道 lastCC；逐通道 sustain 不变 | 不触发 | 不重启尾音 | 无 |
| 无音符踩下 | owner `anySustainDown=true` | 机械 down + shock 各自启动 | owner 独立渲染；不依赖 voice 0 | 不占用 voice |
| 踏板已下时新 NoteOn | 新 voice 得到 `setSustainPedalDown`（并按现有语义继续 note-driven 池） | owner 不重复触发机械/shock | 当前踏板尾音保留 | 正常分配；不因踏板占用 slot |
| 两通道交替/重叠 | 每个通道独立保存 sustain；owner 用 OR 聚合 | 只在聚合边沿触发一次；A 的 shock 同 | 不按通道复制机械声 | 无 |
| CC120 All Sound Off | 清除目标通道 sustain/sostenuto 与历史；其他通道保留 | 强制停止机械/shock，不产生释放声 | 若聚合变为 up，无声硬清 owner 尾音 | 无 |
| CC121 Reset All Controllers | 清除逐通道 sustain/sostenuto；若聚合曾 down，按一次 release 机械事件 | 机械 up 一次；不触发 shock | 尾音进入正常 drain；清空 lastCC | 无 |
| CC123 All Notes Off | 只处理音符；不把 owner 踏板状态当作音符状态 | 不额外触发机械/shock | owner 尾音保留；不强制 silence | 无 |
| 宿主 Panic / `allNotesOff(0,false)` | 清除所有通道控制状态 | 强制停止机械/shock | 强制清空 owner 尾音；之后可重新触发 | 无 |
| 声部偷取/重用 | 不触碰 owner 控制状态 | 不触发 | owner 尾音不被偷声部切断 | 只影响 Voice slot；voice 0 不再是踏板保留位 |
| 采样率重新准备 / 合成器重建 | owner prepare/控制状态硬重置 | 停止机械/shock | 清空尾音并重新按新采样率准备 | 由 rebuild 正常重建 |
| 音源切换（Piano/Sine/VST） | owner 只属于 Piano endpoint | Piano 不渲染时 owner 不输出；切回时重置 | 不泄漏到 Sine/VST | 无 |

CC121 的 release 机械事件是本方案明确的行为收紧（当前它只清 per-voice 标志，不触发机械声）；若要求严格保留旧行为，改为“仅更新控制状态、不触发机械声”，但会留下控制状态与 owner 聚合不一致的风险。CC123 与 CC120/Panic 必须分开：前者保留 owner 踏板尾音，后者强制清空。

#### 聚合规则

机械踏板声采用 **整琴单一机械动作**，不按通道复制：

```text
anySustainDown = OR(channelSustain[1..16])
mechanicalDownEdge = (!previousAnySustainDown && anySustainDown)
mechanicalUpEdge   = (previousAnySustainDown && !anySustainDown)
```

- 逐通道仍保留 `sustainPedalByChannel[16]`，音符身份和 sustain/sostenuto 语义不变。
- `BuiltinSynthesiser` 保存 `lastSustainControllerValues[16]`；owner 不保存逐通道历史。触发力度用造成聚合边沿的通道原始 CC 差值 `abs(new-old)/127`，并保留现有 `jlimit(0.2, 1.0)` 语义。
- `midiChannel <= 0` 的 CC64 是广播操作：按目标通道集合更新后比较聚合前后状态；不逐通道制造重复声学动作。不得按“同一采样最多一个边沿”去重：不同真实 MIDI 事件即使采样位置相同，也严格按事件顺序分别处理；同采样 down/up/down 可以产生多个实际聚合边沿。
- 具体序列 1：ch1 down 100 → ch2 down 127 → ch1 63（ch2 仍 down）→ ch2 up 0。当前 voice 0 在 ch1=63 时触发 release；聚合规则不触发 release，直到 ch2 up 才触发一次 up。
- 具体序列 2：ch1 down 127 → ch2 down 100 → ch1 up 0 → ch1 down 127（ch2 仍 down）→ ch2 up 0。结束时 **ch1 仍 down**，不得产生聚合 up；随后 ch1 up 才产生唯一一次 release。当前实现会在 ch1 up 时提前 release、在 ch1 再 down 时重复 down，聚合规则必须修正。
- 重复同值、阈值内变化均不产生边沿；`PedalTransient` 的原始 down/up 触发次数不因 `handleController` 的多条路径重复。

#### 音频路由与尾音

- `PianoPedalAcoustics` 是 `BuiltinSynthesiser` 的成员，位于内置 Piano endpoint 内；`renderVoices` 每段调用一次 owner，再把结果加入同一 Piano buffer。不得在每个 Voice 内重复混入。
- 机械声直接进入 L/R 的 `0.7071` 等权混合（与当前 `pedalSound * 0.7071` 一致），随后经过 owner 自己的 `PianoLidAcoustics` 和 `PerspectiveProcessor`；owner 的空间状态独立于任何 voice 的起音、停止和偷取。
- A 的 shock-only 池使用稳定的独立增益：沿用旧 idle 分支的左右权重 **0.40/0.60**，不随 noteSounding 或 voice0 状态切换，不乘 `noteOutputGain`，也不添加 resonance/duplex 对踏板输出的新调制。机械声仍为 `0.7071` 左右混入。当前有音符/无音符分支并不一致；本变更明确不是逐样本等价迁移，差异列为受控试听项。
- 当前 `PedalTransient` 在 voice 0 的两个分支都是 `*0.7071`；`noteOutputGain` 不作用于踏板声。owner 保持这两点。
- owner 输出在 `AudioEngine`/离线两条路径的 `pianoGain` 之前，在公共 Room/Master/limiter 之前；固定双层时只属于 Piano 层，不进入 VST3/Sine。
- 尾音由显式状态推进：`mechanical.isActive() || shockTail.isShockActive() || tailDrainSamples > 0 || shockDrainSamples > 0`。机械/激励结束后分别检查 shock 极点排空、`LidAcoustics::delaySize`（1024，覆盖最大 507 抽头）和 `PerspectiveProcessor::tailDrainSamples()`，取有界预算并逐样本递减；结束后才清空 owner 状态。禁止用“当前输出为零”判停，也不能因短时振荡过零休眠；固定 1024 单独不足以证明完整尾音。
- CC120/Panic/采样率重准备/合成器重建强制 `mechanical.reset()`、`shockTail.reset()`、`lid.reset()`、`perspective.reset()` 和尾音计数清零；CC123 不强制清空；CC121 进入正常 drain。
- `PianoLidAcoustics` 从 `PianoSynthVoice.h` 的嵌套 `LidAcoustics` 原样抽取（含 `LidPosition`），不复制算法、不修改 141/344/507 抽头；`PerspectiveProcessor.h` 直接复用。

#### R5 实际文件与符号

- 新增 `source/Audio/PianoPedalAcoustics.h`：`PianoPedalAcoustics`，成员 `mechanical`、`shockTail`、`lid`、`perspective`、`tailDrainSamples`、`shockDrainSamples`；方法 `prepare`、`setTuning`、`setPedalNoiseLevel`、`setLidPosition`、`setSoundPerspective`、`onSustainEdge`、`reset`、`renderNextBlock`、`isActive`。逐通道 `lastSustainControllerValues[16]` 与聚合真值仍只在 `BuiltinSynthesiser`；owner 不保存逐通道历史。
- 新增或抽取 `source/Audio/PianoLidAcoustics.h`：`devpiano::audio::LidPosition` 与 `LidAcoustics`（保留原字段、公式、抽头）。
- `source/Audio/BuiltinSynthesiser.h/.cpp`：增加 `PianoPedalAcoustics pedalAcoustics;`；扩展 `handleSustainPedal` 的 raw CC 入口；在 `renderVoices` 中唯一混入；在 `setCurrentPlaybackSampleRate`、`allNotesOff`、`clearVoices` 处理 owner；增加参数 setter/查询。
- `source/Audio/PianoSynthVoice.h`：删除 `pedalTransient` 与 `isPedalTransientActive`/voice-0 pedal 输出分支；`sympatheticPool.triggerShock` 从 voice 路径移除（A）；`isVoiceActive`/`isPlayingChannel`/`renderNextBlock` 删除 voice-0 踏板条件；`LidAcoustics` 改为 `PianoLidAcoustics`。
- `source/Recording/RenderPipeline.cpp`、`source/Audio/AudioEngine.cpp`：把 `pedalNoiseLevel`、`pianoResonance`、lid position、perspective 从 snapshot/初始化同步到 owner；`AudioEngine::rebuildSynth` 与离线初始化重置 owner。
- `source/tests/PedalAcousticsTest.cpp`、`PianoSynthVoiceTest.cpp`、`MechanicalAcousticsTest.cpp`、`PianoTuningAndDuplexTest.cpp`、`LidAcousticsInteractionTest.cpp`：迁移 owner API、更新 `LidPosition` 符号、删除 voice-0-only 断言。
- `CMakeLists.txt`：登记新头文件；`PianoMechanicalTransients.h` 保持 R3。

#### R5 验收分类

**必须保持字节/行为**：

- 8 个无踏板单音、`seq-steal-nine-notes`、`seq-repeat-same-key`、`seq-panic-reuse`、`integration-60-v70-room` 的 WAV/raw 与 R0 逐字节一致；无踏板路径不应经过 owner 输出。
- 音符身份、NoteOn/Off 采样位置、逐通道 sustain/sostenuto、ADSR/damper、主弦/整音/note-driven sympathetic、文件与参数格式不变。
- `seq-sustain-new-voice` 的 **事件位置和状态语义**保持，但不要求旧音频哈希；它必须被新 owner 测试覆盖。

**允许且预期改变**：

- 踏板机械声不再占用 voice 0、不被偷声部切断；`isVoiceActive`/`isPlayingChannel` 不再因踏板空闲保留 voice 0。
- `seq-sustain-new-voice` 的机械声/尾音音频、pedal shock 的新 owner 分支、CC121 的 release 语义、CC120/Panic 强制清空和采样率重准备后的控制历史。
- owner 的声像/琴盖状态与 voice 0 音符解耦；旧 `PedalAcoustics` voice-0 exclusivity 断言应替换为 owner 行为断言。

**需要受控试听**：

- 独立 `PianoLidAcoustics`/Perspective 路径与旧 voice 0 路径的机械声空间差异。
- shock 从共享 voice 0 池拆出后，在“voice 0 同时有音符”、有音符/无音符切换和踏板释放时的可闻差异。
- CC121 release、CC120/Panic 后重踩、两通道聚合边沿的机械声次数与力度。

判断方法：R5 先在 Windows Debug 渲染固定短序列（R4 的 4 个诊断序列 + `seq-sustain-new-voice`），比较 owner 激活边沿、触发次数、尾音区间、有限性、峰值/能量窗口和事件位置；无踏板 R0 用例继续字节比较。实时/离线使用同一 `BuiltinSynthesiser` owner，分别通过 `AudioEngine` 和 `WavFileExporter` 的相同事件序列比较，不复制实现。尾音检查必须观察 owner `isActive`/状态机而非零样本；Panic 后再触发一次 down/up。回调零分配/零锁通过检查 owner 固定数组、无堆分配路径和现有实时测试观察，不以断言数或内存大小作为成功标准。

#### 待用户裁定

1. **A vs B**：推荐 A（完整解除，需一个 shock-only 12 模态状态和可控差异）；若要求不新增该状态、优先保持当前 shock 样本行为，选 B（只移机械声，voice 0 shock 保留）。
2. **CC121/CC123**：推荐 CC121 清控制器并发送一次 release 机械边沿，CC123 保留踏板尾音和控制历史；若要保持旧行为，CC121 可只清状态不触发，但这会留下聚合状态与 owner 不一致。
3. **pedalDown 时新 NoteOn 的 note-driven `sympatheticPool`**：当前 `BuiltinSynthesiser::noteOn` 只调用 `setSustainPedalDown`，不把踏板状态传给 note-driven 池；R5 不擅自修复。若 A 的 owner shock 独立，这不阻塞机械声迁移；若要同时修正音符驱动交感，需单独授权最小关联变更。
4. **shock 分支增益**：R5 已实现固定 `0.40/0.60` 独立增益，不随 noteSounding/voice0 切换；旧 active 分支的 `wet/(1-duplexMix)` 调制不再用于踏板输出。该项差异进入受控试听。

**R4 证据**：`G:\source\projects\devpiano-evidence\r4-pedal-ownership-20261022\`；包含 `start-state.txt`、`workflow-audit.txt`、`codegraph-explore.txt`、`R4PedalOwnershipDiagnostics.cpp`、`pre-migration-run/`、`diagnostics-summary.md`、Windows Debug/CTest 日志和 `final-state.txt`。R4 未重跑 R3 的 13 case；R3 证据继续作为无踏板和迁移前基线。

### 0.11 R5 整琴踏板声学实现记录（2026-10-22，实现及客观验证完成；用户试听通过）

**状态**：采用修订方案 A 的生产实现已完成。未提交、未推送、未恢复 Phase 40+。用户已试听 R6-F1 修正对照，未听出明显差异或突然截断；R5 所有权迁移试听通过，不扩大为整个音源音色认证。

**实现边界**：

- 新增 `source/Audio/PianoPedalAcoustics.h`：整琴 owner，包含 `PedalTransient`、shock-only `SympatheticResonancePool`、独立 `LidAcoustics`、`PerspectiveProcessor`、显式尾音状态。
- 新增 `source/Audio/PianoLidAcoustics.h`：从 `PianoSynthVoice.h` 抽取 `LidPosition`/`LidAcoustics`，固定抽头、低通和公式未改。
- `BuiltinSynthesiser` 增加 `pedalAcousticsEnabled` 冷启用标志、owner 参数 setter、逐通道 CC64 历史与单入口聚合。Sine 合成器的该标志保持 false。
- `PianoSynthVoice` 删除 `pedalTransient`、`pianoPedalNoiseLevel`、`isPedalTransientActive`/`isSympatheticShockActive`、voice-0 pedal/render/shock 分支和 first-voice CC64 分发条件；note-driven `sympatheticPool` 保留每 Voice 一份。
- 原始 CC64 历史只在 `BuiltinSynthesiser` 保存；owner 只接收真实聚合边沿。同采样事件严格按 MIDI 事件顺序消费，不做采样级去重；广播在更新目标通道集合后比较一次聚合前后状态。
- 机械声固定 `0.7071` 左右混入；shock 固定 `0.40/0.60`，不乘 `noteOutputGain`、不随 noteSounding/voice0 切换、不添加 resonance/duplex 调制。
- 尾音使用显式 `mechanical/shock` 状态、shock 极点排空预算、`LidAcoustics::delaySize` 和 `PerspectiveProcessor::tailDrainSamples()`，不是单一固定 1024 样本或输出零检测。阈值按 -80 dB 相对残差（幅度 `1e-4`）和实际极点计算：shock 预算上限 `1<<20` 样本，Perspective 上限 8192 样本，lid 延迟端点 1024 样本；单位均为当前采样率的样本，正常结束按预算递减，强制 reset 独立清零。

**验证结果**：

- R0 13 case 最终 `run-r5-final`：8 个无踏板单音、`seq-steal-nine-notes`、`seq-repeat-same-key`、`seq-panic-reuse`、`integration-60-v70-room` 的 WAV/raw/帧数全部与 R0 一致；`seq-sustain-new-voice` 在 R0 探针的 `pedalNoiseLevel=0` 条件下也字节一致。
- 生命周期诊断按新拓扑预期改变：`new-voice-after-cc64` 从 index 1 变为 0；`steal-voice0-target` 不再指向 voice 0，旧诊断不作为目标行为继续使用。音频没有因此改变。
- 新增 `PedalAcoustics` 行为测试覆盖空闲踏板不占 Voice、8 音、偷声部不截断 owner、新 NoteOn 不重复触发、双通道聚合、同采样 down/up/down、CC120/121/123、Panic、Sine 隔离、mono 有限性和 44.1/96 kHz 短尾音/重准备；最终全部通过。
- 实时回调 IAT 观察器（复用 Phase 39 探针，Windows Debug）在实际 `AudioEngine::getNextAudioBlock` 回调中覆盖 pedal down/up、8 复音、参数/Panic/偷声部/44.1/96 重准备等场景：allocations=0、critical_sections=0、trig=0，输出 finite。
- `Export: piano snapshot onset and release parity` 已加入 CC64 down/up 与 `pedalNoiseLevel=0.8`，实时/离线 parity 通过（29 assertions）。
- 试听材料：`r5-idle-down.wav`、`r5-idle-down-up.wav`、`r5-held-note-pedal.wav`、`r5-tail-steal.wav`、`r5-multi-reset.wav`、`r5-panic-repedal.wav`，由 production `WavFileExporter` 渲染；R3/旧 voice 直接参考在 `listening-before/`，同输入与电平，无归一化、EQ 或压缩处理。`r5-idle-down` 与旧 idle 参考接近逐样本一致；其他差异是本次 C 类变更/空间路径/偷声部语义的预期试听项。
- 证据目录：`G:\source\projects\devpiano-evidence\r5-pedal-owner-20261022\`。

**流程**：仅 Windows Debug 构建/测试；WSL 用于编辑和 `./scripts/dev.sh format`，`./scripts/dev.sh tidy --all` 最终通过（exit 0）；未执行 WSL 全量构建、Release、物理声卡或商业 VST3。

**试听结论**：R6-F1 修正材料已由用户试听；held-note、tail-steal 及相关 owner 空间路径未听出明显差异或突然截断。R5 所有权迁移试听通过；不据此宣称整个音源音色已经认证。

### 0.12 R6 踏板所有权迁移的定向收口（2026-10-22，客观收口完成；用户试听通过）

**状态**：R6 只补齐 R5 证据、审查和必要的临时探针；未修改生产 DSP、控制契约、声部数、音色参数或 R5 文件格式。未提交、未推送、未恢复 Phase 40+。用户已试听 R6-F1 修正对照，未听出明显差异或截断；本段 R5/R6 所有权迁移试听通过，不扩大为整个音源音色认证。

**起始与隔离**：R6 起始 HEAD 仍为 `09a07b6bf090675f319981e75c8057ea3a022601`，工作树保留 R0–R5 全部改动；起始状态和 R5 源码快照在 `r6-pedal-owner-closure-20261022/start-state.txt`、`r5-start-source-snapshot/`。R3 旧基线由 `r3-mechanical-transients-20261022/source-after` 的精确源码覆盖到基础 HEAD，在独立目录/独立 Windows mirror 重建；R3 归档源码哈希全部匹配，未 stash、reset 或覆盖 R5 工作树。

**Perspective 核查**：R5 只新增只读 `PerspectiveProcessor::tailDrainSamples()`，按实际 `airFilterAlpha` 和 `1e-4` 相对残差计算，未改变字段、系数、平滑、`prepare/reset/processStereo` 或默认行为；消费者为普通 Voice、R5 owner 和标识符转换。`PerspectiveProcessor` 定向回归 10668 通过。

**owner 调度核查**：`processNextBlock` 零长度只处理事件，不调用 `renderVoices`；正长度片段按事件顺序切分，`renderVoices` 先渲染全部 Voice 再调用一次 owner。float 的实时/离线 Piano 路径一致，mono/stereo 在同一 Piano buffer 混合；Sine owner 保持禁用；端点切换、重准备和硬重置走既有 `allNotesOff`，不恢复旧尾音。`AudioBuffer<double>` 重载不调用 owner，且无生产消费者；作为边界报告，未为此新增功能。

**逐通道状态**：`sustainPedalByChannel` 是唯一逐通道 CC64 演奏真值；`lastSustainControllerValues` 只保存原始差值历史，均只在 `BuiltinSynthesiser` 写入。`applySustainPedal` 是唯一入口；`anySustainDown` 由前者派生，没有 owner 侧第二份真值。CC120/121/123、Panic、重准备的通道作用域见 `r6-audit.md` 和已有 `PedalAcousticsTest`。

**尾音依据**：shock 预算从实际二极模态极点计算，-80 dB 相对残差 `1e-4`，`1<<20` 上限；44.1/48/96 kHz 的实测初始值为 23751/25849/51711 样本，上限未截断理论预算。Perspective 实测预算为 12/13/21 样本，8192 上限未截断；琴盖延迟固定 1024。shock 计时结束后才启动 shock 极点排空；上游未结束时下游 tail 预算不递减；owner 实测在 79/86/169 个 512 样本块后 `active=0, tail=0, shock=0`。这是相对状态衰减依据，不是绝对输出幅度上界。

**生产路径对照**：R3/R5 均使用同一 `R6ProductionListeningProbe.cpp`、production `exportTakeAsWavFile`、48 kHz stereo 24-bit、block 512、2.0 s 尾音、相同事件和参数。`production-diff-summary.md` 和 `production-comparison.json` 记录；该临时 probe 的 `note` 布尔把 intended NoteOff 写成了 controllerEvent，导致 held note 实际为 CC60=0、tail-steal 无 NoteOff，不能作为自然 release 尾音证据。
- idle down/up、held note、Panic 的最大 PCM 差异分别约 `1.2e-7`/`5.0e-6`、`8.3e-7`、`5.2e-6`，无可成立的唯一归因；
- tail-steal 最大 PCM 差异 `6.79e-4`、RMS `1.29e-4`，峰值 R3 `0.483501` / R5 `0.483679`，差异持续约 0.401–3.000 s，最大在 2.114 s；结构上与 R3 voice0 踏板路径参与偷声部、R5 owner 独立不同，但无法从该探针把 owner、lid/Perspective 与分配差完全分离；
- 所有 WAV 无削波、末端为零；所有辅助 raw 无 NaN/Inf、末端为零。注意：R6 生产对照仍使用 90000/72000 的 take 长度，R3/R5 都在 1.875/1.500 s 被既有 exporter end-of-take panic 截断，随后 2.0 s 为静音；该对照只在该边界前有效，不能作为自然尾音试听证据，详见 `r5-truncation-diagnosis-20261022/README.md`。

**R0 与回归**：R0 13 case 继续使用 R5 的生产 WAV/raw 字节对照（`r5-comparison-to-r0.json`）；其中 `pedalNoiseLevel=0`，`seq-sustain-new-voice` 的 CC64 不产生 owner 输出。新拓扑生命周期为 `new-voice-after-cc64` voice 0、`steal-voice0-target` voice 7，不恢复旧 voice0 语义。R5 专项 `PedalAcoustics` 576 通过；R6 callback 扩展的 240 个释放后 512 样本块观察 allocations=0、critical_sections=0、trig=0、finite=1；完整 R6 callback 测试 79 通过。`PerspectiveProcessor` 10668 通过。

**流程**：R6 使用 Windows MSVC Debug 构建/CTest；最终格式检查通过，最终 `tidy --all` 退出 0（`r6-tidy-final.log`）。先前的增量 tidy 曾在 600 s 工具超时中止，该次不计为通过；最终全量检查在生产源码与 R5 起始一致后重新完成。

**证据**：`G:\source\projects\devpiano-evidence\r6-pedal-owner-closure-20261022\`；包含 `start-state.txt`、`source-verification.json`、`r6-audit.md`、`r6-coverage.md`、`r3-production/`、`r5-production/`、`production-diff-summary.md`、`production-comparison.json`、`r6-realtime-callback.log`、`r6-tail-budget2.log`、`r3-candidate-verification.txt` 和临时探针源码。

**未验证**：没有 Release、物理声卡、商业 VST3、全 88 键/多力度/多采样率矩阵；R3/R5 生产对照只在 48 kHz 短序列，44.1/96 只覆盖尾音和重准备；旧 direct-voice harness 不再用于严格生产差异归因。用户试听仍需判断 held pedal、tail-steal、Panic 后重踩以及 owner 独立空间路径的音色。

### 0.13 R6-F1 试听材料 take-end 截断修正（2026-10-22，用户试听通过）

**状态**：试听材料已修正，用户已重新试听 R3/R5 修正对照，未听出明显差异或突然截断；R5 所有权迁移试听验收通过，不扩大为整个音源音色认证。未修改生产 DSP、`PianoPedalAcoustics`、声部生命周期、尾音预算、音色/增益参数、控制契约或 `WavFileExporter` 的 end-of-take panic 政策；未提交、推送、发布或恢复 Phase 40+。

**发现两个独立探针缺陷**：原 R5/R6 试听 probe 的 `EventSpec` 只有布尔 `note`，`note=false` 被写成 controller；同一转换也在 `R6LongTakeDiagProbe.cpp` 中，受影响的仅为这些临时试听/诊断 probe；held 的 intended NoteOff（38400）实际是 CC60=0，tail-steal 没有任何 NoteOff。原 probe 同时使用 90000/72000 take 长度，exporter 在这些采样点插入 end-of-take panic，导致 1.875/1.500 s 硬切和随后 2.0 s 静音。两者都不是 R5 owner 尾音耗尽；旧 direct harness 也不是严格生产参考。

**修正材料**：R6-F1 使用显式 `noteOn`/`noteOff`/`controller`；tail-steal 在 52800 为全部 9 个音符补 NoteOff，保留原九音偷声部与 57600 的 CC64 释放；take 长度 960000（20.0 s），exporter 2.0 s tail。R3/R5 使用同一探针、事件、参数和构建配置，严格生产 `exportTakeAsWavFile`、48 kHz stereo 24-bit、block 512、无 Room、无 EQ/压缩/淡入淡出/归一化。

**验证**：held 在 `abs(PCM)>1e-6` 筛选阈值下最后帧为 76395（1.592 s），但 frame 90000 两声道均为 `-1/8388608 = -1.1920929e-7`（24-bit 单 LSB，低于阈值）；精确非零末帧为 111615，之后为零。tail-steal 在相同阈值下最后帧为 80732（1.682 s），精确非零末帧为 109041。最后 1 s peak/RMS 为零；旧 1.875/1.500 s 边界不再强制归零。辅助 no-panic 状态诊断显示 R5 owner 在 1.5/1.875 s 已 inactive，而 1/8 个 note voice 仍 active；20 s 时 voice/owner 均 inactive。修正文件与指标在 `r6-f1-listening-correction-20261022/`，使用 `LISTENING-GUIDE.md` 索引。

**限制**：20 s take 仍有极低残留的旧长 take 诊断不能作为自然尾音唯一证明；本修正文件本身在 20 s 之后由 exporter tail 收口，但正确释放后的末帧已远早于该边界。R3/R5 的既有差异（held max PCM `8.34e-7`；tail max `6.79e-4`）保留，不归一化、不掩盖。用户已试听 R6-F1 修正材料；不扩大为整个音源或所有播放条件的音色认证。

### 0.14 R7 新分配声部 note-driven sustain 初始化（2026-10-22，修复完成）

**状态**：R7 已修复“延音踏板已按下后新分配/复用钢琴声部时，`sympatheticPool.pedalDown` 未继承所属通道状态”的问题。R5/R6 所有权迁移试听验收已按 R6-F1 修正材料收口；本轮只改起音同步，不修改共鸣公式、参数默认值、声部数、增益、尾音预算、owner 混音或 exporter 政策。未提交、推送、发布或恢复 Phase 40+。

**根因与顺序**：`BuiltinSynthesiser::noteOn` 通过 `startVoice` 调用 `PianoSynthVoice::startNote`；`startNote` 只执行 `sympatheticPool.updateCoefficients/noteOnKey`，不写踏板 gate。原实现随后才执行 `voice->setSustainPedalDown(channelTruth)`，因此 JUCE Voice 标志正确，但 note-driven pool 仍是旧值/默认 false。已有活动声部通过 `updateVoiceSustainForChannel -> controllerMoved(64)` 更新；新声部没有这一步。复用或偷声部时，池状态还可能残留上一通道的状态。

**最小修复**：在 `PianoSynthVoice` 增加只接受布尔值的 `setSympatheticPedalDown`，由 `controllerMoved(64)` 复用；`BuiltinSynthesiser::noteOn` 在 `startVoice` 完成后、首次渲染前，从 `sustainPedalByChannel[midiChannel]` 同步 base sustain 标志和 voice pool。该同步不伪造 MIDI 事件、不调用 owner、不触发 mechanical/shock，也不新增第二份通道真值。

**修复前/后定向证据**：`r7-before-fix.log` 在新增测试上 606 passed / 5 failed；`r7-after-fix.log` 611 passed / 0 failed。覆盖新踏板后起音、已有声部 down/up、通道隔离及单声部跨踏板状态复用/偷声部。默认 CTest 1/1 通过；`Export: piano snapshot onset and release parity` 在加入“CC64 down 后再起音”的事件后 29 assertions 通过。Sine 既有隔离测试通过。

**音频对照**：以 R6-F1 修正的显式事件和 20 s take/2 s exporter tail 为同一生产路径，比较 fix 前/后 R5 输出。held-note 完全字节一致（fix 不影响已在踏板前起音的声部）。tail-steal 的差异仅发生在约 0.1–1.21 s：max PCM `0.002973`, RMS `0.000145`，峰值 fix 前/后 `0.483679/0.482965`；末尾均无截断、最后 1 s 为零。该差异被归类为允许的 note-driven 共鸣初始化修正，不宣称必然可听。

**证据**：`G:\source\projects\devpiano-evidence\r7-note-sustain-initialization-20261022\`；包含修复前/后测试日志、源码/测试补丁、`r5-before-fix/`、`r5-after-fix/`、事件表、格式/静态与构建日志、manifest。**限制**：未执行 Release、WSL 全量构建、物理声卡、商业 VST3；只验证声道/复用/实时离线路径与两个受控音频场景。

### 0.15 R8 主弦合成核心边界设计（2026-10-22，仅设计，未实施）

**状态**：R8 只读取当前源码、R0/R5/R7 日志和现有测试，形成 R9 可执行的等价提取方案；未修改生产 DSP、测试、参数、踏板 owner、声部数、增益、尾音政策或 exporter。R7 修复结论保留；用户尚未反馈 R7 音频时，不代写试听结论。Phase 40+ 继续暂缓。

#### R7 生命周期诊断的比较对象

| 记录 | 拓扑 | `new-voice-after-cc64` | `steal-voice0-target` |
|---|---|---|---|
| `r0-baseline-20261022/run1` | R0，R5 前 | voice 1, note 60, pool 0 | voice 0, note 67, pool 1 |
| `r5-pedal-owner-20261022/run-r5-final` | R5 owner 迁移，R7 前 | voice 0, note 60, pool 0 | voice 7, note 67, pool 0 |
| `r7.../r0-before` | R5 生产，R7 修复前 | voice 0, note 60, pool 0 | voice 7, note 67, pool 0 |
| `r7.../r0-after` | R7 修复后 | voice 0, note 60, pool 1 | voice 7, note 67, pool 1 |

R0 的 voice0 承载全局踏板瞬态，7 个音符占用其余声部，第 8 个音符偷 voice0；R5 把 owner 移出 voice0 后，同一序列的第 8 个音符使用空闲 voice7，因此历史 case 名称 `steal-voice0-target` 已不表示实际偷 voice0。R7 汇报的 `0 -> 1` 是**同一 R5 拓扑下 R7 修复前/后**，不是 R0 -> R7；R0 该目标本来已是 pool 1。未为本次澄清重跑音频。

#### 当前主弦实际承担的工作

- `startNote` 内联完成分音频率/准入/首分音降级、权重、同音窗功率、`unisonScale`/L1 归一化、1/2/3 弦 Magic Circle 初始化、初始电平/双阶段衰减/una-corda/Bloom。
- `renderNextBlock` 逐样本推进 Pitch Glide、Spatial Diffusion、部分音振荡器、Mid-Side 展开、Bloom 与两级电平衰减，产生一个 stereo 主弦和。
- `allPartialsSilent` 只按分音电平阈值判断主弦静音，同时受 hammer/damper 瞬态影响；它不是整个 Voice 的停止条件。
- 主弦结果在 `contact/envelope + click + damperThump` 之前输出，之后依次进入软饱和、Duplex、音板、交感、琴盖、视角和输出增益。主弦没有第二组独立输出，也不拥有 ADSR、机械瞬态、Duplex、交感池或整琴生命周期。
- 当前没有耦合弦、数字波导、桥板反馈或逐弦独立衰减；同音 1/2/3 弦共享分音电平，Pitch Glide 是全局倍率，Bloom 是逐分音状态。

#### 归属决定：B

选择 **B**：提取一个完整的 `PianoMainString`，拥有分音配置/安装、分音与 modulation 状态、逐样本递推和主弦静音；Voice 只保留音符身份、JUCE 生命周期、`triggerCounter`、ADSR/接触/机械瞬态、Duplex、交感、音板/琴盖/视角、增益和输出调度。

A（分音状态/递推在组件，准入/归一化留在 Voice）会使 Voice 继续持有可写 `partials`/活动数或逐分音 setter，起音与渲染职责割裂，故不选。B 只新增一个启动标量结果 `unisonScale`（供既有 hammer transient 消费）和一个 stereo 样本结果；不暴露可写数组、Voice 指针、回调或可变上下文。

#### 接口与等价约束

详细草案见 `G:\source\projects\devpiano-evidence\r8-main-string-boundary\interface-draft.h.txt`；字段/调用归属见同目录 `field-ownership.md`，R9 清单见 `r9-implementation-checklist.md`。

- `MainStringStart` 只包含实际消费的 `PianoNoteParams`/`PianoNoteSetup` 子集；不是 `PianoNoteSetup` 的完整拷贝，按值传入、不保存引用。
- `prepare` 只设采样率；`start` 只安装分音并返回 `unisonScale`；`triggerSpatialDiffusion`/`triggerPitchGlide` 保持在现有起音调用位置；`nextSample` 单样本、无分配；`isSilent` 只检查分音电平；`reset` 只清主弦活动/调制状态。
- 保留浮点类型、准入 `< ceiling`、首分音 fallback、循环/弦顺序、Bloom/衰减门限、`1e-4f` 阈值、Pitch Glide 先于 Spatial Diffusion 的读取顺序，以及 Voice 现有的 contact/envelope/click/damper 顺序。
- 不把每采样逻辑改成块逻辑，不新增平滑/缓存失效/首次使用分配。

#### R9 范围与验证出口

- 新增 `source/Audio/PianoMainString.h`；修改 `source/Audio/PianoSynthVoice.h`、`source/tests/PianoSynthVoiceTest.cpp` 和 `CMakeLists.txt`；header-only，不新增 `.cpp` 或独立 target，但按项目惯例把 header 登记到 app/tests 两个 `target_sources`。
- 测试符号更新：`PianoSynthVoice::maxPartials` -> `PianoMainString::maxPartials`；`voice->partials`/`numActivePartials` -> 私有组件（测试 friend）；`SpatialDiffusionEngine` 类型改到组件。`hammerContactEngine` 仍在 Voice，不动。
- 以 **R7 最终源码**为直接前基线：13 个 R0 case 的 WAV/raw 必须与 R7 输出一致；`seq-sustain-new-voice` 保留 R7 的 `pool=1`，不得恢复 R0 错误结果；复用 R6-F1 held/tail、R7 parity、现有 targeted tests，不建全键盘/全采样率矩阵。
- 若任一 WAV/raw 差异、分配/锁、循环顺序或测试 golden 需要改动，先停止并保留证据，不修改期望掩盖。
- R8 未运行构建、CTest、音频矩阵或全量 tidy，因为无生产修改。

**复杂度变化**：Voice 删除分音数组、准入/归一化、振荡器循环和相关公开字段，获得一个可独立测试的主弦组件；总 DSP 步骤和状态数量不变，代码只是移动到独立所有权。新增一个 header 与测试 friend 读取，不属于算法简化。当前没有阻止等价提取的具体问题；测试访问方式（friend）和 modulation trigger 保留位置是实施选择，不是行为变更。

### 0.16 R9 主弦组件提取（2026-10-23，实现与客观验证完成）

**状态**：按 R8 方案 B 完成 header-only `PianoMainString` 提取；未修改算法、参数默认值、声部数、音色、增益、尾音政策、R5 owner、R7 sustain 同步或 exporter。未提交、推送、发布或恢复 Phase 40+。

**起始与结果**：起始 R7 最终生产源码（`PianoSynthVoice.h` `a6a3fc41...`、`BuiltinSynthesiser.cpp` `244506ca...`）保持不变为直接基线。R9 新增 `source/Audio/PianoMainString.h`；修改 `PianoSynthVoice.h`、`PianoSynthVoiceTest.cpp`、`CMakeLists.txt`；源码 before/after、补丁和哈希在 `G:\source\projects\devpiano-evidence\r9-main-string-extraction-20261023\`。

**接口与所有权**：`PianoMainString` 拥有 `Partial`/固定 20 槽、活动分音数、准入/ceiling fallback、权重与同音窗归一化、Magic Circle 初始化/递推、双阶段衰减、Bloom、Pitch Glide、Spatial Diffusion、逐样本 stereo 合成及只按分音电平的 `isSilent`。`start` 返回既有 `unisonScale`；`prepare` 只存采样率；`start` 不提前触发 modulation；`triggerSpatialDiffusion`/`triggerPitchGlide` 保持在原起音位置；`nextSample` 只输出主弦 stereo；`reset` 只清主弦活动/调制状态。Voice 保留音符身份、`triggerCounter`、`PianoNoteSetup`、ADSR、hammer contact/transient、Duplex、交感、Body/Lid/Perspective、增益和整个 Voice 停止条件。

**生命周期**：旧 `startNote` 的准入/安装区段映射到 `mainString.start`；旧 `spatialDiffusionEngine.trigger`/`pitchGlideEngine.trigger` 映射到对应 trigger 方法；旧 `renderNextBlock` 的 glide/diffusion/分音循环映射到 `nextSample`，contact/envelope/click/damper 顺序不变；`stopNote(false)` 与 all-partials-silent 路径的旧空间/glide reset 合并为 `mainString.reset()`。`allPartialsSilent` 仍先查 hammer/damper，再调用 `mainString.isSilent`，没有把主弦静音误当整个 Voice 停止。详细映射见证据目录 `lifecycle-mapping.md`。

**测试访问**：只给 `PianoSynthVoiceTest` 明确 friend 访问 Voice 的私有 `mainString` 与组件的私有 `partials`/活动数/weight helpers/`SpatialDiffusionEngine`；`maxPartials` 与 diffusion 类型保持私有，没有生产可写访问器、private 宏替换或测试 setter。现有测试的输入/断言只改类型/状态访问位置。

**客观等价**：最终生产状态使用归档 R7 探针、同一 48 kHz/24-bit/block 512/2 s tail 协议渲染。13 个 R0 case 的 WAV SHA-256、raw f32 SHA-256、帧数和元数据全部与 R7 final 一致；`cases.csv`、lifecycle 诊断和 F1 `events.csv` 一致；lifecycle 保持 `new-voice-after-cc64,0,60,1`、`steal-voice0-target,7,67,1`，未恢复 R0 错误。R6-F1 held/tail 的 WAV 哈希和元数据也逐项一致。没有使用容差、归一化或更新 golden。

**验证**：Windows MSVC Debug app/tests 构建 exit 0；最终 targeted tests 共 54385 assertions / 0 failed（PianoSynthVoice 10205、tuning/duplex 32840、PedalAcoustics 611、Export parity 29、Perspective 10668、DamperRelease 13、Mechanical 19）；默认 CTest 1/1 passed（37.13 s）；`format --check` 通过；全量 `tidy --all` 退出 0（145/145，785.62 s，`r9-tidy-all.log`）。R0/F1 临时探针已归档到证据 `probes/` 并从 `source/tests`/CMake 移除；最终构建/CTest 在清理后的树执行。

**复杂度**：Voice 移出分音数组、准入/归一化、振荡器循环、modulation 状态和分音静音；总 DSP 步骤没有减少，只是获得独立所有权。仍存在：25 字段 `MainStringStart` 的起音组装、`unisonScale` 跨边界返回值、测试 friend、以及主弦与 hammer contact/ADSR 的固定顺序耦合。未新增动态分配、锁、首次使用初始化、块级处理或通用 DSP 图。

**证据**：`G:\source\projects\devpiano-evidence\r9-main-string-extraction-20261023\`（README、source-before/after、production patch、lifecycle mapping、13-case/F1 音频、comparison JSON、build/CTest/targeted/format/tidy 日志、probes、manifest）。

### 0.17 R10 接口与封装收口（2026-10-23，实现与验证完成）

**状态**：R10 只处理 `PianoSynthVoice` 的真实外部可写状态与职责边界；未修改 DSP 公式、参数默认值、增益、声部数量、处理顺序、控制契约、尾音策略、R5 owner、R7 延音同步或 exporter。未提交、推送、发布或恢复 Phase 40+。

**独立差异**：起始 R9 最终 `PianoSynthVoice.h` `ce1499aa...`；结束 `3961e5a3...`。R10 只改该头文件：在内部 helper/字段前插入 `private:`，并增加 `PianoTuningAndDuplexTest`、`PedalAcousticsTest` 两个明确的既有测试 friend；没有重排数据成员，没有改 DSP 调用或测试断言。完整 diff 在证据 `r10-interface-closure-20261023/r10-interface.patch`。

**消费者结论**：JUCE override、生产配置 setter 和真实只读诊断保留；所有 Voice 实例 DSP/状态字段转为私有。生产代码没有直接写 Voice 内部 DSP 字段：`AudioEngine`/`RenderPipeline` 使用语义 setter，`BuiltinSynthesiser` 使用 `setSoftPedalDown`/`setSympatheticPedalDown`。仅测试保留 friend：`PianoSynthVoiceTest`（`mainString`/partial/hammerContact 观察）、`PianoTuningAndDuplexTest`（`partialFrequency`）、`PedalAcousticsTest`（`sympatheticPool.pedalDown/isShockActive`）。没有新增 getter/setter、可写引用或 private 宏。

**生命周期**：`PianoMainString`、hammer contact/transient、damper、per-voice sympathetic、Duplex、Body、Lid、Perspective 和整琴 `PianoPedalAcoustics` 的准备/触发/release/hard reset/活跃判断已记录在证据 `lifecycle-table.md`。`mainString.isSilent()` 仍只是分音静音，整个 Voice 仍由 ADSR/damper/Duplex 条件决定；整琴 owner 仍有独立 `isActive()`。发现一条既有生命周期不对称：踏板按下时 all-silent 分支不 reset per-voice note pool，但 Voice 仍可被 ADSR/damper/Duplex 条件清除，之后该 pool 无渲染路径；本轮只记录，未修改冻结尾音策略，也未将其写成已复现音色缺陷。

**验证**：Windows MSVC Debug 构建 exit 0；默认 CTest 1/1 passed（36.76 s）；`format --check` 与 `git diff --check` 通过；全量 `tidy --all` 退出 0（145/145，`r10-tidy-all-final.log`）。R10 未改生产行为，复用 R9 的 13-case/F1 音频、生命周期和 parity 证据，不重复音频矩阵。R9 音频来源核对见 `r9-provenance.md`；R9 文档措辞无需更正。

**剩余边界**：R10 没有确认的接口阻塞缺陷；`MainStringStart` 字段、`unisonScale`、contact/envelope 顺序、两个 modulation trigger、测试 friend、header-only、per-Voice 组件和独立 pedal owner 作为已接受依赖保留。死 helper 清理、setter 去重、Room/硬件/Release/VST3 验证和全局声学拓扑为独立后续任务。R11 建议只做最终验收，不启动新重构。

### 0.18 R11 最终验收与交付收口（2026-10-23）

**结论**：本轮物理钢琴音源结构重构与已批准行为修正**验收完成**。没有确认的阻塞项。R0–R10 全部改动保留，未提交、未推送、未发布；Phase 40+ 继续暂停，Phase 39 未决事项保持原状态。

**累积交付分类**：结构提取与封装包含 R1 `PianoVoiceResonance`、R2 `PianoNoteSetup`、R3 机械瞬态/来源/查表辅助、R9 `PianoMainString`、R10 Voice 访问控制；CMake 登记 9 个新头文件（app/tests 各一次）；测试调整覆盖 pedal/voice、main string、tuning、export、felt、mechanical、perspective；文档更新架构、功能契约和本任务入口。没有混入无关业务改动、旧执行路径、兼容开关、临时探针或调试输出。

**已批准行为变化（不是纯重构）**：R5 将机械踏板、shock-only 尾音、独立 Lid/Perspective 和聚合边沿所有权移入 `BuiltinSynthesiser::PianoPedalAcoustics`，不再占用或依赖 voice0；owner 固定机械 `0.7071`、shock `0.40/0.60`，不乘 `noteOutputGain`，不被偷声部切断。R7 在 `startVoice` 完成后、首次渲染前，把通道 CC64 真值同步到新声部 `sympatheticPool.pedalDown`；不重触发 owner、不伪造 MIDI、不新增第二真值。R1/R2/R3/R9/R10 为结构提取/封装，R9 以 R7 最终源码为直接音频基线。

**目标核对**：

| 目标 | 最终证据 |
|---|---|
| 起音参数计算与 Voice 可变状态分离 | `PianoNoteSetup.h` 纯计算；Voice 只在其 startNote 调用并安装 |
| 机械瞬态独立 | `PianoMechanicalTransients.h`；Voice 组合 hammer/damper，owner 组合 pedal |
| 整琴踏板声学不再依赖 voice0 | `BuiltinSynthesiser` 持有 `PianoPedalAcoustics`；Voice 无 `pedalTransient`/voice0 分支；R5 证据 |
| 通道延音在新声部首次渲染前同步 | `BuiltinSynthesiser.cpp` noteOn 的 `setSympatheticPedalDown`；R7 606/5→611/0 与 parity |
| 主弦配置/状态/递推/静音同一组件 | `PianoMainString.h`；`PianoSynthVoice` 只调用 |
| Voice 保留音符生命周期和组件调度 | start/stop/render、ADSR、contact、Duplex、Body、Lid、Perspective 仍在 Voice |
| 内部状态不对生产调用者开放写入 | R10 private 字段；生产仅语义 setter/override |
| 提取保留直接前基线音频行为 | R9 13 case WAV/raw/metadata + F1 held/tail 与 R7 final 逐项一致 |
| 行为修正有明确前后测试与范围 | R5/R6-F1/R7 证据；R7 定向失败→通过 |

**证据关系**：R0 是初始行为记录；R5 有意改变整琴踏板所有权及相关行为；R6-F1 替代有事件编码错误/end-of-take 截断的旧试听材料，用户已签收 R5/R6 修正材料试听；R7 是客观延音初始化修复（未替用户补写 R7 主观试听）；R9 以 R7 最终源码为直接音频基线；R10 仅访问控制变化，复用 R9 音频。旧 probe 的无效结论不作为验收依据。

**Per-voice sympathetic 生命周期记录**：与基准 HEAD 对照，`allPartialsSilent` 中 `!pedalDown` 才 reset per-voice pool、最终 Voice clear 不含 per-voice pool 条件，均不是本轮新增；R1/R9 只是搬移，R7 只同步踏板 gate，R10 只是封装。未验证内容：Voice 停止时池残余、复用时的残余状态、可听影响；本轮不新增跨 Voice 共鸣尾音验收要求，也不宣称已确认截尾或绝无问题。详见 `r11-final-acceptance` 的 lifecycle 记录。

**验证复用**：当前生产源码、测试和 CMake 与 R10 已验证状态哈希一致（`PianoSynthVoice.h` `3961e5a3...`、`PianoMainString.h` `3199fe2f...`、`PianoSynthVoiceTest.cpp` `c0ca389c...`、`CMakeLists.txt` `a61ac6f1...`），故复用 R10 Windows Debug 构建/CTest/tidy（1/1，36.76 s；tidy 145/145）与 R9 13-case/F1 音频/生命周期证据；本轮只做文档/diff/恢复核查，不重跑构建或音频矩阵。

**可恢复交付快照**：`G:\source\projects\devpiano-evidence\r11-final-acceptance-20261023\`，含 baseline、完整 tracked patch、全部 9 个 untracked 新头文件、文件清单、SHA-256 manifest、恢复说明、历史证据索引和隔离恢复校验。`HEAD` 本身不能标识最终未提交成果，恢复以 manifest + patch + untracked 树为准。

**限制**：未认证 Release 性能、物理声卡 deadline、商业 VST3、全键盘/全力度/全采样率矩阵或整个音源主观声学正确性；R5/R6 试听签收范围仍是所有权迁移，不等于全音源认证。

