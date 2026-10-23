# devpiano Current Iteration

> 用途：维护当前候选阶段、恢复条件与实施顺序；项目状态和长期阶段顺序以 [roadmap](roadmap.md) 为准。
> 归档口径：Phase 39 与 R0–R11 的任务、实施记录、验收、失败历史和证据已迁入 `docs/archive/`。本页不重复维护历史状态，也不把归档记录当作当前默认或已通过结论。
> 当前状态：没有正在实施的产品阶段。Phase 39 已冻结，Task 39-2 仍为 **inconclusive**，Phase 39 不记为通过；R0–R11 已完成并归档；Phase 40–42 及后续声学能力暂缓，恢复须由用户明确决定。

## 0. 归档入口

- Phase 39 可复核声学基线与三力度整音校准：[历史归档](../archive/phase39-acoustic-baseline-and-voicing-calibration.md)。Task 39-2 外部参考直接验收缺口、已消费的资格/采集/判决和“不注入 B 表”的收尾结论保持有效。
- R0–R11 内置钢琴音源结构整理：[历史归档](../archive/r0-r11-physical-piano-source-refactor.md)。R5/R6 踏板所有权迁移与 R7 新声部 sustain 初始化是已批准行为变化；R1/R2/R3/R9/R10 为组件提取与封装。
- Phase 37/38 历史任务、声音基线和后续修复：[完成归档](../archive/phase37-38-piano-calibration-and-keyboard-performance.md)。

## 1. 当前候选阶段（暂缓）

### Phase 40：同音弦耦合、双阶段衰减与 Una Corda 深化

深化已有同音弦振荡器与快/慢振幅包络，验证拍频、衰减和辐射之间的关系；区分振幅、功率、能量和时间常数，并验证两弦受击、第三弦被动响应的 Una Corda 拓扑。进入条件仍包含 Phase 39 的生产基线、有效频率、连续整音与状态/实时闭环；当前未满足，不启动。

### Phase 41：音板频变阻抗与开放弦共鸣校准

深化音板频变损耗、琴桥交界和开放弦/跨声部被动传能，区分输出低通、模态阻尼与弦向音板泄漏；共享琴桥只允许存在于固定 Instrument 内部，不扩展为通用路由。进入条件依赖 Phase 39 频率基线及 Phase 40 的主弦耦合/辐射定义。

### Phase 42：能量驱动张力非线性与泛音绽放

按实际衰减模态状态深化 Pitch Glide 与 Blooming，定义模态位移、权重和能量尺度；瞬态频率与泛音轨迹须由量测决定，并保持旁路、弱奏、Nyquist 边界和实时/离线对照。进入条件依赖 Phase 39 激振/频率校准及 Phase 40/41 的耗散与耦合尺度。

## 2. 恢复规则

- Phase 40–42 及“近期不排期”内容全部保留为候选；恢复须由用户明确决定，不因 R0–R11 归档、构建通过或时间推移自动启动。
- Phase 39 不补记为通过；已消费的独立验证、采集和判决不重放、不补批或放宽。未来外部参考校准须另立新任务。
- 实施时的构建、测试、实时/离线、文件保护和验证纪律仍按 [quickstart](../guides/quickstart.md)、[acceptance](../reference/acceptance.md) 与 [AGENTS.md](../../AGENTS.md)；默认仅 Windows Debug 验证，Release 仅在用户明确要求时执行。
