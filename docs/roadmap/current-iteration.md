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

### 开源参考与算法选型矩阵

#### 候选开源项目深度分析矩阵

**类别 A：解析加法 / 模态合成（Modal Synthesis）—— Phase 13 的首选与直接对标**

| 项目 | 核心原理 | 许可证 | 优点 | 局限 / 风险 | 对 devpiano 的复用价值 |
|---|---|---|---|---|---|
| **[pichenettes/eurorack](https://github.com/pichenettes/eurorack)**<br>*(Mutable Instruments: Rings, Elements, Plaits)* | 模态二阶谐振器组（Resonator Bank）+ 冲击激励塑形 | **MIT** | 工业级 DSP 优化、数值极稳定、纯 C++ 无分配、音色极具表现力 | 针对嵌入式 ARM 优化，需移植到现代 C++20 | **★★★★★ (极高)**<br>Phase 13-2（衰减建模）与 13-3（琴体共鸣滤波）的**工业级实现范本** |
| **[electro-smith/DaisySP](https://github.com/electro-smith/DaisySP)** | 模块化 C++ DSP 库（含 `StringVoice`, `ModalVoice`, `Resonator`） | **MIT** | 纯 C++ 类库结构清晰、现代 CMake 支持、开箱即用 | 泛用型模态模型，未针对钢琴 88 键专门调参 | **★★★★☆ (很高)**<br>可直接参考其 `ModalVoice` / `Resonator` 的 Direct Form II 滤波实现 |
| **[GareBear99/Instrudio](https://github.com/GareBear99/Instrudio)** | 基于 JUCE DSP 的物理建模乐器（小提琴、钢琴、竖琴） | **无明确 LICENSE**<br>*(默认保留所有权利)* | 原生 JUCE DSP 架构，含 inharmonicity chorus 与琴体 EQ 建模 | 无 LICENSE 文件，**不能复制任何代码** | **★★★☆☆ (中等)**<br>仅可学习其 JUCE DSP 链路与琴体共鸣 EQ 曲线设计思路 |

**类别 B：数字波导与色散建模（Digital Waveguide & Dispersion）—— Phase 14 的物理模型基石**

| 项目 | 核心原理 | 许可证 | 优点 | 局限 / 风险 | 对 devpiano 的复用价值 |
|---|---|---|---|---|---|
| **[thestk/stk](https://github.com/thestk/stk)**<br>*(Synthesis ToolKit)* | 经典波导合成（DelayA, BiQuad, ModalBar, BandedWG） | **MIT-style** | 物理建模领域常青树，延迟线与滤波零件库非常成熟 | 部分旧 C++ 风格（90 年代写法），需要现代化重构 | **★★★★★ (极高)**<br>Phase 14 构建分数延迟线（`DelayA`）与波导循环的核心零件参考 |
| **[grame-cncm/faustlibraries](https://github.com/grame-cncm/faustlibraries)**<br>*(physmodels.lib, misceffects.lib)* | `piano_dispersion_filter`（Balázs Bank 级联全通色散滤波） | **LGPL-2.1 / BSD** | 数学推导最严谨，全通滤波器拟合琴弦刚度色散的黄金标准 | Faust DSL 语法，需转译其数学公式为 C++ | **★★★★☆ (很高)**<br>学习钢琴高次分音失谐与全通色散滤波设计的**最佳数学范本** |

#### 针对 devpiano 的分阶段开发与参考选型建议

1. **Phase 13（Stiff-String Inharmonic Piano v2）落地推进建议**：
   - **非谐性分音频率计算（Inharmonicity）**：
     - **参考源**：Julius O. Smith (PASP) 与 Faust `physmodels.lib` 的刚度参数模型。
     - **落地方式**：在 `PianoSynthVoice::startNote` 中使用 $f_n = n \cdot f_0 \sqrt{1 + B \cdot n^2}$。刚度系数 $B$ 按音区查表（低音区 $\approx 4 \times 10^{-4}$，高音区 $\approx 1 \times 10^{-5}$）。
     - **CPU 成本**：零新增（仅在按键瞬间计算一次步进增量）。
   - **频率相关分音衰减建模（Decay Modeling）**：
     - **参考源**：Mutable Instruments (Rings/Elements) 的模态能量耗散模型。
     - **落地方式**：将现有固定 8 档表替换为连续函数 $\tau_n = \tau_{\text{base}} / (1 + c \cdot (n - 1))$，使高次谐波在击弦后快速衰减，自然过渡至基频主导。
   - **琴体共鸣滤波（Body Resonator）**：
     - **参考源**：`DaisySP::Resonator` 与 `Mutable Instruments` 的 Direct Form II 二阶带通/谐振器。
     - **落地方式**：在每 voice 输出挂载 2~3 个二阶谐振器，固定极点在单位圆内（$r < 1$），模拟钢琴音板 100~300 Hz 的共振峰，计算量仅增加 8~12 次乘加/采样。

2. **Phase 14（Digital Waveguide Piano v3）真正物理建模参考**：
   - **波导与分数延迟**：参考 **STK (`thestk/stk`)** 中的 `DelayA` / `BiQuad`，编写 header-only、现代 C++20 的环形缓冲波导类。
   - **琴弦色散（Dispersion Allpass Chain）**：参考 **Balázs Bank 论文** 与 **Faust `piano_dispersion_filter`**，用 1~4 阶一阶全通滤波器级联逼近色散效应。
   - **击弦激励（Hammer Excitation）**：采用**换向波导（Commuted Waveguide）**思想，将击弦脉冲经非线性滤波整形后注入波导。

3. **合规与开发纪律**：
   - 第一推荐参考库（代码级）：`pichenettes/eurorack` (MIT)、`electro-smith/DaisySP` (MIT)、`thestk/stk` (MIT-style)。
   - 第一推荐算法与理论源（思想级）：Julius O. Smith (JOS) - 《Physical Audio Signal Processing (PASP)》、Faust Libraries (`misceffects.lib`)。
   - 对 `Instrudio`（无授权）坚决不复制代码，仅在设计层面借鉴。

### 子任务排期

**Phase 13-1：Inharmonicity（刚性琴弦分音失谐偏移）**

- [ ] 引入 JOS PASP 刚性琴弦公式：在 `PianoSynthVoice::startNote` 中计算分音频率 $f_n = n \cdot f_0 \sqrt{1 + B \cdot n^2}$（$n \ge 1$ 为分音序号），步进计算为 `increment = (2π / sampleRate) * f_n`；相位累积维持 `double`。
- [ ] 刚度系数 $B$ 按音区查表（低音弦刚度大、高音小）：
  - region 0（note <48）：$B \approx 4 \times 10^{-4}$（低音区 $n=7$ 时频率偏移 $\approx +1.0\%$，$n=2 \approx +0.06\%$）
  - region 1（48–71）：$B \approx 1 \times 10^{-4}$
  - region 2（72–95）：$B \approx 3 \times 10^{-5}$
  - region 3（≥96）：$B \approx 1 \times 10^{-5}$
- [ ] **核心声学收益**：各分音失去公共整数倍周期 $\to$ 波形不再单调循环，低音区产生真实的"泛音失谐拍频（Beats）"，彻底消除整数倍加法合成的电子蜂鸣感。
- [ ] 参数化决策：推荐刚度 $B$ 按音区硬编码查表（真实钢琴刚度由物理弦径与张力决定，无需向用户暴露额外旋钮，保持 UI 紧凑）。
- [ ] 计算纪律：仅在 `startNote` 按键瞬间计算一次步进增量，**音频渲染线程逐采样 CPU 零新增**。

**Phase 13-2：模态分音衰减速率建模（Modal Decay Modeling）**

- [ ] 借鉴 Mutable Instruments（Rings/Elements）模态能量耗散模型：替换 v1 的 8 档离散表，引入连续分音时间常数模型 $\tau_n = \tau_{\text{base}}(\text{note}) / (1.0 + c_{\text{region}} \cdot (n - 1))$。
- [ ] 确定性衰减因子：在 `startNote` 预计算每分音每采样衰减系数 $\text{decayPerSample}_n = \exp(-1.0 / (\text{sampleRate} \cdot \tau_n))$，低音区 $c_{\text{region}} \approx 0.35$，高音区 $c_{\text{region}} \approx 0.15$。
- [ ] 旋钮映射保持：`pianoResonance` 旋钮继续作用于 $\tau_{\text{base}}$（$\times 0.7 \sim 1.3$），`pianoBrightness` 作用于高次谐波初始幅度与高频衰减斜率。
- [ ] **声学收益**：高次分音在击弦后数十至数百毫秒内快速耗散，音色平滑过渡为基频与低次分音主导的纯净尾音。

**Phase 13-3：简单琴体共鸣滤波（Body Resonator Bank）**

- [ ] 借鉴 `DaisySP::Resonator` 与 Mutable Instruments 标准拓扑：构建轻量 Direct Form II 二阶带通/谐振器组（2~3 个并联峰）。
- [ ] 音板共振频率配置：模拟真实钢琴音板的主共鸣峰（如 $f_{c1} \approx 110\text{ Hz}, Q_1 \approx 6$；$f_{c2} \approx 220\text{ Hz}, Q_2 \approx 5$；$f_{c3} \approx 360\text{ Hz}, Q_3 \approx 4$）。
- [ ] 状态与内存纪律：`std::array<ResonatorState, 3>` 静态作为 `PianoSynthVoice` 私有成员，系数在 `prepareToPlay` 预计算；`renderNextBlock` 中纯直接计算（每 sample 增加 $\le 12$ 次乘加），**零堆分配、无锁**。
- [ ] 信号混合：采用 Wet/Dry 混合策略（`output = (1 - wet) * raw + wet * filtered`，默认 $\text{wet} \approx 0.25$），避免过度滤波染色引起动态压缩。
- [ ] 数值稳定性保证：极点严格约束在单位圆内（$r = \exp(-\pi \cdot \text{bandwidth} / \text{sampleRate}) < 1.0$），长时渲染无发散。

**Phase 13-4：参数化与 UI 适配（向后兼容）**

- [ ] 维持 Phase 12-3 确立的 3 旋钮布局（Brightness / Hammer / Resonance）与 Tone 下拉，刚度 $B$ 与音板共鸣参数默认走物理精调查表，不强行增加第 4 旋钮。
- [ ] 若后续听感回归提出调参诉求，再评估是否扩展 `pianoStiffness`（沿用 `SettingsModel` $\to$ `AudioEngine` $\to$ `Voice` 链路）。

**Phase 13-5：确定性音色测试（与 13-1~3 并行）**

- [ ] 非谐性 DFT 量化：在 `PianoSynthVoiceTest.cpp` 中以单点 DFT 验证低音 note 36（C2）的第 5 分音精确落在 $5 \cdot f_0 \sqrt{1 + 25 B}$ 附近（频率容差 $\le \pm 0.2\%$）。
- [ ] 模态衰减对比断言：量化断言在 $t_0 = 0.1\text{ s}$ 与 $t_1 = 1.0\text{ s}$ 处，第 6 分音与基频的幅度比满足 $\text{Ratio}(t_1) < 0.5 \cdot \text{Ratio}(t_0)$。
- [ ] 琴体谐振器频响与稳定性测试：DFT 断言 100~300 Hz 区域存在预期共鸣增益；100 块长渲染输出有限、无 NaN/Inf、静音后各 voice 彻底清零。
- [ ] 保持全量 2993+ 断言全绿。

**Phase 13-6：听感回归与默认音色决策**

- [ ] Windows 侧手工听觉对比（v1 vs v2）：
  - **低音区（C2~C3）**：重点听非谐性拍频与音板共鸣厚度；
  - **中音区（C4）**：重点听击弦明亮度向基频衰减的过渡平滑度；
  - **高音区（C6+）**：重点听清脆度与短余韵；
  - **3 旋钮效果**：确认 Brightness/Hammer/Resonance 在 v2 引擎上依然协调有效。
- [ ] **阶段决策**：若 v2 听感已显著超越传统采样插件的易用性与表现力，可正式将 BuiltinTone 默认切换为 `Piano`。

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

- **核心技术**：每 voice 纯 C++ 无分配数字波导（环形缓冲 + 全通分数延迟 `DelayA`）+ 换向波导（Commuted Waveguide）击弦脉冲激励 + Balázs Bank 级联全通色散滤波（Allpass Dispersion Chain）+ 损耗滤波（Loss Filter）。
- **参考源**：
  - **STK (`thestk/stk`)**（MIT-style）：分数延迟线（`DelayA`）与二阶滤波（`BiQuad`）零件库架构。
  - **Faust (`misceffects.lib` / `piano_dispersion_filter`) & Balázs Bank 论文**（BSD/LGPL）：级联全通色散滤波器设计。
  - **Julius O. Smith (JOS) PASP**：换向波导合成（Commuted Waveguide Synthesis）理论。
- **决策门**：听感收益 vs CPU 预算（多复音波导计算量）vs 代码复杂度，产物是否取代 Phase 13 成为默认音色；若收益不足则维持 Phase 13 音色为默认。
- **排期参考**：2+ 轮迭代，含 CPU 基准与决策评审。

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
