# Phase 12–14：内置物理建模钢琴音源三部曲（完成记录）

> 归档说明：记录 Phase 12（音源重构 + 谐波钢琴 v1）、Phase 13（Stiff-String Inharmonic Piano v2）与 Phase 14（Enhanced Modal Piano v3）的完整背景、前置事实、方案演进、子任务排期与落地验证结果。
> 完成日期：2026-08-18 ~ 2026-08-19
> 替代文档：[`../roadmap/roadmap.md`](../roadmap/roadmap.md)（内置音源摘要）、[`../reference/architecture.md`](../reference/architecture.md)（音频架构）

---

## 阶段演进概览

| 阶段 | 核心技术 | 突破特性 | 验证结果 |
|---|---|---|---|
| **Phase 12 (v1)** | 共享 `SineSynthVoice` 重构 + 8 分音整数倍谐波加法 | 实时与离线导出音色统一；击弦泛音、按音自然衰减、音区分层；参数化与 UI 接线（Tone + 3 旋钮） | 听觉对比明显优于纯正弦波；52 类 2992 断言全绿 |
| **Phase 13 (v2)** | 刚性琴弦失谐分音（Inharmonicity）+ 模态耗散衰减 + 3 峰音板共鸣 | 刚度系数 $B$ 按音区查表消除电子蜂鸣感；高次谐波快速耗散；音板箱体共振；默认音色正式切为 Piano | 3024 断言全绿；Windows 侧手工听觉回归通过 |
| **Phase 14 (v3)** | Magic Circle 递归振荡器 + 分音扩展 (20/14/8/6) + 双阶段衰减 + 同音三弦拍频 + 8 峰音板模态 | 零 `std::sin` 高性能振荡器；两阶段衰减（击弦快衰减 → 琴体长尾音）；同音三弦微失谐拍频颤动；8 峰音板主模态 | 3101 断言全绿；人工 A/B 对比确认全面超越 v2；固化为唯一默认钢琴音色 |

---

## Phase 12：音源重构 + 谐波钢琴 v1 [已完成，2026-08-18]

### 背景与目标

未加载插件时的内置音源是纯正弦波（`SimpleSineVoice`），声音是"电脑 beep"。目标：以 JUCE 标准机制（`juce::Synthesiser` + `SynthesiserVoice` 派生，不建自定义抽象层、不建 `source/Audio/Builtin/` 目录）分阶段替换为谐波钢琴音色，使无插件时也发出可辨识的钢琴声音。Phase 12 完成"重构 + 谐波 v1"，Phase 13/14 递进到 inharmonicity 与增强模态合成。

### 前置事实（已核实代码）

| 事实 | 位置 | 影响 |
|---|---|---|
| ~~sine 实现两份逐字重复：`SimpleSineVoice`（实时）与 `OfflineSineVoice`（离线导出）~~ | ~~AudioEngine.cpp:31-99 / WavFileExporter.cpp:18-101~~ | **已消除（12-1）**：合并为共享 `SineSynthVoice`，实时/导出同源 |
| 三处调用点：实时渲染、WAV 离线导出、插件离线失败回退 | AudioEngine.cpp:217 / WavFileExporter.cpp:197 / RecordingSessionController.cpp:202 | 全部改接共享 voice |
| 参数接线已有同构模式：`PerformanceSettingsView` → `applyPerformanceSettingsToAudioEngine` + `buildWavExportOptions` | MainComponent.cpp:1059-1063 / ExportFlowSupport.cpp:58 | 新音色参数沿用，含默认值向后兼容 |
| fallback 音频路径无确定性测试（`MidiMessageCollector` wall-clock 时序） | AudioEngineTest.cpp 顶部注释 | 新音色带确定性夹具（直接驱动 voice） |

### 子任务排期与完成记录

**Phase 12-1：共享 SineSynthVoice 重构（零行为变化） [已完成，2026-08-18]**

落地提交：`26772e9`（refactor: extract shared SineSynthVoice for realtime and offline paths，净 -71 行）。

- [x] 新建 `source/Audio/SineSynthVoice.h`（header-only）：`SineSynthSound` + `SineSynthVoice` 从 AudioEngine 私有嵌套提出为独立类（继承 `juce::SynthesiserVoice`），行为逐行不变。
- [x] `AudioEngine::rebuildSynth` / `updateAdsrOnVoices` 改用共享类（`synth.addVoice(new SineSynthVoice())` ×8 + `dynamic_cast<SineSynthVoice*>` 设 ADSR）；`AudioEngine.h` 删除嵌套类前向声明。
- [x] `WavFileExporter` 删除 `OfflineSineVoice`/`OfflineSineSound`/`initialiseOfflineSynth` 逐字副本（-84 行），改注册共享类（保留 `fallbackVoiceCount = 8` 语义）。
- [x] 三处调用点全部改接：实时渲染（AudioEngine.cpp:188）、WAV 离线导出（WavFileExporter.cpp:77）、插件离线失败回退（WavExportTask → `exportTakeAsWavFile` 走同一共享实现）；`grep SimpleSine|OfflineSine` 无残余定义。
- [x] 验证通过：`wsl-build` / `test`（ctest 1/1 passed，断言全绿，行为不变）/ `format --check` / `win-build`（MSVC 构建成功）。

**Phase 12-2：Harmonic PianoVoice v1（Level 1 音色） [已完成，2026-08-18]**

落地：`source/Audio/PianoSynthVoice.h`（header-only，继承 `juce::SynthesiserVoice`，与 SineSynthVoice 并存注册）。

- [x] 新建 `source/Audio/PianoSynthVoice`（继承 `SynthesiserVoice`），与 SineSynthVoice 并存注册；`AudioEngine::BuiltinSynthTone {sine, piano}` + `setBuiltinSynthTone` 可切换（`rebuildSynth` 按 tone 注册），初始默认仍用 sine（行为不变），后续阶段切默认。
- [x] 合成核心：基频 + 2~7 次谐波叠加，分音数与幅度按音区查表（`voiceRegions`：note <48 → 8 分音 / <72 → 6 / <96 → 4 / ≥96 → 3；幅度 1/n 递减），幅度归一避免 clip（`peakLevelAtFullVelocity = 0.28`）。
- [x] velocity 双映射：响度 `level = v^1.5`（弱奏更敏感，sqrt 实现避免 pow）+ 亮度（高次谐波增益随 v 线性提升，最高次 +60%）。
- [x] 分音独立衰减：decay 按音区查表（4.0 / 2.5 / 1.5 / 0.8 s，低音长高音短）+ 高次分音略快（`harmonicDecayFactor` 表）；attack/release 沿用 `setAdsrParameters` 接线（内部变换为 `{attack, 0.001, 1.0, release}` 作门控，decay/sustain 由分音衰减替代）。
- [x] CPU 预算：每 voice ≤ 8 次 `std::sin`（分音数上限 8）+ 每 note 一次 `sqrt`/`exp`；`renderNextBlock` 无堆分配、无锁（`std::array<Partial, 8>` 固定缓冲）；分音衰减至 `1e-4`（-80 dB）后 voice 自清防低音长尾占位。
- [x] 听觉回归（Windows 侧手工，2026-08-18）：Step 1~8 全部通过——sine 持续"哔" vs piano 带泛音 + 按住自然衰减；音区差异（低音厚长衰减 / 高音薄短）；Resonance 余韵 ±30% 明显、Brightness 明暗可闻、Hammer 微妙（±10% 设计如此）；实时/导出一致；参数持久化；Sine 回归正常。
- [x] 验证：`test`（默认套件 2928 断言全绿）/ `format --check` / `win-build`（MSVC 成功）。

**Phase 12-3：参数化与 UI 接线 [已完成，2026-08-18]**

- [x] `SettingsModel::PerformanceSettingsView` + 平铺字段扩展：`builtinTone`（模型层枚举 `BuiltinTone {sine, piano}`）+ `pianoBrightness` / `pianoHammerHardness` / `pianoResonance`（0..1，默认 0.5，与 v1 基准行为一致）。`SettingsStore` 4 个新 key 读写 + 钳制（tone 仅 0|1、参数 0..1）；旧序列化数据缺失字段回退默认（仿 DOC-006，`getDoubleValue(key, model.xxx)`），新增 legacy 文件专项用例。
- [x] `AudioEngine::setPianoParameters`（jlimit + `updatePianoParametersOnVoices`）+ `MainComponent::applyPerformanceSettingsToAudioEngine` 透传（含 `setBuiltinSynthTone` 映射）。**线程安全修正**：核实 JUCE `Synthesiser` 内部锁——`processNextBlock`（音频渲染）与 `clearVoices`/`addVoice` 共用同一 lock，消息线程 `rebuildSynth` 安全，音频线程仅短暂阻塞。
- [x] `WavExportOptions` + `buildWavExportOptions` 扩展同参数；`WavFileExporter::initialiseOfflineSynth` 按 `builtinTone` 注册 Sine/Piano voice 并设置参数——**实时/离线音色一致达成**。
- [x] `PianoSynthVoice` 参数映射：brightness 亮度基准（b=0.5 时与 12-2 完全一致）、hammerHardness 高次起始增益（0.5 中性 ±20%）、resonance 衰减时间缩放（×0.7~1.3）。
- [x] UI 控件（沿用 ADSR 旋钮模式）：`LayoutModel::makeControlsPanelTree` 新增 `piano-row`（`tone-combo` + 3 个 DevKnob）；`MainComponent` wireKnob 接线（0..1/0.01 步进，% 显示）+ `tone-combo` 单选；`MainComponentJiveAccessors` 新访问器；`AppState`/`AppStateBuilder` 同步。
- [x] 验证：`test`（52 类 2992 断言全绿）/ `format --check` / `win-build`（MSVC 成功）。

**Phase 12-4：确定性音色测试 [已完成，2026-08-18]**

- [x] 新建 `source/tests/PianoSynthVoiceTest.cpp`：夹具经 `juce::Synthesiser` 驱动 voice（`noteOn` 事件 → `renderNextBlock`，绕过 `MidiMessageCollector` 时序）。
- [x] 断言：单点 DFT（Hann 窗）验证基频≈261.63 Hz 且低音区 2~7 次 / 中音区 2~5 次谐波存在、velocity 0.2 vs 0.9 响度单调递增、noteOff 后 tail 衰减收敛至零、自然衰减自清（treble 8 s）、`stopNote(false)` 立即静音、100 块长渲染有限无 NaN、`allNotesOff` 生效、`AudioEngine` 音色切换接口。

---

## Phase 13：Stiff-String Inharmonic Piano v2 [已完成，2026-08-18]

### 背景与目标

Phase 12 的 v1 是整数倍谐波叠加：分音频率严格 `n·f₀`，相位有公共周期，波形循环重复，声音带"电子合成"感。真实钢琴琴弦有刚度：高频分音频率系统性偏高（inharmonicity），且高频能量耗散更快（高次分音衰减更短），琴体/音板有共鸣频响。v2 在 v1 基础上加入这三项，使内置音色向真实钢琴逼近，同时保持解析加法、零采样依赖、无新抽象层、实时线程无堆分配无锁。

### 子任务排期与完成记录

**Phase 13-1：Inharmonicity（刚性琴弦分音失谐偏移） [已完成，2026-08-18]**

- [x] 引入 JOS PASP 刚性琴弦公式：在 `PianoSynthVoice::startNote` 中计算分音频率 $f_m = m \cdot f_0 \sqrt{1 + B \cdot m^2}$（$m \ge 1$ 为分音序号，1-based），步进计算为 `increment = (2π / sampleRate) * f_m`；相位累积维持 `double`。
- [x] 刚度系数 $B$ 按音区查表：region 0（note <48）：$B = 4.0 \times 10^{-4}$；region 1（48–71）：$B = 1.0 \times 10^{-4}$；region 2（72–95）：$B = 3.0 \times 10^{-5}$；region 3（≥96）：$B = 1.0 \times 10^{-5}$。
- [x] **核心声学收益**：各分音失去公共整数倍周期 $\to$ 波形不再单调循环，低音区产生真实的"泛音失谐拍频（Beats）"，彻底消除整数倍加法合成的电子蜂鸣感。
- [x] 计算纪律：仅在 `startNote` 按键瞬间计算一次步进增量，**音频渲染线程逐采样 CPU 零新增**。
- [x] 确定性测试：`PianoSynthVoiceTest.cpp` 新增 12 项断言，默认测试套件断言数 2992 $\to$ **3004** 全绿。

**Phase 13-2：模态分音衰减速率建模（Modal Decay Modeling） [已完成，2026-08-18]**

- [x] 借鉴 Mutable Instruments（Rings/Elements）模态能量耗散模型：引入连续分音时间常数模型 $\tau_m = \tau_{\text{base}} / (1.0 + c_{\text{eff}} \cdot (m - 1))$。
- [x] 确定性衰减因子：在 `startNote` 预计算每分音每采样衰减系数 $\text{decayPerSample}_m = \exp(-1.0 / (\text{sampleRate} \cdot \tau_m))$，阻尼斜率按音区查表 $c_{\text{region}} \in \{0.35, 0.25, 0.18, 0.12\}$。
- [x] 旋钮映射协调：`pianoResonance` 作用于 $\tau_{\text{base}}$（$\times 0.7 \sim 1.3$），`pianoBrightness` 作用于高次谐波初始幅度与高频衰减阻尼斜率（$c_{\text{eff}} = c_{\text{region}} \times (1.5 - \text{brightness})$）。
- [x] **声学收益**：高次分音在击弦后快速耗散，音色由击弦瞬态丰富泛音平滑过渡至基频主导的自然尾音。
- [x] 确定性测试：`PianoSynthVoiceTest.cpp` 新增 11 项断言，断言数 3004 $\to$ **3015** 全绿。

**Phase 13-3：简单琴体共鸣滤波（Body Resonator Bank） [已完成，2026-08-18]**

- [x] 借鉴 `DaisySP::Resonator` 与 Mutable Instruments 标准拓扑：构建轻量 Direct Form II 二阶带通/谐振器组（3 个并联峰：110 Hz $Q=6.0$ 权重 0.40、220 Hz $Q=5.0$ 权重 0.35、360 Hz $Q=4.0$ 权重 0.25）。
- [x] 状态与内存纪律：`std::array<BodyResonator, 3>` 静态作为 `PianoSynthVoice` 私有成员，系数在 `startNote` 预计算更新，`stopNote(false)` 与自清时彻底重置状态；`renderNextBlock` 中纯直接计算（每 sample 增加 $\le 12$ 次乘加），**零堆分配、无锁**。
- [x] 信号混合：采用 Wet/Dry 混合策略（`output = (1 - wet) * raw + wet * filtered`，默认 $\text{wet} = 0.25$），为纯干弦声注入温暖的木质共鸣箱体感。
- [x] 确定性测试：`PianoSynthVoiceTest.cpp` 新增 9 项断言，断言数 3015 $\to$ **3024** 全绿。

**Phase 13-4 & 13-6：参数协调、听感回归与默认音色决策 [已完成，2026-08-18]**

- [x] Windows 侧手工听觉对比（v1 vs v2）：低音区泛音失谐拍频与音板共鸣厚度可辨，中高音区击弦明亮度向纯净基频衰减过渡平滑，3 旋钮效果协调。
- [x] **默认音色决策落地**：经人工听觉回归确认，Piano 音色显著优于 Sine 蜂鸣声，正式将 `BuiltinTone` / `BuiltinSynthTone` 默认值由 `Sine` 切换为 `Piano`。

---

## Phase 14：Enhanced Modal Piano v3（增强模态合成）[已完成，2026-08-19]

### 改道决策与关键证据

1. **业界最佳物理建模钢琴 Pianoteq = 模态合成（modal synthesis），不是数字波导**（多个 DAFx / IRCAM 论文确认）。
2. **波导钢琴奠基人本人转向**：Balázs Bank 博士论文（2000）为波导路线；2010 年 IEEE TASLP 发表《A Modal-Based Real-Time Piano Synthesizer》转向模态路线。
3. **波导钢琴固有局限**：无分音粒度独立控制、two-stage decay 需双波导计算翻倍、loss filter 全音域设计困难、音板/框架共振难复现。
4. **决策确认**：主线改道增强模态合成，数字波导实验分支取消。

### 架构方案与子任务完成记录

**Phase 14-A：递归振荡器 + 分音数扩展 [已完成，2026-08-18]**

- [x] Magic Circle 递归振荡器替换 `Partial` 的 `std::sin`（coupled form：u/v 双状态 3 乘 2 加双精度，零 `std::sin`）；幅度衰减经每采样增益乘法维持。
- [x] 分音上限扩展：低音 20 / 中音 14 / 高音 8 / 极高音 6（覆盖 C2 ≈1.4 kHz、C4 ≈3.7 kHz、C5 ≈4.2 kHz 核心频段）；`amplitudeFor`/`brightnessBoost`/`hammerGain` 归一化自动适配。
- [x] 确定性测试：长时频偏测试（25 s 渲染 + 双窗复 DFT 相位差法，漂移 < 1e-4 相对）、分音数边界 20/14/8/6、全区域 DFT 分音检查；断言数增至 **3059**。

**Phase 14-B：Two-stage decay（双阶段衰减）[已完成，2026-08-18]**

- [x] 每分音双指数分量 $A(t) = A \cdot [(1-w) e^{-t/\tau_{\text{fast}}} + w e^{-t/\tau_{\text{slow}}}]$；`Partial` 增加 `levelFast/levelSlow` 与两套 `decayPerSample`，`startNote` 预计算。
- [x] 参数按音区查表：`fastDecayRatio` 0.15/0.20/0.20/0.15、`slowWeight` 0.30/0.25/0.20/0.15；`decaySeconds` 语义变为 $\tau_{\text{slow}}$ 基准。
- [x] 确定性测试：4.2 s 渲染 + 短窗 DFT 对数幅度线性回归（早期斜率 ≈ -1.18 /s vs 晚期 ≈ -0.25 /s，断言 |early| > 2×|late|）；断言数增至 **3070**。

**Phase 14-C：同音三弦拍频（Beating）[已完成，2026-08-18]**

- [x] 每分音微失谐双振荡器对（Magic Circle coupled form，失谐率 0.10%~0.20%）；低中高音区参数查表：`beatingDetuneRatio` 0.0020/0.0015/0.0010/0.0，`beatingPartials` 6/6/4/0；低音基频锁定单振荡器保证主音高稳定，低音泛音与中高音全部分音启用第二弦干涉。
- [x] 确定性测试：C4 3.2 s 渲染测干涉调制（反相下陷点幅度 $< 0.5 \times \text{early}$，同相回弹峰幅度 $> 1.3 \times \text{dip}$，确凿验证物理干涉回弹）；断言数增至 **3086**。

**Phase 14-D：音板模态组扩展 [已完成，2026-08-18]**

- [x] BodyResonator 3 峰 → 8 峰（75/110/160/220/320/460/680/950 Hz，涵盖低音呼吸、琴桥耦合与板面共振模态）；权重归一（和为 1.00），保持 25% wet 混合下峰值电平严格有界。
- [x] 确定性测试：8 峰频点与 Q 查表断言、极点 $|r| < 1.0$ 稳定断言、110 Hz / 220 Hz 共振能量提升与静音清零断言；断言数增至 **3101**。

**Phase 14-E：CPU 基准 + 听感回归 + 决策评审 [已完成，2026-08-19]**

- [x] CPU 基准：Magic Circle 递归振荡器与按需拍频使音频线程单核 CPU 维持在 ≤ 0.5%~0.7%，8 voice 复音开销低于 v2。
- [x] 听感回归（Windows 侧人工 A/B 对比）：低音 20 分音 + 8 峰音板共鸣饱满浑厚；中音长音双阶段衰减 + 2.55 s 周期拍频颤动歌唱性显著；高音木质透亮。
- [x] **决策评审**：
  1. 确认 Enhanced Modal Piano v3 为唯一内置默认钢琴音色；
  2. 拍频失谐率与双衰减比率作为底层物理常量固化，不入设置项，保持「4×4 经典 8 旋钮」极简布局。

**Phase 14-F：数字波导实验分支 [已取消]**

- [x] 决策确认取消实验分支，避免无谓代码膨胀。
