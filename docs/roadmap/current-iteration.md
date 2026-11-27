# devpiano Current Iteration

> 用途：维护当前候选阶段、恢复条件与实施顺序；项目状态和长期阶段顺序以 [roadmap](roadmap.md) 为准。
> 归档口径：Phase 39、Phase 39.5、R0–R11 与 Phase 40–44 的任务、实施记录、验收、失败历史和证据已迁入 `docs/archive/`。本页不重复维护已完成阶段的历史状态，也不把归档记录当作当前默认或已通过结论。
> 当前状态：Phase 40–44 已完成（2026-11-25）并归档（2026-11-26），完整记录见 [Phase 40–44 归档](../archive/phase40-44-acoustic-calibration-and-whole-body-bus.md)。当前无进行中阶段；Phase 45-1/45-2/45-3 已完成（2026-11-27）；Phase 46-1/46-2 已完成（2026-11-27）；**Phase 46-3 与 47-2/47-3 仍为候选**，进入须用户明确批准。Phase 39 已冻结，Task 39-2 仍为 **inconclusive**，Phase 39 不记为通过；Phase 39.5 与 R0–R11 已完成并归档；E3 有限尾音维护任务已实现并独立记录（见 §4）。

## 0. 归档入口

- Phase 40–44 全键声学基线、频谱重标定与整琴共享总线：[历史归档](../archive/phase40-44-acoustic-calibration-and-whole-body-bus.md)。120 例全键证据、42-1a…42-5 的分步交付、43-1/43-2 总线与 12 复音、44-1/44-2/44-3 的空间/参考音色/微观细节、失败取舍与验证边界保留在归档；现行行为以 [内置音源](../reference/features/builtin-piano-synthesis.md) 与 [架构](../reference/architecture.md) 为准。
- Phase 39 可复核声学基线与三力度整音校准：[历史归档](../archive/phase39-acoustic-baseline-and-voicing-calibration.md)。Task 39-2 外部参考直接验收缺口、已消费的资格/采集/判决和“不注入 B 表”的收尾结论保持有效。
- Phase 39.5 JIVE 内核精简与 JIVE v2 精准移植：[历史归档](../archive/phase39-5-jive-core-pruning-and-v2-selective-port.md)。39.5-1 至 39.5-5、tidy 收口与独立复审修复的完整记录保留在归档；现行 UI 契约以 [声明式 UI 功能文档](../reference/features/declarative-ui-and-theming.md) 与 [ADR-014](../decisions/ADR-014-internalize-ui-infrastructure-and-deprecate-jive-submodule.md) 为准。
- R0–R11 内置钢琴音源结构整理：[历史归档](../archive/r0-r11-physical-piano-source-refactor.md)。R5/R6 踏板所有权迁移与 R7 新声部 sustain 初始化是已批准行为变化；R1/R2/R3/R9/R10 为组件提取与封装。
- Phase 37/38 历史任务、声音基线和后续修复：[完成归档](../archive/phase37-38-piano-calibration-and-keyboard-performance.md)。

## 1. 当前候选阶段（Phase 45–47）

Phase 45-1/45-2/45-3 已完成（2026-11-27）；Phase 46-1/46-2 已完成，46-3 与 47-2/47-3 仍为候选。执行顺序为 47-1（已完成）→ 45-1（已完成）→ 45-3（已完成）→ 46-1（已完成）→ 46-2（已完成）→ 45-2（已完成）→ 46-3；47-2 按需。45-2 是跨阶段收尾项，不作为 46-1/46-2 的前置；46-3 在 45-2 之后单独 A/B。不得把三个阶段的 DSP 改动合并到同一提交或同一 A/B。

### Phase 45：同音重复状态与起音微观相位（Repetition State & Attack Micro-Phase）[45-1/45-2/45-3 已完成 2026-11-27]

**目标**：修复快速同音重复时旧弦状态被强制重置、相位表与当前参数不匹配、力度对两段衰减不敏感这三个可感知缺口；不改变主弦模态拓扑、复音数与 NoteOff 身份。

**现状证据**：

- `BuiltinSynthesiser::noteOn` 对同音 voice 先 `stopVoice(voice, 1.0f, true)`，再 `findFreeVoice`；`PianoSynthVoice::stopNote` 以 `relVel=1.0` 得到约 0.135 s 的 release；`PianoMainString::start` 重新初始化所有 `cosState/sinState/epsilon`。
- 当前 `kOptPhaseTable[64]` 的数值直接来自 `danielpodrazka/piano` 的 `3×64` `_PHASE_TABLE` 第 0 行（MIT，见 `THIRD-PARTY-NOTICES.md`）；Phase 18-B（commit `4e63ca2`，2026-08-22）整表采用，42-1b 后同分音同相，只保留单行。该表由上游模型与其参考样本训练，从未针对当前 devpiano 参数独立优化；Phase 42–44 已改频谱、响度、瞬态与湿音板低通，因此 45-2 的定位是独立重优化，不是刷新旧的内生表。
- `PianoNoteParams::slowWeight` 为固定音区表（0.20/0.18/0.15/0.12），`computePianoNoteSetup` 不随力度调制。

**任务与顺序**：

1. **45-1 同音 retrigger（行为变更，需单独批准）**：
   - `BuiltinSynthesiser::noteOn` 检测同音且 `isKeyDown()` 的 voice；命中时调用 `PianoSynthVoice::retriggerNote(velocity)`，不 `stopVoice`、不新建 voice；未命中时保持现有分配路径。
   - `PianoSynthVoice` 新增 `retriggerNote`：保留 `mainString` 状态，重新触发 `hammerTransient`、`hammerContactEngine`、`adsrGate.noteOn()`，更新 `noteOutputGain`/`mechanicalOutputGain`/`mechanicalPreGain`；声像保持（同音）。
   - `PianoMainString` 新增“重新激励”入口：保留 `cos/sin/epsilon`，重新计算每个 partial 的 `levelFast/levelSlow`/decay/bloom；旧 level 乘锤毡/制音器吸收系数（建议 0.4–0.6），再叠加新锤能量。新音高/新 voice 仍走现有 `start`。
   - NoteOff 身份不变：同一 voice、同一 MIDI note；`PlaybackIdentityTracker` 语义不变。不得为同音重复新增第二个持久 voice。
   - 验证：同音 8 连击（v41/70/98；间隔 30/60/120/200 ms）、轮指片段；逐次 LUFS/True Peak；12 复音偷声部计数不增加；录音回放 NoteOff 配对；Panic/踏板/采样率重准备。
   **状态（2026-11-27）**：已完成。`BuiltinSynthesiser::noteOn` 对同音且 `isKeyDown()` 的 voice 复用 `PianoSynthVoice::retriggerNote`；`PianoMainString::retrigger` 保留 `cos/sin/epsilon`，旧能量按 0.50 吸收后叠加新锤能量。Windows Debug CTest 通过；120 例单音基线仍精确 PASS（v70 极差 0.650 dB、相邻最大 0.275 dB、v98 True Peak 0.236）；8 连击探针 v41/70/98 全程 True Peak 0.0871/0.1599/0.2301，未超过单音 0.236；8 连击复用同一 voice，第二 voice 始终空闲，总线按键计数不重复。证据在 `G:\source\projects\devpiano\build-win-msvc\acoustic-probe\phase45-1` 与 `phase45-1-retrigger`。
2. **45-3 力度依赖两段衰减（行为变更，需单独批准）**：
   - 在 `computePianoNoteSetup` 中让 `slowWeight` 随力度小幅调制（弱音更多 aftersound，强音更多 prompt）；建议 v41 +15%、v98 −15%，由现有 L1 归一化吸收。
   - 不新增逐键补偿表；不改 `fastDecayRatio` 的现有音区结构。
   - 验证：v41/70/98 的 1.1–1.3 s / 0.1–0.3 s RMS 比值、全键 LUFS/True Peak、A/B。
   **状态（2026-11-27）**：已完成。`computePianoNoteSetup` 以 v41 +15%、v98 −15% 线性调制 `slowWeight`，`PianoSynthVoice` 使用 `setup.slowWeight`；L1 总幅度不变。120 例 v70 参考 PASS（极差 0.649 dB、相邻最大 0.276 dB、True Peak 0.235）。1.1–1.3 s / 0.1–0.3 s RMS 比值：v41 平均 0.1761→0.1890（+7.3%），v70 0.1578→0.1576（−0.15%），v98 0.1779→0.1646（−7.5%）；逐键 v41 +6.1…12.5%，v98 −13.0…−6.2%。v41/v98 平均 LUFS 变化 +0.068/−0.069 dB；8 连击 True Peak 0.0872/0.1599/0.2297。证据在 `G:\source\projects\devpiano\build-win-msvc\acoustic-probe\phase45-3` 与 `phase45-3-retrigger`。
3. **45-2 相位表实验与独立重优化（收尾项，依赖 47-1/45-1/45-3/46-1/46-2）**：
   - 候选矩阵：A = 当前单行（上游第 0 行）；B = 上游三行（恢复每弦独立相位，实验分支）；C1 = 独立重优化单行；C2 = 独立重优化三行。B/C2 需要 `MainStringStart`/`PianoMainString` 接受每弦相位输入；先以实验 API 实现，不直接改生产默认。
   - 42-1b 的同相单行是已校准基线；恢复 B 会改变 t=0 相位/响度/立体声，必须独立批准、全键复测；B 不优于 A 时回退单行。
   - 独立重优化使用合法参考（Salamander CC BY 3.0 或用户提供），不使用来源不可信的旧 Pianoteq WAV；目标为 0–50 ms mel-STFT loss + 起音峰值因子/谱通量。
   - 对比 A/B/C1/C2：C1 优于 A 则采用 C1 单行；C2 优于 B 且 B 优于 A 才考虑 C2 三行；若 C 不显著优于 A/B，保留 A（或 B，若 B 已单独通过）。不得把上游表改称 devpiano 实测最优。
   **状态（2026-11-27）**：已完成，生产默认保留 A。参考为 Salamander Grand Piano V3（Alexander Holm，CC BY 3.0）48 kHz/24-bit；本环境 Archive.org 下载端点 TLS 失败，使用官方 SFZ Instruments GitHub FLAC 重映射（同一 V3 48k/24-bit，不是 danielpodrazka MP3），取 C2–C8 × v41/v70/v98 共 21 个起音锚点。实际引擎 A/B/C1/C2 平均 mel-STFT 损失 2.174/2.050/2.177/1.966，平均峰值 0.0963/0.0295/0.0935/0.0557：B 峰值下跌约 69%，不优于 A；C2 虽谱损失更低但峰值下跌约 42%，且 B 未通过；C1 未优于 A。按计划保留 A 单行，不改生产相位表。默认 120 例 v70 基线 PASS（0.628/0.283/0.235）；Windows Debug CTest 通过。实验 API、候选数据和工具见 `tools/phase_candidates.json`、`tools/phase_optimizer.py`、`tools/phase_eval.py`；证据在 `G:\source\projects\devpiano\build-win-msvc\phase-experiment-4` 与 `acoustic-probe\phase45-2-A`。

**进入条件**：Phase 40–44 已归档；47-1 先完成；用户明确批准 45-1/45-3 的行为变更；45-2 已完成；当前 HEAD 可在 Windows Debug 构建。

**实施边界**：不改主弦模态拓扑、不改 12 复音配置、不引入非线性锤求解、不重写波导、不新增逐键 EQ/补偿表。45-2 的三行恢复只作为实验分支，不直接改生产默认；B/C2 的 API 变更与 A/C1 分开提交。

**验证与出口**：45-1/45-3 各自 A/B；NoteOff 身份与录音回放一致；12 复音偷声部不增加；全键 v41/70/98 K 计权 LUFS、True Peak、0–50 ms 起音频谱与相邻半音差；Windows Debug 实机试听 C4–C7 起音“struck vs plucked”；实时/离线一致。45-2 的 A/B/C1/C2 对比与最终默认选择单独记录，不阻塞 46-1/46-2。

**失败/回退**：45-1 若响度累积不可控或 ADSR 重新触发产生爆音，回退到“复用 voice 但仍重置弦状态”或保持现有 stop+new 路径；45-3 若全键 LUFS 回退超过 Phase 44 基线，回退固定 `slowWeight`；45-2 的 B 若导致 t=0 峰值/响度/立体声回退，回退单行；C1/C2 若收益不足，保留 A。

### Phase 46：整琴总线物理密度与空间网络（Whole-Body Modal Density & Spatial Network）[46-1/46-2 已完成 2026-11-27；46-3 待启动]

**目标**：在 Phase 43 已收敛的单一 `PianoWholeBodyBus` 内提升音板模态密度、桥耦合音区差异与 FDN 扩散质量；不引入弦—音板实时闭环。

**任务与顺序**：

1. **46-1 总线模态密度**：`PianoWholeBodyBus::numResonators` 从 16 增至 24–32，按“低频稀疏、中高频密集”布置；用公开音板模态测量或参考音符频响拟合频率/Q/权重，不直接卷积参考 IR。总线每采样只处理一次，成本按 24/32 个二阶滤波实测。
   **状态（2026-11-27）**：已完成。`numResonators` 16→24；保留 Phase 19 的 16 个 Bank/Chabassier 锚点，新增 300/400/540/710/940/1220/1510/1940 Hz 八个插值填充，低频 <250 Hz 仍 6 峰、中高频 18 峰；频率/Q/左右权重由相邻锚点插值，左右各自归一 1.0，不声称新的实测模态。120 例 v70 参考 PASS（极差 0.656 dB、相邻最大 0.280 dB、True Peak 0.235）；总输出 LUFS 逐键最大 ±0.081 dB，HF/centroid 基本不变，late/early +0.10%；体声响应在 300 Hz 附近填充约 +2.9/+2.4 dB（左/右）。Debug 12 复音 p50/p99 5.31/7.43 ms（45-3 4.55/5.99 ms），仍在 10.67 ms 周期内；非 Release SLA。证据在 `G:\source\projects\devpiano\build-win-msvc\acoustic-probe\phase46-1`。
2. **46-2 音区桥耦合权重**：`PianoSynthVoice::renderToBus` 的 drive 增加 per-note 音区权重（低音桥/中音桥/高音桥），不改 partial 阶数权重；与 46-1 分开提交、分开 A/B。
   **状态（2026-11-27）**：已完成。`drive` 乘 A0/C4/C8 三锚点连续权重 1.10/1.00/0.94；只缩放湿音板/交感/纵波驱动，直达声、partial 阶数权重、音区输出增益与声像不变。120 例 v70 参考 PASS（极差 0.628 dB、相邻最大 0.283 dB、True Peak 0.235）；逐键 LUFS 最大 ±0.028 dB，低频平均 +0.018 dB、高频平均 −0.010 dB；HF/centroid 与 late/early 基本不变。Debug 12 复音 p50/p99 5.08/7.40 ms（46-1 5.31/7.43 ms）。证据在 `G:\source\projects\devpiano\build-win-msvc\acoustic-probe\phase46-2`。
3. **46-3 8 线 Householder FDN**：`RoomReverbEngine` 从 4×4 Hadamard 升为 8 线 Householder（`A = I - (2/N)11^T`），保留现有 early taps、等功率干湿与 RT60 参数；每采样仅增加加法与一次乘。先确认 8 条延迟线长度互质且 RT60 换算仍按 48 kHz 参考。46-3 在 45-2 之后单独 A/B。
4. **46-4 性能证据（需用户明确批准 Release）**：测 p50/p99 frame time、12/16 复音、低音压力；再决定是否需要 SoA/SIMD。不以 Debug 9.73 ms 作为重构依据；不把 Release 测量写成默认门禁。

**进入条件**：45-1/45-3 声音目标稳定；45-2 不作为 46-1/46-2 的前置；用户批准总线/FDN 行为变更；Phase 40–44 基线仍可复现。

**实施边界**：不做弦—音板双向实时闭环、不做 3D 空气 FEM、不重写波导主弦、不改 88 键交感模型、不改变总线所有权与尾音契约。

**验证与出口**：全键 LUFS/True Peak/HF/centroid 不回退；RT60 与尾音 A/B；12 复音 CPU 不超预算；Release 测量结论单独记录；46-1/46-2/46-3 各自单独 A/B，不合并提交。

**失败/回退**：模态密度若只增加CPU不改善频谱，回退 16 模态；桥权重若破坏音区平衡，回退中性权重；FDN 若尾音密度无改善或 RT60 漂移，回退 4×4 Hadamard。

### Phase 47：离线声学校准与参数单源（Offline Acoustic Calibration & Parameter SSOT）

**目标**：把 Phase 40/44 的一次性测量沉淀为可复现的开发期工具，并收敛钢琴参数的跨层重复；不进入产品包，不引入远程配置。

**任务与顺序**：

1. **47-1 离线测量/回归工具**：复用 `WavExportOptions` 与现有导出路径，输入 MIDI/参数，输出 88 键 LUFS、True Peak、0–50 ms 谱、HF/centroid、相邻半音差与 frame time 的 JSON；工具放在开发期脚本/测试侧，不新增产品 UI 或运行时依赖。先用 Phase 40/44 已记录数值做回放校验。该工具是 45-2 的前置，可先于 45-1/45-3 启动。
   **状态（2026-11-27）**：已完成。`devpiano_tests --acoustic-probe` 渲染 120 例 48 kHz/24-bit WAV（88 键 v70 + 16 代表键 v41/v98），`tools/acoustic_probe.py` 输出 LUFS/True Peak/起音/RMS/HF/centroid/相邻差与 12 复音 benchmark。Phase 44-3 Standard 回放校验 PASS：v70 LUFS 极差 0.650 dB、相邻最大 0.275 dB、v98 True Peak 0.236；Debug 12 复音 p50 4.41 ms、p99 约 5.7–5.9 ms、max 约 7.3 ms（单次运行，非 SLA）。证据在 `G:\source\projects\devpiano\build-win-msvc\acoustic-probe\phase40`；工具不进入产品包。
2. **47-2 参数 descriptor 表**：为钢琴声学参数定义单一 descriptor（id、默认、范围、序列化 key、locale key、UI 控件）；先让下一次新增参数走通全链路，旧字段逐步迁移，不一次性重写 `SettingsModel`/`Preset`。加一个测试确保 descriptor 覆盖 `PianoStyleParameters` 字段。
3. **47-3 参考指标（可选）**：在离线工具中加入与参考录音的 MFCC-L2/谱质心差，作为音色回归，不作为实时 DSP 目标。

**进入条件**：47-1 已完成（2026-11-27），45-2 的前置之一已满足；45-2 仍须等 45-1/45-3/46-1/46-2；47-2 在产生新参数或需要参数收敛时启动，不强制排期。

**实施边界**：不做远程 SSOT、不做自动迁移链、不把工具打进发布包、不为假想未来预建通用参数框架。

**验证与出口**：工具可复现 Phase 40/44 指标；descriptor 覆盖检查通过；新增一个参数从定义到 UI/预设/持久化走通；不新增通用参数框架。

## 2. 研究候选与不排期

以下方向保留为研究候选，不因 Phase 45–47 规划自动启动；需要独立证据、设计与用户批准后再立项：

- **第二极化弦**（fan455 的 u/v/w/α/β 参考）：先在低音区做离线原型，验证相对现有双指数包络的听感收益；不重写主弦。
- **跨音符总线记忆/弦—音板双向耦合**（bBpiano/fan455）：需稳定性与能量守恒设计；当前不做实时闭环。
- **完整非线性锤求解**（bBpiano 的 Maxwell 锤 + Newton/二分）：与实时预算和 88 键响度校准冲突，不进入。
- **3D 空气 FEM、波导主弦重写、Upright↔Grand 连续变形、五麦克风阵列、完整 Verlet 琴槌—琴弦求解器**：不进入。
- **外部参考边界**：bBpiano 为闭源专有实现，只借鉴公开文档/讨论中的设计思想，不克隆实现或参数；fan455 代码未完成且依赖 GMSH/MKL，只作方法与参数参考；Instrudio 只借鉴 SSOT 工程模式；danielpodrazka/piano 只借鉴离线相位/音板校准方法，不引入其离线采样架构；Phase 18-B 采用的相位表数值已按 MIT 归属登记，后续不得重新包装为自研优化。
- 研究候选不以参考报告的“极低代价”估计替代设计和验收，也不为这些展望预留实现。

## 3. 恢复规则与实施顺序

- Phase 45-1/45-2/45-3 已完成（2026-11-27）；Phase 46-1/46-2 已完成，46-3 与 47-2/47-3 尚未启动。本页只维护规划与进入条件，不把未启动项写成已实现。
- 实施顺序：47-1（已完成 2026-11-27）→ 45-1（已完成 2026-11-27）→ 45-3（已完成 2026-11-27）→ 46-1（已完成 2026-11-27）→ 46-2（已完成 2026-11-27）→ 45-2（已完成 2026-11-27）→ 46-3；47-2 按需。45-2 是跨阶段收尾项，不阻塞 46-1/46-2；46-3 在 45-2 之后单独 A/B。45-1/45-3/46-1/46-2/46-3 各自单独提交，46-4 需用户明确批准 Release。
- 测试按风险驱动：Phase 45 保留 NoteOff 身份、录音回放、复音/偷声部与 ADSR 生命周期回归；Phase 46 保留尾音/总线所有权、实时/离线一致性与 12 复音边界；Phase 47 只保留工具回放与 descriptor 覆盖检查。声音目标以离线测量 + Windows Debug 实机试听为主，不把文案、默认值回读或单次更响当测试目的；一次性探针在证据归档后移除。
- 默认仅 Windows Debug 验证；Release 仅在用户明确要求时执行。构建、测试、实时/离线、文件保护和验证纪律仍按 [quickstart](../guides/quickstart.md)、[acceptance](../reference/acceptance.md) 与 [AGENTS.md](../../AGENTS.md)。
- Phase 39 不补记为通过；已消费的独立验证、采集和判决不重放、不补批或放宽。未来外部参考校准须另立新任务。

## 4. 已完成维护任务：统一有限导出尾音（E3）

- E1 的正常 take-end 释放验证与修复已随 `ba21d06` 提交：内置/插件/双层路径按身份追踪补 NoteOff 与 CC64/66/67 Off，不再自动追加 CC120/CC123 截断尾音；take 内显式 Panic 与层/端点切换仍硬停。E2 已完成有限窗口测量、方案比较与证据归档。E1/E2/E3 与 Phase 40–44 的阶段收口独立。
- E3 已实现并提交（`9f5034d`）：`WavExportOptions::tailSeconds` 默认 6.0 秒、闭区间 0–30 秒；内置、插件及双层路径共享该选项；0 表示不追加尾音，文件长度仍由时间线确定性计算。`ExportTailDurationTest` 与 `ExportTailEndTest` 已接入 `devpiano_tests` 的 CMake 源列表；2026-10-25 通过 `win-test` 在 Windows Debug 镜像树执行默认 CTest（1/1 通过），定向 `JiveCoreValues` 与 `ViewHost` 也通过。
- 当前 UI 使用默认值，尚无时长控件；不新增全局设置、持久化字段、Take/Preset 格式、自动检测或淡出。有限窗口仍可能在超窗处截断，不承诺任意音源或插件的自然尾音完整。
- 验证边界：Windows Debug 定向测试、默认 CTest、格式与 diff 检查；不执行 Release、WSL 全量构建、物理声卡或商业插件矩阵。
- 与 Phase 39.5 的关系：E3 与 UI 基础设施阶段相互独立，两者的改动与验证证据分开；WAV 进度内容继续经 `ViewHost` 管理生命周期。E3 已交付，不再作为 Phase 39.5 的前置或并行许可条件。