# Phase 40–44 历史归档：全键声学基线、频谱重标定与整琴共享总线

> 阶段：Phase 40–44。阶段状态：全部完成（2026-11-25）；Phase 40 证据归档，Phase 41–44 已交付，用户已实机试听。
> 归档日期：2026-11-26。来源：归档前的 `docs/roadmap/roadmap.md` 与 `docs/roadmap/current-iteration.md`。
> 归档范围：Phase 40 的全键响度/频谱证据基线，Phase 41 的低风险声学修复，Phase 42 的低/高音频谱与音区响度重标定，Phase 43 的整琴共享声学总线与 12 复音收敛，Phase 44 的空间/参考音色/微观细节；各阶段的测量数据、提交归属、失败取舍与验证边界。
> 历史边界：以下保留原记录的日期、数值、结论和未验证范围，不因归档改写。Windows Debug 测量、默认 CTest 与用户实机试听是当时的证据形态，不外推为 Release、硬件矩阵或商业插件认证。
> 替代关系：项目状态与长期阶段顺序见 [roadmap](../roadmap/roadmap.md)；当前候选阶段（Phase 45–47）与进入条件见 [当前迭代](../roadmap/current-iteration.md)；现行声学行为见 [内置音源](../reference/features/builtin-piano-synthesis.md) 与 [架构](../reference/architecture.md)。本页不维护未来任务。

---

## 1. 阶段总览与交付边界

Phase 40–44 是 2026-11-24 复审后的规划，替代旧 Phase 40–42 的同音弦耦合、音板频变阻抗与能量驱动张力非线性候选。五阶段按“先证据、后低风险修复、再频谱/响度重标定、再整琴总线、最后空间/参考音色”排序：

| 阶段 | 主题 | 结论 |
|---|---|---|
| Phase 40 | 全键感知响度与频谱证据基线 | 完成；120 例 48 kHz/24-bit 证据与 8 复音数据归档于仓库外 |
| Phase 41 | 低风险声学缺陷修复与清理 | 完成；除预期路径外 48 kHz 声音不变 |
| Phase 42 | 低/高音频谱重塑与音区响度重标定 | 完成；1a/1b/2/3a/3b/4/5 全部交付并复测 |
| Phase 43 | 整琴共享声学总线收敛 | 完成；43-1 总线所有权、43-2 采用 12 复音 |
| Phase 44 | 空间、参考音色与细节收敛 | 完成；44-1 RoomReverb、44-2 NY 风格、44-3 微观细节 |

Phase 40 是证据门禁；Phase 41 在 Phase 40 记录 before 基线后可并行；Phase 42 依赖 Phase 40/41；Phase 43 依赖 Phase 42 的声音目标；Phase 44 依赖 Phase 42/43。未跳过 Phase 40 直接改音区曲线或总线。

---

## 2. Phase 40：全键感知响度与频谱证据基线

**目标**：在修改任何 DSP 前，用当前 HEAD 的生产渲染路径建立 88 键（或每三半音 30 键）在 v41/70/98 的感知响度、真峰值与频谱基线，定位音区断层、C6 热点、低/高音频谱缺陷和复音/CPU 余量。本阶段只产出证据，不改 DSP、参数默认值、文件格式或 UI。

1. **渲染协议**：复用 `exportTakeAsWavFile` -> `BuiltinSynthesiser` / `PianoSynthVoice`，48 kHz/24-bit、block 512、Room wet 0、Master 1、中性 `HammerVoicing`、Brightness/Hardness/Resonance 0.5、老化 0、Duplex 0.15、琴盖全开、Player、CC64/66/67=0、踏板噪声 0、双层关闭；ADSR 使用 Settings 默认 0.01/0.20/0.80/0.30 s；记录 Voice 实际门控（attack 0.0002、decay 0.001、sustain 1.0、release 0.30 s）。
2. **指标**：起音后 0–400 ms K 计权短期响度（LUFS）、0–50 ms 起音峰值、0.1–0.3 s RMS、1.1–1.3 s / 0.1–0.3 s RMS 比值、True Peak、>2.5 kHz HF 占比与 centroid；35/36、47/48、71/72、80/81、88/89、95/96、100/101、107/108 交界单列。
3. **模型核对**：用一次性诊断钩子读取生产路径实际算出的 `unisonScale`、`feltFilter(f1)`、`noteOutputGain` 与 `hammerTransient` 增益，复算 L1/L2 预算和声像功率；重点确认 C2–C6 的 `unisonScale` 全音域放大（C3/C4 约 +18.45/+14.56 dB）是否与起音/后窗失衡一致。模型与诊断只用于定位，不得替代生产路径测量；钩子在证据归档后移除。
4. **性能基线**：测量 8 复音下每声部成本、最大同时发声与偷声部次数，为 Phase 43 的复音评估提供数据。

**进入条件**：Phase 39.5 完成，当前 HEAD 可在 Windows Debug 构建；用户批准启动。**出口**：可复核的全键响度/频谱表、交界差表、热点定位与 CPU/复音数据；明确当前问题的主次顺序；一次性探针在证据归档后移除，不新增永久测试。**边界**：不改 DSP；若确认必须修改，进入 Phase 41/42。

**状态（2026-11-24）**：证据已采集并归档；Phase 40 完成。120 个 48 kHz/24-bit 案例（88 键 v70 + 16 代表键 v41/v98）及 8 复音密集序列归档于仓库外 `G:\source\projects\devpiano-evidence\phase40-baseline-20261124-run1`。生产诊断确认 C3/C4 `unisonScale` 为 `8.359/5.348`，后窗/前窗 RMS 为 `+3.80/+1.16 dB`；v70 全键 K 计权响度最大在 C2 `-24.48 LUFS`，最小在 MIDI 81 `-36.43 LUFS`，C5/C6 约 `-34 dB`；单音 v41/70/98 未触及 `softSaturate` 或 Master Limiter。Phase 42 参数已在后续 42-1a…42-5 中确定并完成。

---

## 3. Phase 41：低风险声学缺陷修复与清理

**目标**：修复与高音真实感、音区一致性直接相关且不改变整体声学拓扑的低风险缺陷；除预期路径外，48 kHz 既有声音保持不变。

1. **高音无制音器区**：`PianoSynthVoice::stopNote` 对 MIDI > 88 跳过 `adsrGate.noteOff()`（或改用不截断的自然释放），保留 `DamperTransient` 的木质回弹与 `sympatheticPool.noteOffKey`；验证 C8 快速离键、声部生命周期、偷声部与 Panic。
2. **琴盖采样率归一化**：把 141/344/507 采样抽头改为约 2.94/7.17/10.56 ms × sampleRate；48 kHz 行为不变，补 44.1/96 kHz 断言；不改变琴盖位置语义。
3. **死代码清理**：删除 `isBassBridge`、`decayDampingC`、`detuneCents` 与无生产消费者的旧辅助函数；清理 `RoomReverbEngine::sr`、`PerspectiveProcessor::sr`；测试改用行为断言，不重新钉住内部值。

**进入条件**：Phase 40 已记录当前 HEAD 的 before 基线；用户批准。**出口**：高音快速离键不再被 ADSR 截断；琴盖在 44.1/48/96/192 kHz 时间一致；清理后构建/测试通过；除预期修复外无音色变化；不保留兼容分支。**边界**：不改 L1/L2、不调音区锚点、不启动共享总线。

**状态（2026-11-24）**：已完成。`MIDI > 88` 松键跳过 ADSR release，保留木质回弹并自然衰减至 `allPartialsSilent()`；`allowTailOff=false` 保持立即重置。琴盖抽头按 48 kHz 时间常量换算，48 kHz 精确落回 `141/344/507`，延迟线上限 2048 覆盖 192 kHz；`PianoPedalAcoustics` 改为堆持有以保持 `AudioEngine` 内联占位稳定。删除死字段/无消费者辅助函数并迁移测试。Windows Debug app/tests、默认 CTest 通过；抽头测试覆盖 44.1/48/96/192 kHz；48 kHz 21–88 键 held-note WAV 与 Phase 40 基线 SHA-256 逐文件一致。

---

## 4. Phase 42：低/高音频谱重塑与音区响度重标定

**目标**：在 Phase 40 基线上修复低音“频谱贫困”、高音“音板/基频被压”、音区曲线“峰值校准/微笑曲线”、`unisonScale` 全音域放大（C3/C4 约 +18.45/+14.56 dB）与声像翘曲；以 K 计权短期响度 + True Peak 重标定 `PianoRegisterVoicing`。以下 1a/1b/2–5 分步 A/B，不把多个机制合并进同一提交。

1a. **瞬态解耦（低风险）**：将 `hammerTransient` 的主木核与 3 ms 裂音从 `unisonScale` 解耦，并将 hammer/damper 瞬态从 `noteOutputGain` 解耦，改用温和独立的机械音区包络；字符串模型不变，单独 A/B 确认 C3/C4 起音不再比 A0/C8 突出 15–18 dB。

1b. **同分音同相与退役 unisonScale（行为变更）**：同一分音的三根弦使用同一初始相位（保留 `kOptPhaseTable[0]` 的跨分音色散），由 `beatingDetuneRatio` 自然产生拍频与 Mid-Side 展开；退役 `gUnisonExcitation` 的 80–100 收敛补丁与 `unisonWindowPower`/`unisonScale`。以新基线验证 C3/C4“先瘪后涨”、E6 二次热点与点击/本体比消失，再重标定音区曲线。

2. **低音**：按证据评估低音分音预算（32–40，仅 MIDI <48，平滑过渡），调整低音 `1/m^rolloff` 与毛毡截止，加入 <80 Hz 温和辐射滚降；不以“分音数增加”单独作为成功依据。

3. **高音**：为音板湿路径补高频漫射支路，或让 C6+ 的 4.2 kHz 截止随音高上移/旁路；保证 `fc_ham` 不低于当前基频；改进木质琴槌冲击模态；必要时在 100–108 保留极小残余失谐。每项单独 A/B，不引入隐藏 EQ。

4. **归一化与声像**：确定单一音符级归一化目标（0.1–0.3 s RMS + 峰值上限），评估 L1/L2 取舍；改等功率声像；在 1b 之后确认字符串归一化与多弦能量尺度，不保留双路径。

5. **音区曲线**：用 Phase 40 同协议复测并重标定；目标全音域平滑、相邻半音 ≤1.5 dB、无 C3 凹坑/C6 热点、limiter 不抽吸，残余修正控制在 ±2–3 dB。默认中性或可选 Classical 预设由用户敲定。

**进入条件**：Phase 40 基线 + Phase 41 低风险修复完成；用户批准。**出口**：1a/1b/2/3/4/5 各自 A/B 与 Phase 40 同协议复测、Windows Debug 实机试听（低/中/中高/高、和弦、踏板）与全键频谱证据；1b 必须新基线且不与音区曲线同提交；不以更响当音色改善；实时/离线一致。**边界**：不启动共享总线、不升级 RoomReverb、不改复音数。

**状态（2026-11-24）**：Step 42-1a（`ef2c6d3`）与 Step 42-1b 已完成。1a 将 hammer/damper 从 `unisonScale`/`noteOutputGain` 解耦；机械点击内部降低约 20 dB，但 WAV 边界由弦体主导，1a 单独未改变 35/36、47/48。1b 改为同分音同相 + 固定 `1/N` 平均弦数尺度，退役 `gUnisonExcitation`/`unisonWindowPower`/`unisonScale` 及 `kOptPhaseTable` 第 1/2 行；新基线全键 v70 K 计权范围 `-33.65…-25.58 LUFS`（8.07 dB），35/36 `-0.09/-0.16 dB`、47/48 `-0.64/-0.51 dB`，C3/C4 后窗/前窗 `-19.28/-18.91 dB`，无 softSaturate/limiter。96–108 旁路 LidAcoustics 对照：正常最大相邻 LUFS 2.52 dB，旁路 0.12 dB，确认高音锯齿由固定三抽头琴盖引起。Step 42-3a 已完成：2 级串联 Schroeder 全通扩散器替代三抽头梳状，96–108 最大相邻 LUFS 降至 0.12 dB，全键 v70 范围降至 6.997 dB，35/36、47/48、71/72 边界未退化；`fullOpen/halfStick/closed` 均值 `-29.24/-30.63/-34.09 LUFS`。Step 42-3b 已完成：hammer 截止下限 `max(2.5/Tc, (1.8 + 2.6 h_contact^2) f1)` 使 C7/C8 `feltFilter(f1)` 在 v41/70/98 恢复为 `0.773/0.794/0.820` 且单调，湿音板低通 4.2→8 kHz；96–108 最大相邻 LUFS 0.17 dB，全键 v70 范围 7.03 dB，无 limiter/soft-clamp。Step 42-2 已完成：`maxPartials` 36，低音分音预算 36/28/18/14/8/6，低频辐射 `f/sqrt(f^2+70^2)`，A0 滚降 1.20→0.90 并在 keyPos 0.45 平滑回到现行，低音 `tcBase` 3.0→2.2 ms；全键 v70 范围 5.75 dB，35/36、47/48、59/60、71/72 边界 ≤0.68 dB，96–108 0.17 dB；8 复音标准序列 2.51 ms/block 平均、6.42 ms 最大，低音压力 3.40/8.42 ms（Debug）。Step 42-4 已完成：线性 `sqrt2` 声像改为等功率 `cos/sin`，中央 0.7071 不变，A0/C1/C8 分别降低 1.68/1.48/1.74 dB；L1/L2 对照保留 L1（L2 True Peak 0.98、892 次 limiter；L1 0.383、0 次）。Step 42-5 已完成：8 锚点重标定为 `+5.8/+3.0/+1.2/0/+0.2/-0.2/-3.0/-2.5 dB`；全键 v70 极差 0.619 dB，C2–C7 0.541 dB，87 个相邻半音最大 0.210 dB，96–108 0.112 dB，16 代表键 v41/70/98 极差 0.622/0.570/0.544 dB；最大 True Peak 0.236，无 softSaturate/limiter。Phase 42 已完成；Phase 43/44 已完成。

---

## 5. Phase 43：整琴共享声学总线收敛

**目标**：把每声部音板/交感/琴盖/视角收敛为 `BuiltinSynthesiser` 持有的单一 `PianoWholeBodyBus`，修复声部结束 `reset()` 截断音板/琴盖尾音、跨音符交感缺失和 9 份空间链路重复。本阶段是行为变更，必须单独批准并建立新基线。

1. **总线边界**：总线拥有音板、整琴交感池、Lid、Perspective；交感池先明确是 12 音级近似还是 88 键模型，不在本阶段顺手改模型。
2. **Voice 输出契约**：`PianoSynthVoice` 只输出 bridge drive + 音符增益；`PianoPedalAcoustics` 只输出机械瞬态 + shock 激励；总线统一处理并逐子块连续推进，状态预分配、无锁零分配。
3. **尾音/active 契约**：voice 结束不 reset 总线；总线在尾音静音后仅停止处理、不硬清空；Panic / `allNotesOff(0,false)` / CC120（无其他活动声部）/ 采样率重准备 / rebuild 才 reset；实时与 `WavFileExporter` / `PluginOfflineRenderer` 同构。
4. **复音评估**：以 Phase 40/本阶段 CPU 基线决定 8→12/16；先测后改，不直接跳到 32–48。

**进入条件**：Phase 42 声音目标稳定；用户明确批准行为变更。**出口**：离线 A/B + Windows Debug 实机试听；尾音不再被声部 reset 截断；跨音符共鸣可听/可测；CPU 与复音数据；实时/离线一致；以新基线为准，不声称“零音色倒退”。**边界**：不顺手改弦耦合、张力非线性、88 键交感模型或通用路由。

**状态（2026-11-25）**：43-1 已完成：唯一 `PianoWholeBodyBus`、跨音符交感、voice 结束不截断尾音；42-5 全键指标零回退；8 复音 Debug 标准 1.69/3.66 ms、低音压力 2.44/5.35 ms。43-2 采用 **12 复音**：标准 2.37/6.71 ms，低音压力 3.11/7.83 ms，偷声部 24→20；16 复音低音压力最大 9.73 ms（周期 10.67 ms 的约 91%），未采用。Phase 44-1/44-2/44-3 已完成。

---

## 6. Phase 44：空间、参考音色与细节收敛

**目标**：在 Phase 42/43 稳定后提升空间感与参考音色，不改变核心声学拓扑。

1. **RoomReverb**：升级立体声输入、早期反射、FDN/频率相关阻尼与等功率干湿；保留 Studio/Chamber/Concert Hall，实时/离线同构；影响所有音源，单独 A/B。**状态（2026-11-25）**：Phase 44-1 已完成并验证（`RoomReverbEngine.h`）。
2. **NY Steinway D Classical 风格预设**：用当前内核重新校准的非零 `HammerVoicing` 锚点、琴盖/视角/Room 组合；默认中性不变；沿用现有预设 Schema；参考 WAV 的未知后级不作为 DSP 目标。**状态（2026-11-25）**：Phase 44-2 已完成；C4 v41/70/98 HF `-61.85/-39.91/-29.69 dB`，参考 `-60.04/-40.73/-28.24 dB`；16 键 v70 0.955 LUFS 极差，v98 True Peak 0.235，无 limiter；Standard 默认中性不变。
3. **细节**：木质琴槌冲击、纵波先驱、高音残余失谐、轻量机械噪声逐项试听，不堆叠。**状态（2026-11-25）**：Phase 44-3 已完成；Standard 全键 v70 0.650 LUFS、NY 0.917 LUFS；96–108 相邻最大 0.275/0.467 dB；C4 HF 跨度 5.79/32.24 dB；True Peak 0.236/0.242，0 limiter。NY 风格 Room 干湿由 0.18 收敛为 0.09，以消除 Room 尾音造成的极高音相邻波动；12 复音 + FDN Debug 平均 2.70/3.43 ms。Phase 40–44 全部完成。

**进入条件**：Phase 42/43 完成；用户批准。**出口**：Room 实际表面/离线测量；预设可切换/持久化；默认行为不变；参考趋势（pp→ff HF 跨度）有证据；不声称与 Pianoteq 微观实现一致。**边界**：Verlet 求解器、Upright↔Grand 变形、五麦克风阵列不进入本阶段。

---

## 7. 横切结论与遗留

### 已解决

- `unisonScale` 全音域放大与 C3/C4“先瘪后涨”：42-1b 同分音同相 + 固定 `1/N` 平均尺度退役旧补偿路径。
- 高音固定三抽头琴盖梳状：42-3a 全通扩散器；42-3b 湿音板低通随音高上移。
- 低音频谱贫困与 <80 Hz 辐射：42-2 分音预算、A0 滚降、低频辐射一阶平滑、`tcBase` 调整。
- 音区响度与声像：42-4 等功率声像；42-5 八锚点重标定，全键 v70 极差 0.619 dB。
- 声部结束截断音板/琴盖尾音与跨音符交感缺失：43-1 单一 `PianoWholeBodyBus`。
- 复音与 CPU：43-2 采用 12 复音；16 复音 Debug 低音压力 9.73 ms 未采用。
- 空间感：44-1 立体声早期反射 + 4×4 Hadamard FDN + 频率相关阻尼 + 等功率干湿。
- 参考音色：44-2 NY Steinway D Classical 三锚点整音与 Room 组合。
- 微观细节：44-3 木质模态、纵波先驱共享音板、极高音残余失谐。

### 未进入本阶段、保留为研究候选

- 同音弦能量耦合、音板频变阻抗、能量驱动张力非线性：Phase 40–44 复审时转入“近期不排期”，不因阶段完成自动启动。
- 完整 Verlet 琴槌—琴弦求解器、连续弦长 Upright↔Grand 琴型变形、五麦克风 3D 阵列、更多机械噪声旋钮：未进入。
- 双极化弦、弦—音板双向耦合、3D 空气 FEM、波导主弦重写：未进入；若后续有独立证据与需求，再单独立项。

### 性能与验证边界

- 12 复音 Debug：标准 2.37/6.71 ms，低音压力 3.11/7.83 ms；43-1 8 复音标准 1.69/3.66 ms、低音压力 2.44/5.35 ms；44-3 12 复音 + FDN 平均 2.70/3.43 ms。
- 16 复音 Debug 低音压力 9.73 ms（512/48 kHz 周期 10.67 ms 的约 91%），未采用。
- 未执行 Release 测量；Debug 单次耗时不得外推为 Release 性能或长期 SLA 认证。
- 验证形态：Windows Debug app/tests、默认 CTest、格式检查、离线 WAV 测量与用户实机试听；未认证硬件矩阵、商业插件组合、超窗尾音或全平台实时稳定性。

### 证据入口

- Phase 40 基线：仓库外 `G:\source\projects\devpiano-evidence\phase40-baseline-20261124-run1`（120 例与 8 复音数据）。
- Phase 39 留下的证据治理与外部参考边界保持有效，见 [Phase 39 归档](phase39-acoustic-baseline-and-voicing-calibration.md)；本轮不重放已消费校准。
- Phase 42/43/44 的测量数值保留在本页状态段；原始 WAV/分析产物按当时记录保存在 Windows 构建目录或仓库外证据根，不随仓库分发大体积资产。

---

## 8. 归档后的现行契约

- 当前声学行为、88 键参数来源、12 复音配置与实时契约：[内置音源](../reference/features/builtin-piano-synthesis.md)。
- 当前模块所有权与 DSP 拓扑：[架构](../reference/architecture.md)。
- 当前阶段顺序与 Phase 45–47 候选：[roadmap](../roadmap/roadmap.md) 与 [当前迭代](../roadmap/current-iteration.md)。
- 本页只保留 Phase 40–44 的历史事实；不把归档中的已完成状态写成未来计划，也不在归档中追加新任务。