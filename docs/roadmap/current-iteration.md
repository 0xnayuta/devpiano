# devpiano Current Iteration

> 用途：维护当前候选阶段、恢复条件与实施顺序；项目状态和长期阶段顺序以 [roadmap](roadmap.md) 为准。
> 归档口径：Phase 39、Phase 39.5 与 R0–R11 的任务、实施记录、验收、失败历史和证据已迁入 `docs/archive/`。本页不重复维护历史状态，也不把归档记录当作当前默认或已通过结论。
> 当前状态：Phase 39.5（JIVE 内核精简与 JIVE v2 精准移植）**已完成（2026-10-23）并归档**；完整任务、tidy 收口与独立复审修复记录见 [Phase 39.5 归档](../archive/phase39-5-jive-core-pruning-and-v2-selective-port.md)。Phase 39 已冻结，Task 39-2 仍为 **inconclusive**，Phase 39 不记为通过；R0–R11 已完成并归档；Phase 40 证据已采集，Phase 41 已完成；Phase 42 客观验收已完成（2026-11-25，待用户人工试听签收）；Phase 43/44 尚未启动，启动须由用户明确决定；E3 有限尾音维护任务已实现并独立记录（见 §3）。

## 0. 归档入口

- Phase 39 可复核声学基线与三力度整音校准：[历史归档](../archive/phase39-acoustic-baseline-and-voicing-calibration.md)。Task 39-2 外部参考直接验收缺口、已消费的资格/采集/判决和“不注入 B 表”的收尾结论保持有效。
- Phase 39.5 JIVE 内核精简与 JIVE v2 精准移植：[历史归档](../archive/phase39-5-jive-core-pruning-and-v2-selective-port.md)。39.5-1 至 39.5-5、tidy 收口与独立复审修复的完整记录保留在归档；现行 UI 契约以 [声明式 UI 功能文档](../reference/features/declarative-ui-and-theming.md) 与 [ADR-014](../decisions/ADR-014-internalize-ui-infrastructure-and-deprecate-jive-submodule.md) 为准。
- R0–R11 内置钢琴音源结构整理：[历史归档](../archive/r0-r11-physical-piano-source-refactor.md)。R5/R6 踏板所有权迁移与 R7 新声部 sustain 初始化是已批准行为变化；R1/R2/R3/R9/R10 为组件提取与封装。
- Phase 37/38 历史任务、声音基线和后续修复：[完成归档](../archive/phase37-38-piano-calibration-and-keyboard-performance.md)。

## 1. 后续阶段路线（Phase 40–44）

Phase 40–44 是 2026-11-24 复审后的当前规划，替代旧 Phase 40–42 的同音弦耦合、音板频变阻抗与能量驱动张力非线性候选。Phase 40 证据已采集，Phase 41 已完成，Phase 42 客观验收已完成（2026-11-25，待用户人工试听签收），Phase 43/44 尚未启动。五阶段按“先证据、后低风险修复、再频谱/响度重标定、再整琴总线、最后空间/参考音色”排序；后续步骤启动须用户明确决定。

### Phase 40：全键感知响度与频谱证据基线

用当前 HEAD 的生产渲染路径建立 88 键（或每三半音 30 键）× v41/70/98 的 K 计权短期响度、0–50 ms 起音峰值、0.1–0.3 s RMS、1.1–1.3 s / 0.1–0.3 s RMS 比值、True Peak、HF/centroid 与交界差表，并测量 8 复音 CPU/偷声部余量。用一次性诊断钩子读取生产路径的 `unisonScale`、`feltFilter(f1)`、`noteOutputGain` 与 `hammerTransient` 增益，重点确认 C2–C6 的 `unisonScale` 全音域放大（C3/C4 约 +18.45/+14.56 dB）与起音/后窗失衡。本阶段不改 DSP；一次性探针在证据归档后移除。**状态（2026-11-24）**：证据已采集，待审阅；120 例与 8 复音数据在仓库外 `phase40-baseline-20261124-run1`。

### Phase 41：低风险声学缺陷修复与清理

修复 MIDI > 88 的 `stopNote` ADSR 截断（保留木质回弹与交感释放）；把琴盖 141/344/507 抽头改为毫秒 × sampleRate；删除 `isBassBridge`/`decayDampingC`/`detuneCents` 与无消费者辅助函数。除预期路径外 48 kHz 声音不变；不改 L1/L2、不调音区锚点、不启动共享总线。**状态（2026-11-24）**：已完成；Windows Debug app/tests、默认 CTest 通过，48 kHz 21–88 键 held-note WAV 与 Phase 40 基线 SHA-256 逐文件一致。

### Phase 42：低/高音频谱重塑与音区响度重标定

在 Phase 40 基线上修低音频谱贫困、高音音板/基频被压、峰值校准造成的微笑曲线、`unisonScale` 全音域放大（C3/C4 约 +18.45/+14.56 dB）与声像翘曲；用 K 计权响度 + True Peak 重标定 `PianoRegisterVoicing`。先做 1a 瞬态解耦（hammer/damper 脱离 `unisonScale` 与 `noteOutputGain`，改用独立机械音区包络），再做 1b 同分音同相并退役 `unisonScale`/80–100 收敛补丁；随后 2 低音、3 高音、4 归一化与声像、5 音区曲线。目标全音域平滑、相邻半音 ≤1.5 dB、无 C3 凹坑/C6 热点、limiter 不抽吸。每项单独 A/B，不引入隐藏 EQ/逐键补偿表。**状态（2026-11-24）**：Step 42-1a（`ef2c6d3`）与 Step 42-1b 已完成。1b 新基线全键 v70 K 计权范围 `-33.65…-25.58 LUFS`（8.07 dB），35/36 `-0.09/-0.16 dB`、47/48 `-0.64/-0.51 dB`，C3/C4 后窗/前窗 `-19.28/-18.91 dB`；96–108 旁路 LidAcoustics 最大相邻 LUFS 从 2.52 dB 降到 0.12 dB，确认高音锯齿来自固定三抽头琴盖。Step 42-3a 已完成：全通扩散器替代三抽头梳状后，96–108 最大相邻 LUFS 为 0.12 dB，全键 v70 范围 6.997 dB，35/36、47/48、71/72 未退化，三档琴盖均值 `-29.24/-30.63/-34.09 LUFS`。Step 42-3b 已完成：C7/C8 `feltFilter(f1)` 在 v41/70/98 为 `0.773/0.794/0.820`，湿音板低通 8 kHz；96–108 最大相邻 LUFS 0.17 dB，全键 v70 范围 7.03 dB，无 limiter/soft-clamp。Step 42-2 已完成：低音分音预算 36/28/18/14/8/6，低频辐射 `f/sqrt(f^2+70^2)`，A0 滚降 0.90，低音 `tcBase` 2.2 ms；全键 v70 范围 5.75 dB，35/36、47/48、59/60、71/72 边界 ≤0.68 dB，96–108 0.17 dB；8 复音标准 2.51/6.42 ms，低音压力 3.40/8.42 ms（Debug）。Step 42-4 已完成：等功率声像，A0/C1/C8 降低 1.68/1.48/1.74 dB；L1/L2 对照保留 L1。Step 42-5 已完成：锚点 `+5.8/+3.0/+1.2/0/+0.2/-0.2/-3.0/-2.5 dB`；全键 v70 极差 0.619 dB，C2–C7 0.541 dB，最大相邻 0.210 dB，96–108 0.112 dB，v41/70/98 代表键极差 0.622/0.570/0.544 dB，True Peak 0.236，无 softSaturate/limiter。Phase 42 客观验收完成，待用户人工试听签收。

### Phase 43：整琴共享声学总线收敛

将每声部音板/交感/琴盖/视角收敛为 `BuiltinSynthesiser` 持有的单一 `PianoWholeBodyBus`；Voice 只输出 bridge drive + 音符增益，`PianoPedalAcoustics` 只输出机械瞬态 + shock 激励；总线状态连续、无锁零分配、实时/离线同构。修复声部结束 reset 截断尾音与跨音符交感缺失；8→16 复音先测后改。行为变更须单独批准。

### Phase 44：空间、参考音色与细节收敛

在 Phase 42/43 稳定后升级 RoomReverb 为立体声输入 + 早期反射 + FDN/频率相关阻尼与等功率干湿；新增“NY Steinway D Classical”风格预设（当前内核重新校准的非零 `HammerVoicing`，默认中性不变）；木质琴槌冲击/纵波先驱/高音残余失谐逐项试听。不改变核心拓扑，不声称与 Pianoteq 微观实现一致。

## 2. 恢复规则

- Phase 43–44 及“近期不排期”内容保留为候选；启动须用户明确决定，不因 Phase 39.5、R0–R11 归档、Phase 42 客观验收完成、构建通过或时间推移自动启动。Phase 42 客观验收已完成，停在 Phase 43 入口，待用户人工试听签收。
- 实施顺序：Phase 40 是证据门禁；Phase 41 在 Phase 40 记录 before 基线后可并行；Phase 42 依赖 Phase 40/41；Phase 43 依赖 Phase 42 的声音目标；Phase 44 依赖 Phase 42/43。不得跳过 Phase 40 直接改音区曲线或总线。
- 测试按风险驱动：Phase 41/43 的生命周期、尾音、实时/离线一致性和偷声部边界保留必要回归；Phase 42/44 的声音目标以离线测量 + Windows Debug 实机试听为主，不把文案、默认值回读或单次更响当测试目的；一次性探针在证据归档后移除。
- 旧 Phase 40–42 的同音弦能量耦合、音板频变阻抗、能量驱动张力非线性不再作为当前阶段入口，转入“近期不排期”研究候选；若后续有独立证据与需求，再单独立项。
- Phase 39.5 是 UI 基础设施阶段，不改变 Phase 39 的结论，也不作为 Phase 40–44 的进入条件。
- Phase 39 不补记为通过；已消费的独立验证、采集和判决不重放、不补批或放宽。未来外部参考校准须另立新任务。
- 实施时的构建、测试、实时/离线、文件保护和验证纪律仍按 [quickstart](../guides/quickstart.md)、[acceptance](../reference/acceptance.md) 与 [AGENTS.md](../../AGENTS.md)；默认仅 Windows Debug 验证，Release 仅在用户明确要求时执行。

## 3. 已完成维护任务：统一有限导出尾音（E3）

- E1 的正常 take-end 释放验证与修复已随 `ba21d06` 提交：内置/插件/双层路径按身份追踪补 NoteOff 与 CC64/66/67 Off，不再自动追加 CC120/CC123 截断尾音；take 内显式 Panic 与层/端点切换仍硬停。E2 已完成有限窗口测量、方案比较与证据归档。两者不改变 Phase 40–44 的未启动状态。
- E3 已实现并提交（`9f5034d`）：`WavExportOptions::tailSeconds` 默认 6.0 秒、闭区间 0–30 秒；内置、插件及双层路径共享该选项；0 表示不追加尾音，文件长度仍由时间线确定性计算。`ExportTailDurationTest` 与 `ExportTailEndTest` 已接入 `devpiano_tests` 的 CMake 源列表；2026-10-25 通过 `win-test` 在 Windows Debug 镜像树执行默认 CTest（1/1 通过），定向 `JiveCoreValues` 与 `ViewHost` 也通过。
- 当前 UI 使用默认值，尚无时长控件；不新增全局设置、持久化字段、Take/Preset 格式、自动检测或淡出。有限窗口仍可能在超窗处截断，不承诺任意音源或插件的自然尾音完整。
- 验证边界：Windows Debug 定向测试、默认 CTest、格式与 diff 检查；不执行 Release、WSL 全量构建、物理声卡或商业插件矩阵。
- 与 Phase 39.5 的关系：E3 与 UI 基础设施阶段相互独立，两者的改动与验证证据分开；WAV 进度内容继续经 `ViewHost` 管理生命周期。E3 已交付，不再作为 Phase 39.5 的前置或并行许可条件。