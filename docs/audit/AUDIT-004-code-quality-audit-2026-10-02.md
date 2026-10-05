# devpiano 代码质量审计报告 · 2026-10-02

> 只读审计与有界文档复审：第8章是唯一状态源。结论严格区分实际Windows观测、静态证据、Phase 0/A-H软件实施证据与§4.5待用户实机验证范围；不把软件闭环冒充全平台硬件认证。

---

## 0. 审计看板

### 0.1 基本信息

| 字段 | 值 |
| --- | --- |
| 项目 / 版本 | devpiano / CMake项目版本1.3.0 |
| 审计范围 | 首次覆盖 source/ 289 个 .cpp/.h；本次复审原 54 项及其实施证据，非新全量代码审计；当前文件统计见 §4.2.2 |
| 审计日期 | 2026-10-02（首次审计）；2026-10-05（Phase H 软件完成复审） |
| 审计基线 | 首次基线 main @ `732cde18dc8116cb21f0325377415cedfef03811`；Phase H 软件交付基线 `0f60769`（代码基线 `166c545`）；当前工作树基线 `4a77f66`（仅另一次本地化小修复，不伪称在本轮源码重跑门禁） |
| 审计人 | devpiano-audit，主审与6个独立只读切片（首次）；devpiano-audit 复审（2026-10-05） |
| 复审状态 | 已复审（2026-10-05）；基于 Phase 0/A-H 归档实施记录与直接验证证据进行有边界复审，不改写历史报告基线 |
| 写入范围 | 用户授权的文档联动：本报告/索引、实施归档、当前小阶段规划、新 ADR 与现行引用；不修改业务源码、配置或第三方代码，不将历史产物清理写成本轮构建缓存删除 |

### 0.2 风险与状态汇总

| 优先级 | 合计 | 未处理 | 处理中 | 已缓解 | 已暂缓 | 已关闭 |
| --- | --- | --- | --- | --- | --- | --- |
| P0 | 0 | 0 | 0 | 0 | 0 | 0 |
| P1 | 25 | 0 | 0 | 2 | 0 | 23 |
| P2 | 26 | 0 | 0 | 0 | 0 | 26 |
| P3 | 3 | 0 | 0 | 0 | 0 | 3 |
| **合计** | 54 | 0 | 0 | 2 | 0 | 52 |

共**54项**：**52项已关闭，2项已缓解（`THR-001`、`PERF-001`），0项未处理/处理中/已暂缓**。计数包含第8章全部行，历史/known-issues不重新编号。状态、首页、结论及路线图均由第8章派生。

**特别说明**：软件实施闭环不等于全平台综合认证。原报告 §4.5 待用户实机验证（目标厂商插件、物理声卡、IME/DPI 全矩阵、强杀/断电等）仍独立保留未验证，不因自动化测试通过而推断为全平台通过。

### 0.3 关键结论

- 总体评级：首次审计评级为 **C**。本次复审评价：**软件实现层面已达成收敛（52 项关闭，2 项已缓解），但因未开展全新全量代码审计，且缺乏物理声卡/商业插件实机验证，维持受控的保守评价，不得伪称 A 级认证**。综合平台条件仍需待 §4.5 实机验证。
- 当前是否适合继续新增功能：**可有条件推进小步收口**。P1/P2/P3 软件缺陷已闭环或分层缓解，当前首要任务为完成 ADR-015 本地化消息模板小型收口阶段；在大规模新增声学/分区功能（Phase 36/37）前，仍建议完成 §4.5 实机抽样复核。
- 当前是否建议优先重构：**否**。原架构缺口已在 Phase A-H 窄边界修复（事务替换、UUID 预设、不可变 Take 快照、纯音频所有无锁调度、双看板统一投影、ViewHost 严格门面等），无需且不应开展全局重写。
- 最大剩余风险：第三方 VST3 插件适配器的框架级锁与 2048 消息上限（已分层接受）；物理声卡热插拔与驱动毛刺；特定商业插件内部稳定性；断电/强杀下非事务边界的数据丢失风险。
- 下一步最高优先级：执行本地化消息模板收口（遵循 ADR-015）；在可丢弃测试环境中由用户配合执行 §4.5 实机验证；在本地化小阶段验收及下一次 JUCE/插件路径变更前复核 `THR-001` 与 `PERF-001`。
- 门禁状态：Phase G/H 记录的是其交付基线上的 Windows Debug、默认单测（含 Chord/lifecycle）、格式及全量 tidy 结果；增量 no-work 不等于新编译警告测量。本次不重跑产品门禁，也不将旧基线结果无条件移植到 `4a77f66`。

### 0.4 重点发现

| ID | 优先级 | 状态 | 标题 | 当前复审结论 |
| --- | --- | --- | --- | --- |
| ERR-001 | P1 | 已关闭 | 已有导出目标不是事务替换：成功追加，失败删除原文件 | 同目录临时写出、关闭后提交，失败/提交前取消保留原目标；Phase A EVID-006 与 Phase B EVID-013 支持关闭，不外推断电认证 |
| SEC-001 | P1 | 已关闭 | 预设重命名可覆盖另一预设或删除自身目标 | 规范化源/目标路径，同路径不删、独立冲突显式确认；Phase A EVID-007、Phase H EVID-062 确认关闭 |
| SEC-002 | P1 | 已关闭 | 原生文件绑定未随 Take 替换/Save As 更新 | Take 替换解除旧绑定，成功打开/保存绑定新文件，generation 拒绝迟到回调；Phase A EVID-008、Phase G EVID-059 支持关闭 |
| SEC-003 | P1 | 已关闭 | 原生 MIDI 编码信任未校验的解码长度前缀 | 校验专有前缀长度、数据预算与帧形状，拒绝畸形数据；Phase B EVID-012/016 确认关闭 |
| AUDIT-001 THR-004 | P1 | 已关闭 | 增量重扫绕过停音频/关 Editor 守卫 | 重扫前先停止 callback 并关闭 Editor 窗口；Phase C EVID-021/024 确认关闭 |
| THR-001 | P1 | 已缓解 | 实时回调常规路径仍有阻塞锁 | 产品自有链路零锁闭环；第三方 VST3 框架锁由用户批准分层接受；Phase E EVID-041/043，待实机与路径变更复核 |
| PERF-001 | P1 | 已缓解 | 密集播放和重复预设循环突破回调预分配 | 产品自有链路零堆分配闭环；第三方 VST3 适配器框架分配及 2048 限制由用户批准分层接受；Phase E EVID-042，待实机与路径变更复核 |
| AUDIT-002 TEST-014 | P2 | 已关闭 | 和弦识别7个子测试被默认类别过滤漏跑 | 迁入 DevPiano/Core 默认前缀执行，7个子测试纳入默认门禁；Phase 0 EVID-002、Phase G EVID-060 确认关闭 |

---

## 1. 审计范围与方法

### 1.1 审计范围

首次审计逐文件覆盖 `source/**/*.cpp` 与 `source/**/*.h`。下表保留首次基线的物理行/文件清单，不冒称当前新全量覆盖；本次复审范围为原问题及归档实施证据。

| 模块/路径 | 文件数 | 物理行数 | 职责 |
| --- | --- | --- | --- |
| source/Audio | 13 | 4378 | 音频、物理建模、调律、空间、踏板、节拍器 |
| source/Core | 6 | 1063 | 键位/状态/乐理/节拍/视图数据 |
| source/Diagnostics | 5 | 257 | 日志与MIDI描述 |
| source/Export | 5 | 555 | WAV任务、选项与流程 |
| source/Input | 4 | 828 | 电脑键盘映射、修饰符与触键动态 |
| source/Layout | 4 | 1154 | 预设格式、发现与CRUD |
| source/Locale | 1 | 93 | 语言映射与内嵌语言包 |
| source/root | 5 | 2770 | 应用入口、MainComponent装配/访问器、STL PCH |
| source/Midi | 3 | 206 | 通道矩阵与发音身份路由 |
| source/Plugin | 6 | 1122 | VST3扫描、加载、Editor与恢复 |
| source/Recording | 23 | 4239 | 捕获、Transport、文件及离线渲染 |
| source/Settings | 13 | 2905 | 持久化、快照与设置窗口 |
| source/UI | 149 | 16003 | 业务/原生UI与内化声明式运行时 |
| source/tests | 52 | 18005 | 基础设施及行为测试 |

UI的149文件中110文件/9,494行属于`source/UI/jive/core/`，已内化，**在审计范围内**。排除第三方submodules实现、scripts/、docs/及构建脚本/配置的缺陷面；只读核对它们以确认API/接入/文档契约，不审查或修改JUCE自身。
本次 2026-10-05 复审是基于归档实施记录（`docs/archive/audit-004-code-quality-fix-phases.md`）和 Phase H 交付证据的有边界文档复审，不修改源码，不做新的全面代码审计。

### 1.2 审计输入与执行顺序

1. 阅读项目范围、architecture、AGENTS、全部ADR-001~014、known-issues、roadmap/current-iteration、模板及既有审计登记表。
2. 按顺序执行WSL Debug构建→默认测试→格式检查。用户本次skill明确要求完整WSL门禁，按此执行，不把它当日常Windows镜像工作流的改变。补self-check、完整只读tidy和Windows验证。
3. 无重叠所有权分为输入/看板35、持久化26、插件10、录制导出18、UI运行时122、测试52；主审音频/入口/MIDI/诊断26。并集289，未分配0。切片不执行检查，主审统一运行。
4. 复核可达生产消费者，在Windows调用实际业务对象，执行§4.4探针。不以mock echo、源文本或编译成功作功能证明。
5. 第8章收敛/去重，第5章覆盖全部未处理ID，统计/状态/首页由同一登记表派生。
6. **2026-10-05 复审**：核对 Phase 0/A-H 归档实施记录（EVID-001～EVID-067）、Phase H 54项索引、known-issues、acceptance、ADR-015 决策边界，更新第0..8章，不推翻首次历史事实。

**首次工具限制**：初审时 codegraph 未挂载及 LSP references 不完整的记录保留；当时按项目规则降级，不把采样 diagnostics 当全量门禁。本次文档核对可使用已挂载 codegraph，但不据此改写初审工具能力或声称重新全面扫描。

### 1.3 严重级别定义

沿用模板P0~P3。崩溃/数据损坏/音频毛刺无声/泄漏/线程安全缺陷从严列P0~P1；未实测主窗口崩溃不使已证明危险路径降为P2。P2为中等功能/边界/维护缺口，P3为低风险契约/测试整理。本轮未登记P0，不表示不存在未发现的P0。

### 1.4 状态与确定性定义

固定状态：未处理/处理中/已缓解/已暂缓/已关闭。
- **已关闭**：需由逐项代码实现、测试覆盖与消费者直接验证证据支持，确认消除原根因。
- **已缓解**：需明确已修自有路径、用户既有分层接受来源、重开触发条件与具体复审时点；不由审计代用户接受未知风险。
- **待验证实机边界**：§4.5 独立保留为实机矩阵未验证，不因软件缺陷关闭而外推全平台通过。历史新反证保留原ID，既有报告状态不覆盖。

---

## 2. 项目画像

### 2.1 项目类型与核心能力

专用电脑键盘钢琴桌面宿主：默认7声学系统物理钢琴、可切Sine、VST3扩展；Group/修饰符/发音身份、双映射看板、16通道矩阵、录制/MIDI/原生文件/WAV、节拍/count-in、Seek/A-B练琴、双语。坚持`Performance Input -> Instrument -> Master -> Output`，不要求外部MIDI、Patchbay、多轨DAW或视频栈。
在 Phase E-F 修复后，已实现不可变 RecordedPreset 表、UUID 永久身份、双看板统一只读投影、纯音频所有无锁调度。

### 2.2 技术栈与运行环境

| 类别 | 当前核对 |
| --- | --- |
| 语言/框架 | C++20，JUCE 9.0.1，内化声明式UI |
| 构建 | CMake+Ninja；WSL Debug/mold；Windows MSVC 19.51.36260.0 Debug |
| 测试 | JUCE UnitTest聚合目标devpiano_tests；默认类别已纳入 Chord 与 lifecycle |
| 门禁 | clang-format-21 / clang-tidy-21 只检查不自动 fix；Phase G/H 历史结果按 §4.1.2 的配置与基线保留，不宣称本次产品门禁重跑 |
| 资产/状态 | BinaryData、ValueTree/JSON/XML；原生MIDI编码是JUCE特有格式，不是标准Base64；预设采用 UUID 永久身份与内嵌不可变快照 |
| 验证基线 | 首次审计 `732cde1`；Phase H 软件交付 `0f60769`（代码 `166c545`）；当前工作树 `4a77f66`（本地化小修复，ADR-015 决策已接受待迁移） |

### 2.3 目录与模块边界

Core/AppState已消除原Settings/Midi逆向include；PluginFlowSupport无持有状态、显式注入。Phase A-H 窄边界修复收敛了原边界缺陷：
1. 导出与持久化统一同目录 TemporaryFile 事务替换，彻底消除成功追加、失败删除原文件风险；
2. Take 替换与 Save As 解除旧绑定并绑定新文件；
3. PluginFlowSupport 与 AudioEngine 建立停机重扫/音色切换守卫；
4. 内置音源重写为纯音频所有无锁调度（BuiltinSynthesiser），MIDI Listener 经有界 SPSC 队列移入消息线程；
5. ViewHost 严格门面封装原生热重载与内置弹窗，消除 raw GuiItem 逃逸。

---

## 3. 分领域审计结果

本章包含各领域首次分析与本次复审结论摘要，具体状态以第8章为准。

### 3.1 架构与模块边界

- 首次评级：**C**。复审评价：**收敛达标**。
- 结论：固定拓扑和分层保留。Phase C/E/F 通过稳定插件 description 身份（`ARCH-002`）、预设 RFC 4122 UUID 永久身份与 Take 内嵌不可变 `RecordedPreset` 表（`ARCH-003`）、实时/离线同构快照执行闭包（`ARCH-004`）及两张演奏看板统一消费只读映射投影（`ARCH-001`），系统性消除了原架构缺口。
- 关联问题：`ARCH-001`（已关闭）、`ARCH-002`（已关闭）、`ARCH-003`（已关闭）、`ARCH-004`（已关闭）。

### 3.2 代码质量与可维护性

- 首次评级：**C**。复审评价：**收敛达标**。
- 结论：Phase A/D/F 系统性补齐深拷贝字段遗漏（`QUAL-006`）、启动恢复预设身份（`QUAL-016`）、移调/掩码 NoteOff 身份锁定（`QUAL-001`）、物理键重复持有者（`QUAL-002`）、暂停捕获补齐（`QUAL-003`）、末尾 NoteOff 交付（`QUAL-004`）、设备采样率重基准（`QUAL-018`）、Seek/循环回跳状态恢复（`QUAL-017`）、预备拍下拍对齐（`FIX-035`）、柔音 CC67 继承（`QUAL-019`）、输入输出通道解耦（`QUAL-007`）、几何重建标签保持（`QUAL-008`）、静音绑定优先级（`QUAL-009`）、fadeSpeed 收缩终止（`QUAL-010`）、圆角新值重建（`QUAL-012`）、Notes 正常编辑（`QUAL-013`）、离线插件 nonRealtime 模式（`QUAL-014`）、重复插件重新识别（`QUAL-015`）及最低八度标注（`QUAL-011`）。
- 关联问题：`QUAL-001`～`QUAL-019`、`FIX-035`（全部已关闭）。

### 3.3 线程安全与并发

- 首次评级：**D**。复审评价：**自有路径达标，框架层受控缓解**。
- 结论：Phase C 消除增量重扫与音色重建并发（`AUDIT-001 THR-004`、`AUDIT-002 THR-001`），WAV 任务改为协作取消与异步退出等待（`THR-002`），变速/Stop 游标一致性由音频块入口处理（`known-issues §2/Phase 6-2 播放速度控制`）；Phase E 核心发声链路重写为纯音频所有无锁调度（`BuiltinSynthesiser`），MIDI Listener 经有界 SPSC 队列移入消息线程（`AUDIT-002 THR-003`）；自有路径全面达成零锁。第三方 VST3 适配器仍含 SpinLock 与 CriticalSection（`THR-001`），符合用户批准的分层验收决策，评为已缓解。
- 关联问题：`AUDIT-001 THR-004`（已关闭）、`AUDIT-002 THR-001`（已关闭）、`known-issues §2/Phase 6-2 播放速度控制`（已关闭）、`THR-002`（已关闭）、`AUDIT-002 THR-003`（已关闭）；`THR-001`（已缓解）。

### 3.4 安全边界

- 首次评级：**C**。复审评价：**原登记的数据完整性与准入反例已闭环；异常存储/实机边界另验**。
- 结论：Phase A 同目录事务替换、同路径区分/冲突确认与 Take 绑定消除原文件保护反例；Phase B 校验专有编码长度/预算、有限且支持的采样率、派生长度加法可表示性、MIDI 拍号负载/指数、稳定时间线及完整声明轨（`SEC-001`～`SEC-006`、`QUAL-005`、`ERR-004`）。关闭原根因不等于任意断电/存储故障均已认证。
- 关联问题：`SEC-001`～`SEC-006`、`ERR-001`、`ERR-004`、`QUAL-005`（全部已关闭）。

| 检查项 | 复审结果 | 事实边界 |
| --- | --- | --- |
| 文件/已有数据所有权 | 通过 | 事务替换、同路径规范化与 Take 绑定已闭环；断电/强杀下非事务边界另列实机矩阵 |
| 插件加载 | 受控通过 | 进程内受信 VST3；重扫生命周期与 Editor 卸载守卫闭环；真实厂商稳定性另验 |
| 键位/矩阵/数组 | 通过 | 双看板统一只读投影，静音最高优先，keyUp 显式拒绝 |
| JSON/版本/原生消息 | 通过 | 专有编码前缀校验、版本 v3 不可变快照，畸形数据拒绝 |
| MIDI结构/meta | 通过 | 声明全轨校验，0x58 元数据安全防护，CRLF 兼容 |
| 资源准入 | 通过 | 预设/MIDI/语言包守卫健全，防范解码放大 |
| 时间线数值 | 通过 | 非负有限采样率、防溢出加法与安全长度 |

### 3.5 资源与性能

- 首次评级：**C**。复审评价：**自有零堆分配与日志轮转达标，第三方框架缓解**。
- 结论：Phase E 自有音频引擎达成全回调零堆分配、零三角函数（Metronome 查表/递推消除三角函数，`known-issues §1` 零三角 SLA 关闭），消除异常尺寸 setSize 兜底改为安全静音与原子计数（`known-issues ERR-002` 关闭）；Phase G 诊断日志实现 512 KiB 会话有界轮转与故障保护（`RES-001` 关闭）。密集播放与预设循环自有路径已无内存增长，但第三方 VST3 适配器框架分配及 2048 事件上限（`PERF-001`）由用户批准分层接受，评为已缓解。8 复音单核 CPU $\le 0.7\%$ 保持物理 SLA，实机性能仍待用户真实硬件实测。
- 关联问题：`RES-001`（已关闭）、`known-issues ERR-002`（已关闭）、`known-issues §1/节拍器每拍三角函数与全回调零三角 SLA 不一致`（已关闭）；`PERF-001`（已缓解）。

### 3.6 错误处理与可观测性

- 首次评级：**C**。复审评价：**已闭环**。
- 结论：Phase A 同步保存取代旧 timer 快照消除数据回滚（`ERR-002`）；Phase B 截断轨拒绝并报告（`ERR-004`）；Phase C 重复插件更新 metadata（`QUAL-015`）；Phase D 显式 keyUp/未知 trigger 准入明确拒绝（`ERR-003`）；Phase G MIDI 诊断直接输出原始 0..127 整数力度（`OBS-001`）；Phase A/B 导出失败保留原始文件（`ERR-001`）。
- 关联问题：`ERR-001`（已关闭）、`ERR-002`（已关闭）、`ERR-003`（已关闭）、`ERR-004`（已关闭）、`QUAL-015`（已关闭）、`OBS-001`（已关闭）。

### 3.7 测试体系

- 首次评级：**C**。复审评价：**质量提升与安全隔离就位**。
- 结论：Phase 0 修复 AudioEngine fixture 消除可选 NRVO 依赖（`TEST-001`），删除生产目录探针并使用 ScopedTempDir 隔离用户环境（`TEST-002`），将和弦识别迁入 DevPiano/Core 默认门禁（`AUDIT-002 TEST-014`）；Phase G 删除自证断言与硬编码中文，接入真实 Metronome lifecycle 用例（`TEST-003`）。测试体系从“偶然绿灯”提升为确定性安全防护。
- 关联问题：`TEST-001`（已关闭）、`TEST-002`（已关闭）、`AUDIT-002 TEST-014`（已关闭）、`TEST-003`（已关闭）。

### 3.8 文档与配置契约

- 首次评级：**C**。复审评价：**现行契约已同步**。
- 结论：Phase F 统一 A4 基准音高范围为 400.0~480.0 Hz（`known-issues §1/A4` 关闭）；Phase H 系统性同步现行功能与验收文档，使预设调号、rename 行为、MIDI 导出格式、日志轮转及测试说明与真实代码实现契约完全一致，消除原反证承诺（`DOC-001` 关闭）。
- 关联问题：`DOC-001`（已关闭）、`known-issues §1/A4 基准音高范围与项目契约不一致`（已关闭）。

### 3.9 工程化与构建

- 首次评级：**C**。复审评价：**门禁收敛达标**。
- 结论：Phase G 修复初审登记的编译/静态诊断位点，Windows Debug 编译和全量 tidy 有实际后验（EVID-061/066）；Phase H 增量未重编与前次完整编译分开记录。未在 Phase G/H 执行 WSL 产品构建，不能声称 WSL 警告计数归零；独立树通过也不表示旧默认 Ninja 缓存已清理/修复。
- 关联问题：`ENG-001`（已关闭）。

### 3.10 ADR 合规审计

- 首次评级：**C**。复审评价：**原确认的 ADR-012/014 违例已闭环；退役决策和新接受决策分别记录**。
- 结论：Phase G 消除业务图标头聚合 include（`CMPL-001`）与 raw GuiItem 门面逃逸（`CMPL-002`）。本次承接首次 ADR 核对，只对原违例及实施边界做证据复审，不宣称在当前树重新执行全部旧 API 检查。
- **ADR-015 处理**：用户本次已接受 [完整消息模板与分类标点](../decisions/ADR-015-localized-message-templates-and-punctuation.md)，既有调用迁移待实施；不追溯为初审违例、不扩充原 54 项集合。决策生效与迁移完成是不同状态。

| ADR | 决策要点 | 复审证据/承接范围 | 首次判定（保留） | 本次判定 |
| --- | --- | --- | --- | --- |
| ADR-001 | WSL主源与Windows镜像分离 | win-build路由G盘镜像；新镜像Debug验证成功；source无编辑 | 合规 | 合规 |
| ADR-002 | 旧源码仅历史参考、已移除 | freepiano-src不存在；source旧引用/原生入口复合grep零命中 | 合规 | 不适用（已废止/被替代） |
| ADR-003 | 无持有状态、显式注入 | PluginFlowSupport.h:17-40/.cpp:29-54，free functions，host/settings/callback参数注入 | 合规 | 合规 |
| ADR-004 | JUCE AudioDeviceManager | MainComponent.cpp；SettingsComponent.cpp；旧原生入口/SDK include检查零命中 | 合规 | 合规 |
| ADR-005 | JUCE管理VST3实例 | PluginHost.cpp；旧AEffect/effOpen/audioMaster零命中 | 合规 | 合规 |
| ADR-006 | 无外MIDI硬件输入 | MidiInput枚举/open、MidiInputCallback、MidiRouter、externalMidi残留检查零命中 | 合规 | 合规 |
| ADR-007 | tidy只检查、不自动fix | 全量无--fix；144/144 cpp 0 诊断通过（EVID-066） | 合规 | 合规 |
| ADR-008 | 已被014替代 | 按废止关系核对，声明式理念保留，不恢复退役子模块 | 合规 | 不适用（已废止/被替代） |
| ADR-009 | 增强模态Piano默认、Sine可切 | AudioEngine.cpp，Magic Circle分音循环；全回调零三角SLA达成（EVID-043） | 合规 | 合规 |
| ADR-010 | BinaryData嵌入资产 | CMakeLists.txt；Locale/DesignTokens/StyleCatalog读嵌入资产 | 合规 | 合规 |
| ADR-011 | Embedded/PCH/快速链接器 | CMakeLists.txt；pch无JUCE聚合头，mold日志及真实MSVC Debug | 合规 | 合规 |
| ADR-012 | 业务头禁JuceHeader | WindowIconUtils.h 聚合头已移除，细粒度 include，CMPL-001 已闭环（EVID-057） | 部分合规 | 合规 |
| ADR-013 | 移除inspector | 目录不存在；source/.gitmodules/CMake残留检查零命中 | 合规 | 合规 |
| ADR-014 | 内化许可与严格ViewHost门面 | 110文件保留版权/MIT；ViewHost 严格门面封装，raw GuiItem 逃逸消除，CMPL-002 已闭环（EVID-058） | 部分合规 | 合规 |
| ADR-015 | 本地化完整消息模板与标点规范 | docs/decisions/ADR-015 决策已接受（2026-10-05）；既有调用迁移待实施；不追溯为首次违例，不另开54集合新CMPL | 不适用（当时未接受） | 已接受（迁移待实施） |

---

## 4. 验证记录

### 4.1 命令执行结果

#### 4.1.1 首次审计命令执行记录（2026-10-02，历史基线 `732cde1`）

| 命令 | 结果 | 说明 |
| --- | --- | --- |
| ./scripts/dev.sh wsl-build | 通过（非零warning） | Debug完成148.68s，20次warning/2源位点，刷新compile_commands |
| ./scripts/dev.sh test | 通过（默认选择范围） | CTest1/1，18.42s，95套件530子测试97,823通过0失败；未含Chord |
| ./scripts/dev.sh format --check | 通过 | clang-format-21，0差异，1.00s |
| ./scripts/dev.sh self-check | 通过 | PowerShell、G盘镜像、vswhere可用，默认变量提示非失败 |
| ./scripts/dev.sh tidy --all | 失败 exit1 | 全144cpp，824.22s，MidiTextDecoder:200/211/289/297与PerformanceModifierTest:220，共5项目诊断 |
| clang-tidy -p build-wsl-clang source/**/*.cpp | 未执行原字面写法 | 用项目标准tidy --all全量替代，不重复可能漏glob的扫描 |
| ./scripts/dev.sh win-build | 失败（原命令） | 同步成功，旧CMAKE_MAKE_PROGRAM的D:/PROGRA~1/MICROS~2/18/ENTERP~1/.../ninja.exe不存在 |
| Windows cmake --preset windows-msvc-debug -B build-win-msvc-audit004 -DBUILD_TESTS=ON；cmake --build ... | 通过（镜像独立Debug） | 相同preset、新树避旧缓存，不删原树、不改配置；MSVC19.51，含build/test395.90s |
| Windows ctest --test-dir build-win-msvc-audit004 -C Debug --output-on-failure | 通过 | 1/1，63.38s，95套件530子测试97,822通过0失败 |
| Windows devpiano_tests.exe --category devpiano | 通过补跑漏项 | 1套件7子测试，115通过0失败，0.46s；默认类别未修 |
| Windows临时消费者探针S01-S29 | 已执行，复现缺陷 | 真实业务对象/生产工厂/绘制及编译边界；见§4.4，不是修复后通过 |
| LSP diagnostics/references | 采样/不完整 | AudioEngine TU查询未报错误，但references漏跨TU，不能作全量零诊断/完整影响面 |
| Release/TSAN/真声卡/真实VST3组合 | 未执行 | 未要求Release；未在用户硬件/插件上冒险强杀/崩溃重现 |

三闸门是本轮真实基线复验，未修源码，不作修复后全绿承诺。两端断言相差1是实际结果，均0失败；额外Chord补跑不修复默认选择集合。

#### 4.1.2 Phase G/H 交付后验与复审记录（2026-10-05，代码基线 `166c545`，交付基线 `0f60769`）

| 命令 / 门禁 | 结果 | 归档证据 | 说明 |
| --- | --- | --- | --- |
| Windows MSVC Debug 增量构建 (`devpiano`, `devpiano_tests`) | 通过（未重编） | EVID-065 | ninja: no work to do；未发出编译 warning，不等于新零警告编译证明；fixture 禁可选 NRVO 的实际 command 含 `/Zc:nrvo-`，先前完整编译见 EVID-061 |
| 默认 CTest (`ctest --test-dir build-win-msvc/audit004-phaseg -C Debug`) | 通过（实际默认选择） | EVID-065 | 本次历史输出 99 套件、287,676 通过断言、0 失败；Chord 子测试和 Metronome lifecycle 确有执行，不外推所有未来用例均完整 |
| clang-format-21 (`./scripts/dev.sh format --check`) | 通过 | EVID-065 | 0差异，格式完全合规 |
| 全量 clang-tidy (`./scripts/dev.sh tidy --all`) | 通过 exit 0 | EVID-066 | 全144/144源码cpp零缺失，项目severity诊断清零，无自动--fix，未修改静态规则 |
| 真实业务与消费者后验 (文件/预设/热重载/UI/日志) | 通过 | EVID-062, EVID-063, EVID-064 | 覆盖重命名/UUID、MIDI导出/导入、v3不可变快照、日志预算轮转、主窗口热重载像素与组件身份保持 |
| 54项原问题集合 comm 校验 | 通过 (0 差异) | EVID-067 | 原54项ID、优先级与Phase H证据索引完全一致，零缺失零多余 |

注：Phase G/H 后验基于交付基线 `0f60769`（代码基线 `166c545`），上述证据均记录于归档 `EVID-001`～`EVID-067` 独立命名空间。当前工作树基线为 `4a77f66`，仅包含另一次本地化小修复，未在当前源码上本轮重跑整体验证，后验结果源自 Phase G/H 归档交付。

### 4.2 文件统计

#### 4.2.1 首次审计文件统计（2026-10-02，历史基线 `732cde1`）

| 指标 | 值 |
| --- | --- |
| cpp/h/审计并集 | 144 / 145 / 289 |
| 总物理行（含空行/注释，排除.inl/.loc/.json） | 53578 |
| tests cpp/h | 51 / 1 |
| 默认套件/唯一子测试 | 95 / 530 |
| 额外Chord套件/子测试 | 1 / 7（默认漏跑） |
| beginTest源码调用数 | 344，testCase动态子测试另计，不能当执行数 |
| WSL/Windows默认断言通过 | 97,823 / 97,822，各自0失败 |
| 最大文件 | source/Audio/PianoSynthVoice.h，1,734行 |
| 次大文件 | source/tests/StyleCatalogTest.cpp，1,713行 |
| 编译数据库实际source cpp/未编译cpp | 144 / 0 |
| 内化UI文件/行 | 110 / 9,494 |

统计：枚举source/.cpp/.h，UTF-8 splitlines计物理行，与compile_commands实际file集合做差集。子测试取LastTest.log唯一Starting tests in: suite/name观测，避免stdout/logger重复；不引用旧报告断言数。

#### 4.2.2 本次文档复审统计与历史后验（2026-10-05）

当前文件统计基线为 `4a77f66`，与首次表同样只计 `source/**/*.cpp/.h` 及含空行/注释的物理行；统计不构成新逐文件代码审计。Phase H 的门禁数字单列为 `166c545` / `0f60769` 历史观察，不把初审文件数复制成新测量。

| 指标 | 本次实际或有基线的历史结果 | 边界 |
| --- | --- | --- |
| 当前 cpp / h / 并集 | 144 / 152 / 296 | `4a77f66` 当前枚举；不沿用初审 145 个头文件 |
| 当前物理行 | 62,560 | UTF-8 splitlines；不包括 .loc/.json/.inl |
| 当前 tests cpp / h | 50 / 1 | 不把已删除/合并测试仍计作初审数量 |
| 当前内化 UI core 文件 | 110 | 未重新计其历史行数或做新全面源审 |
| Phase H 默认执行 | 99 套件、287,676 通过断言、0 失败 | 归档 EVID-065 的一次性观察；Chord/lifecycle 已实际执行 |
| Phase H 完整 tidy | 144/144 cpp、项目诊断 0、returncode 0 | 归档 EVID-066；框架 warnings 汇总不等于项目诊断 |
| Phase G/H WSL 产品编译 warning | 未重测 | 不把 Windows/no-work 或 tidy 结果写成 WSL 编译 0 warning |

### 4.3 未执行验证、环境与只读边界

- 首次 win-build 失败与后续独立树通过分别保留；Phase G/H 未通过删除旧缓存修复该环境。本次不为确认旧失败而重跑，也不无证据宣布旧默认路径已恢复。
- 软件已闭环范围：原文件保护、格式准入、发音身份锁定、不可变快照、自有链路零锁零分配、日志轮转与测试隔离均已获得直接代码与测试证据（Phase 0/A-H）。
- 未验证边界：目标厂商插件、物理声卡/热插拔、IME/DPI 全矩阵、强杀/断电与新的性能测量；Release/TSAN 未执行。原生 VST3 阻塞夹具已验证超过旧有限强停窗口后的协作收尾，不等于永久阻塞或全部厂商已认证。
- 本次是用户授权的文档联动与证据复审，不修改源码或重跑产品门禁；文档渲染、集合对照与归档配方提取验证另列，不冒称软件/硬件重测。修改留工作区未提交。

### 4.4 Windows 消费者冒烟证据与复核输入

#### 4.4.1 首次审计 Windows 消费者冒烟证据（2026-10-02，探针 S01～S29）

沿用MSVC Debug测试目标宏/PCH和当前项目对象，不替换映射/写出/生产工厂。S24只对探针用/Zc:nrvo-；S28只算长度，不渲染巨大时间线；S20仅捕获独立进程异常，不执行2GiB负载/主窗口破坏性输入。

| 观测ID | 复核输入/调用 | 实际输出 |
| --- | --- | --- |
| S01 | 播放 NoteOn60→offset+1→原NoteOff@256 | old_note_after_off=1；new_note=0；position=384 |
| S02 | prepare(128,48000)后首块5000个有效CC | callback_alloc_or_realloc=10；plugin_resize=0 |
| S03 | 另一线程在keyboard Listener中持有状态锁 | callback在50ms观察窗口未完成；释放竞争者后完成 |
| S04 | 独立日志limit1024，64×128字节写入 | observed_bytes=8465 |
| S05 | 默认Q/K同为Ch1 MIDI72，只松Q | K仍held=1；pitch72=off |
| S06 | 真实鼠标C4两次触发Ch1→2、Ch2→3矩阵 | first_click_ch2=1；second_click_ch3=1 |
| S07 | setKeyboardLayout后setKeyboardSettings | 绑定标签数31→0 |
| S08 | fadeSpeed=1，松键后推进120帧 | fade=1.000000；目标previewAlpha=0 |
| S09 | 静音A binding.velocity=0并启用Shift | snapshot.velocity=1.000000 |
| S10 | A经矩阵transpose+12/outputCh4 | 显示note60/ch1，实际ch4/note72=on |
| S11 | 173BPM/启用/两小节/关闭cadence经store防抖保存 | 保存后120BPM/disabled/countin0/cadence1 |
| S12 | schedule旧设置→save新设置→pump timer | new-cache被旧payload写回old-cache |
| S13 | 同WAV路径先8kHz再16kHz导出 | 两次成功；64,904→194,608字节；reader仍8,000Hz |
| S14 | 同MIDI路径先note60再note72导出 | 两次成功；完整readFrom=false；已解析首note仍60 |
| S15 | 预置目标文件，WavExportTask以sampleRate0失败 | export_ok=0；原文件survives=0 |
| S16 | 录On60→pause期间Off60→resume→stop | 保留take On=1/Off=0，length256 |
| S17 | length128、Off@128，128帧块渲染/advance | ended=1，On=1/Off=0 |
| S18 | 原生JSON序列Off@1024排在On@0前面 | 加载admitted=1，播放交付On=0 |
| S19 | 原生sampleRate=1e-300 | 加载admitted=1；派生位置1即ended |
| S20 | 原生midiData="-1."，仅在独立进程边界捕获异常 | std::bad_alloc=1；未测试主窗口退出表现 |
| S21 | Type1 header声明2轨，只提供完整第1 MTrk | import_success=1；parsed_tracks=1 |
| S22 | 真实BackgroundCanvas固定100×100，半径0→30→0 | corner alpha依次255→0，均与当前半径相反；离屏图片已查看 |
| S23 | 注册生产ViewHost默认工厂后解释metadata布局，键入x | readonly=1；key_handled=0；text保持before |
| S24 | 复制实际makeBlock工厂，仅探针使用/Zc:nrvo- | info_bound_to_live_buffer=0；未解引用悬垂指针 |
| S25 | 真实describeMidiMessage(noteOn velocity0.5) | raw_velocity=64；日志vel=8128 |
| S26 | keyUp A绑定，完整按下/释放周期 | down_handled=0；up_handled=0；emitted_note=0；held=0 |
| S27 | 真实MusicTheory标签0/1/11/12边界 | C-1 / C#0 / B0 / C0，1和11八度错误 |
| S28 | finalEvent.timestamp=INT64_MAX，仅计算渲染长度 | 最终时间戳9223372036854775807；derived_length=1 |
| S29 | 当前8voice PianoSynth拓扑，CC67先于同通道两个音 | sounding_voices=2；soft_pedal_voices=1 |

#### 4.4.2 复审证据命名空间说明与引用规范

- **首次缺陷复现证据（S01～S29）**：上述探针观测记录为 2026-10-02 首次审计时复现 54 项缺陷的历史事实，保留原输入与输出，不因后续修复修改其历史记录。
- **阶段实施与闭环证据（EVID-001～EVID-067）**：Phase 0/A-H 软件实施期间产生的直接消费者证据使用专有 `EVID` 命名空间，完整记录于归档 [`docs/archive/audit-004-code-quality-fix-phases.md`](../archive/audit-004-code-quality-fix-phases.md)。
- **严禁混淆**：复审引用使用 `EVID-xxx`，首次问题证据引用使用 `Sxx`，两套证据系统严格分立，不将 EVID 误写为 S，亦不以 S 探针通过代指已修复。

### 4.5 待用户Windows实机验证

本节为独立未验证的实机测试矩阵。**52项软件缺陷关闭与2项分层缓解不因软件自动化门禁与单测通过而自动勾选全平台通过**。以下项目必须在具备真实硬件与第三方软件的环境中由用户手动复核：

| 组合 | 应验证不变量 | 验证状态 |
| --- | --- | --- |
| 已加载VST3+Editor+重扫 | 可丢弃会话观察editor/callback在unload前停止；本轮未实测商业插件崩溃 | 待用户实机验证 |
| 同名/再次拖入已知VST3 | 乐器/效果过滤与真实description一致；unload后拖回已扫描插件可加载 | 待用户实机验证 |
| 替换原生Take/Save As/预设rename | 用复制数据验证，文件A不被B元数据编辑改写；规范化等路径不删除、碰撞先确认 | 待用户实机验证 |
| 慢插件取消/退出 | 记录取消完成、实际线程退出、句柄/头关闭，不冒险强杀正式会话 | 待用户实机验证 |
| Count-in/采样率切换 | 一/两完整小节到下一downbeat；活动48k↔44.1k位置/时长不变形 | 待用户实机验证 |
| 辅助窗口/失焦/输入法 | 按住一键转到settings/editor后再松，确认后续释放；IME、DPI及多窗口键状态完整测试 | 待用户实机验证 |
| 密集回放/热插拔/声卡 | 记录真实驱动分配、Listener/UI、异常几何计数和声音；真实声卡物理插拔与毛刺 | 待用户实机验证 |
| 8复音物理SLA实测 | 单核 CPU $\le 0.7\%$ SLA 实测；目前仅保持代码逻辑零三角，新实机未测 | 待用户实机验证 |

---

## 5. 修复路线图

状态与项集合严格以第8章为准。本节列出全部非关闭项的复审与维护路线，已关闭项索引归档记录，不再排为未完成修复。

### 5.1 立即处理（P0）

本轮无 P0 登记（0 项）。

### 5.2 非关闭项复审与维护路线（P1 已缓解项）

当前存在 **2 项已缓解（Mitigated）** 缺陷，均为 P1。两项的核心自有发声路径已实现零锁零分配闭环，但由于保留使用 JUCE 原生 VST3 适配器（submodules 未修改），其框架层仍存在 SpinLock 与 CriticalSection / 2048 消息上限。依据用户批准的分层验收决策，评定为“已缓解”，按以下路线受控跟踪：

#### 5.2.1 `THR-001`：实时回调常规路径仍有阻塞锁（P1，已缓解）

- **已修自有路径**：产品自有发声与调度链路（`BuiltinSynthesiser`、`AudioEngine`、`RealtimeQueue`、`MetronomeProcessor`）已消除阻塞锁，键盘事件入队消息线程 `dispatchPendingDisplayEvents()` 处理，达成产品自有路径零锁闭环（EVID-041, EVID-043）。
- **用户既有分层接受来源**：`docs/issues/known-issues.md` §1“第三方 VST3 插件适配器的框架级并发锁与事件限制”、`docs/archive/audit-004-code-quality-fix-phases.md` §Phase E及Phase H中用户批准的分层验收决策；保留使用 JUCE 原生 `AudioPluginFormatManager` 适配器（不修改 submodules），其 `juce_VST3PluginFormatImpl.h` 的 `processBlock()` 仍获取 `SpinLock processMutex`。
- **重开触发条件**：自有路径重新引入阻塞锁/争用，或升级 JUCE/重构 VST3 宿主层导致锁争用扩大，或用户撤销分层验收时重开。
- **具体复审时点**：本地化小阶段验收时复核接口状态，以及下一次 JUCE 或插件事件路径变更前（非伪造固定期限）。
- **维护与下一步**：维持自有路径无锁单向通信；在未来若改造 VST3 宿主层时跟进无锁适配器。

#### 5.2.2 `PERF-001`：密集播放和重复预设循环突破回调预分配（P1，已缓解）

- **已修自有路径**：产品自有音频链路达成零堆分配，密集 MIDI 播放及未 drain 预设循环无堆增长（EVID-042）；消除异常缓冲尺寸时的 `setSize` 重新分配兜底，改为超协商尺寸安全静音并记录原子计数（known-issues ERR-002 闭环）。
- **用户既有分层接受来源**：`docs/issues/known-issues.md` §1、`docs/archive/audit-004-code-quality-fix-phases.md` §Phase E及Phase H中用户批准的分层验收决策；第三方 VST3 插件适配器（JUCE `juce_VST3Common.h`）MIDI 事件转换使用带 `CriticalSection` 的 `Array`，且单块存在 2048 条事件上限，超额截断，此框架级行为由用户分层接受。
- **重开触发条件**：自有音频回调检测到堆分配、预分配溢出，或第三方插件通道架构变更时重开。
- **具体复审时点**：本地化小阶段验收时复核，以及下一次 JUCE 或插件事件路径变更前（非伪造固定期限）。
- **维护与下一步**：维持自有路径零堆分配与有界 SPSC 交换；未来评估第三方插件事件通道架构改造。

### 5.3 已关闭项归档索引（52 项）

原登记的其余 52 项按逐项原触发消费者、实现路径及适用回归证据关闭；不是每项都新增永久测试，也不是凭默认测试绿灯一键关闭。详细过程与完整输入在 [实施归档](../archive/audit-004-code-quality-fix-phases.md)，第 8 章保留初始影响/证据并追加复审引用。

- **Phase 0：测试基础设施与安全前置（3 项）**
  - `TEST-001` (P1)、`TEST-002` (P1)、`AUDIT-002 TEST-014` (P2)
  - 闭环要点：消除 NRVO 依赖、隔离用户目录、Chord 纳入默认门禁；证据 EVID-001～EVID-003。
- **Phase A：已有数据保护与持久化一致性（6 项）**
  - `ERR-001` (P1)、`SEC-001` (P1)、`SEC-002` (P1)、`ERR-002` (P1)、`QUAL-006` (P2)、`QUAL-016` (P2)
  - 闭环要点：同目录事务替换、重命名同路径保护与冲突确认、Take 绑定解绑、同步保存取代旧 timer、深拷贝完整、启动预设一致；证据 EVID-006～EVID-009, EVID-013。
- **Phase B：文件准入与时间线数值安全（6 项）**
  - `SEC-003` (P1)、`SEC-004` (P1)、`SEC-005` (P1)、`SEC-006` (P1)、`QUAL-005` (P1)、`ERR-004` (P2)
  - 闭环要点：专有编码校验、采样率范围、防溢出加法、0x58 元数据、乱序时间线规范化、截断轨拒绝；证据 EVID-012～EVID-016。
- **Phase C：插件与活动 DSP / Transport 所有权（7 项）**
  - `AUDIT-001 THR-004` (P1)、`AUDIT-002 THR-001` (P1)、`known-issues §2/Phase 6-2 播放速度控制` (P1)、`THR-002` (P1)、`QUAL-014` (P2)、`QUAL-015` (P2)、`ARCH-002` (P2)
  - 闭环要点：重扫停 callback 关 Editor、变速/Stop 音频块入口消费、协作取消与异步等待、离线 nonRealtime 模式、插件 description 稳定身份；证据 EVID-020～EVID-024。
- **Phase D：发音身份与采样级 Transport 边界（9 项）**
  - `QUAL-001` (P1)、`QUAL-002` (P1)、`QUAL-003` (P1)、`QUAL-004` (P1)、`ERR-003` (P1)、`QUAL-018` (P1)、`QUAL-017` (P2)、`FIX-035` (P2)、`QUAL-019` (P2)
  - 闭环要点：NoteOff 身份锁定、重复物理键释放、暂停捕获终结、末尾 NoteOff 交付、keyUp 显式拒绝、采样率切换重基准、Seek 状态恢复、预备拍下拍对齐、柔音 CC67 继承；证据 EVID-028～EVID-035。
- **Phase E：预设永久身份与实时/离线执行闭包（5 项）**
  - `ARCH-003` (P2)、`ARCH-004` (P2)、`AUDIT-002 THR-003` (P1)、`known-issues ERR-002` (P1)、`known-issues §1/节拍器每拍三角函数与全回调零三角 SLA 不一致` (P2)
  - 闭环要点：预设 UUID 永久身份、不可变 RecordedPreset 表、实时/离线同构快照、Listener 移入消息线程 SPSC 队列、超协商尺寸安全静音、全回调零三角函数；证据 EVID-039～EVID-043。
- **Phase F：映射看板、交互与声学边界（9 项）**
  - `ARCH-001` (P2)、`QUAL-007` (P2)、`QUAL-008` (P2)、`QUAL-009` (P2)、`QUAL-010` (P2)、`QUAL-012` (P2)、`QUAL-013` (P2)、`QUAL-011` (P3)、`known-issues §1/A4 基准音高范围与项目契约不一致` (P2)
  - 闭环要点：双看板统一只读投影、输入输出通道解耦、几何重建标签保持、静音最高优先、fadeSpeed 收缩终止、圆角新值重建、Notes 正常编辑、最低八度标注、A4 400~480 Hz 支持；证据 EVID-046～EVID-053。
- **Phase G：诊断资源、ADR 与工程门禁收敛（6 项）**
  - `RES-001` (P2)、`OBS-001` (P2)、`CMPL-001` (P2)、`CMPL-002` (P2)、`ENG-001` (P2)、`TEST-003` (P3)
  - 闭环要点：日志有界/故障保护、MIDI raw 力度、细粒度 include、严格 ViewHost 与默认 lifecycle；Windows 编译位点修复与全量 tidy 后验分开，不声称 WSL 产品重编归零；证据 EVID-054～061、065/066。
- **Phase H：契约文档与最终集成验收（1 项）**
  - `DOC-001` (P3)
  - 闭环要点：现行功能/验收文档与真实实现契约同步；证据 EVID-062～EVID-064, EVID-067。

### 5.4 后续演进与迁移任务（非 54 缺陷集合）

- **任务**：[本地化完整消息模板收口](../roadmap/current-iteration.md)，遵循 [ADR-015](../decisions/ADR-015-localized-message-templates-and-punctuation.md)，规划已确定、代码迁移待开始。
- **性质**：演进阶段任务，不追溯为首次违例，不另开 54 项集合新 CMPL。

### 5.5 覆盖率与集合校验

- **非关闭项集合（2 项）**：`THR-001`、`PERF-001`，与第 8 章“已缓解”项完全一致。
- **已关闭项集合（52 项）**：与第 8 章“已关闭”项完全一致。
- **并集与基线对比**：2 + 52 = 54 项，与第 8 章登记表完全一致，差集为空。
- **本次实际 `comm` 对照**：归档实施表与第 8 章分别提取完整身份＋原优先级、去重排序；另将第 8 章非关闭身份与 §5.2 路线身份对照。以下两条命令均无输出、exit 0；临时文件清理后可由上述 Markdown 表重新生成，不依赖会话 artifact：

```bash
LC_ALL=C comm -3 /tmp/devpiano-audit004-doc-original.ids /tmp/devpiano-audit004-doc-current.ids
LC_ALL=C comm -3 /tmp/devpiano-audit004-doc-nonclosed.ids /tmp/devpiano-audit004-doc-route.ids
```

---

## 6. 最终结论

### 6.1 当前判断

**软件实现层面收敛完成，综合平台维持受控评级；不得伪称 A 级认证。**
1. **软件实现层面**：原 54 项缺陷中 52 项已关闭，2 项已缓解，P1/P2/P3 全部有直接闭环证据或用户分层接受依据。固定拓扑、所有权边界、数据完整性事务和自有音频无锁零分配均已建立。
2. **综合平台层面**：维持受控的保守评级。因本次为文档复审而非全新全量代码审计，且真实物理声卡、商业插件、操作系统 IME/DPI 矩阵（§4.5）仍待实机验证，故不能宣布整体平台已达 A 级认证。

### 6.2 是否建议继续新增功能

**可有条件开展小步收口**。
当前代码库已具备承载小步演进的能力，首要推进 ADR-015 本地化消息模板收口阶段。对于大规模新增声学（Phase 36）或分区叠层（Phase 37）功能，仍建议在完成 §4.5 实机抽样复核后开展。

### 6.3 是否建议先重构 / 补测试 / 补文档

- **重构**：**否**。原架构缺陷已在 Phase A-H 窄边界闭环，无须开展全局架构重写。
- **补测试**：保留已验证的确定性回归；本地化迁移仅对不确定的参数/回退及消费者边界补必要机制测试，不钉译文。不凭此次问题关闭宣称软件测试已完备；§4.5 实机组合独立补验。
- **补文档**：Phase H 已同步其原契约；本次已接受 ADR-015 并建立新规划，消息迁移实施后按实际行为更新国际化说明。

### 6.4 下一步三件事

1. **实施 ADR-015 本地化完整消息模板与标点规范的既有调用迁移**。
2. **保持自有音频路径无锁零分配**；在本地化小阶段验收及下一次 JUCE/插件路径变更前复核 `THR-001` 与 `PERF-001`。
3. **协助用户在 Windows 真实环境下完成 §4.5 插件/声卡/IME 实机抽样复核**。

---

## 7. 复审记录

### 7.1 首次审计登记（2026-10-02）

- 基线：main @ `732cde18dc8116cb21f0325377415cedfef03811`。
- 已关闭：无。没有源码/测试/既有文档修复，不声明历史问题本轮已修。
- 新反证的历史问题保留原ID列未处理，既有报告状态不覆盖。
- 验证：§4.1三闸门/独立Windows默认测试/Chord补跑及§4.4消费者冒烟；tidy和原win-build失败如实保留。
- 结论：54项未处理，45新发现、9引用/重开。下次复审追加小节，第8章状态驱动首页/路线图；修复历史写实现迭代/roadmap，不堆入登记表。

### 7.2 Phase H 软件交付后复审（2026-10-05）

- **复审基线**：软件交付 `0f60769`（Phase H 使用代码基线 `166c545`；各阶段各自基线见归档）。本次文档工作基线 `4a77f66`，另一次本地化修复不等于本轮重跑 Phase H 产品门禁。
- **复审依据**：
  1. 完整查阅归档实施记录 [`docs/archive/audit-004-code-quality-fix-phases.md`](../archive/audit-004-code-quality-fix-phases.md) 中 Phase 0/A-H 的 54 项实施记录、复建配方及 EVID-001～EVID-067 直接证据；
  2. 查阅 [`docs/issues/known-issues.md`](../issues/known-issues.md) 与 [`docs/reference/acceptance.md`](../reference/acceptance.md) 确认当前已知风险与阶段验收边界；
  3. 核对 [ADR-015](../decisions/ADR-015-localized-message-templates-and-punctuation.md)：本次用户已接受决策，既有调用待迁移，不追溯为初审违例或重开已关闭 DOC-001。
- **状态更新判定**：
  - **已关闭（52 项）**：逐项核对归档索引和对应消费者，原根因已消除；运行、代码路径、适用回归与文档证据按各项范围记录，不声称本轮重新执行每个程序。
  - **已缓解（2 项：`THR-001`、`PERF-001`）**：原全回调范围仍含已批准的第三方框架锁与分配。产品自有发声链路（`BuiltinSynthesiser`、`AudioEngine` 等）已达成零锁零分配，但第三方 JUCE VST3 插件适配器仍包含 SpinLock、CriticalSection 及 2048 消息上限。依据用户批准的分层验收决策（见 known-issues §1 及 Phase E 归档），保守评定为“已缓解”，明确重开触发条件与具体复审时点。
  - **未处理 / 处理中 / 已暂缓**：0 项。
- **实机验证独立性**：§4.5 保留原实机组合并另列性能 SLA 测量；强杀/断电及永久阻塞等限制不由软件关闭或归档变成认证通过。
- **纪律遵守**：第 8 章登记表为唯一问题状态源；原 54 项身份、标题、级别、来源、影响和证据内容严格保留；已关闭项索引归档，不塞入修复流水账；新评价区分软件进展与平台条件，不伪称 A 级认证。
- **本次文档验证**：实际 Markdown 渲染与本地目标检查通过；原 54 个身份/优先级无缺失或多余，首页统计与第 8 章一致，两个非关闭项均有维护路线。原首次命令/统计、§7.1 与 S01～S29 输入输出保留，原 C++/PowerShell 消费者输入逐字相同；归档 Phase H 提取配方实际执行，四份复建输入与迁移前一致。业务源码及旧审计/ADR/其他归档 hash 未变，`./scripts/dev.sh format --check` 通过。
- **本次未执行**：产品构建、单元测试、全量 tidy、Release 或实机认证；这里的复审结论依据已有分阶段直接证据，不把文档检查冒称软件重测。所有文档修改留在未暂存工作区，等待用户审查。

---

## 8. 附录：问题总表（登记表）

唯一状态源。原 54 项身份、标题、级别、来源、初始影响与原始证据内容严格保留。本次 2026-10-05 复审更新状态、复审直接证据引用、风险接受、重开条件与下一步。
当前状态分布：**52 项已关闭，2 项已缓解（`THR-001`、`PERF-001`），0 项未处理/处理中/已暂缓**。
说明：关闭基于 Phase 0/A-H 代码实现与直接消费者验证证据（EVID-001～EVID-067）；已缓解项基于用户批准的分层验收决策（产品自有路径闭环，第三方框架限制分层受控缓解）；实机硬件与商业插件验证边界（§4.5）独立保留，不因软件关闭而扩大为全平台认证。

| ID | 领域 | 问题标题 | 优先级 | 状态 | 来源 | 影响摘要 | 证据 | 风险接受原因 | 重开条件 | 下一步 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ERR-001 | 错误处理/持久化 | 已有导出目标不是事务替换：成功追加，失败删除原文件 | P1 | 已关闭 | 审计 | 覆盖 WAV/MIDI 时仍读取旧音频/音符；WAV 失败或取消可删除用户原文件。 | source/Recording/WavFileExporter.cpp:77-91；source/Recording/MidiFileExporter.cpp:47-49；source/Recording/PluginOfflineRenderer.cpp:87-100；source/Export/WavExportTask.cpp:207-216；S13-S15；[Windows 冒烟确认内置 WAV、MIDI 覆盖及失败删原文件；插件覆盖同根静态证据]；复审：[Phase A EVID-006, EVID-013](../archive/audit-004-code-quality-fix-phases.md#phase-a-实施记录与直接验证2026-10-02)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| SEC-001 | 安全/数据完整性 | 预设重命名可覆盖另一预设或删除自身目标 | P1 | 已关闭 | 审计 | A→已有 B 无确认覆盖；A→A?、Windows 大小写等同路径重命名可能随后删除新文件。 | source/Layout/PresetFlowSupport.cpp:302-330；source/Layout/PerformancePreset.cpp:209-223；docs/reference/features/performance-presets.md:156；[静态确认；运行触发未验证]；复审：[Phase A EVID-007, EVID-062](../archive/audit-004-code-quality-fix-phases.md#phase-a-实施记录与直接验证2026-10-02)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| SEC-002 | 安全/数据完整性 | 原生文件绑定未随 Take 替换/Save As 更新 | P1 | 已关闭 | 审计 | 打开原生 A 后导入/录制 B，再保存歌曲信息会把 B 全量事件覆盖到 A；Save As 也不重绑定。 | source/Recording/RecordingSessionController.cpp:66-71,350-359,370-373,396-403,667-695,765-775；[静态确认；运行触发未验证]；复审：[Phase A EVID-008, EVID-059](../archive/audit-004-code-quality-fix-phases.md#phase-a-实施记录与直接验证2026-10-02)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| SEC-003 | 安全/数据完整性 | 原生 MIDI 编码信任未校验的解码长度前缀 | P1 | 已关闭 | 审计 | 极小 .devpiano 的 midiData="-1." 可请求 SIZE_MAX 并抛 bad_alloc；正的大长度可放大内存；未声明主 GUI 的退出表现。 | source/Recording/PerformanceFile.cpp:70-79,269-279；JUCE API 证据 juce_MemoryBlock.cpp:394-419；S20；[Windows 冒烟观察到 bad_alloc；实际主窗口异常处理结果未验证]；复审：[Phase B EVID-012, EVID-016](../archive/audit-004-code-quality-fix-phases.md#phase-b-实施记录与直接验证2026-10-03)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| SEC-004 | 安全/数据完整性 | 原生采样率/长度缺少可表示范围验证 | P1 | 已关闭 | 审计 | sampleRate=1e-300 通过正数检查；播放缩放得到无穷并转换为 int64，MSVC 冒烟一块即结束，时长错误。 | source/Recording/PerformanceFile.cpp:206-213；source/Recording/RecordingEngine.cpp:225-231,643-654；S19；[Windows 冒烟确认极小正率被接受并错误结束；C++ 越界转换为静态证据]；复审：[Phase B EVID-013, EVID-016](../archive/audit-004-code-quality-fix-phases.md#phase-b-实施记录与直接验证2026-10-03)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| SEC-005 | 安全/数据完整性 | 饱和时间戳后 +1/尾部采样加法仍溢出 | P1 | 已关闭 | 审计 | INT64_MAX 经 clampToInt64 后，finalEvent+1 和两秒尾部加法可能有符号溢出，输出时间线/终止条件失效。 | source/Recording/RenderPipeline.cpp:40-47；source/Recording/WavFileExporter.cpp:105-107；source/Recording/PluginOfflineRenderer.cpp:108-112；相关 AUDIT-002 SEC-006 的转换限幅仍有效；S28；[Windows MSVC边界冒烟确认最终时间戳MAX却派生长度1；有符号溢出为静态证据]；复审：[Phase B EVID-013](../archive/audit-004-code-quality-fix-phases.md#phase-b-实施记录与直接验证2026-10-03)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| SEC-006 | 安全/数据完整性 | MIDI 拍号元数据未验证负载及位移指数 | P1 | 已关闭 | 审计 | 普通小 .mid 的 0x58 元数据可令 JUCE 执行 1<<255，属于未定义位移；未声称已复现崩溃。 | source/Recording/MidiTrackMergeEngine.cpp:226-234；JUCE API 证据 juce_MidiMessage.cpp:867-873；[静态确认；运行触发未验证]；复审：[Phase B EVID-014, EVID-016](../archive/audit-004-code-quality-fix-phases.md#phase-b-实施记录与直接验证2026-10-03)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| AUDIT-001 THR-004 | 线程安全 | 增量重扫绕过停音频/关 Editor 守卫 | P1 | 已关闭 | 历史问题重开（新绕过路径） | Scan 直接卸载仍被音频回调和 Editor 使用的实例；存在 UAF/崩溃风险，真实 VST3 崩溃未实测。 | source/MainComponent.cpp:219；source/Plugin/PluginOperationController.cpp:128-145；source/Plugin/PluginHost.cpp:82-83；source/Audio/AudioEngine.cpp:129-146；AUDIT-003:450；[静态确认；运行触发未验证]；复审：[Phase C EVID-021, EVID-024](../archive/audit-004-code-quality-fix-phases.md#phase-c-实施记录与直接验证2026-10-03)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| AUDIT-002 THR-001 | 线程安全 | 音色重建仍从消息线程应用活动 DSP 参数 | P1 | 已关闭 | 历史问题重开（tone switch 新反证） | 普通参数 setter 已原子化，但 rebuildSynth 最后直接 applyPendingParameters，可能与音频渲染/快照应用并发写 voice、roomReverb 和派生参数。 | source/Audio/AudioEngine.cpp:248-276,315-378；source/Main.cpp:31-38,52-63；source/MainComponent.cpp:1094-1097；AUDIT-002 第8章 THR-001；[静态确认；运行触发未验证]；复审：[Phase C EVID-021, EVID-024](../archive/audit-004-code-quality-fix-phases.md#phase-c-实施记录与直接验证2026-10-03)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| THR-001 | 线程安全 | 实时回调常规路径仍有阻塞锁 | P1 | 已缓解 | 审计 | collector、keyboard state、Synthesiser 与 presetChangeLock 均可在 audio callback 阻塞；试验观察至少50ms争用等待。 | source/Audio/AudioEngine.cpp:117-118,159,491；source/Recording/RecordingEngine.cpp:547-549,635-640；本地 JUCE collector:86、keyboard state:152、Synthesiser:193；S03；[Windows 人为争用冒烟确认 callback 等待；实际声卡毛刺未测]；复审：[Phase E EVID-041, EVID-043](../archive/audit-004-code-quality-fix-phases.md#phase-e-实施记录与直接验证2026-10-04)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | 用户批准的分层验收（docs/issues/known-issues.md §1、Phase E 归档；自有路径无锁闭环，第三方 JUCE VST3 适配器框架 SpinLock 由用户接受，不修改 submodules） | 自有路径重新引入阻塞锁/争用，或升级 JUCE/重构 VST3 宿主层导致锁争用扩大，或用户撤销分层验收时重开 | 维持自有路径无锁单向通信；具体复审时点：本地化小阶段验收及下一次 JUCE 或插件事件路径变更前 |
| PERF-001 | 性能/实时性 | 密集播放和重复预设循环突破回调预分配 | P1 | 已缓解 | 审计 | 有效5000 CC/128帧块触发10次alloc/realloc，plugin resize为0；循环反复排同一预设也超过单次Take计数reserve。 | source/Audio/AudioEngine.cpp:77-81,452-492；source/Recording/RecordingEngine.cpp:247-255,547-549,635-640；S02；[Windows 密集 MIDI 冒烟确认分配；重复预设循环为静态证据]；复审：[Phase E EVID-042](../archive/audit-004-code-quality-fix-phases.md#phase-e-实施记录与直接验证2026-10-04)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引)；框架边界另见 [EVID-043](../archive/audit-004-code-quality-fix-phases.md#phase-e-实施记录与直接验证2026-10-04) | 用户批准的分层验收（docs/issues/known-issues.md §1、Phase E 归档；自有路径零堆分配闭环，第三方 JUCE VST3 适配器框架分配及 2048 限制由用户接受，不修改 submodules） | 自有音频回调检测到堆分配、预分配溢出，或第三方插件通道架构变更时重开 | 维持自有路径零堆分配与有界 SPSC 交换；具体复审时点：本地化小阶段验收及下一次 JUCE 或插件事件路径变更前 |
| known-issues §2/Phase 6-2 播放速度控制 | 线程安全 | 活动变速/Stop 在消息线程改写音频游标 | P1 | 已关闭 | 已修复项新游标反证 | 原子倍率并未保护 playbackEventIndex/hasRenderedPlaybackBlock/loopWrapPending；已进入render的callback不会被 stopped 原子写等待。 | source/Recording/RecordingEngine.cpp:409-442,519-566,577-590,694-704；source/Recording/RecordingSessionController.cpp:440-443,594-600；known-issues §2 Phase 6-2 播放速度控制；[静态确认；运行触发未验证]；复审：[Phase C EVID-022](../archive/audit-004-code-quality-fix-phases.md#phase-c-实施记录与直接验证2026-10-03)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| THR-002 | 线程安全 | 取消/析构 WAV 任务可能强制终止工作线程 | P1 | 已关闭 | 审计 | stopThread(3000) 超时可进入 Windows TerminateThread；writer/stream/plugin 栈析构被跳过，句柄/文件收尾风险。 | source/Export/WavExportTask.cpp:74-87,163-166,169-188；JUCE Thread.cpp:250-274 与 Windows thread kill API 证据；[静态确认；运行触发未验证]；复审：[Phase C EVID-023, EVID-024](../archive/audit-004-code-quality-fix-phases.md#phase-c-实施记录与直接验证2026-10-03)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| ERR-002 | 错误处理/持久化 | 即时同步保存未取代旧防抖快照 | P1 | 已关闭 | 审计 | 新扫描缓存/设置同步落盘后，旧timer仍可整份回写旧值，破坏增量崩溃安全及新恢复信息。 | source/Settings/SettingsStore.cpp:136-148,439-450；source/Plugin/PluginOperationController.cpp:21-23,150-153；S12；[Windows 独立设置文件冒烟确认 new-cache 被旧timer回滚成 old-cache]；复审：[Phase A EVID-009](../archive/audit-004-code-quality-fix-phases.md#phase-a-实施记录与直接验证2026-10-02)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| QUAL-001 | 质量/功能 | 播放移调/掩码变化不锁定已发音身份 | P1 | 已关闭 | 审计 | NoteOn60 后将offset改成+1，NoteOff发61；原60仍亮/发声到后续panic。enabled/mask切换同样存在身份不一致。 | source/Audio/AudioEngine.cpp:455-483；source/MainComponent.cpp:163-174；S01；[Windows AudioEngine/RecordingEngine 冒烟确认原音未释放]；复审：[Phase D EVID-028, EVID-030](../archive/audit-004-code-quality-fix-phases.md#phase-d-实施记录与直接验证2026-10-04)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| QUAL-002 | 质量/功能 | 重复物理键同音在首个松键时被提前关闭 | P1 | 已关闭 | 审计 | 默认 Q/K 都为Ch1 MIDI72，松Q时K仍held却pitch72已off；K自动重复被抑制，不能自动补响。 | source/Core/KeyMapTypes.h 默认布局；source/Input/KeyboardMidiMapper.cpp:267-291；S05；[Windows 真实 MidiKeyboardState 冒烟确认]；复审：[Phase D EVID-028](../archive/audit-004-code-quality-fix-phases.md#phase-d-实施记录与直接验证2026-10-04)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| QUAL-003 | 质量/功能 | 暂停录制丢掉期间唯一的 NoteOff/踏板释放 | P1 | 已关闭 | 审计 | 暂停前记录On、暂停期间真实松键Off不采集，保留Take出现孤儿On；导出的MIDI也无法配对。 | source/Recording/RecordingEngine.cpp:176-197,391-406；source/Audio/AudioEngine.cpp:436-444；S16；[Windows 捕获状态机冒烟确认 on=1/off=0]；复审：[Phase D EVID-029](../archive/audit-004-code-quality-fix-phases.md#phase-d-实施记录与直接验证2026-10-04)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| QUAL-004 | 质量/功能 | 精确播放末尾的 NoteOff 未在音频路径交付 | P1 | 已关闭 | 审计 | length=128 且Off@128时半开首块排除Off，advance随即停止，下块再不渲染Off，依赖30Hz UI后续panic。 | source/Recording/RecordingEngine.cpp:543-545,611-617；source/Recording/RenderPipeline.cpp:40-47；S17；[Windows 终端事件冒烟确认 ended=1、off=0]；复审：[Phase D EVID-030](../archive/audit-004-code-quality-fix-phases.md#phase-d-实施记录与直接验证2026-10-04)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| QUAL-005 | 质量/功能 | 原生加载保留乱序事件但播放器假定有序 | P1 | 已关闭 | 审计 | 未来Off排在On@0前面时加载成功但On永不交付；seek lower_bound 同样依赖有序。 | source/Recording/PerformanceFile.cpp:225-235；source/Recording/RecordingEngine.cpp:535-545,694-704；S18；[Windows 原生往返后播放冒烟确认 admitted=1/on=0]；复审：[Phase B EVID-015](../archive/audit-004-code-quality-fix-phases.md#phase-b-实施记录与直接验证2026-10-03)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| ERR-003 | 错误处理/持久化 | 接受并序列化的 keyUp 绑定没有执行入口 | P1 | 已关闭 | 审计 | down时trigger不匹配不进入heldKeys，up时wasHeld仍false，完整按/松周期无声且无反馈。 | source/Input/KeyboardMidiMapper.cpp:267-280,324-328；source/Layout/PerformancePreset.cpp:45-46；source/Core/KeyMapTypes.h:15-17,161-177；S26；[Windows 完整按下/释放周期确认未处理、未发音]；复审：[Phase D EVID-031](../archive/audit-004-code-quality-fix-phases.md#phase-d-实施记录与直接验证2026-10-04)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| QUAL-018 | 质量/功能 | 设备采样率变更未重基准活动播放/录制时间域 | P1 | 已关闭 | 审计 | 设置允许活动时48k→44.1k；prepare仅更改AudioEngine及blockSize，播放ratio和Take采样率不更新，余下播放变慢/录制时长变形。 | source/Settings/SettingsComponent.cpp:149-156；source/Audio/AudioEngine.cpp:57-62,436-444；source/Recording/RecordingEngine.cpp:149-156,225-228；[静态确认；运行触发未验证]；复审：[Phase D EVID-032, EVID-034](../archive/audit-004-code-quality-fix-phases.md#phase-d-实施记录与直接验证2026-10-04)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| TEST-001 | 测试 | 音频fixture依赖可选 NRVO 保持自引用指针 | P1 | 已关闭 | 审计 | makeBlock返回具名pair，移动后info.buffer不会重绑定；C++17不保证NRVO，关闭可选NRVO后可能向callback传悬垂栈指针。 | source/tests/AudioEngineTest.cpp:28-46；Microsoft /Zc:nrvo文档；S24；[Windows /Zc:nrvo- 冒烟确认info.buffer不指向调用者live buffer；未解引用悬垂指针制造崩溃]；复审：[Phase 0 EVID-001, EVID-003](../archive/audit-004-code-quality-fix-phases.md#phase-0-实施记录与直接验证2026-10-02)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| TEST-002 | 测试 | 默认路径测试触碰真实用户日志/预设目录 | P1 | 已关闭 | 审计 | DiagnosticsTest构造生产logger可截减用户既有>512KiB日志并追加banner；Preset目录probe也创建真实用户目录。 | source/tests/DiagnosticsTest.cpp:17-21；source/Diagnostics/DevPianoLogger.cpp:5-16；source/tests/PerformancePresetTest.cpp:274-278；FileLogger构造裁剪契约；[测试调用链静态确认；未用用户真实日志做破坏性重现]；复审：[Phase 0 EVID-002, EVID-003, EVID-060](../archive/audit-004-code-quality-fix-phases.md#phase-0-实施记录与直接验证2026-10-02)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| AUDIT-002 THR-003 | 线程安全 | 音频线程 MIDI Listener 同步进入 UI/Timer | P1 | 已关闭 | known-issues §1 原问题引用 | Main断言消息线程但播放callback同步通知；CustomKeyboard启动Timer。原子颜色数组已存在，不能声称再原子化即修复。 | docs/issues/known-issues.md:58-64；source/Audio/AudioEngine.cpp:118,491；source/MainComponent.cpp:597-601；source/UI/CustomKeyboard.cpp:653-666；[既有确认问题；本轮静态复核，不重新编号]；复审：[Phase E EVID-041](../archive/audit-004-code-quality-fix-phases.md#phase-e-实施记录与直接验证2026-10-04)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| known-issues ERR-002 | 性能/实时性 | 异常插件缓冲尺寸仍保留重分配兜底 | P1 | 已关闭 | known-issues 原编号引用 | 超过协商预分配几何时callback setSize；计数/异步日志只降低I/O影响，不使异常帧零分配。 | docs/issues/known-issues.md:66-77（ERR-002）；source/Audio/AudioEngine.cpp:133-143；[既有风险；本轮未触发异常驱动块]；复审：[Phase E EVID-042](../archive/audit-004-code-quality-fix-phases.md#phase-e-实施记录与直接验证2026-10-04)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| QUAL-006 | 质量/功能 | Phase35字段在 SettingsModel 深拷贝中遗漏 | P2 | 已关闭 | 审计 | 防抖快照将173 BPM/启用/count-in/动态力度等还原为默认或旧值；正常退出直接保存不消除中途快照错误。 | source/Settings/SettingsModel.h:135-143,232-283；source/Settings/SettingsStore.cpp:136-148；S11；[Windows store+timer 落盘/读取冒烟确认]；复审：[Phase A EVID-009](../archive/audit-004-code-quality-fix-phases.md#phase-a-实施记录与直接验证2026-10-02)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| QUAL-007 | 质量/功能 | 鼠标输入通道被观察到的输出通道反向污染 | P2 | 已关闭 | 审计 | 同一C4在Ch1→2、Ch2→3矩阵下两次点击分别发Ch2/Ch3；回放也能改随后鼠标路由。 | source/UI/CustomKeyboard.cpp:544-546,596-598,653-660；source/MainComponent.cpp:377-395；S06；[Windows 真实 CustomKeyboard+MidiChannelMapper 冒烟确认]；复审：[Phase F EVID-046, EVID-063](../archive/audit-004-code-quality-fix-phases.md#phase-f-实施记录与直接验证2026-10-05)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| ARCH-001 | 架构 | 两张演奏映射看板未共同消费最终映射投影 | P2 | 已关闭 | 审计 | 实际A→Ch4/C5，QWERTY仍Ch1/C4，钢琴自行按原始binding反向匹配；违反Live Performance Map单一事实源。 | source/Input/KeyboardMidiMapper.cpp:434-447；source/UI/CustomKeyboard.cpp:249-293；source/MainComponentJiveAccessors.cpp:545-550,650-667；S10；[Windows 矩阵映射/快照冒烟确认；钢琴分支静态证据]；复审：[Phase F EVID-046, EVID-053, EVID-063](../archive/audit-004-code-quality-fix-phases.md#phase-f-实施记录与直接验证2026-10-05)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| QUAL-008 | 质量/功能 | 几何重建清空钢琴绑定标签 | P2 | 已关闭 | 审计 | 先setKeyboardLayout再setKeyboardSettings即标签31→0，resize/viewport变化也丢标签。 | source/UI/CustomKeyboard.cpp:280-303；source/MainComponent.cpp:1214-1217；S07；[Windows 真实 CustomKeyboard 冒烟确认]；复审：[Phase F EVID-046, EVID-063](../archive/audit-004-code-quality-fix-phases.md#phase-f-实施记录与直接验证2026-10-05)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| QUAL-009 | 质量/功能 | 静音绑定在 Shift/QWERTY 鼠标入口变为满力度 | P2 | 已关闭 | 审计 | 物理trigger保留rawVelocity=0，快照却transformVelocity(0)=1，鼠标使用该值发声；物理与鼠标优先级不同。 | source/Input/KeyboardMidiMapper.cpp:366,442；source/UI/QwertyComponent.cpp:347；source/MainComponent.cpp:417-431；S09；[Windows 快照冒烟确认0→1；鼠标发声消费端静态确认]；复审：[Phase F EVID-047, EVID-063](../archive/audit-004-code-quality-fix-phases.md#phase-f-实施记录与直接验证2026-10-05)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| QUAL-010 | 质量/功能 | fadeSpeed=1 合法端点不衰减且计时器不停止 | P2 | 已关闭 | 审计 | 设置UI允许1.00，松键120帧后fade仍1；加载器还允许>1，导致非收缩动画/越界alpha风险。 | source/Settings/SettingsComponent.cpp:243-250；source/UI/CustomKeyboard.cpp:617-641；source/Settings/SettingsStore.cpp:237-239；source/Layout/PerformancePreset.cpp:418；S08；[Windows 1.00端点冒烟确认；>1影响为静态证据]；复审：[Phase F EVID-048](../archive/audit-004-code-quality-fix-phases.md#phase-f-实施记录与直接验证2026-10-05)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| QUAL-012 | 质量/功能 | 实时圆角样式路径落后一版 | P2 | 已关闭 | 审计 | setBorderRadii先用旧值重建shape再赋新值；radius30角alpha255，改回0反而alpha0，未resize即显示反向。 | source/UI/jive/core/jive_BackgroundCanvas.cpp:87-93,158-173；source/UI/jive/core/jive_StyleSheet.cpp applyStyles；S22；[Windows 真实组件绘制与像素检查确认]；复审：[Phase F EVID-049](../archive/audit-004-code-quality-fix-phases.md#phase-f-实施记录与直接验证2026-10-05)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| QUAL-013 | 质量/功能 | 歌曲信息 Notes 继承只读 ListEditor | P2 | 已关闭 | 审计 | 生产factory设置readOnly=true，Metadata初始化未恢复可写；普通按键不改变Notes。 | source/UI/ViewHost.cpp:58-67；source/UI/jive/JiveModalDialog.cpp:286-298,439-444；source/tests/JiveModalDialogTest.cpp:264-289；S23；[Windows 生产 ViewHost 工厂冒烟确认 readonly=1、text不变]；复审：[Phase F EVID-050, EVID-059, EVID-063](../archive/audit-004-code-quality-fix-phases.md#phase-f-实施记录与直接验证2026-10-05)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| QUAL-014 | 质量/功能 | 离线插件实例未声明 nonRealtime 模式 | P2 | 已关闭 | 审计 | 独立导出实例默认仍向VST3传kRealtime，依赖offline分支的质量/流式等待行为不会启用。 | source/Recording/PluginOfflineRenderer.cpp:41-60；本地JUCE AudioProcessor.h默认false及VST3 setup/process mode消费点；[静态确认；运行触发未验证]；复审：[Phase C EVID-020](../archive/audit-004-code-quality-fix-phases.md#phase-c-实施记录与直接验证2026-10-03)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| QUAL-015 | 质量/功能 | 再次拖入已经发现的 VST3 被误判为没有类型 | P2 | 已关闭 | 审计 | addType重复身份返回false但仍有有效description；返回names仅包含新增项，导致扫描后unload再拖入无法加载。 | source/Plugin/PluginHost.cpp:181-190；source/Plugin/PluginOperationController.cpp:87-97；[静态确认；运行触发未验证]；复审：[Phase C EVID-020, EVID-024](../archive/audit-004-code-quality-fix-phases.md#phase-c-实施记录与直接验证2026-10-03)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| ARCH-002 | 架构 | 插件选择/恢复以显示名代替 description 身份 | P2 | 已关闭 | 审计 | 同名不同文件/ID的插件在列表去重且load首条匹配，另一插件不可选；过滤乐器时也可能载入同名效果。 | source/Plugin/PluginHost.cpp:getKnownPluginNames/getInstrumentPluginNames/getEffectPluginNames,296-300；source/MainComponentJiveAccessors.cpp:75-78；[静态确认；运行触发未验证]；复审：[Phase C EVID-020, EVID-024](../archive/audit-004-code-quality-fix-phases.md#phase-c-实施记录与直接验证2026-10-03)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| ARCH-003 | 架构 | 录制预设事件用可变目录索引作为永久身份 | P2 | 已关闭 | 审计 | [A,B]中B索引1，新增AA后索引1变AA；保存演奏回放到错误预设；uint8也会截断大索引。 | source/Layout/PresetFlowSupport.cpp:88-113；source/Layout/PerformancePreset.cpp:579-580；source/Recording/RecordingEngine.h:17-37；source/MainComponent.cpp:659-661；[静态确认；运行触发未验证]；复审：[Phase E EVID-039, EVID-062](../archive/audit-004-code-quality-fix-phases.md#phase-e-实施记录与直接验证2026-10-04)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| QUAL-016 | 质量/功能 | 启动恢复预设未设置控制器当前身份 | P2 | 已关闭 | 审计 | 启动applyPresetData只写Settings.lastActivePresetId；currentPresetId仍空，combo回退首项，随后绑定编辑autoSave直接失败，重启丢改动。 | source/MainComponent.cpp:145-153,501-537；source/Layout/PresetFlowSupport.cpp:99-117,163-164,203-209；source/MainComponentJiveAccessors.cpp:373-385；[静态确认；运行触发未验证]；复审：[Phase A EVID-007](../archive/audit-004-code-quality-fix-phases.md#phase-a-实施记录与直接验证2026-10-02)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| ARCH-004 | 架构 | 预设事件丢失可执行时序和离线语义 | P2 | 已关闭 | 审计 | 实时事件只传presetId等30Hz UI处理，块内后续音符用旧参数，末块queue可被Stop清掉；离线RenderEvent只复制默认MIDI，整段未执行声学预设变化。 | source/Recording/RecordingEngine.cpp:547-549,409-413；source/MainComponent.cpp:606-607,659-662；source/Recording/RenderPipeline.cpp:28-31；source/Recording/WavFileExporter.cpp:129-145；source/Recording/PluginOfflineRenderer.cpp:151-168；[静态确认；运行触发未验证]；复审：[Phase E EVID-040](../archive/audit-004-code-quality-fix-phases.md#phase-e-实施记录与直接验证2026-10-04)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| QUAL-017 | 质量/功能 | Seek/循环回跳未恢复目的位置控制器及音色状态 | P2 | 已关闭 | 审计 | panic释放踏板后只lower_bound游标，A前program/CC/pitch状态不chase；循环内后来的program可污染下一轮A前音符。 | source/Recording/RecordingEngine.cpp:335-340,478-507,569-574,694-704；[静态确认；运行触发未验证]；复审：[Phase D EVID-033](../archive/audit-004-code-quality-fix-phases.md#phase-d-实施记录与直接验证2026-10-04)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| FIX-035 | 质量/功能 | 预备拍在最后一拍起音而非下一下拍完成 | P2 | 已关闭 | 已修复项时长反证 | 120BPM/4/4一小节按第4个click在1.5s开始，而非2s；跨多拍的sequence变化只减1，UI延迟会改变倒计时。 | source/Recording/RecordingSessionController.cpp:55-62,802-815；source/Audio/MetronomeProcessor.h:154-163,219-221；known-issues §2 FIX-035；[静态确认；运行触发未验证]；复审：[Phase D EVID-034](../archive/audit-004-code-quality-fix-phases.md#phase-d-实施记录与直接验证2026-10-04)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| ERR-004 | 错误处理/持久化 | MIDI宽容尾字节也误接收缺失/截断轨 | P2 | 已关闭 | 审计 | readFrom失败后只要任一既有轨有事件就成功，2轨声明实际仅1轨仍报告导入成功。 | source/Recording/MidiFileImporter.cpp:16-23,33-40；JUCE MidiFile.cpp:385-412；S21；[Windows 缺失第二MTrk文件冒烟确认 import_success=1/trackCount=1]；复审：[Phase B EVID-014, EVID-062](../archive/audit-004-code-quality-fix-phases.md#phase-b-实施记录与直接验证2026-10-03)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| RES-001 | 资源 | 日志大小上限仅构造时截减而非会话滚动 | P2 | 已关闭 | 审计 | maxInitialFileSizeBytes不会限制后续logMessage；配置1024字节后同一会话写出8465字节，长期日志可无限增长。 | source/Diagnostics/DevPianoLogger.cpp:10-18,47-59；source/Diagnostics/DevPianoLogger.h:8-16；docs/reference/architecture.md:288；官方FileLogger构造契约；S04；[Windows 自有临时日志冒烟确认增长]；复审：[Phase G EVID-054, EVID-055, EVID-063](../archive/audit-004-code-quality-fix-phases.md#phase-g-实施记录与直接验证2026-10-05)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| CMPL-001 | 决策合规 | 业务头 WindowIconUtils 重新引入 JuceHeader | P2 | 已关闭 | 审计 | 违反ADR-012所有source头禁聚合头的决策本体，增加全量模块传递依赖。 | source/UI/WindowIconUtils.h:3；source/Main.cpp:10；docs/decisions/ADR-012-header-iwyu-and-granular-include-discipline.md:19-40；[精确grep及实际应用消费者确认]；复审：[Phase G EVID-057](../archive/audit-004-code-quality-fix-phases.md#phase-g-实施记录与直接验证2026-10-05)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| CMPL-002 | 决策合规 | 声明式业务仍使用 raw GuiItem 逃逸接口 | P2 | 已关闭 | 审计 | Main热重载直接rootItem->state，内置modal保留raw回调；ViewHost advanced接口与ADR-014 strict facade冲突，不宣称UAF。 | source/MainComponent.cpp:862-865；source/UI/ViewHost.h:122-126；source/UI/jive/JiveModalDialog.h:39-49；source/UI/jive/JiveModalDialog.cpp:286-312；ADR-014:113-128；[决策与源码逐条核对]；复审：[Phase G EVID-058, EVID-059, EVID-063](../archive/audit-004-code-quality-fix-phases.md#phase-g-实施记录与直接验证2026-10-05)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| ENG-001 | 工程化 | 编译零警告及全量 tidy 清零门禁不成立 | P2 | 已关闭 | 审计 | WSL Debug build有20次warning/2个源位点；全量144cpp tidy以5处项目诊断exit1，格式/测试通过不能替代这些门禁。 | Audio/MetronomeProcessor.h:183；tests/MetronomeTest.cpp:200；Recording/MidiTextDecoder.cpp:200,211,289,297；tests/PerformanceModifierTest.cpp:220；§4.1输出；[本轮命令输出确认]；复审：[Phase G EVID-061, EVID-066](../archive/audit-004-code-quality-fix-phases.md#phase-g-实施记录与直接验证2026-10-05)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| AUDIT-002 TEST-014 | 测试 | 和弦识别7个子测试被默认类别过滤漏跑 | P2 | 已关闭 | 历史类别门禁问题重开 | ChordRecognitionTest类别为小写devpiano，不满足DevPiano/白名单；默认门禁95套件未含它，额外定向运行115断言通过仍未修默认缺口。 | source/tests/ChordRecognitionTest.cpp:8-19；source/tests/TestRunner.cpp:141-146；CMakeLists.txt:588,629-631；§4.1定向Windows category结果；[默认日志及Windows --category devpiano 执行确认]；复审：[Phase 0 EVID-002, EVID-060](../archive/audit-004-code-quality-fix-phases.md#phase-0-实施记录与直接验证2026-10-02)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| known-issues §1/节拍器每拍三角函数与全回调零三角 SLA 不一致 | 性能/实时性 | 全回调零三角函数 SLA 尚未达到 | P2 | 已关闭 | known-issues 原heading，扩充回调闭包证据 | 已知每拍Metronome sin/cos/exp；另有Piano机械瞬态逐采样sin与起音系数计算。Magic Circle分音循环不等于完整callback零sin。 | docs/issues/known-issues.md:52-56；source/Audio/MetronomeProcessor.h:209-216；source/Audio/PianoSynthVoice.h:1081-1107,1232-1272,1426-1433；SineSynthVoice.h:104；[源码调用确认；当前CPU/声卡毛刺未测]；复审：[Phase E EVID-043](../archive/audit-004-code-quality-fix-phases.md#phase-e-实施记录与直接验证2026-10-04)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| known-issues §1/A4 基准音高范围与项目契约不一致 | 质量/功能 | A4实际410..450Hz未覆盖400..480Hz契约 | P2 | 已关闭 | known-issues 原heading引用 | 设置/预设/离线统一钳制到较窄范围，两端目标频率无法选择。 | docs/issues/known-issues.md:46-50；source/Audio/TemperamentEngine.h:34-48；SettingsComponent使用同常量；[既有确认问题；本轮静态复核，不重新编号]；复审：[Phase F EVID-052, EVID-063](../archive/audit-004-code-quality-fix-phases.md#phase-f-实施记录与直接验证2026-10-05)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| OBS-001 | 可观测性 | MIDI诊断将已是0..127的力度再次乘127 | P2 | 已关闭 | 审计 | raw_velocity=64显示成vel=8128，错误日志会误导击键动态/音色排障；现有测试未断言力度数值。 | source/Diagnostics/MidiTrace.cpp:26-30；source/tests/DiagnosticsTest.cpp:68-76；S25；[Windows describeMidiMessage真实调用输出确认]；复审：[Phase G EVID-056, EVID-063](../archive/audit-004-code-quality-fix-phases.md#phase-g-实施记录与直接验证2026-10-05)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| QUAL-019 | 质量/功能 | 踩下柔音踏板后新分配声部不继承CC67状态 | P2 | 已关闭 | 审计 | 相同PianoSynthSound/8voice拓扑下先CC67再两音，仅1/2活跃声部为soft pedal；和弦一部分不执行Una Corda，区别于触键曲线。 | source/Audio/AudioEngine.cpp:261-267；source/Audio/PianoSynthVoice.h:118-129,188-193,203-239,485-490；JUCE Synthesiser.cpp:340-349,424-428；S29；[Windows 当前PianoSynthVoice+真实JUCE Synthesiser拓扑冒烟确认；主窗口演奏听感未测]；复审：[Phase D EVID-035](../archive/audit-004-code-quality-fix-phases.md#phase-d-实施记录与直接验证2026-10-04)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| QUAL-011 | 质量/功能 | MIDI 1至11的八度标注高一组 | P3 | 已关闭 | 审计 | 整数向零截断将C#-1…B-1显示成C#0…B0；编辑器/卡片接受0..127，不仅88键物理键盘范围。 | source/Core/MusicTheory.h:28-55；source/Input/KeyboardMidiMapper.cpp:444-447；source/UI/KeyBindingEditDialog.cpp:103；S27；[Windows 真实标签函数边界输出确认]；复审：[Phase F EVID-051, EVID-063](../archive/audit-004-code-quality-fix-phases.md#phase-f-实施记录与直接验证2026-10-05)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| TEST-003 | 测试 | 测试存在译文/自证断言及未调用行为用例 | P3 | 已关闭 | 审计 | UTF8转义仍硬钉中文文案；若干手动赋值后读取自身不约束生产，Metronome lifecycle私有方法未被runTest调度。 | source/tests/StyleCatalogTest.cpp:1679-1682,1472-1494；source/tests/JiveModalDialogTest.cpp:337-372；source/tests/SettingsLayoutModelTest.cpp:126-136；source/tests/MetronomeTest.cpp:15-28,384-456；AGENTS.md §6；[逐段测试审查及runTest调用清单确认]；复审：[Phase G EVID-060, EVID-065](../archive/audit-004-code-quality-fix-phases.md#phase-g-实施记录与直接验证2026-10-05)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |
| DOC-001 | 文档/契约 | 现行行为说明含已被源码证伪的承诺 | P3 | 已关闭 | 审计 | 预设文档称切换调号/重命名确认，MIDI导出描述两轨/拍号名称；实现分别是app-global调号、无rename冲突确认、单轨仅tempo；日志滚动已关联RES。 | docs/reference/features/performance-presets.md:11-17,145-162 对 PresetFlowSupport.cpp:131-134,302-330；docs/reference/features/recording-playback.md:101-104 对 MidiFileExporter.cpp:25-45；docs/reference/features/plugin-hosting.md 重扫/控制器测试说明；[现行文档与实际消费者对照]；复审：[Phase H EVID-062, EVID-063, EVID-064, EVID-067](../archive/audit-004-code-quality-fix-phases.md#phase-h-实施记录与最终集成验收2026-10-05)；[逐项索引](../archive/audit-004-code-quality-fix-phases.md#全部原项与直接证据索引) | - | 出现针对该项根因的新反证、行为回退或测试失败时重开 | 见逐项归档证据；维持对应消费者与既有回归，出现原根因新反证时携原身份重开 |

### ID 命名与领域前缀

| 前缀 | 领域 |
| --- | --- |
| `SEC` | 安全（缓冲区、文件路径、插件加载、MIDI 消息有效性、JSON 解析） |
| `RES` | 资源（内存泄漏、句柄泄漏、分配热点） |
| `PERF` | 性能（实时路径分配、容器策略） |
| `ARCH` | 架构（模块边界、依赖方向、职责切分） |
| `QUAL` | 代码质量（命名、const、RAII、死代码、重复） |
| `ERR` | 错误处理（返回值忽略、静默失败、Logger 覆盖） |
| `THR` | 线程安全（消息线程、音频回调、数据竞争） |
| `OBS` | 可观测性（日志完整性、diagnostics 覆盖） |
| `TEST` | 测试（覆盖缺口、测试质量、可维护性） |
| `DOC` | 文档（源码注释、架构文档一致性） |
| `ENG` | 工程化（CMake、clang-tidy、clang-format、警告） |
| `CMPL` | 决策合规（ADR 决策被违反/部分遵守；ADR 事实性描述过时属修正原文，不开问题） |

### 去重与未升级结论

- Listener、ERR-002、A4、零三角SLA及Phase6-2/FIX-035保留原编号/heading。同根不重编号；裸实例只重开具体绕过guard，不否定正常guard。
- AUDIT-003 Core逆向include、room reverb、解码scratch优化和headless socket无新反证；32MiB MIDI上限、128固定数组、find观察指针和事件驱动扫描仍合理。
- Chord补跑通过不关闭默认漏项；cpp编译接入不证明case执行。controller helper/Notes假factory作为对应功能回归要求，不另开泛化零覆盖。
- 未证实的Type2拒绝政策、bank/program排序、任意动态em/transition、回调任意自毁、插件沙箱等不作确认漏洞。历史ADR规模变化不因数值过时开CMPL。

### 官方API事实来源（结合本地版本核对）

- [MidiMessageCollector](https://docs.juce.com/master/classjuce_1_1MidiMessageCollector.html)、[Synthesiser](https://docs.juce.com/master/classjuce_1_1Synthesiser.html)、[MidiKeyboardState](https://docs.juce.com/master/classjuce_1_1MidiKeyboardState.html)：线程安全不等于lock-free，具体锁/Listener线程核对本地实现。
- [MemoryBlock](https://docs.juce.com/master/classjuce_1_1MemoryBlock.html)：JUCE长度前缀编码非标准Base64。
- [FileOutputStream](https://docs.juce.com/master/classjuce_1_1FileOutputStream.html)、[TemporaryFile](https://docs.juce.com/master/classjuce_1_1TemporaryFile.html)：已有文件位置与替换边界。
- [FileLogger](https://docs.juce.com/master/classjuce_1_1FileLogger.html)：初始尺寸仅构造检查。
- [AudioProcessor](https://docs.juce.com/master/classjuce_1_1AudioProcessor.html)：setNonRealtime告知离线模式。
- [Microsoft /Zc:nrvo](https://learn.microsoft.com/en-us/cpp/build/reference/zc-nrvo)：可选NRVO与强制prvalue消除不同。

---
