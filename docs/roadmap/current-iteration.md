# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

**Phase 12（内置物理建模钢琴音源，SineSynth → PianoSynth）进行中**——把当前内置 fallback 正弦合成器逐步替换为自主拥有、纯 C++、零/极低采样依赖的物理建模钢琴音源。整体路线见 [`roadmap.md`](roadmap.md) Phase 12；本文件为详细排期与子任务。**Phase 12-1/12-2/12-3/12-4 已完成（2026-08-18）**，剩余 Windows 侧手工听觉回归与默认音色决策，详见下文。

代码质量审计（[`AUDIT-001`](../audit/AUDIT-001-code-quality-audit-2026-08-16.md)，2026-08-16）修复 **AUDIT Phase A–H 已全部完成**（2026-08-17）：56 项未处理全关闭，16 项已暂缓复核关闭 2 项（QUAL-019/PERF-002），剩余 14 项维持；断言总数 754 → 2921 全绿。逐项完成记录已归档至 [`../archive/audit-001-code-quality-fix-phases.md`](../archive/audit-001-code-quality-fix-phases.md)。

---

## Phase 12：音源重构 + 谐波钢琴 v1 [进行中]

### 背景与目标

当前未加载插件时的内置音源是纯正弦波（`SimpleSineVoice`），声音是"电脑 beep"。目标：以 JUCE 标准机制（`juce::Synthesiser` + `SynthesiserVoice` 派生，不建自定义抽象层、不建 `source/Audio/Builtin/` 目录）分阶段替换为谐波钢琴音色，使无插件时也发出可辨识的钢琴声音。Phase 12 完成"重构 + 谐波 v1"，Phase 13/14 递进到 inharmonicity 与 wave guide（见下）。

### 前置事实（已核实代码）

| 事实 | 位置 | 影响 |
|---|---|---|
| ~~sine 实现两份逐字重复：`SimpleSineVoice`（实时）与 `OfflineSineVoice`（离线导出）~~ | ~~AudioEngine.cpp:31-99 / WavFileExporter.cpp:18-101~~ | **已消除（12-1）**：合并为共享 `SineSynthVoice`，实时/导出同源 |
| 三处调用点：实时渲染、WAV 离线导出、插件离线失败回退 | AudioEngine.cpp:217 / WavFileExporter.cpp:197 / RecordingSessionController.cpp:202 | 全部改接共享 voice |
| 参数接线已有同构模式：`PerformanceSettingsView` → `applyPerformanceSettingsToAudioEngine` + `buildWavExportOptions` | MainComponent.cpp:1059-1063 / ExportFlowSupport.cpp:58 | 新音色参数沿用，含默认值向后兼容 |
| fallback 音频路径无确定性测试（`MidiMessageCollector` wall-clock 时序） | AudioEngineTest.cpp 顶部注释 | 新音色必须带确定性夹具（直接驱动 voice） |

### 子任务排期

**Phase 12-1：共享 SineSynthVoice 重构（零行为变化） [已完成，2026-08-18]**

落地提交：`26772e9`（refactor: extract shared SineSynthVoice for realtime and offline paths，净 -71 行）。

- [x] 新建 `source/Audio/SineSynthVoice.h`（header-only）：`SineSynthSound` + `SineSynthVoice` 从 AudioEngine 私有嵌套提出为独立类（继承 `juce::SynthesiserVoice`），行为逐行不变。
- [x] `AudioEngine::rebuildSynth` / `updateAdsrOnVoices` 改用共享类（`synth.addVoice(new SineSynthVoice())` ×8 + `dynamic_cast<SineSynthVoice*>` 设 ADSR）；`AudioEngine.h` 删除嵌套类前向声明。
- [x] `WavFileExporter` 删除 `OfflineSineVoice`/`OfflineSineSound`/`initialiseOfflineSynth` 逐字副本（-84 行），改注册共享类（保留 `fallbackVoiceCount = 8` 语义）。
- [x] 三处调用点全部改接：实时渲染（AudioEngine.cpp:188）、WAV 离线导出（WavFileExporter.cpp:77）、插件离线失败回退（WavExportTask → `exportTakeAsWavFile` 走同一共享实现）；`grep SimpleSine|OfflineSine` 无残余定义。
- [x] 验证通过：`wsl-build`（经 `dev.sh test` 构建 0 新警告）/ `test`（ctest 1/1 passed，断言全绿，行为不变）/ `format --check`（归零）/ `win-build`（MSVC 构建成功，2026-08-18）。

**Phase 12-2：Harmonic PianoVoice v1（Level 1 音色） [已完成，2026-08-18]**

落地：`source/Audio/PianoSynthVoice.h`（header-only，继承 `juce::SynthesiserVoice`，与 SineSynthVoice 并存注册）。

- [x] 新建 `source/Audio/PianoSynthVoice`（继承 `SynthesiserVoice`），与 SineSynthVoice 并存注册；`AudioEngine::BuiltinSynthTone {sine, piano}` + `setBuiltinSynthTone` 可切换（`rebuildSynth` 按 tone 注册），初始默认仍用 sine（行为不变），后续阶段切默认。
- [x] 合成核心：基频 + 2~7 次谐波叠加，分音数与幅度按音区查表（`voiceRegions`：note <48 → 8 分音 / <72 → 6 / <96 → 4 / ≥96 → 3；幅度 1/n 递减），幅度归一避免 clip（`peakLevelAtFullVelocity = 0.28`）。
- [x] velocity 双映射：响度 `level = v^1.5`（弱奏更敏感，sqrt 实现避免 pow）+ 亮度（高次谐波增益随 v 线性提升，最高次 +60%）。
- [x] 分音独立衰减：decay 按音区查表（4.0 / 2.5 / 1.5 / 0.8 s，低音长高音短）+ 高次分音略快（`harmonicDecayFactor` 表）；attack/release 沿用 `setAdsrParameters` 接线（内部变换为 `{attack, 0.001, 1.0, release}` 作门控，decay/sustain 由分音衰减替代）。
- [x] CPU 预算：每 voice ≤ 8 次 `std::sin`（分音数上限 8）+ 每 note 一次 `sqrt`/`exp`；`renderNextBlock` 无堆分配、无锁（`std::array<Partial, 8>` 固定缓冲）；分音衰减至 `1e-4`（-80 dB）后 voice 自清防低音长尾占位。
- [ ] 听觉回归（Windows 侧手工）：与 sine 对比击弦瞬间 / 衰减 / 力度分层——待用户手工验证（音色已可经切换接口启用）。
- 验证：`test`（52 类 2968 断言全绿）/ `format --check`（归零）/ `win-build`（MSVC 成功，2026-08-18）。
- 范围说明：导出路径（`WavExportOptions`）接入 Piano 音色归 12-3（与参数扩展一起做）；12-2 保持实时默认 sine = 导出 sine 的一致性。

**Phase 12-3：参数化与 UI 接线 [已完成，2026-08-18]**

- [x] `SettingsModel::PerformanceSettingsView` + 平铺字段扩展：`builtinTone`（模型层枚举 `BuiltinTone {sine, piano}`）+ `pianoBrightness` / `pianoHammerHardness` / `pianoResonance`（0..1，默认 0.5，与 v1 基准行为一致）。`SettingsStore` 4 个新 key 读写 + 钳制（tone 仅 0|1、参数 0..1）；旧序列化数据缺失字段回退默认（仿 DOC-006，`getDoubleValue(key, model.xxx)`），新增 legacy 文件专项用例。
- [x] `AudioEngine::setPianoParameters`（jlimit + `updatePianoParametersOnVoices`）+ `MainComponent::applyPerformanceSettingsToAudioEngine` 透传（含 `setBuiltinSynthTone` 映射）。**线程安全修正**：核实 JUCE `Synthesiser` 内部锁——`processNextBlock`（音频渲染）与 `clearVoices`/`addVoice` 共用同一 lock，消息线程 `rebuildSynth` 安全，音频线程仅短暂阻塞；更新 12-2 的保守注释。
- [x] `WavExportOptions` + `buildWavExportOptions` 扩展同参数；`WavFileExporter::initialiseOfflineSynth` 按 `builtinTone` 注册 Sine/Piano voice 并设置参数——**实时/离线音色一致达成**（12-2 遗留的导出路径接入一并完成）。
- [x] `PianoSynthVoice` 参数映射：brightness 亮度基准（b=0.5 时与 12-2 完全一致）、hammerHardness 高次起始增益（0.5 中性 ±20%）、resonance 衰减时间缩放（×0.7~1.3）。
- [x] UI 控件（沿用 ADSR 旋钮模式）：`LayoutModel::makeControlsPanelTree` 新增 `piano-row`（`tone-combo` + 3 个 DevKnob）；`MainComponent` wireKnob 接线（0..1/0.01 步进，% 显示）+ `tone-combo` 单选；`MainComponentJiveAccessors` 新访问器（getBuiltinToneFromUi / getPiano* / setControlsPianoValues）；`AppState`/`AppStateBuilder` 同步。
- 验证：`test`（52 类 2992 断言全绿，新增 24 断言：SettingsStore round-trip + legacy 回退 / ExportFlow 透传 / StyleCatalog 控件 id / PianoSynthVoice 参数映射）/ `format --check`（归零）/ `win-build`（MSVC 成功，2026-08-18）。
- 待办：Windows 侧手工听觉回归（Phase 12 收尾，验证 sine vs piano 音色与 3 参数旋钮的实际效果）。

**Phase 12-4：确定性音色测试 [已完成，2026-08-18]**

落地：`source/tests/PianoSynthVoiceTest.cpp`（`DevPiano/Engine` 类别，10 个 beginTest，37 断言，与 12-2 并行推进）。

- [x] 新建 `source/tests/PianoSynthVoiceTest.cpp`：夹具经 `juce::Synthesiser` 驱动 voice（`noteOn` 事件 → `renderNextBlock`，绕过 `MidiMessageCollector` 时序）——**必须走 Synthesiser**：`currentlyPlayingSound` 仅由 `Synthesiser::startVoice` 设置，直接调 `startNote` 会让 voice 处于非活跃态。
- [x] 断言：单点 DFT（Hann 窗）验证基频≈261.63 Hz 且低音区 2~7 次 / 中音区 2~5 次谐波存在、velocity 0.2 vs 0.9 响度单调递增、noteOff 后 tail 衰减收敛至零、自然衰减自清（treble 8 s）、`stopNote(false)` 立即静音、100 块长渲染有限无 NaN、`allNotesOff` 生效、`AudioEngine` 音色切换接口（默认 sine → piano → sine，`prepareToPlay` 不崩溃）。
- [x] 现有断言保持全绿：默认套件 52 类 2968 断言（2928 + 37 新 + 3 切换）。
- [x] 注意：TestRunner 默认按类别白名单 `{DevPiano/Core, Recording, Engine, UI}` 筛选——TEST-012 记录的 "DevPiano/Audio" 前缀与白名单不一致，本测试沿用现有惯例用 `DevPiano/Engine`（与 AudioEngineTest 一致），否则默认套件不会执行它。

**排期参考**：12-1 / 12-2 / 12-3 / 12-4 已完成（2026-08-18）。Phase 12 剩余：Windows 侧手工听觉回归 + 默认音色决策（sine → piano 切换时点）。

### 验收标准

- ~~实时演奏与 WAV 导出使用同一 voice 类，音色一致。~~ **已达成（12-2 + 12-3）**：实时与导出共用 `SineSynthVoice`/`PianoSynthVoice`，`WavExportOptions` 携带音色与参数，`buildWavExportOptions` 从同一 `PerformanceSettingsView` 派生。
- ~~确定性测试覆盖基频 / 谐波 / 力度单调 / tail 收敛。~~ **已达成（12-4）**：`PianoSynthVoiceTest` 单点 DFT 验证基频 + 谐波 2~7、velocity 单调、tail 收敛、自清、allNotesOff、参数映射。
- 听觉对比明显优于 sine（击弦瞬间、衰减、力度分层可辨）——**待 Windows 侧手工验证**。
- 三闸门全绿：`wsl-build` 0 warning / `test` 全绿 / `format --check` 归零 / `win-build` 通过——已达成（2026-08-18）。

### 验证命令

```bash
./scripts/dev.sh wsl-build --configure-only
./scripts/dev.sh test
./scripts/dev.sh format --check
./scripts/dev.sh win-build
```

---

## Phase 13：Stiff-String Inharmonic Piano v2 [规划中]

> 概要排期，详细计划在 Phase 12 完成后展开。Level 2：在 v1 谐波基础上加入琴弦刚性特征。

- inharmonicity：`fₙ = n·f₀·√(1 + B·n²)`，B 按音区查表（低音区 B 大、高音区 B 小），替换 v1 的整数倍谐波频率。
- 分音衰减速率建模：高次分音衰减更快（独立 decay 曲线，非单一 ADSR）。
- 简单 body 共鸣：一阶/二阶谐振器组或预计算滤波系数（仍纯解析加法，无需延迟线）。
- 排期参考：约 1–2 轮迭代，含调参与听感回归。

## Phase 14：Digital Waveguide Piano v3 [规划中]

> 概要排期，研究性阶段。Level 3：进入真正物理建模层（数字波导）。

- 每 voice 数字波导：延迟线 + 分数延迟（`DelayA` 级） + loss 滤波器 + dispersion 近似 + hammer excitation 模型。
- 参考：bBpiano（PolyForm Internal Use，**仅读思想**，不复制代码）；STK（MIT，delay/filter 类可作零件库，注意其乐器依赖 rawwaves 采样）。
- **决策门**：听感收益 vs CPU 预算 vs 代码复杂度，产物是否成为默认音色；若收益不足则维持 Phase 13 音色为默认。
- 排期参考：2+ 轮迭代，含 CPU 基准与决策评审。

---

## 验证命令

代码修改后优先执行：

```bash
./scripts/dev.sh wsl-build --configure-only
./scripts/dev.sh win-build
```

涉及环境或路径问题时执行：

```bash
./scripts/dev.sh self-check
```

## 相关文档

- 项目路线图：[`roadmap.md`](roadmap.md)（Phase 12 整体路线、许可证核实、外部参考）
- AUDIT-001 修复阶段归档：[`../archive/audit-001-code-quality-fix-phases.md`](../archive/audit-001-code-quality-fix-phases.md)
- 审计报告：[`../audit/AUDIT-001-code-quality-audit-2026-08-16.md`](../audit/AUDIT-001-code-quality-audit-2026-08-16.md)
- 架构概览：[`../reference/architecture.md`](../reference/architecture.md)
- Phase 11 归档：[`../archive/phase11-declarative-ui-jive.md`](../archive/phase11-declarative-ui-jive.md)
