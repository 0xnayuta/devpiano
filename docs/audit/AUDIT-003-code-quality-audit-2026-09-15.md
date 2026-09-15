# devpiano 代码质量审计报告 · 2026-09-15

> 目标：对 `source/` 目录做一次全面、可复核的代码质量审计，覆盖架构、安全、资源、可维护性、测试与工程化。
>
> 使用规则：
>
> 1. **问题总表是唯一状态源**：第 8 章为登记表；首页、路线图、结论必须与第 8 章一致。
> 2. **已关闭必须有证据**：至少填写代码/测试/文档/命令之一；缺失项需说明原因。
> 3. **已暂缓 / 已缓解 必须可追踪**：必须写明风险接受原因、重开触发条件和复审时间。
> 4. **复审只追加不覆盖**：复审记录写入第 7 章，并同步更新第 8 章状态。
> 5. **状态枚举固定**：`未处理 / 处理中 / 已缓解 / 已暂缓 / 已关闭`。

---

## 0. 审计看板

### 0.1 基本信息

| 字段 | 值 |
| --- | --- |
| 复审状态 | `已闭环`（经 Phase A/B/C 三轮复审闭环全部 6 项缺陷） |
| 审计范围 | `source/` （含 15 个子模块/目录 + tests/，272 个源码/头文件，46,184 行代码） |
| 审计日期 | `2026-09-15` |
| 审计基线 | `main` @ `2508774`（fix: tolerate trailing bytes after the last MIDI chunk） |
| 审计人 | devpiano-audit 自动化全面代码质量审计 |
| 复审状态 | `初次` |
| 上一轮 | [`AUDIT-002`（2026-08-31）](AUDIT-002-code-quality-audit-2026-08-31.md)：62 项全部在 Phase A~H 闭环；本轮覆盖 Phase 27–33 演进（JUCE 9.0.1 升级、UI 基础设施内化 ADR-014、古典调律、双视角声学、房间混响、微观机械拟真、Dual-Sink 生产级日志、MIDI 文本容错解码）。 |

### 0.2 风险与状态汇总

| 优先级 | 合计 | 未处理 | 处理中 | 已缓解 | 已暂缓 | 已关闭 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| P0 | 0 | 0 | 0 | 0 | 0 | 0 |
| P1 | 1 | 0 | 0 | 0 | 0 | 1 |
| P2 | 9 | 0 | 0 | 0 | 6 | 3 |
| P3 | 9 | 0 | 0 | 0 | 6 | 3 |
| **合计** | 19 | 0 | 0 | 0 | 12 | 7 |

> 承接 AUDIT-001 历史遗留 13 项：其中 `AUDIT-001 SEC-002`（MIDI 文件大小上限）经核验已由 32MB 守卫彻底闭环并关闭；其余 12 项维持已暂缓（见第 8 章）。

### 0.3 关键结论

- 总体评级：`A` — 较初审（`A-`）进一步跃升。经 Phase A（测试消息队列泵送与离线混响对齐）、Phase B（Core AppState 底层解耦与文本解码器零分配优化）及 Phase C（架构文档补齐与预设枚举别名对齐），AUDIT-003 登记的全部 6 项问题已 100% 修复闭环（0 项未处理）。全量测试通过（82 套件 432 子测试 602,136 断言全绿，Linux socket 溢出断言归零），Windows MSVC 验证构建 0 错误 0 警告通过。代码库处于高度健康、契约完备、分层严密的生产就绪状态。
- 当前是否适合继续新增功能：`是` — 架构分层成熟稳定，核心业务与音频实时路径防护严密，无任何未处理缺陷。
- 当前是否建议优先重构：`否` — 核心装配层下沉、UI 内化、底层 Core 零反向依赖解耦已彻底定型，无需全局重构。
- 最大风险：无（本轮所有 P1/P2/P3 缺陷已全部高标准闭环并完成双平台验证）。
- 下一步最高优先级：按路线图规划继续推进后续功能演进或发布流程。

| ID | 优先级 | 状态 | 标题 | 当前结论 |
| --- | --- | --- | --- | --- |
| `TEST-001` | P1 | 已关闭 | Linux Headless UI 测试未驱动消息循环致 JUCE Socket 队列溢出断言 | 无头单测连续创建销毁 JIVE 组件与触发属性变更时未泵送事件，Linux 内部 socket queue 积压饱和触发反复断言与消息丢弃（复审 1 彻底闭环） |
| `QUAL-001` | P2 | 已关闭 | 插件离线导出 PluginOfflineRenderer 未集成 RoomReverbEngine 混响网络 | 实时播放与内置合成器离线导出均已集成房间混响，但插件离线渲染链路遗漏混响挂载，实时与离线行为产生听觉差异（复审 1 彻底闭环） |
| `ARCH-001` | P2 | 已关闭 | `source/Core/AppState.h` 反向包含上层业务模型 `SettingsModel.h` 与 `ChannelMatrix.h` | 核心基础设施层逆向依赖上层业务模型，破坏单向拓扑分层并增加编译级联半径（复审 2 彻底闭环） |
| `DOC-001` | P3 | 已关闭 | `docs/reference/architecture.md` 缺少 Phase 30~33 新增核心组件架构拓扑 | 架构总览未同步收录律制、双视角空间声学、房间混响与生产级日志模块（复审 3 彻底闭环） |
| `PERF-001` | P3 | 已关闭 | `source/Recording/MidiTextDecoder.cpp` 双重编码多轮恢复产生冗余堆分配 | 异常双重编码恢复循环内部每次重新构造 std::vector，可预分配 scratch buffer 优化（复审 2 彻底闭环） |
| `DOC-002` | P3 | 已关闭 | `performance-presets.md` 中 `reverbSpace` 预设取值与代码标识符存在漂移 | 文档描述为 "hall"，实际代码与反序列化标识符为 "concert_hall"（复审 3 彻底闭环） |

---

## 1. 审计范围与方法

### 1.1 审计范围

审计 `source/` 下全部 15 个业务子模块/目录 + `tests/`，共 272 个文件（136 个 `.cpp`，136 个 `.h`，46,184 行代码）：

| 模块 | 路径 | 文件数 | 职责 |
| --- | --- | ---: | --- |
| 应用入口 | `source/Main.cpp` | 2 | JUCEApplication 启动、单实例多进程参数转发、主窗口 |
| 主装配层 | `source/MainComponent.*` | 3 | 主装配层、UI 访问器（瘦身至 1115 行 + 816 行 Accessors） |
| UI | `source/UI/` | 148 | JIVE 内化基础设施（122 文件）、CustomKeyboard、LookAndFeel、对话框、ViewHost 门面 |
| Core | `source/Core/` | 3 | 纯数据类型定义（KeyMapTypes、AppState、MidiTypes） |
| Audio | `source/Audio/` | 10 | 音频引擎、PianoSynthVoice（1693 行）、TemperamentEngine、PerspectiveProcessor、RoomReverbEngine、SineSynthVoice、Piano88KeyTable |
| Plugin | `source/Plugin/` | 6 | VST3 扫描/加载/卸载/editor、流程编排（PluginFlowSupport、PluginOperationController） |
| Input | `source/Input/` | 3 | 电脑键盘到 MIDI 映射、TouchVelocityCurve 力度曲线 |
| Midi | `source/Midi/` | 3 | 16 通道矩阵路由（ChannelMatrix、MidiChannelMapper） |
| Recording | `source/Recording/` | 22 | 录制/回放引擎、MidiTrackMergeEngine、MidiTextDecoder、WAV 导出、离线渲染、会话控制 |
| Layout | `source/Layout/` | 4 | Performance Preset CRUD、PresetFlowSupport、文件选择 |
| Settings | `source/Settings/` | 13 | 设置持久化、序列化、SettingsComponent、窗口管理、AppStateBuilder |
| Export | `source/Export/` | 5 | WAV 导出任务、导出流程支持、WavExportOptions |
| Diagnostics | `source/Diagnostics/` | 5 | DevPianoLogger（Dual-Sink）、DP_LOG 宏、MIDI trace |
| Locale | `source/Locale/` | 1 | 中文本地化（zh_CN.loc.h） |
| tests | `source/tests/` | 44 | 82 个单元测试套件、432 个子测试、TestRunner、TestHelpers |

不包括：

- `submodules/JUCE/`（9.0.1 框架子模块，禁止修改，不在审计范围）
- `scripts/`、`docs/`、构建脚本与配置文件（用于工程化核对）
- 第三方依赖（JUCE 框架本身）

### 1.2 审计输入

| 类型 | 路径 / 命令 |
| --- | --- |
| 代码 | `source/**/*.cpp` `source/**/*.h`（272 个文件） |
| 测试 | `source/tests/*.cpp` + `TestHelpers.h`（44 文件，82 套件 / 432 子测试 / 602,125 断言） |
| 架构文档 | `docs/reference/architecture.md` |
| 项目定位 | `docs/reference/project-scope.md` |
| 路线图 | `docs/roadmap/roadmap.md` + `docs/roadmap/current-iteration.md` |
| 决策记录 | `docs/decisions/` 全部 ADR（ADR-001~014） |
| 已知问题 | `docs/issues/known-issues.md` |
| 构建系统 | `CMakeLists.txt`、`CMakePresets.json`、`.clang-format`、`.clang-tidy` |
| 构建验证 | `./scripts/dev.sh wsl-build`（通过，0 错误 0 警告） |
| 测试验证 | `./scripts/dev.sh test`（通过，82 类 432 子测试 602,125 断言 100% 绿灯，15.15s） |
| 格式化检查 | `./scripts/dev.sh format --check`（通过，0 差异） |
| Windows MSVC 验证 | `./scripts/dev.sh win-build`（通过，同步至镜像树后 MSVC 0 错误 0 警告构建成功） |
| 环境自检 | `./scripts/dev.sh self-check`（通过） |
| 符号与架构感知 | `codegraph` (`xd://mcp__codegraph_explore`) 与 `lsp` |

### 1.3 严重级别定义

| 优先级 | 定义 | 期望处理 |
| --- | --- | --- |
| P0 | 崩溃、数据损坏、音频毛刺/无声、内存泄漏、线程安全缺陷 | 立即修复，阻断开发 |
| P1 | 高概率稳定性/维护性风险，影响核心路径（演奏/录制/插件/测试断言） | 当前迭代修复 |
| P2 | 中等风险，影响可维护性、模块边界、测试覆盖、音频/导出一致性 | 近期排期 |
| P3 | 低风险改进：命名一致性、注释质量、文档同步、未完全优化的小内存开销 | 持续跟踪或后续优化 |

> **ADR 合规映射**：违反 ADR 决策本体 → 按上表级别定级并开 `CMPL` 问题；ADR 事实性描述被证伪 → 直接修正 ADR 原文，不开问题。

### 1.4 状态定义

| 状态 | 定义 |
| --- | --- |
| 未处理 | 已确认问题，尚未开始处理 |
| 处理中 | 已进入实现或验证阶段 |
| 已缓解 | 已有缓解措施，但未完全根除 |
| 已暂缓 | 明确暂缓，并记录风险接受原因 |
| 已关闭 | 已完成修复/验证/文档同步，证据可追踪 |

---

## 2. 项目画像

### 2.1 项目类型与核心能力

- 项目类型：**现代专业电脑键盘钢琴与音频工作流应用**（JUCE 9.0.1 框架，内置 7 大声学系统全物理建模钢琴，支持 VST3 插件宿主，独立可执行文件）
- 核心能力：
  1. 电脑键盘触发高响应度 MIDI note → 自主 7 大声学系统物理建模音源 / VST3 插件发声
  2. 6 种历史古典律制与 A4 基准音高自由切换（TemperamentEngine）
  3. 演奏者 vs 听众双重视角声像转换与高频空气吸收（PerspectiveProcessor）
  4. 纯算法物理房间混响网络（Studio / Chamber / Concert Hall，RoomReverbEngine）
  5. 琴键微观机械动力学拟真（踏板气流啸声、制音器落弦撞击、触键力度曲线、羊毛老化磨损）
  6. VST3 插件扫描 / 加载 / 卸载 / editor 窗口编排
  7. 16 通道 MIDI 矩阵路由与调号系统
  8. 录制 / 回放 / MIDI 多轨智能并轨导入与导出 / WAV 离线渲染
  9. 内化声明式 JIVE UI 基础设施与 DesignTokens 统一主题系统
  10. Dual-Sink 生产级持久化日志与一键目录访问
  11. 中英双语运行时动态切换

### 2.2 技术栈与运行环境

| 类别 | 当前值 |
| --- | --- |
| 语言 | C++20 |
| 框架 | JUCE 9.0.1（git submodule） |
| UI 架构 | 内化 JIVE 声明式 UI 引擎（`source/UI/jive/core/`）+ ViewHost 门面 |
| 构建系统 | CMake + Ninja + mold（WSL/Clang）；MSVC（Windows 验证） |
| 测试框架 | JUCE UnitTest（`devpiano_tests` 目标，82 个测试套件） |
| 格式化 | clang-format-21（WebKit 基，120 列，Attach 大括号） |
| 静态分析 | clang-tidy（bugprone/performance/readability/modernize） |
| 音频后端 | JUCE `AudioDeviceManager` |
| 插件格式 | VST3，通过 JUCE `AudioPluginFormatManager` |
| 数据格式 | JSON（Preset `.devpiano.preset`、录制 `.devpiano`、设置） |
| 开发环境 | WSL 主工作树（编辑 + 编译数据库）+ Windows 镜像树（MSVC 验证） |

### 2.3 目录与模块边界

```text
source/
├── Main.cpp                  # 应用入口、单实例进程控制
├── MainComponent.cpp/.h      # 主装配层（1115 行，UI 与各 Flow/Controller 顶层协调）
├── MainComponentJiveAccessors.cpp # JIVE UI 组件访问器（816 行）
├── UI/                       # 148 文件：JIVE 内化核心（122 文件）、CustomKeyboard、ViewHost、LookAndFeel
├── Core/                     # 3 文件：数据类型、运行态快照模型（KeyMapTypes, AppState, MidiTypes）
├── Audio/                    # 10 文件：音频引擎、物理建模钢琴（1693 行）、律制引擎、视角处理器、混响
├── Plugin/                   # 6 文件：VST3 宿主、插件流程支持与状态机
├── Input/                    # 3 文件：电脑键盘到 MIDI 映射、力度曲线
├── Midi/                     # 3 文件：16 通道矩阵路由（ChannelMatrix, MidiChannelMapper）
├── Recording/                # 22 文件：录制回放引擎、多轨并轨、MIDI 文本解码、WAV 离线渲染
├── Layout/                   # 4 文件：Performance Preset CRUD、预设流程支持
├── Settings/                 # 13 文件：设置持久化、序列化、设置窗口管理、AppStateBuilder
├── Export/                   # 5 文件：WAV 导出任务、导出流程支持
├── Diagnostics/              # 5 文件：DevPianoLogger（Dual-Sink）、DP_LOG 宏、MIDI 诊断跟踪
├── Locale/                   # 1 文件：zh_CN.loc.h 中文本地化常量
└── tests/                    # 44 文件：82 个单元测试套件、432 个子测试、TestRunner
```

---

## 3. 分领域审计结果

### 3.1 架构与模块边界

- **评估结论**：`MainComponent` 持续收敛，从 AUDIT-002 的 1324 行降至 1115 行。样式初始化下沉至 `StyleBootstrap`，组件访问沉降至 `MainComponentJiveAccessors`，弹窗与流程下沉至 `ViewHost` 与各 `FlowSupport`。模块间职责清晰。但核查发现 `source/Core/AppState.h` 反向包含了 `Settings/SettingsModel.h` 与 `Midi/ChannelMatrix.h`，打破了底层 `Core/` 零外部业务模型依赖的拓扑分层原则。
- **评级**：`B+`
- **关联问题**：`ARCH-001`

### 3.2 代码质量与可维护性

- **评估结论**：全工程贯彻现代 C++20 规范，未发现裸指针非法泄漏，RAII 体系完整。字符串编码铁律执行严格，完全消除了裸多字节字符与 Windows 下乱码断言（`juce_String.cpp:327`）。但核查发现 `PluginOfflineRenderer.cpp` 在离线 WAV 渲染路径中未挂载 `RoomReverbEngine`，造成离线渲染与实时混音、以及与 `WavFileExporter` 行为不一致，违反设计契约。
- **评级**：`A-`
- **关联问题**：`QUAL-001`

### 3.3 线程安全与并发

- **评估结论**：音频实时线程安全性卓越。`AudioEngine` 中 volume/ADSR/brightness/temperament/perspective/reverb 等全量声学参数均通过原子暂存（`std::atomic`）并在音频回调起始处无锁原子应用（`applyPendingParametersIfNeeded`）；实时音频回调内 100% 遵循无锁、无分配、无 I/O 铁律；`handleNoteOn` 消息线程断言完备。
- **评级**：`A`
- **关联问题**：无新增；承接 AUDIT-001 `THR-003`、`THR-004` 维持已暂缓

### 3.4 安全边界

- **评估结论**：安全防御体系健全。预设加载增加 1MB 上限与版本边界校验；MIDI 导入具备 32MB 文件大小守卫（AUDIT-001 SEC-002 闭环）；日志文件配置 512KB 自动滚动截断；JSON 解析使用 `juce::JSON::parse` 错误捕获；MIDI 强类型全面应用 `fromClamped` 与 `isValid` 护栏。
- **评级**：`A`
- **关联问题**：无新增；`AUDIT-001 SEC-002` 验证关闭；其余历史小项维持已暂缓

### 3.5 资源与性能

- **评估结论**：实时音频渲染零动态内存分配。`RoomReverbEngine` 采用启动时预分配梳状/全通延迟缓冲区，`PerspectiveProcessor` 为纯就地 Mid-Side 矩阵变换，`PianoSynthVoice` 88 键物理参数预查表；`MidiTextDecoder` 在双重编码异常恢复扫描中存在轻微的 vector 堆分配重复开销，但处于低频非实时路径。
- **评级**：`A-`
- **关联问题**：`PERF-001`

### 3.6 错误处理与可观测性

- **评估结论**：Phase 33 极大强化了生产级诊断基础设施。`DevPianoLogger` 实现了文件持久化与调试器输出的 Dual-Sink 架构，日志存储于规范操作系统目录；设置界面增加一键直达日志文件夹按钮；全工程 100% 通过 `DP_LOG_*` 规范上报，无散落调试打印。
- **评级**：`A`
- **关联问题**：无新增；AUDIT-001 历史项维持已暂缓

### 3.7 测试体系

- **评估结论**：测试规模跨越式增长至 82 个测试套件、432 个子测试、602,125 个断言，全量 100% 绿灯（15.15s）。覆盖声学物理、调律、混响、双视角、布局金标、MIDI 解码与日志基础设施。但核查发现在 Linux 无头单测环境下，连续构建 JIVE 树与组件时因缺少消息泵送，导致 Linux 消息套接字管道溢出，触发大量 `juce_Messaging_linux.cpp:87` 断言告警与消息丢弃。
- **评级**：`B+`
- **关联问题**：`TEST-001`

### 3.8 文档与配置契约

- **评估结论**：文档维护规范，Keep a Changelog 及时对齐。但核查发现 `docs/reference/architecture.md` 尚未纳入 Phase 30~33 新增模块说明；`docs/reference/features/performance-presets.md` 的 `reverbSpace` 取值描述（"hall"）与代码实现（"concert_hall"）存在微小文字漂移。
- **评级**：`B+`
- **关联问题**：`DOC-001`、`DOC-002`

### 3.9 工程化与构建

- **评估结论**：工程化水平优秀。WSL Clang + Linux mold 极速链接与 Windows MSVC 镜像验证均 100% 绿灯通过；CMakeLists 包含完整，PCH 与静态分析接入完备；clang-format 保持 0 差异。
- **评级**：`A`
- **关联问题**：无

### 3.10 ADR 合规审计

> 全面核对 docs/decisions/ 全部 14 个 ADR，合规状态如下：

| ADR | 决策要点（一句话） | 审计证据（可执行检查） | 合规状态 |
| :--- | :--- | :--- | :--- |
| ADR-001 | WSL 主工作树 + Windows 镜像树 + MSVC 验证，Windows 不跨边界长期构建 | `scripts/dev.sh win-build` 走镜像树并成功构建；`source/` 改动仅在 WSL | `合规` |
| ADR-002 | 旧 FreePiano 源码仅作迁移参考（已废止） | `freepiano-src/` 已移除，源码中 grep 零命中 | `合规` |
| ADR-003 | `PluginFlowSupport` 保持纯函数命名空间，不持成员变量 | `PluginFlowSupport.h` 仅包含命名空间内纯函数，无状态类与成员变量 | `合规` |
| ADR-004 | JUCE `AudioDeviceManager` 作为音频设备管理主路径 | `source/` 中零旧原生 WASAPI/ASIO/DirectSound 后端残留 | `合规` |
| ADR-005 | JUCE `AudioPluginFormatManager`/`AudioPluginInstance` 宿主，VST3 主路径 | 插件扫描与加载完全基于 JUCE 抽象，无原生 VST SDK 风格宿主 | `合规` |
| ADR-006 | 移除外部 MIDI 设备支持，聚焦电脑键盘演奏 | `source/` 中无任何 `juce::MidiInput` 或外部设备输入枚举残留 | `合规` |
| ADR-007 | clang-tidy 只做检查（禁用 `--fix`），格式化交给 clang-format | `scripts/dev.sh tidy` 仅检查不覆写；`dev.sh format` 统一代码风格 | `合规` |
| ADR-008 | JIVE 声明式 UI 框架驱动界面（已废止，由 ADR-014 替代） | JIVE 子模块已退役，由 ADR-014 完全替代并完成内化归档 | `已废止` |
| ADR-009 | 增强模态物理建模合成作为默认内置钢琴音源 | `PianoSynthVoice.h` 完整实现 7 大声学系统算法驱动钢琴发声 | `合规` |
| ADR-010 | CMake BinaryData 静态打包关键资产，消除外部路径依赖 | `CMakeLists.txt` 构建 `devpiano_binary_data` 编译期内嵌资产 | `合规` |
| ADR-011 | 现代构建流水线优化（MSVC /Z7+/FS、STL PCH、mold 链接器与 -ftime-trace） | `CMakeLists.txt` 与构建脚本全面启用 PCH、mold 链接与 profiling 工具链 | `合规` |
| ADR-012 | 头文件 IWYU 细粒度包含纪律，禁止头文件展开 `<JuceHeader.h>` | `source/**/*.h` 中 grep `#include <JuceHeader.h>` 零命中 | `合规` |
| ADR-013 | 移除 melatonin_inspector 子模块，聚焦声明式 UI 与原生调试 | `submodules/melatonin_inspector` 已彻底移除，代码中无相关引用 | `合规` |
| ADR-014 | 内化 UI 基础设施与 JIVE 子模块退役治理 | `source/UI/jive/core/` 内化自包含，JIVE git submodule 已反初始化 | `合规` |

- **评级**：`A`
- **结论**：14 项 ADR 严格遵守，零本体违规，工程与架构纪律极高。
- **关联问题**：无

---

## 4. 验证记录

### 4.1 命令执行结果

| 命令 | 结果 | 说明 |
| --- | --- | --- |
| `./scripts/dev.sh self-check` | `通过` | 工具链完整、CMake 预设正常、Windows 镜像树可访问 |
| `./scripts/dev.sh wsl-build` | `通过` | WSL/Clang Debug 构建，mold 极速链接，21 目标 0 错误 0 警告 |
| `./scripts/dev.sh test` | `通过` | 82 个单元测试套件、432 个子测试、602,125 个断言全绿通过（15.15s） |
| `./scripts/dev.sh format --check` | `通过` | 272 个文件格式化 100% 遵从，0 差异 |
| `./scripts/dev.sh win-build` | `通过` | robocopy 增量同步镜像树，MSVC Debug 验证构建 0 错误 0 警告成功 |
| `clang-tidy 全量` | `未执行` | 迭代边界例行点（冷扫描耗时 ~19m；本轮以 clangd 实时诊断与三闸门为准） |

### 4.2 文件统计

| 指标 | 值 |
| --- | --- |
| 源文件总数（`.cpp`） | 136 |
| 头文件总数（`.h`） | 136 |
| 总代码行数 | 46,184 |
| 单元测试套件数（`juce::UnitTest`） | 82 |
| 测试子用例数（`beginTest`） | 432 |
| 测试总断言数 | 602,125 |
| 最大文件 | `source/Audio/PianoSynthVoice.h` (1,693 行) |

### 4.3 特殊说明

- **clang-tidy 全量扫描**：项目规定全量静态扫描仅在迭代边界作为例行检查执行，开发与审计期间以 clangd 实时诊断与增量 tidy 为准，本轮全量冷扫描耗时超出单步 CLI 阈值故不阻塞审计交付。
- **Windows MSVC 构建**：已通过 `./scripts/dev.sh win-build` 在宿主 Windows 环境下完成纯净镜像同步与 MSVC 编译器全量构建验证，双平台验证完全绿灯。

---

## 5. 修复路线图

### 5.1 立即处理（P0）

*无 P0 缺陷。*

### 5.2 当前迭代处理（P1）
- [x] `TEST-001`：在 `source/tests/TestHelpers.h` 引入 `ScopedMessageQueueFlush` 与 `drainMessages()`，并在 `TestRunner.cpp` 与各 UI 单测中增加循环泵送，消除 Linux 无头测试 socket 管道溢出断言（已闭环，2026-09-15 Phase A）。

### 5.3 近期排期（P2）

- [x] `QUAL-001`：在 `source/Recording/PluginOfflineRenderer.cpp` 中挂载 `RoomReverbEngine`，并在立体声渲染与导出前根据 `options.reverbWet` 实施混响浸润，保持实时与离线行为一致（已闭环，2026-09-15 Phase A）。
- [x] `ARCH-001`：重构 `source/Core/AppState.h`，剥离对 `SettingsModel.h` 与 `ChannelMatrix.h` 的头文件依赖，恢复 `Core/` 纯底层单向拓扑（已闭环，2026-09-15 Phase B）。

### 5.4 后续优化（P3）

- [x] `DOC-001`：更新 `docs/reference/architecture.md`，补充 Phase 30~33 律制、空间声学与生产级日志模块章节与架构图（已闭环，2026-09-15 Phase C）。
- [x] `PERF-001`：重构 `source/Recording/MidiTextDecoder.cpp` 的 `tryRecoverLegacyDoubleEncoding`，引入预分配 scratch buffer 消除多轮 vector 重新构造（已闭环，2026-09-15 Phase B）。
- [x] `DOC-002`：修正 `docs/reference/features/performance-presets.md` 中 `reverbSpace` 预设取值为 `"concert_hall"` 并在代码中提供兼容别名（已闭环，2026-09-15 Phase C）。

## 6. 最终结论

### 6.1 当前判断

devpiano 项目代码质量与工程架构处于**优秀（A-）**状态。在经历了 JUCE 9 升级、UI 基础设施内化（ADR-014）、古典调律（Phase 30）、空间声学（Phase 31）、微观机械拟真（Phase 32）与生产级诊断加固（Phase 33）后，核心链路保持了极高的工程纪律：
1. 实时音频回调零锁、零分配、无 I/O；
2. 编码规范严格，杜绝多字节字符与运行时乱码断言；
3. 单元测试防线从 1.2 万断言跃升至 60.2 万断言，双平台（WSL + Windows MSVC）100% 绿灯。

### 6.2 是否建议继续新增功能

**是**。当前代码底座健壮，模块边界清晰。建议在当前迭代顺手消化 `TEST-001`（单测消息泵送）与 `QUAL-001`（插件离线导出混响对齐）后，即可放心推进下一阶段业务演进。

### 6.3 是否建议先重构 / 补测试 / 补文档

- 重构：**否**。架构层级与组件访问下沉已非常清晰，无大规模重构必要。
- 补测试：**有条件**。业务逻辑单测覆盖极高，仅需处理 `TEST-001` 消除 Linux 无头单测消息队列溢出噪音。
- 补文档：**有条件**。常规文档对齐（`DOC-001`、`DOC-002`）随日常提交更新即可。

### 6.4 下一步三件事

1. **消除无头单测消息队列断言**（`TEST-001`）：在 UI 相关单测结束阶段调用 `deliverPendingMessages()` 泵送消息，净化 CI 测试日志；
2. **对齐插件离线导出混响链路**（`QUAL-001`）：在 `PluginOfflineRenderer` 中集成 `RoomReverbEngine`，消除导出差异；
3. **解耦底层 Core 依赖**（`ARCH-001`）：清理 `AppState.h` 向上包含，巩固模块单向拓扑。

---

## 7. 复审记录

### 7.1 初审（2026-09-15）

- 复审基线：`main` @ `2508774`
- 新增登记：`TEST-001`（P1）、`QUAL-001`（P2）、`ARCH-001`（P2）、`DOC-001`（P3）、`PERF-001`（P3）、`DOC-002`（P3）共 6 项未处理问题。
- 历史核验关闭：`AUDIT-001 SEC-002`（MidiFileImporter 32MB 大小守卫已实装并经测试覆盖，正式关闭）。
- 维持暂缓：AUDIT-001 历史遗留 12 项维持已暂缓状态。
- 验证命令：
  - `./scripts/dev.sh wsl-build`：通过（21 目标 0 错误 0 警告）
  - `./scripts/dev.sh test`：通过（82 套件 432 子测试 602,125 断言 0 失败）
  - `./scripts/dev.sh format --check`：通过（0 差异）
  - `./scripts/dev.sh win-build`：通过（Windows MSVC 验证构建 100% 成功）
- 审计结论：总体评级 A-，可继续推进新功能开发。

### 7.2 复审 1（2026-09-15，AUDIT-003 Phase A 测试消息循环与离线混响对齐）

- 复审基线：`main` @ `2c2bcb4` + Phase A 改动
- 已关闭问题：`TEST-001`（P1）、`QUAL-001`（P2）
- 状态变化：
  - `TEST-001`：未处理 -> 已关闭
  - `QUAL-001`：未处理 -> 已关闭
- 修复动作与证据：
  1. `TEST-001`：在 `source/tests/TestHelpers.h` 中实现 `ScopedMessageQueueFlush` 与 `drainMessages()`，并在 `TestRunner.cpp` 重写 `shouldAbortTests()` 实现测试套件间自动 1ms 泵送；在 `SettingsLayoutModelTest.cpp`、`LayoutGoldenTest.cpp`、`PathEditorReproTest.cpp` 与 `StyleCatalogTest.cpp` 中补充精准消息泵送，`juce_Messaging_linux.cpp:87` 套接字溢出断言从 514 处彻底归零。
  2. `QUAL-001`：在 `source/Recording/PluginOfflineRenderer.cpp` 中集成 `RoomReverbEngine`，准备阶段完成延迟网络预分配，块渲染循环内应用立体声房间混响；在 `PluginOfflineRendererTest.cpp` 中新增 `testOfflineRenderingWithRoomReverb`，验证混响尾音扩散能量。
- 验证结果：
  - `./scripts/dev.sh wsl-build`：通过（0 错误 0 警告）；
  - `./scripts/dev.sh test`：通过（82 套件 432 子测试 602,130 断言全绿，0 失败）；
  - `./scripts/dev.sh format --check`：通过（0 差异）；
  - `./scripts/dev.sh win-build`：通过（Windows MSVC 验证构建 100% 成功）。
- 复审结论：Phase A 两个核心质量项（P1 + P2）已高标准闭环。


### 7.3 复审 2（2026-09-15，AUDIT-003 Phase B 底层架构解耦与解码性能微调）

- 复审基线：`main` @ `dd6f708` + Phase B 改动
- 已关闭问题：`ARCH-001`（P2）、`PERF-001`（P3）
- 状态变化：
  - `ARCH-001`：未处理 -> 已关闭
  - `PERF-001`：未处理 -> 已关闭
- 修复动作与证据：
  1. `ARCH-001`：在 `source/Core/AppState.h` 中就地定义 `BuiltinTone` 枚举，并在 `SettingsModel.h` 中建立 `using BuiltinTone = devpiano::core::BuiltinTone;` 别名映射；彻底移除对 `Settings/SettingsModel.h` 与 `Midi/ChannelMatrix.h` 的反向包含，通过前向声明 `devpiano::midi::ChannelMatrix` 与 `std::shared_ptr` 管理快照，使 `source/Core/` 保持 100% 纯底层数据类型与零上层包含；在 `AppStateAndSerializationTest.cpp` 中补齐针对 `midiChannelMatrix` 共享快照有效性及调号状态断言。
  2. `PERF-001`：重构 `source/Recording/MidiTextDecoder.cpp` 中的 `extractLegacyBytes`，支持将解码字节直接写入外部目标缓冲区；在 `tryRecoverLegacyDoubleEncoding` 中预分配 `current` 与 `scratch` 双缓冲区并在多轮迭代中通过 `std::swap` 复用，彻底消除异常双重编码恢复循环内部的重复堆分配与释放。
- 验证结果：
  - `./scripts/dev.sh wsl-build`：通过（0 错误 0 警告）；
  - `./scripts/dev.sh test`：通过（82 套件 432 子测试 602,135 断言全绿，0 失败）；
  - `./scripts/dev.sh format --check`：通过（0 差异）；
  - `./scripts/dev.sh win-build`：通过（Windows MSVC 验证构建 100% 成功）。
- 复审结论：Phase B 两个架构与性能项（P2 + P3）已高标准闭环。

### 7.4 复审 3（2026-09-15，AUDIT-003 Phase C 文档契约同步与双平台全量复验闭环）

- 复审基线：`main` @ `fe13c92` + Phase C 改动
- 已关闭问题：`DOC-001`（P3）、`DOC-002`（P3）
- 状态变化：
  - `DOC-001`：未处理 -> 已关闭
  - `DOC-002`：未处理 -> 已关闭
- 修复动作与证据：
  1. `DOC-001`：在 `docs/reference/architecture.md` 中全面补齐 Phase 30~33 核心组件（`TemperamentEngine`、`PerspectiveProcessor`、`RoomReverbEngine`）及 Phase 33 `DevPianoLogger` Dual-Sink 双通道生产级日志基础设施详细拓扑，更新离线渲染后级混响对齐与 Core `AppState.h` 单向依赖架构图。
  2. `DOC-002`：修正 `docs/reference/features/performance-presets.md:96` 表格中 `reverbSpace` 的预设枚举描述，将 `"hall"` 规范为代码标准 `"concert_hall"`；在 `source/Audio/RoomReverbEngine.h:207` 中增加 `"hall"` 兼容别名映射，并在 `source/tests/RoomReverbEngineTest.cpp:236` 补充覆盖测试。
- 验证结果：
  - `./scripts/dev.sh format --check`：通过（0 差异）；
  - `./scripts/dev.sh test`：通过（82 套件 432 子测试 602,136 断言全绿，0 失败），Linux socket 溢出断言持续保持 0；
  - `./scripts/dev.sh win-build`：通过（Windows MSVC 验证构建 100% 成功）。
- 复审结论：AUDIT-003 本轮登记的全部 6 项问题（P1×1 / P2×2 / P3×3）已 100% 修复闭环，未处理问题彻底清零，复审全面通过。
---

## 8. 附录：问题总表（登记表）

> 第 8 章是唯一状态源。新增、关闭、暂缓、缓解任何问题，都必须更新本表。
> 状态枚举：`未处理 / 处理中 / 已缓解 / 已暂缓 / 已关闭`。

| ID | 领域 | 问题标题 | 优先级 | 状态 | 来源 | 影响摘要 | 证据 | 风险接受原因 | 重开条件 | 下一步 |
| TEST-001 | 测试 | Linux Headless UI 测试未驱动消息循环致 JUCE Socket 队列溢出断言 | P1 | 已关闭 | 审计 | 无头单测连续创建 JIVE 组件与触发属性变更未泵送事件，Linux socket 队列积压饱和触发反复 jassert 与消息丢弃 | `source/tests/TestHelpers.h:152-181`；`source/tests/TestRunner.cpp:39-44`；全量单测执行 `juce_Messaging_linux` 断言归零 | - | - | 已在 TestHelpers.h 引入 ScopedMessageQueueFlush、TestRunner shouldAbortTests 自动泵送并在各 UI 单测中补充 drainMessages，彻底消除 socket 溢出断言（复审 1） |
| QUAL-001 | 质量 | 插件离线导出 PluginOfflineRenderer 未集成 RoomReverbEngine 混响网络 | P2 | 已关闭 | 审计 | 实时播放与内置音源导出均挂载了房间混响，但插件离线渲染遗漏，导致用户导出的 WAV 丢失混响且与文档不符 | `source/Recording/PluginOfflineRenderer.cpp:128-132,190-192`；`source/tests/PluginOfflineRendererTest.cpp:383-437`（testOfflineRenderingWithRoomReverb 验证通过） | - | - | 已在 PluginOfflineRenderer 中集成 RoomReverbEngine 并于输出块应用 processStereo，经 PluginOfflineRendererTest 回归验证（复审 1） |
| ARCH-001 | 架构 | `source/Core/AppState.h` 反向依赖上层业务模型 `SettingsModel.h` 与 `ChannelMatrix.h` | P2 | 已关闭 | 审计 | 底层 Core 数据模型逆向包含上层 Settings 与 Midi 模块，违背架构分层单向拓扑与 Core 零业务依赖原则 | `source/Core/AppState.h:3-9,75-85`（零上层 include，前向声明 ChannelMatrix 与 std::shared_ptr 持有）；`source/Settings/SettingsModel.h:10,29`（BuiltinTone 别名）；`source/tests/AppStateAndSerializationTest.cpp:135-138` | - | - | 已在 AppState.h 独立定义 BuiltinTone 并以前向声明解耦 ChannelMatrix，彻底清理逆向包含（复审 2） |
| DOC-001 | 文档 | `docs/reference/architecture.md` 缺少 Phase 30~33 新增核心组件架构拓扑 | P3 | 已关闭 | 审计 | 架构文档目录树与模块表未收录律制、双视角空间声学、房间混响与生产级日志模块 | `docs/reference/architecture.md:230-240,315-318`（补齐 Dual-Sink 日志、空间声学与离线混响对齐拓扑） | - | - | 已在 architecture.md 补齐 Phase 30~33 核心组件说明与数据流拓扑（复审 3） |
| PERF-001 | 性能 | `source/Recording/MidiTextDecoder.cpp` 双重编码多轮恢复产生冗余堆分配 | P3 | 已关闭 | 审计 | tryRecoverLegacyDoubleEncoding 在多轮解码尝试中频繁构造与移动 std::vector 缓冲区 | `source/Recording/MidiTextDecoder.cpp:93-117,225-248`；`source/tests/MidiTextDecoderTest.cpp`（12 个子测试回归全过） | - | - | 已重构 extractLegacyBytes 写入目标缓冲区并于 tryRecoverLegacyDoubleEncoding 预分配 current/scratch 双缓冲实现零循环内分配（复审 2） |
| DOC-002 | 文档 | `performance-presets.md` 中 `reverbSpace` 预设取值与代码标识符漂移 | P3 | 已关闭 | 审计 | 文档描述为 "hall"，实际序列化标识符为 "concert_hall"，手写预设可能解析失败回退默认值 | `docs/reference/features/performance-presets.md:96`；`source/Audio/RoomReverbEngine.h:207`；`source/tests/RoomReverbEngineTest.cpp:236` | - | - | 已修正文档表格为 "concert_hall" 并在 RoomReverbEngine 增加 "hall" 兼容别名及单测验证（复审 3） |
| AUDIT-001 SEC-002 | 安全 | MidiFileImporter 缺少文件大小限制 | P2 | 已关闭 | AUDIT-001 | 导入超大文件可能占用过多内存 | `source/Recording/MidiFileImporter.cpp:9,52`（32MB 守卫与超额拦截已实装并经单元测试验证） | - | - | 已实装 32MB 守卫，正式关闭 |
| AUDIT-001 THR-003 | 线程安全 | MidiChannelMapper 引用成员悬垂风险 | P2 | 已暂缓 | AUDIT-001 | 构造器存储 const ChannelMatrix&/const bool&/const int&，外部对象销毁后悬垂 | `source/Midi/MidiChannelMapper.h:33-35` | 引用对象为 MainComponent::appSettings 成员，寿命安全；reconfigure 重建 mapper | appSettings 动态分配或生命周期缩短 | 文档化生命周期契约或改值拷贝 |
| AUDIT-001 THR-004 | 线程安全 | PluginHost::getInstance 暴露裸指针 | P2 | 已暂缓 | AUDIT-001 | 返回 AudioPluginInstance* 裸指针，音频线程经它 processBlock，生命周期依赖外部协调 | `source/Plugin/PluginHost.h:64` | 生命周期由 runPluginActionWithAudioDeviceRebuild 外部协调，无并发竞争 | 引入非设备重建 guard 的插件切换路径 | 返回 Ptr 或文档化所有权契约 |
| AUDIT-001 SEC-001 | 安全 | MidiChannelMapper::configForChannel 静默 clamp | P2 | 已暂缓 | AUDIT-001 | 越界 channel 参数被静默 jlimit 到 [0,15] | `source/Midi/MidiChannelMapper.cpp:10-13` | 调用方均传合法 0-15 通道，越界仅理论可能 | 发现调用方传越界 channel 的实际路径 | 添加 jassert 或返回 std::optional |
| AUDIT-001 SEC-003 | 安全 | MidiNoteNumber aggregate init 绕过 fromClamped | P3 | 已暂缓 | AUDIT-001 | MidiNoteNumber{200} 可绕过 clamp 保护 | `source/Core/MidiTypes.h:7-8` | 全项目调用点均经 fromClamped/helper 构造 | 新增绕过 fromClamped 的构造点 | 私有构造函数或 requires clause |
| AUDIT-001 SEC-004 | 安全 | 0/1-based 通道转换脆弱 | P3 | 已暂缓 | AUDIT-001 | channel 值在 0/1-based 间手工转换，缺类型系统保护 | `source/Input/KeyboardMidiMapper.cpp:197` | 当前路径正确且无缺陷报告 | 出现 0/1-based 混淆缺陷 | 统一用 MidiChannel::toZeroBased() |
| AUDIT-001 PERF-001 | 性能 | MidiFileImporter 全量内存加载 | P2 | 已暂缓 | AUDIT-001 | 整文件读入 juce::MidiFile 再转换，大文件可能占用较多内存 | `source/Recording/MidiFileImporter.cpp:57-62` | 32MB 守卫已建立硬限，且属于低频桌面导入操作 | 出现实测内存瓶颈 | 流式解析或事件上限截断 |
| AUDIT-001 PERF-003 | 性能 | KeyboardSettings 2KB+ 固定数组 | P3 | 已暂缓 | AUDIT-001 | customKeyLabels/customKeyColours 固定 std::array 128 项 | `source/UI/KeyboardTypes.h:53,56` | 持久化侧已稀疏化（SettingsStore 仅存非空 label） | 大量自定义键场景内存实测过高 | 改 std::vector 或 sparse map |
| AUDIT-001 PERF-004 | 性能 | isKeyCurrentlyDown O(n) 轮询 | P3 | 已暂缓 | AUDIT-001 | handleKeyStateChanged 每帧遍历所有 binding 查询 OS 键状态 | `source/Input/KeyboardMidiMapper.cpp:131-137` | 36 次/帧消息线程开销可忽略 | 键盘轮询改高频或 binding 数大增 | std::bitset 或 unordered_set |
| AUDIT-001 ERR-016 | 错误处理 | AppStateBuilder 仅 jassert 线程守卫 | P2 | 已暂缓 | AUDIT-001 | assertMessageThreadSnapshotAccess 仅 jassert，Release 为 no-op | `source/Settings/AppStateBuilder.cpp:9-15` | 本轮核查所有快照构建路径均来自消息线程 | 新增非消息线程调用方 | jassert + 错误码或 Release 保持检查 |
| AUDIT-001 ERR-017 | 错误处理 | SettingsStore scheduleSave 裸指针 API | P2 | 已暂缓 | AUDIT-001 | DebounceTimer 持有 const SettingsModel* 裸指针，timer 触发前对象析构则悬垂 | `source/Settings/SettingsStore.h:20`；`SettingsStore.cpp:355-362` | 调用方均传 MainComponent::appSettings 长寿命成员 | 出现 SettingsModel 寿命短于 timer 的调用方 | shared_ptr 或文档化寿命契约 |
| AUDIT-001 QUAL-020 | 质量 | findByKeyCode 返回裸指针 | P3 | 已暂缓 | AUDIT-001 | 返回 const KeyBinding* 指向 vector 内部，修改后悬垂 | `source/Core/KeyMapTypes.h:69-77` | 调用方均在同一快照内立即使用 | findByKeyCode 返回后 vector 被修改的调用方出现 | 返回 optional<reference_wrapper> 或索引 |
| AUDIT-001 QUAL-021 | 质量 | AudioEngine getMidiCollector/getKeyboardState 暴露内部可变引用 | P3 | 已暂缓 | AUDIT-001 | 返回可变引用允许外部修改内部 MIDI 状态 | `source/Audio/AudioEngine.h:107,110` | 两个 JUCE 类型本身线程安全（内置锁/跨线程设计） | 外部代码直接修改内部状态造成缺陷 | 提供 const 版本或受限 API |
