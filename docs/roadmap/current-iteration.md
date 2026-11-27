# devpiano Current Iteration

> 用途：维护当前候选阶段、恢复条件与实施顺序；项目状态和长期阶段顺序以 [roadmap](roadmap.md) 为准。
> 归档口径：Phase 39、Phase 39.5、R0–R11、Phase 40–44 与 Phase 45–46 的任务、实施记录、验收、失败历史和证据已迁入 `docs/archive/`。本页不重复维护已完成阶段的历史状态，也不把归档记录当作当前默认或已通过结论。
> 当前状态：Phase 40–44 已完成（2026-11-25）并归档（2026-11-26）；Phase 45–46 已完成（2026-11-27）并归档（2026-11-28），完整记录见 [Phase 45–46 归档](../archive/phase45-46-piano-acoustics-and-retrigger.md)。当前无进行中阶段；Phase 47-1 已完成（2026-11-27）；**Phase 47-2/47-3 仍为候选**，进入须用户明确批准。Phase 39 已冻结，Task 39-2 仍为 **inconclusive**，Phase 39 不记为通过；Phase 39.5 与 R0–R11 已完成并归档；E3 有限尾音维护任务已实现并独立记录（见 §4）。

## 0. 归档入口

- Phase 45–46 同音重击、力度衰减、相位表实验与整琴总线/FDN：[历史归档](../archive/phase45-46-piano-acoustics-and-retrigger.md)。45-1/45-2/45-3 与 46-1/46-2/46-3/46-4 的任务、分步测量、失败回退、用户试听与证据边界保留在归档；现行行为以 [内置音源](../reference/features/builtin-piano-synthesis.md) 与 [架构](../reference/architecture.md) 为准。
- Phase 40–44 全键声学基线、频谱重标定与整琴共享总线：[历史归档](../archive/phase40-44-acoustic-calibration-and-whole-body-bus.md)。120 例全键证据、42-1a…42-5 的分步交付、43-1/43-2 总线与 12 复音、44-1/44-2/44-3 的空间/参考音色/微观细节、失败取舍与验证边界保留在归档；现行行为以 [内置音源](../reference/features/builtin-piano-synthesis.md) 与 [架构](../reference/architecture.md) 为准。
- Phase 39 可复核声学基线与三力度整音校准：[历史归档](../archive/phase39-acoustic-baseline-and-voicing-calibration.md)。Task 39-2 外部参考直接验收缺口、已消费的资格/采集/判决和“不注入 B 表”的收尾结论保持有效。
- Phase 39.5 JIVE 内核精简与 JIVE v2 精准移植：[历史归档](../archive/phase39-5-jive-core-pruning-and-v2-selective-port.md)。39.5-1 至 39.5-5、tidy 收口与独立复审修复的完整记录保留在归档；现行 UI 契约以 [声明式 UI 功能文档](../reference/features/declarative-ui-and-theming.md) 与 [ADR-014](../decisions/ADR-014-internalize-ui-infrastructure-and-deprecate-jive-submodule.md) 为准。
- R0–R11 内置钢琴音源结构整理：[历史归档](../archive/r0-r11-physical-piano-source-refactor.md)。R5/R6 踏板所有权迁移与 R7 新声部 sustain 初始化是已批准行为变化；R1/R2/R3/R9/R10 为组件提取与封装。
- Phase 37/38 历史任务、声音基线和后续修复：[完成归档](../archive/phase37-38-piano-calibration-and-keyboard-performance.md)。

## 1. 当前候选阶段（Phase 47-2/47-3）

Phase 45–46 已完成并归档；Phase 47-1 已完成（2026-11-27）。Phase 47-2/47-3 尚未启动，进入须用户明确批准；47-2 按需启动，不强制排期。本页只维护尚未启动的候选与进入条件，不把规划写成已实现。

### Phase 47：离线声学校准与参数单源（Offline Acoustic Calibration & Parameter SSOT）

**目标**：把 Phase 40/44 的一次性测量沉淀为可复现的开发期工具，并收敛钢琴参数的跨层重复；不进入产品包，不引入远程配置。

**任务与顺序**：

1. **47-1 离线测量/回归工具**：复用 `WavExportOptions` 与现有导出路径，输入 MIDI/参数，输出 88 键 LUFS、True Peak、0–50 ms 谱、HF/centroid、相邻半音差与 frame time 的 JSON；工具放在开发期脚本/测试侧，不新增产品 UI 或运行时依赖。先用 Phase 40/44 已记录数值做回放校验。
   **状态（2026-11-27）**：已完成。`devpiano_tests --acoustic-probe` 渲染 120 例 48 kHz/24-bit WAV（88 键 v70 + 16 代表键 v41/v98），`tools/acoustic_probe.py` 输出 LUFS/True Peak/起音/RMS/HF/centroid/相邻差与 12 复音 benchmark。Phase 44-3 Standard 回放校验 PASS：v70 LUFS 极差 0.650 dB、相邻最大 0.275 dB、v98 True Peak 0.236；Debug 12 复音 p50 4.41 ms、p99 约 5.7–5.9 ms、max 约 7.3 ms（单次运行，非 SLA）。证据在 `G:\source\projects\devpiano\build-win-msvc\acoustic-probe\phase40`；工具不进入产品包。
2. **47-2 参数 descriptor 表**：为钢琴声学参数定义单一 descriptor（id、默认、范围、序列化 key、locale key、UI 控件）；先让下一次新增参数走通全链路，旧字段逐步迁移，不一次性重写 `SettingsModel`/`Preset`。加一个测试确保 descriptor 覆盖 `PianoStyleParameters` 字段。
3. **47-3 参考指标（可选）**：在离线工具中加入与参考录音的 MFCC-L2/谱质心差，作为音色回归，不作为实时 DSP 目标。

**进入条件**：47-1 已完成（2026-11-27）；Phase 45–46 已完成并归档，不再作为 45-2 或 46 的前置；47-2 在产生新参数或需要参数收敛时启动，不强制排期。

**实施边界**：不做远程 SSOT、不做自动迁移链、不把工具打进发布包、不为假想未来预建通用参数框架。

**验证与出口**：工具可复现 Phase 40/44 指标；descriptor 覆盖检查通过；新增一个参数从定义到 UI/预设/持久化走通；不新增通用参数框架。

## 2. 研究候选与不排期

以下方向保留为研究候选，不因 Phase 47 规划自动启动；需要独立证据、设计与用户批准后再立项：

- **第二极化弦**（fan455 的 u/v/w/α/β 参考）：先在低音区做离线原型，验证相对现有双指数包络的听感收益；不重写主弦。
- **跨音符总线记忆/弦—音板双向耦合**（bBpiano/fan455）：需稳定性与能量守恒设计；当前不做实时闭环。
- **完整非线性锤求解**（bBpiano 的 Maxwell 锤 + Newton/二分）：与实时预算和 88 键响度校准冲突，不进入。
- **3D 空气 FEM、波导主弦重写、Upright↔Grand 连续变形、五麦克风阵列、完整 Verlet 琴槌—琴弦求解器**：不进入。
- **外部参考边界**：bBpiano 为闭源专有实现，只借鉴公开文档/讨论中的设计思想，不克隆实现或参数；fan455 代码未完成且依赖 GMSH/MKL，只作方法与参数参考；Instrudio 只借鉴 SSOT 工程模式；danielpodrazka/piano 只借鉴离线相位/音板校准方法，不引入其离线采样架构；Phase 18-B 采用的相位表数值已按 MIT 归属登记，后续不得重新包装为自研优化。
- 研究候选不以参考报告的“极低代价”估计替代设计和验收，也不为这些展望预留实现。

## 3. 恢复规则与实施顺序

- Phase 45–46 已完成并归档；Phase 47-1 已完成（2026-11-27）；47-2/47-3 尚未启动。本页只维护规划与进入条件，不把未启动项写成已实现。
- 实施顺序：Phase 45–46 已按 47-1 → 45-1 → 45-3 → 46-1 → 46-2 → 45-2 → 46-3 → 46-4 完成并归档；后续按需启动 47-2，47-3 可选。历史任务的提交拆分与 A/B 边界见 [Phase 45–46 归档](../archive/phase45-46-piano-acoustics-and-retrigger.md)。
- 测试按风险驱动：Phase 47 只保留工具回放与 descriptor 覆盖检查。Phase 45/46 的 NoteOff 身份、录音回放、复音/偷声部、ADSR 生命周期、尾音/总线所有权与实时/离线一致性回归证据保留在归档，不重新钉住旧测量或文案。
- 默认仅 Windows Debug 验证；Release 仅在用户明确要求时执行。构建、测试、实时/离线、文件保护和验证纪律仍按 [quickstart](../guides/quickstart.md)、[acceptance](../reference/acceptance.md) 与 [AGENTS.md](../../AGENTS.md)。
- Phase 39 不补记为通过；已消费的独立验证、采集和判决不重放、不补批或放宽。未来外部参考校准须另立新任务。

## 4. 已完成维护任务：统一有限导出尾音（E3）

- E1 的正常 take-end 释放验证与修复已随 `ba21d06` 提交：内置/插件/双层路径按身份追踪补 NoteOff 与 CC64/66/67 Off，不再自动追加 CC120/CC123 截断尾音；take 内显式 Panic 与层/端点切换仍硬停。E2 已完成有限窗口测量、方案比较与证据归档。E1/E2/E3 与 Phase 40–44 的阶段收口独立。
- E3 已实现并提交（`9f5034d`）：`WavExportOptions::tailSeconds` 默认 6.0 秒、闭区间 0–30 秒；内置、插件及双层路径共享该选项；0 表示不追加尾音，文件长度仍由时间线确定性计算。`ExportTailDurationTest` 与 `ExportTailEndTest` 已接入 `devpiano_tests` 的 CMake 源列表；2026-10-25 通过 `win-test` 在 Windows Debug 镜像树执行默认 CTest（1/1 通过），定向 `JiveCoreValues` 与 `ViewHost` 也通过。
- 当前 UI 使用默认值，尚无时长控件；不新增全局设置、持久化字段、Take/Preset 格式、自动检测或淡出。有限窗口仍可能在超窗处截断，不承诺任意音源或插件的自然尾音完整。
- 验证边界：Windows Debug 定向测试、默认 CTest、格式与 diff 检查；不执行 Release、WSL 全量构建、物理声卡或商业插件矩阵。
- 与 Phase 39.5 的关系：E3 与 UI 基础设施阶段相互独立，两者的改动与验证证据分开；WAV 进度内容继续经 `ViewHost` 管理生命周期。E3 已交付，不再作为 Phase 39.5 的前置或并行许可条件。