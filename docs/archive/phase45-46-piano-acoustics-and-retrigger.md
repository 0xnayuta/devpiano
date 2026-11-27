# Phase 45–46 历史归档：同音重击、力度衰减、相位实验与整琴总线密度

> 阶段：Phase 45–46。阶段状态：全部完成（2026-11-27）；45-1/45-2/45-3 与 46-1/46-2/46-3/46-4 已交付，Windows Debug CTest 与用户人工试听通过。
> 归档日期：2026-11-28。来源：归档前的 `docs/roadmap/current-iteration.md` 与 `docs/roadmap/roadmap.md`。
> 现行行为以 [内置音源](../reference/features/builtin-piano-synthesis.md) 与 [架构](../reference/architecture.md) 为准；本页只保留历史任务、测量与证据边界，不维护未来任务。

## Phase 45：同音重复状态与起音微观相位（Repetition State & Attack Micro-Phase）

**目标**：修复快速同音重复时旧弦状态被强制重置、相位表与当前参数不匹配、力度对两段衰减不敏感这三个可感知缺口；不改变主弦模态拓扑、复音数与 NoteOff 身份。

**规划时现状证据**：

- `BuiltinSynthesiser::noteOn` 对同音 voice 先 `stopVoice(voice, 1.0f, true)`，再 `findFreeVoice`；`PianoSynthVoice::stopNote` 以 `relVel=1.0` 得到约 0.135 s 的 release；`PianoMainString::start` 重新初始化所有 `cosState/sinState/epsilon`。
- 当前 `kOptPhaseTable[64]` 的数值直接来自 `danielpodrazka/piano` 的 `3×64` `_PHASE_TABLE` 第 0 行（MIT，见 `THIRD-PARTY-NOTICES.md`）；Phase 18-B（commit `4e63ca2`，2026-08-22）整表采用，42-1b 后同分音同相，只保留单行。该表由上游模型与其参考样本训练，从未针对当前 devpiano 参数独立优化；Phase 42–44 已改频谱、响度、瞬态与湿音板低通，因此 45-2 的定位是独立重优化，不是刷新旧的内生表。
- 规划时 `PianoNoteParams::slowWeight` 为固定音区表（0.20/0.18/0.15/0.12），`computePianoNoteSetup` 不随力度调制；45-3 已改为随力度调制（见任务 2 状态）。

**任务与顺序**：

1. **45-1 同音 retrigger（行为变更，需单独批准）**：
   - `BuiltinSynthesiser::noteOn` 检测同音且 `isKeyDown()` 的 voice；命中时调用 `PianoSynthVoice::retriggerNote(velocity)`，不 `stopVoice`、不新建 voice；未命中时保持现有分配路径。
   - `PianoSynthVoice` 新增 `retriggerNote`：保留 `mainString` 状态，重新触发 `hammerTransient`、`hammerContactEngine`、`adsrGate.noteOn()`，更新 `noteOutputGain`/`mechanicalOutputGain`/`mechanicalPreGain`；声像保持（同音）。
   - `PianoMainString` 新增“重新激励”入口：保留 `cos/sin/epsilon`，重新计算每个 partial 的 `levelFast/levelSlow`/decay/bloom；旧 level 乘锤毡/制音器吸收系数（建议 0.4–0.6），再叠加新锤能量。新音高/新 voice 仍走现有 `start`。
   - NoteOff 身份不变：同一 voice、同一 MIDI note；`PlaybackIdentityTracker` 语义不变。不得为同音重复新增第二个持久 voice。
   - 验证：同音 8 连击（v41/70/98；间隔 30/60/120/200 ms）、轮指片段；逐次 LUFS/True Peak；12 复音偷声部计数不增加；录音回放 NoteOff 配对；Panic/踏板/采样率重准备。
   **状态（2026-11-27）**：已完成。`BuiltinSynthesiser::noteOn` 对同音且 `isKeyDown()` 的 voice 复用 `PianoSynthVoice::retriggerNote`；`PianoMainString::retrigger` 保留 `cos/sin/epsilon`，旧能量按 0.50 吸收后叠加新锤能量。Windows Debug CTest 通过；120 例单音基线仍精确 PASS（v70 极差 0.650 dB、相邻最大 0.275 dB、v98 True Peak 0.236）；8 连击探针 v41/70/98 全程 True Peak 0.0871/0.1599/0.2301，未超过单音 0.236；8 连击复用同一 voice，第二 voice 始终空闲，总线按键计数不重复。证据在 `G:\source\projects\devpiano\build-win-msvc\acoustic-probe\phase45-1` 与 `phase45-1-retrigger`。用户人工试听（2026-11-27）：同音重击、力度/间隔矩阵与踏板组合未发现明显问题，可接受。
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
   **状态（2026-11-27）**：已完成，生产默认保留 A。参考为 Salamander Grand Piano V3（Alexander Holm，CC BY 3.0）48 kHz/24-bit；本环境 Archive.org 下载端点 TLS 失败，使用官方 SFZ Instruments GitHub FLAC 重映射（同一 V3 48k/24-bit，不是 danielpodrazka MP3），取 C2–C8 × v41/v70/v98 共 21 个起音锚点。实际引擎 A/B/C1/C2 平均 mel-STFT 损失 2.174/2.050/2.177/1.966，平均峰值 0.0963/0.0295/0.0935/0.0557：B 峰值下跌约 69%，不优于 A；C2 虽谱损失更低但峰值下跌约 42%，且 B 未通过；C1 未优于 A。按计划保留 A 单行，不改生产相位表。2026-11-28 以修正弦模型（屏蔽未使用弦列、应用生产 1/3 或 0.38 三弦缩放）重跑 C1/C2：A/B 不变，C1/C2 谱损失 2.192/2.004、峰值 0.0938/0.0723；C1 仍未优于 A，C2 峰值较 A 低约 25% 且 B 仍不合格，保留 A 的结论不变。默认 120 例 v70 基线 PASS（0.628/0.283/0.235）；Windows Debug CTest 通过。实验 API、候选数据和工具见 `tools/phase_candidates.json`、`tools/phase_optimizer.py`、`tools/phase_eval.py`；证据在 `G:\source\projects\devpiano\build-win-msvc\phase-experiment-4` 与 `acoustic-probe\phase45-2-A`。

**进入条件**：Phase 40–44 已归档；47-1 先完成；用户明确批准 45-1/45-3 的行为变更；当前 HEAD 可在 Windows Debug 构建。

**实施边界**：不改主弦模态拓扑、不改 12 复音配置、不引入非线性锤求解、不重写波导、不新增逐键 EQ/补偿表。45-2 的三行恢复只作为实验分支，不直接改生产默认；B/C2 的 API 变更与 A/C1 分开提交。

**验证与出口**：45-1/45-3 各自 A/B；NoteOff 身份与录音回放一致；12 复音偷声部不增加；全键 v41/70/98 K 计权 LUFS、True Peak、0–50 ms 起音频谱与相邻半音差；Windows Debug 实机试听 C4–C7 起音“struck vs plucked”；实时/离线一致。45-2 的 A/B/C1/C2 对比与最终默认选择单独记录，不阻塞 46-1/46-2。

**失败/回退**：45-1 若响度累积不可控或 ADSR 重新触发产生爆音，回退到“复用 voice 但仍重置弦状态”或保持现有 stop+new 路径；45-3 若全键 LUFS 回退超过 Phase 44 基线，回退固定 `slowWeight`；45-2 的 B 若导致 t=0 峰值/响度/立体声回退，回退单行；C1/C2 若收益不足，保留 A。

## Phase 46：整琴总线物理密度与空间网络（Whole-Body Modal Density & Spatial Network）

**目标**：在 Phase 43 已收敛的单一 `PianoWholeBodyBus` 内提升音板模态密度、桥耦合音区差异与 FDN 扩散质量；不引入弦—音板实时闭环。

**任务与顺序**：

1. **46-1 总线模态密度**：`PianoWholeBodyBus::numResonators` 从 16 增至 24–32，按“低频稀疏、中高频密集”布置；用公开音板模态测量或参考音符频响拟合频率/Q/权重，不直接卷积参考 IR。总线每采样只处理一次，成本按 24/32 个二阶滤波实测。
   **状态（2026-11-27）**：已完成。`numResonators` 16→24；保留 Phase 19 的 16 个 Bank/Chabassier 锚点，新增 300/400/540/710/940/1220/1510/1940 Hz 八个插值填充，低频 ≤250 Hz 仍 6 峰、中高频 18 峰；频率/Q/左右权重由相邻锚点插值，左右各自归一 1.0，不声称新的实测模态。120 例 v70 参考 PASS（极差 0.656 dB、相邻最大 0.280 dB、True Peak 0.235）；总输出 LUFS 逐键最大 ±0.081 dB，HF/centroid 基本不变，late/early +0.10%；体声响应在 300 Hz 附近填充约 +2.9/+2.4 dB（左/右）。Debug 12 复音 p50/p99 5.31/7.43 ms（45-3 4.55/5.99 ms），仍在 10.67 ms 周期内；非 Release SLA。证据在 `G:\source\projects\devpiano\build-win-msvc\acoustic-probe\phase46-1`。
2. **46-2 音区桥耦合权重**：`PianoSynthVoice::renderToBus` 的 drive 增加 per-note 音区权重（低音桥/中音桥/高音桥），不改 partial 阶数权重；与 46-1 分开提交、分开 A/B。
   **状态（2026-11-27）**：已完成。`drive` 乘 A0/C4/C8 三锚点连续权重 1.10/1.00/0.94；只缩放湿音板/交感/纵波驱动，直达声、partial 阶数权重、音区输出增益与声像不变。120 例 v70 参考 PASS（极差 0.628 dB、相邻最大 0.283 dB、True Peak 0.235）；逐键 LUFS 最大 ±0.028 dB，低频平均 +0.018 dB、高频平均 −0.010 dB；HF/centroid 与 late/early 基本不变。Debug 12 复音 p50/p99 5.08/7.40 ms（46-1 5.31/7.43 ms）。证据在 `G:\source\projects\devpiano\build-win-msvc\acoustic-probe\phase46-2`。
3. **46-3 8 线 Householder FDN**：`RoomReverbEngine` 从 4×4 Hadamard 升为 8 线 Householder（`A = I - (2/N)11^T`），保留现有 early taps、等功率干湿与 RT60 参数；每采样仅增加加法与一次乘。先确认 8 条延迟线长度互质且 RT60 换算仍按 48 kHz 参考。46-3 在 45-2 之后单独 A/B。
   **状态（2026-11-27）**：已完成。8 条 48 kHz 基础延迟为 `1601/1861/2027/2237/2371/2539/2683/2903`（互质质数）；Householder 变换使用一次求和+一次乘法并保持支路能量。early taps、等功率干湿、feedback/damping 参数未改。旧峰值阈值尾音测试改为 Schroeder T30；48 kHz T30 约 `1.22/4.36/7.77 s`（旧 4 线 `1.15/3.98/6.99 s`，增加约 7–11%），跨 44.1/48/96 kHz 测试通过；120 例无 Room 基线 PASS（0.628/0.283/0.235）；O2 单立体声流处理约 64 ns/sample（旧 4 线约 52 ns/sample），约 33 µs/512 块；无 Room 的 12 复音基线 p50/p99 4.90/7.34 ms。证据在 `acoustic-probe\phase46-3`。用户人工试听（2026-11-27）：Studio/Chamber/Concert Hall 尾音长度与空间感未发现明显问题，可接受。
4. **46-4 性能证据（Release）**：测 p50/p99 frame time、12/16 复音、低音压力；再决定是否需要 SoA/SIMD。不以 Debug 9.73 ms 作为重构依据；不把 Release 测量写成默认门禁。
   **状态（2026-11-27）**：已完成。Windows MSVC Release（`build-win-msvc-release`）、48 kHz/24-bit、block 512、warmup 94 blocks、5 s × 3 repeats、每配置 5 个独立进程；AMD Ryzen 5 5600X，Windows Ultimate Performance。基准只测 `BuiltinSynthesiser`（可选 `RoomReverbEngine`），不含 WAV I/O、`buffer.clear()`、AudioDevice/插件/limiter。5 进程 p50/p99 中位数与最大 max（ms）：

   | 配置 | p50 | p99 | max |
   |---|---:|---:|---:|
   | 12 中音 dry | 0.531 | 0.978 | 1.823 |
   | 12 中音 Room | 0.560 | 1.032 | 2.170 |
   | 12 低音 dry | 0.795 | 1.539 | 5.311 |
   | 12 低音 Room | 0.826 | 1.589 | 5.938 |
   | 16 中音 dry | 0.654 | 1.199 | 4.252 |
   | 16 中音 Room | 0.684 | 1.239 | 4.179 |
   | 16 低音 dry | 1.004 | 1.929 | 4.277 |
   | 16 低音 Room | 1.028 | 1.968 | 2.093 |

   **决策**：12 复音保留为产品默认；16 复音在 Release 下仍有充足余量（最差 p99 1.97 ms，约 18% 周期），但不改 `kPianoVoiceCount`，如需启用须单独批准并复测偷声部/尾音。Room 路径在 12/16 复音下 p50 仅增加约 0.02–0.04 ms、p99 增加约 0.04–0.05 ms，与 46-3 的 O2 单流 64 ns/sample 一致。少数进程出现 4–6 ms 的 max 离群（Windows 调度），p99 稳定，最大 max 5.94 ms 仍低于 10.67 ms 周期。Release 余量远高于 SoA/SIMD 门槛，**不做 SoA/SIMD**，性能线关闭。工具新增 `--benchmark-only`、`--benchmark-polyphony`、`--benchmark-notes`、`--bass-stress`、`--benchmark-warmup`、`--benchmark-repeats`、`--benchmark-room`、`--wet`；证据在 `G:\source\projects\devpiano\build-win-msvc-release\acoustic-probe\phase46-4`。

**进入条件**：45-1/45-3 声音目标稳定；45-2 不作为 46-1/46-2 的前置；用户批准总线/FDN 行为变更；Phase 40–44 基线仍可复现。

**实施边界**：不做弦—音板双向实时闭环、不做 3D 空气 FEM、不重写波导主弦、不改 88 键交感模型、不改变总线所有权与尾音契约。

**验证与出口**：全键 LUFS/True Peak/HF/centroid 不回退；RT60 与尾音 A/B；12 复音 CPU 不超预算；Release 测量结论单独记录；46-1/46-2/46-3 各自单独 A/B，不合并提交；46-4 测量单独记录。

**失败/回退**：模态密度若只增加 CPU 不改善频谱，回退 16 模态；桥权重若破坏音区平衡，回退中性权重；FDN 若尾音密度无改善或 RT60 漂移，回退 4×4 Hadamard。