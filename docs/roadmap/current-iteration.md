# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

**Phase 12（内置物理建模钢琴音源，SineSynth → PianoSynth）已完成（2026-08-18）**——Phase 12-1~12-4 全部落地 + Windows 侧手工听觉回归（Step 1~8）通过，谐波钢琴 v1 可经 Tone 切换启用（默认仍 sine）。当前进入 **Phase 13（Stiff-String Inharmonic Piano v2）**，详细计划见下文。

代码质量审计（[`AUDIT-001`](../audit/AUDIT-001-code-quality-audit-2026-08-16.md)，2026-08-16）修复 **AUDIT Phase A–H 已全部完成**（2026-08-17）：56 项未处理全关闭，16 项已暂缓复核关闭 2 项（QUAL-019/PERF-002），剩余 14 项维持；断言总数 754 → 2921 全绿。逐项完成记录已归档至 [`../archive/audit-001-code-quality-fix-phases.md`](../archive/audit-001-code-quality-fix-phases.md)。

---

## Phase 12：音源重构 + 谐波钢琴 v1 [已完成，2026-08-18]

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
- [x] 听觉回归（Windows 侧手工，2026-08-18）：Step 1~8 全部通过——sine 持续"哔" vs piano 带泛音 + 按住自然衰减；音区差异（低音厚长衰减 / 高音薄短）；Resonance 余韵 ±30% 明显、Brightness 明暗可闻、Hammer 微妙（±10% 设计如此）；实时/导出一致；参数持久化；Sine 回归正常。力度分层未验证（键盘 velocity 固定 1.0，需 MIDI 输入设备/文件）。
- 验证：`test`（默认套件 2928 断言全绿；PianoSynthVoice 确定性测试随后经 12-4 落地，见下）/ `format --check`（归零）/ `win-build`（MSVC 成功，2026-08-18）。
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

落地：`source/tests/PianoSynthVoiceTest.cpp`（`DevPiano/Engine` 类别，11 个 beginTest，43 断言——含 12-3 追加的参数映射用例；与 12-2 并行推进）。

- [x] 新建 `source/tests/PianoSynthVoiceTest.cpp`：夹具经 `juce::Synthesiser` 驱动 voice（`noteOn` 事件 → `renderNextBlock`，绕过 `MidiMessageCollector` 时序）——**必须走 Synthesiser**：`currentlyPlayingSound` 仅由 `Synthesiser::startVoice` 设置，直接调 `startNote` 会让 voice 处于非活跃态。
- [x] 断言：单点 DFT（Hann 窗）验证基频≈261.63 Hz 且低音区 2~7 次 / 中音区 2~5 次谐波存在、velocity 0.2 vs 0.9 响度单调递增、noteOff 后 tail 衰减收敛至零、自然衰减自清（treble 8 s）、`stopNote(false)` 立即静音、100 块长渲染有限无 NaN、`allNotesOff` 生效、`AudioEngine` 音色切换接口（默认 sine → piano → sine，`prepareToPlay` 不崩溃）。
- [x] 现有断言保持全绿：12-4 完成时默认套件 52 类 2968 断言（2928 + 37 新 + 3 切换）；12-3 追加参数映射用例后为 **2992**（2026-08-18 最终）。
- [x] 注意：TestRunner 默认按类别白名单 `{DevPiano/Core, Recording, Engine, UI}` 筛选——TEST-012 记录的 "DevPiano/Audio" 前缀与白名单不一致，本测试沿用现有惯例用 `DevPiano/Engine`（与 AudioEngineTest 一致），否则默认套件不会执行它。

**排期参考**：12-1 / 12-2 / 12-3 / 12-4 已完成（2026-08-18）；Windows 侧手工听觉回归通过（2026-08-18）。Phase 12 全部完成。默认音色决策（sine → piano 切换时点）与 Phase 13/14 路线衔接，见下。

### 验收标准

- ~~实时演奏与 WAV 导出使用同一 voice 类，音色一致。~~ **已达成（12-2 + 12-3）**：实时与导出共用 `SineSynthVoice`/`PianoSynthVoice`，`WavExportOptions` 携带音色与参数，`buildWavExportOptions` 从同一 `PerformanceSettingsView` 派生。
- ~~确定性测试覆盖基频 / 谐波 / 力度单调 / tail 收敛。~~ **已达成（12-4）**：`PianoSynthVoiceTest` 单点 DFT 验证基频 + 谐波 2~7、velocity 单调、tail 收敛、自清、allNotesOff、参数映射。
- ~~听觉对比明显优于 sine（击弦瞬间、衰减、力度分层可辨）~~ **已达成（2026-08-18 手工回归 Step 1~8）**：击弦瞬间（Piano 泛音起始）、自然衰减（按住消失 vs sine 持续）、音区分层均通过；力度分层因键盘 velocity 固定 1.0 未验证（需 MIDI 输入）。
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

### 背景与目标

Phase 12 的 v1 是**整数倍谐波**叠加：分音频率严格 `n·f₀`，相位有公共周期，波形循环重复，声音带"电子合成"感。真实钢琴琴弦有**刚度**：高频分音频率系统性偏高（inharmonicity），且高频能量耗散更快（高次分音衰减更短），琴体/音板有**共鸣**频响。v2 在 v1 基础上加入这三项，使内置音色向真实钢琴逼近，同时保持 Phase 12 确立的约束：纯解析加法、零采样依赖、无新抽象层、实时线程无堆分配无锁。

### 前置事实（已核实代码，Phase 12 落地后）

| 事实 | 位置 | 影响 |
|---|---|---|
| v1 分音频率为整数倍：`increment = 2π·f₀·(n+1)/sampleRate` | PianoSynthVoice.h `startNote` | 替换为 `fₙ = n·f₀·√(1+B·n²)`，B 按音区查表；仅 startNote 计算，**CPU 零新增** |
| v1 分音衰减为固定因子表 `harmonicDecayFactor`（1.0→0.43，8 档） | PianoSynthVoice.h | v2 改为与分音频率相关的连续模型（高次更快），仍 startNote 计算 |
| v1 输出为分音叠加直通：`value = Σ level·sin(phase)`，`output = value × envelope` | PianoSynthVoice.h `renderNextBlock` | body 共鸣滤波挂在此输出后（每 voice 谐振器链，状态在 voice 成员） |
| 参数链路已就绪：`PerformanceSettingsView`（builtinTone + 3 旋钮）→ `AudioEngine::setPianoParameters` → voice；导出路径同参数 | Phase 12-3 落地 | v2 若加新参数（如 stiffness）沿用同一链路，默认值向后兼容（DOC-006 模式） |
| 确定性测试夹具已就绪：Synthesiser 驱动 + 单点 DFT（Hann 窗） | PianoSynthVoiceTest.cpp | 可量化验证分音频率偏移 / 衰减速率 / 共鸣频响 |
| UI 空间：piano-row 已有 Tone 下拉 + 3 旋钮（Brightness/Hammer/Resonance），高度 72px | LayoutModel.cpp `makeControlsPanelTree` | 新增旋钮需评估布局空间；语言切换/样式同步已有 12-3 修复的先例 |

### 子任务排期

**Phase 13-1：Inharmonicity（分音频率偏移）**

- [ ] `startNote` 分音频率公式：`fₙ = n·f₀·√(1 + B·n²)`（n 从 1 起 = 基频，`increment` 按此计算；相位仍 double 累积）。
- [ ] B 按音区查表（真实钢琴量级，低音大 / 高音小）：
  - region 0（note <48）：B ≈ 4e-4（低音弦刚度效应最强）
  - region 1（48–71）：B ≈ 1e-4
  - region 2（72–95）：B ≈ 3e-5
  - region 3（≥96）：B ≈ 1e-5
  - 效果量级：低音区 n=7 时偏移 ≈ √(1+0.0004×49)−1 ≈ +1.0%；n=2 ≈ +0.06%（次谐波几乎不失谐）。
- [ ] **关键听感**：分音频率失去公共周期 → 波形不再循环重复，低音区出现真实的"泛音失谐"感（beats/拍频），这是 v2 相对 v1 最显著的提升点。
- [ ] 参数化决策（在 13-2 前定案）：B 按音区硬编码查表（**推荐**，无新旋钮，UI 空间留给后续）vs `pianoStiffness` 旋钮缩放 B（沿用 12-3 链路，需第 4 旋钮）。倾向硬编码——真实钢琴 B 由物理决定，用户可调性低；若用户要求再参数化。
- [ ] 验证：确定性测试量化偏移（见 13-5）；听感低音区"更真实"。

**Phase 13-2：分音衰减速率建模**

- [ ] 替换 v1 固定因子表为与分音频率相关的连续模型：`tau_n = tau_base(note) / (1 + c·(n−1))`，c 按音区（低音区 c 大 = 高次衰减相对更快）；或 `decayRate ∝ fₙ^k`（k≈0.5~1，物理上高频能量耗散快）。
- [ ] 效果：击弦瞬间的"明亮"（高次谐波强）在数百 ms 内衰减为"基频主导"（真实钢琴特征），比 v1 的 8 档表更平滑连续。
- [ ] 保持 `resonance` 旋钮语义不变（全局 decay 缩放 ×0.7~1.3 叠加在新模型之上），避免旋钮语义混乱。
- [ ] CPU：零新增（startNote 一次性计算）。

**Phase 13-3：简单 body 共鸣**

- [ ] 每 voice 输出（`value × envelope` 后）经过 2~3 个并联/级联**二阶谐振器**（Direct Form II，每谐振器 2 状态变量，系数 startNote 预计算）。
- [ ] 共振频率按音区（如低音 100~300 Hz 区间 2~3 个峰，模拟音板），Q ≈ 5~15，增益归一避免整体响度偏移。
- [ ] CPU 预算：每 voice 每 sample 2~3 谐振器 × 4 乘加 ≈ 8~12 乘加（叠加在 ≤8 次 sin 之上，仍远低于实时预算）；状态在 `std::array` 成员，无堆分配无锁。
- [ ] 备选降级（若谐振器调参/稳定性成本高）：静态分音增益塑形（按分音号 EQ 表，零新增状态）——计划中标注为决策点，优先尝试真谐振器。
- [ ] 稳定性：系数极点在单位圆内（`r < 1`），渲染前后输出有限（测试断言无 NaN/Inf）。

**Phase 13-4：参数化与 UI（可选，视 13-1 决策）**

- [ ] 若加 `pianoStiffness`：`PerformanceSettingsView` + `SettingsStore`（默认 0.5，旧数据回退）+ `AudioEngine::setPianoParameters` + `WavExportOptions` 同步 + `LayoutModel` 第 4 旋钮 + 语言切换/样式同步（沿用 12-3 修复先例）。
- [ ] 默认走硬编码路线时本子任务跳过。

**Phase 13-5：确定性音色测试（与 13-1~3 并行推进）**

- [ ] inharmonicity：单点 DFT 验证低音 note 36 的分音峰值频率偏移与 B 一致（如 5 次分音频率 ∈ [5·f₀·(1+0.4%), 5·f₀·(1+0.7%)]；基频 ≈ f₀ 无偏移）。
- [ ] 衰减速率：长时窗后高次分音相对基频的能量比下降（对比 t0 与 t1 窗口的 DFT 幅度比）。
- [ ] body 共鸣：对比滤波前后 DFT，共振峰附近频段增益可测（或等价断言）。
- [ ] 现有 2993 断言保持全绿；类别沿用 `DevPiano/Engine`；实时纪律断言（100 块渲染有限、无 NaN）。

**Phase 13-6：听感回归与默认音色决策**

- [ ] Windows 侧手工（仿 Phase 12 Step 流程）：v1（整数倍）vs v2（失谐 + 新衰减 + 共鸣）对比——低音区真实感、击弦瞬态、共鸣感；3 旋钮在 v2 上仍生效。
- [ ] **阶段决策**：v2 是否提升明显 → 是否将 Piano 切为默认音色（衔接 Phase 14 决策门；若 v2 已足够好可提前切默认）。

### 验收标准

- 分音频率失谐符合 `fₙ = n·f₀·√(1+B·n²)`，低音区高次分音偏移可量化（确定性测试）。
- 高次分音衰减快于低次（确定性测试可测）。
- body 共鸣频响有可测峰值；输出有限无 NaN。
- 实时纪律保持：每 voice ≤ 8 次 `std::sin` + ≤12 乘加（谐振器），无堆分配、无锁。
- 三闸门全绿：`wsl-build` 0 warning / `test` 全绿 / `format --check` 归零 / `win-build` 通过。
- 听感：v2 明显比 v1 更接近钢琴（低音真实感、瞬态自然、共鸣感）。

### 验证命令

```bash
./scripts/dev.sh wsl-build --configure-only
./scripts/dev.sh test
./scripts/dev.sh format --check
./scripts/dev.sh win-build
```

### 排期参考

13-1 + 13-2 约 1 轮；13-3 约 1 轮（含谐振器调参）；13-5 与 13-1~3 并行；13-4 视决策 0~1 轮；13-6 听感回归约 1 次。Phase 13 总计约 2 轮迭代 + Windows 侧手工回归。

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
