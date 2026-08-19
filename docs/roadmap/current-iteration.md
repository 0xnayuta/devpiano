# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

**Phase 13（Stiff-String Inharmonic Piano v2）已完成（2026-08-18）**；**Phase 14-A/B/C/D（Enhanced Modal Piano v3 物理建模四项增强）已全部落地（2026-08-18）**——Magic Circle 振荡器零 `std::sin`、分音上限 20/14/8/6、每分音双阶段衰减（快辐射期 + 慢尾音）、低中音微失谐双振荡器拍频、8 峰音板模态滤波组（75~950 Hz），三闸门全绿（46/46 测试、3101 断言）。当前进入 **Phase 14-E（CPU 基准 + 听感回归 + 决策评审）**。

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

## Phase 13：Stiff-String Inharmonic Piano v2 [已完成，2026-08-18]

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

**Phase 13-1：Inharmonicity（刚性琴弦分音失谐偏移） [已完成，2026-08-18]**

- [x] 引入 JOS PASP 刚性琴弦公式：在 `PianoSynthVoice::startNote` 中计算分音频率 $f_m = m \cdot f_0 \sqrt{1 + B \cdot m^2}$（$m \ge 1$ 为分音序号，1-based），步进计算为 `increment = (2π / sampleRate) * f_m`；相位累积维持 `double`。
- [x] 刚度系数 $B$ 按音区查表（低音弦刚度大、高音小）：
  - region 0（note <48）：$B = 4.0 \times 10^{-4}$（低音区 $m=7$ 时频率偏移 $+0.975\% \approx +1.0\%$，$m=2$ 时 $+0.08\%$）
  - region 1（48–71）：$B = 1.0 \times 10^{-4}$
  - region 2（72–95）：$B = 3.0 \times 10^{-5}$
  - region 3（≥96）：$B = 1.0 \times 10^{-5}$
- [x] **核心声学收益**：各分音失去公共整数倍周期 $\to$ 波形不再单调循环，低音区产生真实的"泛音失谐拍频（Beats）"，彻底消除整数倍加法合成的电子蜂鸣感。
- [x] 参数化决策：刚度 $B$ 按音区硬编码查表（真实钢琴刚度由物理弦径与张力决定，无需向用户暴露额外旋钮，保持 UI 紧凑）。
- [x] 计算纪律：仅在 `startNote` 按键瞬间计算一次步进增量，**音频渲染线程逐采样 CPU 零新增**。
- [x] 确定性测试（Phase 13-5 先行）：`PianoSynthVoiceTest.cpp` 新增 12 项断言（音区 B 参数边界、刚性琴弦分音公式量化校验、低音 note 36 第 5/7 分音频偏幅度断言、实际输出 DFT 在非谐频率处能量显著高于整数倍谐波处能量的对比断言），默认测试套件断言数 2992 $\to$ **3004** 全绿。
- [x] 验证：`wsl-build`（0 warning）/ `test`（3004 断言全绿）/ `format --check`（0 违规）/ `win-build`（MSVC 构建成功）。

**Phase 13-2：模态分音衰减速率建模（Modal Decay Modeling） [已完成，2026-08-18]**

- [x] 借鉴 Mutable Instruments（Rings/Elements）模态能量耗散模型：删除 v1 的 8 档离散表，引入连续分音时间常数模型 $\tau_m = \tau_{\text{base}} / (1.0 + c_{\text{eff}} \cdot (m - 1))$。
- [x] 确定性衰减因子：在 `startNote` 预计算每分音每采样衰减系数 $\text{decayPerSample}_m = \exp(-1.0 / (\text{sampleRate} \cdot \tau_m))$，阻尼斜率按音区查表 $c_{\text{region}} \in \{0.35, 0.25, 0.18, 0.12\}$（低音弦长阻尼斜率大、极高音小）。
- [x] 旋钮映射协调：`pianoResonance` 作用于 $\tau_{\text{base}}$（$\times 0.7 \sim 1.3$），`pianoBrightness` 作用于高次谐波初始幅度与高频衰减阻尼斜率（$c_{\text{eff}} = c_{\text{region}} \times (1.5 - \text{brightness})$，默认 0.5 时为 1.0 完全保持物理基准）。
- [x] **声学收益**：高次分音在击弦后快速耗散，音色由击弦瞬态丰富泛音平滑过渡至基频主导的自然尾音，彻底消除泛音长期不衰减的合成器质感。
- [x] 确定性测试（Phase 13-5 推进）：`PianoSynthVoiceTest.cpp` 新增 11 项断言（4 个音区阻尼斜率 $c$ 边界、时间常数物理公式量化校验、分音衰减时间单调递减校验、动态时域/频域早期 $t_0$ vs 后期 $t_1$ 高次分音能量比下降断言），默认测试套件断言数 3004 $\to$ **3015** 全绿。
- [x] 验证：`wsl-build`（0 warning）/ `test`（3015 断言全绿）/ `format --check`（0 违规）/ `win-build`（MSVC 构建成功）。

**Phase 13-3：简单琴体共鸣滤波（Body Resonator Bank） [已完成，2026-08-18]**

- [x] 借鉴 `DaisySP::Resonator` 与 Mutable Instruments 标准拓扑：构建轻量 Direct Form II 二阶带通/谐振器组（3 个并联峰：110 Hz $Q=6.0$ 权重 0.40、220 Hz $Q=5.0$ 权重 0.35、360 Hz $Q=4.0$ 权重 0.25）。
- [x] 状态与内存纪律：`std::array<BodyResonator, 3>` 静态作为 `PianoSynthVoice` 私有成员，系数在 `startNote` 预计算更新，`stopNote(false)` 与自清时彻底重置状态；`renderNextBlock` 中纯直接计算（每 sample 增加 $\le 12$ 次乘加），**零堆分配、无锁**。
- [x] 信号混合：采用 Wet/Dry 混合策略（`output = (1 - wet) * raw + wet * filtered`，默认 $\text{wet} = 0.25$），为纯干弦声注入温暖的木质共鸣箱体感，且整体峰值电平严格归一化。
- [x] 数值稳定性保证：极点严格约束在单位圆内（$r = \exp(-\pi \cdot \text{bandwidth} / \text{sampleRate}) < 1.0$），长时渲染无发散，静音后状态快速衰减至零。
- [x] 确定性测试（Phase 13-5 推进）：`PianoSynthVoiceTest.cpp` 新增 9 项断言（Wet 比例与共鸣峰参数边界、A2 110 Hz / A3 220 Hz 共振能量提升与归一化输出无 clip 断言、立即停止后谐振器状态清空断言），默认测试套件断言数 3015 $\to$ **3024** 全绿。
- [x] 验证：`wsl-build`（0 warning）/ `test`（3024 断言全绿）/ `format --check`（0 违规）/ `win-build`（MSVC 构建成功）。

**Phase 13-4：参数化与 UI 适配（向后兼容） [已完成，2026-08-18]**

- [x] 维持 Phase 12-3 确立的 3 旋钮布局（Brightness / Hammer / Resonance）与 Tone 下拉，刚度 $B$ 与音板共振参数走物理精调查表，不强行增加第 4 旋钮，保持 UI 紧凑。
- [x] 经人工验证，既有 3 旋钮与 Tone 下拉在 v2 引擎上的映射协调性良好，音色调控自然有效。

**Phase 13-5：确定性音色测试（与 13-1~3 并行） [已完成，2026-08-18]**

- [x] 非谐性 DFT 量化：在 `PianoSynthVoiceTest.cpp` 中以单点 DFT 验证低音 note 36（C2）的第 5、7 分音精确落在 $m \cdot f_0 \sqrt{1 + B \cdot m^2}$ 附近，且能量显著高于整数倍谐波。
- [x] 模态衰减对比断言：量化断言在 $t_0 = 0.1\text{ s}$ 与 $t_1 = 2.0\text{ s}$ 处，第 6 分音与基频的幅度比满足 $\text{Ratio}(t_1) < 0.5 \cdot \text{Ratio}(t_0)$。
- [x] 琴体谐振器频响与稳定性测试：验证 110 Hz / 220 Hz / 360 Hz 音板共鸣峰存在且稳态渲染无溢出；立即停止后谐振器状态彻底清空；100 块长渲染输出有限、无 NaN/Inf、静音后各 voice 彻底自清。
- [x] 保持全量 **3024** 断言全绿（Phase 13 净增 32 项断言）。

**Phase 13-6：听感回归与默认音色决策 [已完成，2026-08-18]**

- [x] Windows 侧手工听觉对比（v1 vs v2）：低音区泛音失谐拍频与音板共鸣厚度可辨，中高音区击弦明亮度向纯净基频衰减过渡平滑，3 旋钮效果协调。
- [x] **默认音色决策落地**：经人工听觉回归确认，Piano 音色显著优于 Sine 蜂鸣声，正式将 `BuiltinTone` / `BuiltinSynthTone` 默认值由 `Sine` 切换为 `Piano`（覆盖 `AudioEngine`、`SettingsModel`、`AppState`、`WavExportOptions` 及 UI 初始状态与回退）。
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

## Phase 14：Enhanced Modal Piano v3（增强模态合成）[改道，2026-08-18]

> 原计划为数字波导（Digital Waveguide Piano v3）。经网络与文献核实（2026-08-18），数字波导对钢琴不是业界最佳路线，本阶段正式**改道为增强模态合成**——Phase 13 解析加法的自然延伸；数字波导降级为实验分支（见文末）。

### 改道决策与关键证据

1. **业界最佳物理建模钢琴 Pianoteq = 模态合成（modal synthesis），不是数字波导**（多个 DAFx / IRCAM 论文确认，如 Renault DAFx22）。
2. **波导钢琴奠基人本人转向**：Balázs Bank 博士论文《Physics-Based Sound Synthesis of the Piano》（2000）为波导路线；同一作者 2010 年在 IEEE TASLP 发表《A Modal-Based Real-Time Piano Synthesizer》——实时合成器选了模态路线（700~1200 个二阶谐振器覆盖 10 kHz + 5~10 个 secondary resonators 建模拍频与两阶段衰减）。
3. **波导钢琴固有局限**（Välimäki / Bank 综述）：无分音粒度控制（所有分音由同一延迟线+滤波决定，而钢琴音色演化正依赖每分音独立幅度/频率控制）；two-stage decay 需双波导（计算量加倍）；loss filter 全音域稳定设计困难；音板/框架共振难复现。
4. **换向波导钢琴从未商业化**：Yamaha × Stanford/CCRMA（JOS 换向波导）后，VL1 成功于管乐/弓弦，钢琴转向采样 + 混合路线。
5. **对照本项目现状**：Phase 13（8 分音独立频率/衰减 + 3 音板谐振器）已是简化模态合成雏形；音频线程 CPU 仅 ~1%~1.5% 单核（大头在 UI），波导省 DSP 的收益落在非瓶颈处，代价（失去分音控制）正中钢琴音色命门。

### 背景与目标

Phase 12/13 的 v1/v2 是**有限分音解析加法**：≤8 分音逐个 `std::sin`，高音区仅 2~3 分音，且缺失真实钢琴两个核心声学特征——**两阶段衰减（two-stage decay：击弦快衰减 → 琴体慢尾音）**与**同音三弦拍频（beating：三根弦微失谐的厚实颤动）**。v3 目标：在既有模态架构上补齐这两项 + 分音覆盖 + 音板模态密度，用**每分音递归振荡器**对冲计算量，听感向"真实钢琴"再进一步。

### 前置事实（已核实代码，Phase 12/13 落地后）

| 事实 | 位置 | 影响 |
|---|---|---|
| PianoSynthVoice：≤8 分音、每分音独立频率（B 失谐查表）/ 独立衰减（τ_m 模态耗散）+ 3 个 BodyResonator | PianoSynthVoice.h | v3 在**同一 voice 类内增强**（分音数、双衰减、拍频、谐振器数），不新注册第三音色，UI/默认值零变更 |
| 每分音逐采样 `std::sin` 是音频线程主要 DSP 成本（8 voice ≈ 28M op/s） | PianoSynthVoice.h `renderNextBlock` | 换 Magic Circle 递归振荡器（2 乘加/分音）后分音数可安全扩展，CPU 不升反降 |
| 参数链路：`PerformanceSettingsView` → `applyPerformanceSettingsToAudioEngine` + `buildWavExportOptions`（DOC-006 回退模式） | MainComponent.cpp / ExportFlowSupport.cpp / SettingsStore.cpp | v3 若加参数（如拍频深度）沿用同一链路，默认值向后兼容 |
| 确定性测试夹具：`Synthesiser` 驱动 + 单点 DFT + 长时稳定性断言（3022 断言全绿） | PianoSynthVoiceTest.cpp | 双衰减斜率、拍频周期、递归振荡器频偏均可量化断言 |
| 主界面 8 旋钮（上四：音量/亮度/击弦/共鸣，下四：ADSR）+ `--sine` 启动参数 | LayoutModel.cpp / Main.cpp | v3 不新增旋钮（保持紧凑）；新参数默认物理值，必要时才扩展 |

### 架构方案（增强模态合成）

**v3 = v2 架构 + 四项增强（全部在 `PianoSynthVoice` 内演进）：**

1. **递归振荡器（Magic Circle / Coupled Form）**：每分音二阶递归正弦 $u[n] = u[n-1] - \epsilon v[n-1],\; v[n] = v[n-1] + \epsilon u[n]$（2 乘加，零 `std::sin`），相位/幅度状态存入 `Partial`；幅度衰减经每采样增益乘法维持。
2. **分音数扩展**：上限 8 → 低音区 20 / 中音区 14 / 高音区 8 / 极高音区 6（顶分音覆盖 C2≈1.4 kHz / C4≈3.7 kHz / C5≈4.2 kHz，感知核心频段）；归一化峰值电平逻辑沿用（`peakLevelAtFullVelocity`）。
3. **Two-stage decay**：每分音双指数衰减 $\text{level}(t) = A \left[ (1-w) e^{-t/\tau_{\text{fast}}} + w\, e^{-t/\tau_{\text{slow}}} \right]$（$\tau_{\text{fast}}$ 击弦段、$\tau_{\text{slow}}$ 琴体段，$w$ 按音区），在 `startNote` 预计算两套 `decayPerSample` 与交叉时间；或采用 Bank 2010 的 secondary resonators（5~10 个副谐振器）。
4. **同音三弦拍频（Beating）**：每分音微失谐双振荡器对（±0.1%~0.3%，同音三弦近似为双组拍频），或 Bank 的 beating equalizer（调谐峰值滤波器调制分音包络）——低中音区厚度关键。
5. **音板模态组扩展**：BodyResonator 3 峰 → 8~12 峰（音板主模态集，参考 Bank/Pianoteq 频点），保持每 sample ≤ 32 乘加的预算内。

### CPU 预算（改道后）

| 项 | 预算 | 说明 |
|---|---|---|
| 递归振荡器 | 2 乘加/分音（零 `std::sin`） | 20 分音 ≈ 40 乘加/采样 ≈ 旧 8 分音 `std::sin` 成本的 ~1/5 |
| 双衰减 | 每采样 1 次乘加 + 交叉比较（`startNote` 预计算） | 增量可忽略 |
| 拍频双振荡器 | 分音数 ×2 振荡器（但低音区拍频仅取前 N 分音，高音区关闭） | 低音区最贵 ≈ 2× 振荡器成本 |
| 音板模态组 8~12 峰 | ≤ 32 乘加/采样/voice | 与 3 峰相比 ×3，仍远低于 `std::sin` 节省 |
| **合计（低音区最坏）** | **≈ 80~100 乘加/采样/voice**，8 voice ≈ 0.5%~0.7% 单核 | 仍低于 v2 音频线程成本（~1%~1.5%），且音色维度远超 |

### 子任务排期

**Phase 14-A：递归振荡器 + 分音数扩展 [已完成，2026-08-18]**

- [x] Magic Circle 递归振荡器替换 `Partial` 的 `std::sin`（coupled form：u/v 双状态 3 乘 2 加双精度，零 `std::sin`）；幅度衰减经每采样增益乘法维持。
- [x] 分音上限扩展：低音 20 / 中音 14 / 高音 8 / 极高音 6（真实覆盖：C2 顶分音 ≈1.4 kHz、C4 ≈3.7 kHz、C5 ≈4.2 kHz）；`amplitudeFor`/`brightnessBoost`/`hammerGain` 归一化自动适配（scale 重算）。
- [x] 确定性测试：递归振荡器长时频偏（25 s 渲染 + 双窗复 DFT 相位差法，漂移 < 1e-4 相对；30 s 与 voice 自清阈值冲突，25 s 处基频幅度 ≥2e-4、相位分辨率 ≈1e-8 相对）、分音数边界 20/14/8/6、全区域 DFT 分音检查（低音 2..20 / 中音 2..14 / 高音 2..8 / 极高音 2..6）、现有断言全绿（46/46，3059 断言）。
- [x] 验证：`wsl-build` / `test` / `format --check` / `win-build` 三闸门全绿。

**Phase 14-B：Two-stage decay（双阶段衰减）[已完成，2026-08-18]**

- [x] 每分音双指数分量 `A(t) = A·[(1-w)·e^{-t/τ_fast} + w·e^{-t/τ_slow}]`（方案选型：双指数衰减而非 Bank 2010 secondary resonators——每分音 +1 乘加、零新抽象、确定性可断言；副谐振器组 CPU 更贵且与 per-partial 控制重叠）；`Partial` 增加 `levelFast/levelSlow` 与两套 `decayPerSample`，`startNote` 预计算（τ_fast,m = τ_slow,m × ratio）。
- [x] 参数按音区查表：`fastDecayRatio` 0.15/0.20/0.20/0.15（低/中/高/极高音，τ_fast = 0.6/0.5/0.3/0.12 s）、`slowWeight` 0.30/0.25/0.20/0.15；`decaySeconds` 语义变为 τ_slow 基准（数值不变，向后兼容）。
- [x] 确定性测试：4.2 s 渲染 + 短窗 DFT（4096 样本）对数幅度线性回归——早期斜率 ≈ -1.18 /s vs 晚期 ≈ -0.25 /s（断言 |early| > 2×|late|）；`partialFastDecaySeconds` 解析公式断言；频偏测试时长 25 s → 20 s（双衰减后 voice 自清 ≈24 s）；modal decay 晚期窗口后移至 3.15 s；低音顶分音 DFT 阈值 0.025（m=20 快分量在窗口内耗尽）；全绿 46/46、3070 断言。
- [x] 验证：`wsl-build` / `test` / `format --check` / `win-build` 三闸门全绿。
- [ ] Windows 侧手工听感（待执行）：尾音自然度对比 v2——与 14-E 合并执行。

**Phase 14-C：同音三弦拍频（Beating）[已完成，2026-08-18]**

- [x] 每分音微失谐双振荡器对（Magic Circle coupled form，失谐率 0.10%~0.20%）；低中高音区参数查表：`beatingDetuneRatio` 0.0020/0.0015/0.0010/0.0（低/中/高/极高音），`beatingPartials` 6/6/4/0；低音基频锁定单振荡器（保证单弦/双弦低音主音高稳定），低音泛音 2..6 及中高音全部分音启用第二弦干涉。
- [x] 确定性测试：`beatingDetuneRatioForNote` / `beatingPartialCountForNote` / `beatingFrequency` 区域查询与公式断言；C4（note 60）3.2 s 渲染测干涉调制——反相下陷点（$t \approx 1.28\text{ s}$）幅度 $< 0.5 \times \text{early}$，同相回弹峰（$t \approx 2.55\text{ s}$）幅度 $> 1.3 \times \text{dip}$（打破纯指数单调衰减，确凿验证物理干涉回弹）；全绿 46/46、3086 断言。
- [x] 验证：`wsl-build` / `test` / `format --check` / `win-build` 三闸门全绿。
- [ ] Windows 侧手工听感（待执行）：低中音"厚实颤动"与 chorus 感——与 14-E 合并执行。
**Phase 14-D：音板模态组扩展 [已完成，2026-08-18]**

- [x] BodyResonator 3 峰 → 8 峰（75/110/160/220/320/460/680/950 Hz，涵盖低音呼吸、琴桥耦合与板面共振模态）；权重归一（和为 1.00），保持 25% wet / 75% dry 混合下峰值电平严格有界。
- [x] 确定性测试：`resonatorCount` == 8、8 峰频点与 Q 查表断言、权重和为 1.0 断言；极点 $|r| < 1.0$ 渐近绝对稳定断言；110 Hz（A2）与 220 Hz（A3）共鸣峰响应及即时静音清零测试；全绿 46/46、3101 断言。
- [x] 验证：`wsl-build` / `test` / `format --check` / `win-build` 三闸门全绿。
**Phase 14-E：CPU 基准 + 听感回归 + 决策评审**

- [ ] CPU 基准：8 voice 复音实测 v3 vs v2（Windows 侧任务管理器 / 内部计时），验证预算评估（v3 音频线程应 ≤ v2）。
- [ ] 听感回归（Windows 侧手工）：低音（C2~C3 厚度与拍频）、中音（C4 双衰减过渡）、高音（C6+ 清脆度）；3 旋钮协调性。
- [ ] **决策评审**：v3 是否作为默认音色（预期替代 v2 注册，v2 保留回退）；新参数（拍频深度等）是否入 `SettingsModel`。

**Phase 14-F（实验分支，可选）：数字波导实验**

- [ ] 波导降级为实验分支，不作为默认音色候选主线；如推进，目标改为验证拨弦类音色（吉他/羽管键琴）潜力。
- [ ] 原波导 14-1~14-6 排期归档为实验参考，待拨弦类音色需求出现时再启用。

### 验收标准

- 递归振荡器零 `std::sin`，长时频偏可忽略（确定性测试）。
- 分音上限扩展 20/14/8/6，全区域分音在非谐频率处可测（分音数边界 + DFT 断言）。
- 双阶段衰减可测（早期/晚期斜率比断言）；拍频周期可测（时域/DFT 断言）。
- 音板 8~12 峰频响可测且稳定，输出有限无 NaN。
- 三闸门全绿：`wsl-build` 0 warning / `test` 全绿 / `format --check` 归零 / `win-build` 通过。
- CPU 基准：8 voice v3 音频线程成本 ≤ v2（递归振荡器对冲分音增加）。
- 听感：v3 明显超越 v2（尾音自然、低中音厚实颤动、共鸣感）。

### 验证命令

```bash
./scripts/dev.sh wsl-build --configure-only
./scripts/dev.sh test
./scripts/dev.sh format --check
./scripts/dev.sh win-build
```

### 排期参考

14-A 约 1 轮；14-B 约 1 轮（含听感）；14-C 约 0.5~1 轮；14-D 约 0.5~1 轮；14-E（基准 + 决策）约 1 轮；14-F 视兴趣可选。Phase 14 总计约 3~4 轮迭代 + Windows 侧手工回归。

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
