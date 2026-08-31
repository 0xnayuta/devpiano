# devpiano 代码质量审计报告 · 2026-08-31

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
| 项目 | devpiano |
| 审计范围 | `source/` （含 15 个子目录 + tests/，106 个业务 .cpp/.h + 22 个测试文件，~28,064 行） |
| 审计日期 | `2026-08-31` |
| 审计基线 | `main` @ `57080f8`（chore: remove melatonin_inspector submodule and obsolete references / ADR-013） |
| 审计人 | AI code audit（主代理核心审查 Audio/Plugin/线程契约 + 3 个只读 scout 并行分片 + 三闸门手工验证） |
| 复审状态 | `初次` |
| 上一轮 | [`AUDIT-001`（2026-08-16）](AUDIT-001-code-quality-audit-2026-08-16.md)：85 项全部闭环（72 关闭 + 13 暂缓）。本报告覆盖 Phase 12–26 新增代码（物理建模音源、多轨 MIDI 合并、多轨 WAV 导出、Linux 支持、CI/构建流水线）。 |

### 0.2 风险与状态汇总

| 优先级 | 合计 | 未处理 | 处理中 | 已缓解 | 已暂缓 | 已关闭 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| P0 | 0 | 0 | 0 | 0 | 0 | 0 |
| P1 | 6 | 6 | 0 | 0 | 0 | 0 |
| P2 | 13 | 13 | 0 | 0 | 0 | 0 |
| P3 | 43 | 43 | 0 | 0 | 0 | 0 |
| **合计** | 62 | 62 | 0 | 0 | 0 | 0 |

> 另承接 AUDIT-001 已暂缓 13 项（状态维持，本轮全部复查，见第 8 章登记表与 3.10 备注）。

### 0.3 关键结论

- 总体评级：`B+` — 三闸门全绿（wsl-build / test 12187 断言 / format --check 归零）；新增代码（PianoSynthVoice、MidiTrackMergeEngine、多轨导出）结构清晰、测试投入充足（测试规模 180→387 子测试、2921→12187 断言）；但 Phase 12–26 快速演进引入了 6 个 P1（3 个线程/内存安全缺陷 + 1 个功能静默失效 + 1 个编排层测试空洞 + 1 个导出对话框 UAF），且 MainComponent 持续回流（1143→1324 行）。
- 当前是否适合继续新增功能：`有条件` — 三闸门与测试基础健康；6 个 P1 均为高概率触发场景（拖 knob 调音、导出 WAV、切语言、拖放预设），建议先在本迭代消化 P1 再推进 Phase 27。
- 当前是否建议优先重构：`有条件` — 无需大范围重构；但 MainComponent 绑定编辑业务内嵌（ARCH-001）与 SettingsComponent 759 行全内联（ARCH-002）两个 P2 应近期处理，防止装配层再次膨胀。
- 最大风险：`AudioEngine` 参数更新路径（setAdsr/setPianoParameters）在 Synthesiser 锁外写活跃 voice 状态——演奏中拖 knob 的高频数据竞争（THR-001），与 AUDIT-001 已修复的 masterGain 竞争同类但范围更广。
- 下一步最高优先级：修复 THR-001 / THR-002 / SEC-001 三个实时/内存安全缺陷（见 5.2）。

### 0.4 重点发现

| ID | 优先级 | 状态 | 标题 | 当前结论 |
| --- | --- | --- | --- | --- |
| `THR-001` | P1 | 未处理 | AudioEngine 参数更新锁外写活跃 voice（数据竞争） | setAdsr/setPianoParameters 经 knob onChange 高频触发；Synthesiser::lock 仅保护管理操作，voice 参数写入在锁外与音频渲染并发（证据链见第 8 章） |
| `THR-002` | P1 | 未处理 | WAV 导出 Phase 1 无设备重建守卫读插件状态 | handleExportWavClicked 直接 snapshotPluginState(*liveInstance)，违反 PluginHost.h 自身线程契约；插件加载时导出 WAV 为常见路径 |
| `SEC-001` | P1 | 未处理 | 设置窗口语言切换/关闭 Viewport 悬挂指针 UAF | safeCleanupJiveTree 销毁组件后 Viewport::contentComp 悬挂，setViewedComponent/~Viewport 解引用已释放组件；每次语言切换必踩 |
| `QUAL-001` | P1 | 未处理 | 拖放 .devpiano.preset 扩展名判断死代码 | getFileExtension 只返回末段 ".preset"，两处 `== ".devpiano.preset"` 永不成立——预设拖放导入静默失效 |
| `ERR-001` | P1 | 未处理 | 导出进度框 X 关闭后 activeDialog 悬垂 UAF | deleteOnClose 对话框被删除后嵌套循环退出路径仍调用 activeDialog->exitModalState；点 X 不触发取消，渲染期间点 X 即 UAF |
| `TEST-001` | P1 | 未处理 | PluginOperationController 编排状态机零测试 | 插件扫描/加载/恢复的两步提交异步状态机无任何测试保护；与 AUDIT-001 TEST-001 同类缺口 |
| `SEC-002` | P2 | 未处理 | MIDI 导出混入 presetChange 伪 SysEx 且乱序 | 录制中切预设后导出：F0 F7 空 SysEx 混入文件 + 预置事件时间戳乱序（负 delta 被钳 0） |
| `PERF-001` | P2 | 未处理 | 回放移调路径音频回调每块堆分配 | renderPlaybackEventsIfNeeded 的 transposedBuffer + swap 每块 alloc/free 一对（transpose 启用时） |
| `ARCH-001` | P2 | 未处理 | MainComponent 绑定编辑业务内嵌 UI lambda | 约 60 行合并/落盘逻辑在 initialiseUi 接线内，不可单测，装配层继续膨胀 |
| `ARCH-002` | P2 | 未处理 | SettingsComponent.h 759 行全内联 | 修改一行触发两个下游 TU 全量重编；消费者仅 2 个，拆分成本低 |

---

## 1. 审计范围与方法

### 1.1 审计范围

审计 `source/` 下全部业务子模块 + tests/，共 128 个文件（106 业务 + 22 测试，~28,064 行）：

| 模块 | 路径 | 文件数 | 职责 |
| --- | --- | ---: | --- |
| 应用入口 | `source/Main.cpp` | 1 | JUCEApplication 启动、主窗口、Windows WNDPROC 钩子/焦点胶水、CLI 音色参数 |
| 主装配层 | `source/MainComponent.*` + `MainComponentJiveAccessors.cpp` | 3 | 装配 JIVE UI 树，初始化音频/MIDI/插件/设置，顶层协调与访问器 |
| UI | `source/UI/`（含 `jive/`、`native/`） | 20 | JIVE 布局/样式/设计 token、CustomKeyboard、LookAndFeel、对话框、原生注入组件 |
| Core | `source/Core/` | 3 | 纯数据类型（KeyMapTypes、AppState、MidiTypes） |
| Audio | `source/Audio/` | 6 | 音频引擎、设备诊断、PianoSynthVoice（1184 行）、Piano88KeyTable、SineSynthVoice |
| Plugin | `source/Plugin/` | 6 | VST3 扫描/加载/卸载/editor、流程编排（PluginFlowSupport、PluginOperationController） |
| Input | `source/Input/` | 2 | 电脑键盘到 MIDI 映射 |
| Midi | `source/Midi/` | 3 | 16 通道矩阵路由（ChannelMatrix、MidiChannelMapper） |
| Recording | `source/Recording/` | 20 | 录制/回放引擎、MidiTrackMergeEngine（Phase 26 新增）、MIDI 导入/导出、PerformanceFile、WAV、离线渲染、会话控制 |
| Layout | `source/Layout/` | 4 | Performance Preset CRUD、PresetFlowSupport |
| Settings | `source/Settings/`（含 `jive/`） | 10 | 设置持久化、序列化、AppStateBuilder、SettingsComponent（759 行头文件）、窗口管理 |
| Export | `source/Export/` | 5 | WAV 导出任务、导出流程支持 |
| Diagnostics | `source/Diagnostics/` | 5 | DP_LOG 宏、DevPianoLogger、MIDI trace |
| Locale | `source/Locale/` | 1 | 中文本地化（zh_CN.loc.h） |
| tests | `source/tests/` | 22 | 62 个测试类 / ~387 子测试 / 12187 断言 + TestRunner |

不包括：

- `submodules/JUCE/`、`submodules/JIVE/`（禁止修改，不审；仅用于交叉验证 API 行为）
- `scripts/`、`docs/`、构建脚本与配置文件（CMakeLists.txt/.clang-tidy/.clang-format/CI 仅作工程化核查输入）
- 第三方依赖（JUCE 框架本身）

### 1.2 审计输入

| 类型 | 路径 / 命令 |
| --- | --- |
| 代码 | `source/**/*.cpp` `source/**/*.h`（106 业务文件） |
| 测试 | `source/tests/*.cpp` + `TestHelpers.h`（22 文件，62 类 / 12187 断言） |
| 架构文档 | `docs/reference/architecture.md` |
| 项目定位 | `docs/reference/project-scope.md` |
| 路线图 | `docs/roadmap/roadmap.md` + `docs/roadmap/current-iteration.md` |
| 决策记录 | `docs/decisions/` ADR-001~013 |
| 已知问题 | `docs/issues/known-issues.md` |
| 构建系统 | `CMakeLists.txt`、`.clang-tidy`、`.clang-format`、`.github/workflows/` |
| 构建验证 | `./scripts/dev.sh wsl-build`（通过，21 目标 0 错误） |
| 测试验证 | `./scripts/dev.sh test`（通过，12187 断言 / 0 失败 / 11.0s） |
| 格式化检查 | `./scripts/dev.sh format --check`（通过，0 差异） |
| Windows 验证 | `./scripts/dev.sh win-build`（**未执行**——审计环境无法访问 Windows 镜像树，见 4.3） |
| 静态分析 | `clang-tidy` 全量（**未执行**——迭代边界例行点，约 19 分钟；本轮以 clangd 实时诊断与三闸门为准，见 4.3） |
| 交叉验证 | JUCE 源码（juce_Synthesiser.cpp、juce_Viewport.cpp、juce_File.cpp、juce_DialogWindow.cpp 等） |

### 1.3 严重级别定义

| 优先级 | 定义 | 期望处理 |
| --- | --- | --- |
| P0 | 崩溃、数据损坏、音频毛刺/无声、内存泄漏、线程安全缺陷 | 立即修复，阻断开发 |
| P1 | 高概率稳定性/维护性风险，影响核心路径（演奏/录制/插件） | 当前迭代修复 |
| P2 | 中等风险，影响可维护性、模块边界、测试覆盖或协作效率 | 近期排期 |
| P3 | 低风险改进：命名一致性、注释质量、const 正确性、未使用代码 | 持续跟踪或后续优化 |

> **ADR 合规映射**：违反 ADR 决策本体 → 按上表级别定级并开 `CMPL` 问题；ADR 事实性描述被证伪 → 直接修正 ADR 原文，不开问题（本轮无事实性描述被证伪）。

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

- 项目类型：**桌面 GUI 应用**（JUCE 8 框架，VST3 插件宿主，独立可执行文件，Windows + Linux 双平台）
- 核心能力：
  1. 电脑键盘触发 MIDI note → VST3 插件或内置音源发声
  2. 内置全物理建模钢琴音源（`PianoSynthVoice`，7 大声学系统，Phase 12–24 演进至 v3+）
  3. VST3 插件扫描 / 加载 / 卸载 / editor 窗口
  4. 可配置键位映射 + Performance Preset 系统
  5. 16 通道 MIDI 矩阵路由
  6. 录制 / 回放 / MIDI 导入导出 / WAV 离线渲染
  7. **MIDI 多轨时间线合并（Phase 26 新增）**：Type 0/1 全轨并轨、通道策略（Pass-through / Auto-Assignment）、多轨多通道 WAV 导出
  8. 逐键个性化（颜色 + 标签）+ 调号系统
  9. 中文本地化（运行时切换，BinaryData 内嵌）

### 2.2 技术栈与运行环境

| 类别 | 当前值 |
| --- | --- |
| 语言 | C++20 |
| 框架 | JUCE 8（git submodule）、JIVE（声明式 UI 子模块） |
| 构建系统 | CMake + Ninja（WSL/Clang-21）；MSVC（Windows 镜像树验证）；GitHub Actions（linux-gate + windows 门禁 + release） |
| 构建优化 | STL PCH、mold/lld 探测、MSVC `/Z7`+`/FS`、ccache/sccache、`-ftime-trace`（ADR-011） |
| 测试框架 | JUCE UnitTest（`devpiano_tests` 目标，62 类 / 12187 断言） |
| 格式化 | clang-format-21（WebKit 基，120 列，InsertBraces） |
| 静态分析 | clang-tidy-21（bugprone/performance/readability/modernize，只检查不 --fix，ADR-007） |
| 音频后端 | JUCE `AudioDeviceManager` |
| 插件格式 | VST3（主路径），通过 JUCE `AudioPluginFormatManager` |
| 数据格式 | JSON（Preset `.devpiano.preset`、录制 `.devpiano`）、XML（设置 PropertiesFile）、BinaryData 内嵌资产（ADR-010） |
| 开发环境 | WSL（编辑 + compile_commands.json）+ Windows/MSVC（构建验证 + 运行测试）+ Linux（CachyOS/Ubuntu 24.04） |

### 2.3 目录与模块边界

```text
source/
├── Main.cpp                  # 应用入口（262 行）
├── MainComponent.cpp/.h      # 主装配层（1324 行 + 225 行头）
├── MainComponentJiveAccessors.cpp  # JIVE 访问器（1066 行）
├── UI/                       # 20 文件：虚拟键盘、对话框、JIVE 布局/样式/token、原生注入
├── Core/                     # 3 文件：纯数据类型
├── Audio/                    # 6 文件：AudioEngine、PianoSynthVoice（1184 行）、Piano88KeyTable
├── Plugin/                   # 6 文件：插件宿主、流程编排
├── Input/                    # 2 文件：键盘→MIDI 映射
├── Midi/                     # 3 文件：通道矩阵路由
├── Recording/                # 20 文件：录制/回放、MidiTrackMergeEngine（Phase 26）、导入/导出、渲染管线
├── Layout/                   # 4 文件：Performance Preset CRUD
├── Settings/                 # 10 文件：持久化、SettingsComponent（759 行全内联头）
├── Export/                   # 5 文件：WAV 导出任务、流程支持
├── Diagnostics/              # 5 文件：DP_LOG 宏、Logger、MIDI trace
├── Locale/                   # 1 文件：中文本地化（BinaryData 内嵌）
└── tests/                    # 22 文件：62 测试类 / 12187 断言
```

边界判断：

- 清晰边界：`Core/`（纯数据）、`Input/`（单向映射）、`Midi/`（独立路由）、`MidiTrackMergeEngine`（纯静态无共享状态）、`Export/`（独立任务）
- 模糊边界：`MainComponent` 的绑定编辑业务仍内嵌 UI 接线（ARCH-001）；`SettingsComponent.h` 759 行全内联（ARCH-002）；`MainComponent` 与 `JiveUtils.h` 拆树逻辑双份（QUAL-008）
- 高复杂度热点：`PianoSynthVoice.h`（1184 行，实时 DSP）、`MainComponent.cpp`（1324 行）、`SettingsComponent.h`（759 行）、`MainComponentJiveAccessors.cpp`（1066 行）

---

## 3. 分领域审计结果

> 本章只记录分析结论与证据摘要；具体问题必须进入第 8 章问题总表。

### 3.1 架构与模块边界

评估项：

- `MainComponent` 是否仍然承担了不属于装配层的逻辑
- FlowSupport / Controller 拆分是否彻底，是否存在循环依赖或隐性耦合
- 头文件依赖图是否合理（`#include` 深度、传递包含、前向声明使用）
- `Core/` 类型是否真正零业务逻辑、零 JUCE GUI 依赖

- 评级：`B-`
- 结论：四控制器委托模式（PresetFlowSupport / RecordingSessionController / PluginOperationController / SettingsWindowManager）总体收敛；MidiTrackMergeEngine 纯静态无状态、PianoSynthVoice 自包含，新增模块边界良好。但 MainComponent 从上次审计的 ~1143 行回流至 1324 行，绑定编辑合并+预设自动落盘约 60 行业务逻辑内嵌在 initialiseUi 接线 lambda（ARCH-001）；SettingsComponent.h 759 行全内联（ARCH-002）；Core 零 GUI 依赖经 ADR-012 细粒度包含纪律强化，仍保持纯净。
- 关联问题：`ARCH-001`、`ARCH-002`、`QUAL-008`、`DOC-003`

### 3.2 代码质量与可维护性

评估项：

- RAII 与资源生命周期管理（`std::unique_ptr`、JUCE `OwnedArray`、文件句柄、插件实例）
- const 正确性（函数参数、成员函数、局部变量）
- 命名一致性（与 `docs/reference/architecture.md` 中约定是否一致）
- 注释质量（关键路径是否有意图说明，是否存在过期注释；注释遵循简体中文规范）
- 死代码 / 未使用函数 / 遗留 TODO
- 重复代码模式

- 评级：`B`
- 结论：RAII 与 const 正确性整体良好（ScopedTempDir、unique_ptr 编辑器窗口、原子状态机）。新增代码注释质量高（PianoSynthVoice 逐段标注物理依据与 Phase 来源）。扣分项：JIVE 构建辅助函数（node/text/button/flexRow/settingRow）在 4 个文件同构复制（QUAL-002）、访问器 lambda 样板重复 8 处（QUAL-009）、拖放预设扩展名判断死代码（QUAL-001）、singleTrackOnly 生产不可达（QUAL-006）、WavExportTask 死成员与过期注释（QUAL-005）、滞留 Phase 注释（QUAL-012）。
- 关联问题：`QUAL-001`~`QUAL-015`

### 3.3 线程安全与并发

评估项：

- JUCE `MessageManager` / `MessageThread` 正确使用：UI 操作是否仅在消息线程
- 音频回调（`AudioEngine::audioDeviceIOCallback`）是否实时安全（无锁、无分配、无 I/O）
- `std::atomic` / `CriticalSection` 使用是否正确
- 插件回调线程与 UI 线程之间的数据竞争风险

- 评级：`C+`
- 结论：AudioEngine 原子化状态（masterGain/currentSampleRate/currentBlockSize/warmup/transpose）与 RecordingEngine 设备重建守卫线程契约经逐点核实成立（全部结构性变更经 runPluginActionWithAudioDeviceRebuild 暂停音频回调）。但 Phase 12 后的参数更新路径引入两类新竞争：**setAdsr/setPianoParameters 在 Synthesiser 锁外写活跃 voice 参数**（knob 拖动高频触发，THR-001）；**WAV 导出 Phase 1 无守卫读插件状态**（THR-002，违反 PluginHost.h 自身契约）。此外设置窗口语言切换存在消息线程内 Viewport UAF（SEC-001，生命周期缺陷），MidiKeyboardState 监听器依赖未文档化的消息线程契约（THR-003）。
- 关联问题：`THR-001`、`THR-002`、`THR-003`、`SEC-001`、`PERF-001`

### 3.4 安全边界

检查项：

| 检查项 | 评估 |
| --- | --- |
| 文件系统边界：文件读写路径校验（Preset 加载、MIDI 导入、设置文件） | `部分` |
| 插件加载安全：DLL/so 加载前校验、路径规范化 | `通过` |
| 用户输入消毒：键位绑定配置解析 | `通过` |
| JSON 解析健壮性：Preset/录制/设置文件损坏或版本不兼容处理 | `部分` |
| MIDI 消息有效性：note 0–127、channel 0–15、velocity 钳制 | `通过` |
| 缓冲区溢出：MIDI 数据数组访问、键盘状态数组边界 | `通过` |
| 数值安全：类型转换、整数溢出、浮点精度 | `部分` |

- 结论：getNoteParams 越界 clamp、MidiNoteNumber/Velocity 强类型、文件名 sanitise、预设原子写均到位。缺口集中在**用户可控文件解析**：预设/locale 文件无大小上限（SEC-003）、设置/预设数值加载无钳制（SEC-005、SEC-006）、时间戳 double→int64 极端值转换未定义 [推断]（SEC-007）、MIDI 导出混入伪 SysEx 事件（SEC-002）。均为本地桌面威胁面，无远程路径。
- 关联问题：`SEC-001`~`SEC-007`

### 3.5 资源与性能

评估项：

- 实时音频路径的内存分配（`malloc/new` 在 audio callback 中）
- 插件实例生命周期管理（加载/卸载泄漏、editor 窗口泄漏）
- 数组/容器默认大小与增长策略（`std::vector`、`std::array`、`juce::Array`）
- MIDI 事件缓冲区上限
- 大文件处理（MIDI 文件导入、WAV 导出）的内存峰值

- 评级：`B-`
- 结论：PianoSynthVoice 逐采样路径零分配零超越函数（Magic Circle 振荡器 + 常量查表，符合 ADR-009）；插件 editor/实例生命周期 RAII。缺口：回放移调路径每块 MidiBuffer 分配+释放（PERF-001，P2）；回放每块全量扫描事件向量（PERF-002）；MIDI 导入消息线程同步解析（PERF-003）；take 多副本峰值内存 ~45MB（RES-001）；StyleCatalog ownedStyles 无释放累积（RES-002）；master limiter 超阈每采样 std::tanh（PERF-004）。AUDIT-001 暂缓项 PERF-001（全量内存加载）因 Phase 26 默认展开全部轨道而风险上升（见第 8 章）。
- 关联问题：`PERF-001`~`PERF-005`、`RES-001`、`RES-002`

### 3.6 错误处理与可观测性

评估项：

- 异常安全性 vs JUCE 的无异常约定
- 错误传播路径：返回值 → Logger → 用户通知 是否完整
- `DevPianoLogger` 使用覆盖率（是否存在散落 `std::cout` / `DBG()`）
- 静默失败点（忽略返回值、吞异常、空 catch）

- 评级：`B`
- 结论：DP_LOG_* 覆盖率 100%（全量 grep 零 std::cout/DBG/printf 残留）；WavExportTask 取消/失败清理路径健全；预设原子写、ERR-007 式行:列解析沿用。缺口：导出进度框 X 关闭路径 activeDialog 悬垂且不触发取消（ERR-001，P1）；Unbind 路径双重 onComplete（ERR-002）；SettingsStore 静默回退空 PropertiesFile（ERR-004）；预设加载失败静默回退默认（OBS-001）。
- 关联问题：`ERR-001`~`ERR-004`、`OBS-001`

### 3.7 测试体系

评估项：

- 现有测试覆盖的核心行为（KeyMapTypes、MidiFileImporter、AudioEngine、PianoSynthVoice 等）
- 缺少测试的关键模块（音频引擎、录制引擎、插件宿主、键盘映射运行时）
- 测试可维护性（辅助函数复用、fixture 管理、magic number）
- 测试是否独立（不依赖全局状态、不依赖音频设备、不依赖文件系统副作用）
- 测试是否接入 `devpiano_tests` 目标并随 `./scripts/dev.sh test` 运行

- 评级：`B`
- 结论：62 类全部注册进默认运行白名单，12187 断言全绿（较 AUDIT-001 的 2921 增长 4.2 倍）；Phase 26 新增模块（MidiTrackMergeEngine 9 用例、多轨 WAV 往返、SettingsLayoutModel、JiveModalDialog、PianoSynthVoice ~800 断言）全部有实质测试。最大缺口：PluginOperationController 编排状态机零覆盖（TEST-001，P1，与 AUDIT-001 TEST-001 同类）；WavExportTask 后台任务本体、PluginOfflineRenderer、SineSynthVoice、AppStateBuilder/SettingsSerialization、PresetFlowSupport 零直接覆盖（TEST-002~006）；PerformanceFileTest 未用 ScopedTempDir（TEST-007）；若干断言空洞/名不副实/全局状态泄漏小项（TEST-008~016）。
- 关联问题：`TEST-001`~`TEST-016`

### 3.8 文档与配置契约

评估项：

- `docs/reference/architecture.md` 与源码模块拆分一致性
- 配置默认值漂移（`SettingsModel` 默认值与 `AppState` 初始值是否一致）
- 头文件注释与实现是否同步
- Locale 表与代码内字符串一致性（中英运行时切换无缺漏）

- 评级：`B`
- 结论：architecture.md 已收录 PianoSynthVoice/Piano88KeyTable 与 Export 管线，但**缺 Phase 26 新增的 MidiTrackMergeEngine**（DOC-001）；roadmap 风险表 MainComponent ~1310 行与实测 1338 行漂移（DOC-002）；project-scope "多轨 / 完整 DAW 工作站功能超出定位" 与 Phase 26 多轨并轨能力表述需澄清（DOC-003）；smoothedPitchBend 注释与实现不符（DOC-004）。设置默认值经 AUDIT-001 DOC-006 收敛后无新漂移。
- 关联问题：`DOC-001`~`DOC-004`

### 3.9 工程化与构建

评估项：

- CMakeLists.txt 源文件列表完整性（是否存在未参与构建的孤立文件）
- clang-tidy 诊断清零状态
- clang-format 合规性
- 编译器警告清零状态（`-Wall -Wextra`）
- Debug / Release 构建一致性
- git 纪律：提交规范符合 AGENTS.md §4（Conventional Commits 子集，一个提交一件事）

- 评级：`B+`
- 结论：GLOB CONFIGURE_DEPENDS 模式下磁盘与构建无孤立文件差异；format --check 归零（.githooks 门禁在岗）；wsl-build 0 警告；git 纪律核查通过（近 30 条提交全部符合 Conventional Commits，feat/fix/refactor/docs/chore/build/ci 分离清晰）；ADR-011 构建优化（PCH/mold//Z7/ccache）全部落地。小项：多实例共享 settings 文件无跨进程保护（ENG-001）；设置内容高度 960 魔法数双处维护（ENG-002）。clang-tidy 全量本轮未执行（见 4.3）。
- 关联问题：`ENG-001`、`ENG-002`

### 3.10 ADR 合规审计

> 全面审计的决策合规维度。每个 ADR 逐条核对，**合规状态枚举 `合规 / 部分合规 / 违反`**；违反/部分合规必须开 `CMPL-XXX` 问题并写证据。
> 区分两类处理：**ADR 事实性描述**（数值/状态描述）被证伪 → 直接修正 ADR 原文，不开问题；**实现违反 ADR 决策本体** → 开 `CMPL` 问题。

| ADR | 决策要点（一句话） | 审计证据（可执行检查） | 合规状态 |
| :--- | :--- | :--- | :--- |
| ADR-001 | WSL 主工作树 + Windows 镜像树 + MSVC 验证 | `scripts/build_msvc_from_wsl.sh:57,166-196` 走 WIN_MIRROR_DIR（默认 G:\source\projects\devpiano）；审计侧未执行 win-build（4.3） | 合规 |
| ADR-002 | 旧 FreePiano 源码仅作迁移参考（已废止） | grep `freepiano`/`freepiano-src` 于 source/ 零命中 | 合规 |
| ADR-003 | `PluginFlowSupport` 纯函数命名空间，不持成员变量 | PluginFlowSupport.h:7-41 仅 namespace + 数据结构 StartupPluginRestorePlan + 自由函数，无全局状态 | 合规 |
| ADR-004 | JUCE `AudioDeviceManager` 音频主路径 | grep WASAPI/DirectSound/ASIO：命中均为误报（`addSound` 子串、JIVE "ASIO Control Panel" UI 行——JUCE 抽象层的 ASIO 设备控制面板，非原生后端代码） | 合规 |
| ADR-005 | JUCE `AudioPluginFormatManager` 宿主，VST3 主路径 | PluginHost.h 注册 VST3PluginFormat；grep `AEffect`/`dispatch()`/`audioMasterCallback` 零命中 | 合规 |
| ADR-006 | 移除外部 MIDI 设备支持 | grep `MidiRouter`/`externalMidi` 零命中；MidiChannelMapper 共享基础设施保留（符合 ADR 边界） | 合规 |
| ADR-007 | clang-tidy 只检查不 --fix，机械修复交 clang-format | `.clang-format` InsertBraces: true；`.clang-tidy` 禁用 4+2 类噪音、HeaderFilterRegex '^.*/source/.*' 均在岗 | 合规 |
| ADR-008 | JIVE 声明式 UI 主布局引擎 | MainComponent::resized() 核心 3 行（setBounds）；JiveModalDialog 声明式模板驱动全部弹窗；CustomKeyboard/AdsrCurve 经组件工厂注入 | 合规 |
| ADR-009 | PianoSynthVoice 默认内置音源，SineSynth 可切换 | AudioEngine.h:102 `builtinTone = BuiltinSynthTone::piano` 默认；rebuildSynth 双音色注册；逐采样零三角函数（renderNextBlock 无 std::sin/cos） | 合规 |
| ADR-010 | BinaryData 静态打包关键资产 | CMakeLists.txt:238 `juce_add_binary_data(devpiano_binary_data)`；MainComponent/LocaleManager 读 BinaryData::；热重载文件路径保留 | 合规 |
| ADR-011 | 现代构建流水线（/Z7、PCH、mold/lld、time-trace、缓存） | CMakeLists.txt:3-8（CMP0141 NEW + Embedded）、:20-21（/FS）、:24-34（mold/lld 探测）、:41-47（ENABLE_TIME_TRACE）、:108-111（-fno-pch-timestamp）；ci.yml linux-gate ubuntu-24.04 | 合规 |
| ADR-012 | 业务头文件禁止 JuceHeader.h，IWYU 细粒度包含 | 业务 .h 全部细粒度包含（零 JuceHeader）；**例外：source/tests/TestHelpers.h:3 使用 `<JuceHeader.h>`**（测试辅助头，非业务头，但字面违反 ADR 第 1 条"所有 source/ 目录下的 .h 头文件禁止出现"） | 部分合规 |
| ADR-013 | 移除 melatonin_inspector 子模块，主装配层与构建解耦 | grep `melatonin_inspector` 于 source/、CMakeLists.txt、.gitmodules 零命中；MainComponent 无 DEBUG 侵入式 inspector | 合规 |

- 评级：`A-`
- 结论：13 个 ADR 中 12 个合规、1 个部分合规（ADR-012）。无 ADR 事实性描述被证伪。部分合规项开 `CMPL-001`（P3：TestHelpers.h 迁移至细粒度包含，消除字面违反并降低测试编译级联）。
- 关联问题：`CMPL-001`

> **AUDIT-001 已暂缓 13 项本轮复查**：THR-003/THR-004、SEC-001~004、PERF-001/003/004、ERR-016/017、QUAL-020/021 全部仍成立（状态维持已暂缓，证据见第 8 章）；其中 PERF-001（MIDI 全量内存加载）原缓解前提（仅导入单轨）已随 Phase 26 默认展开全部轨道失效，风险上升（重开条件不变，但记录于证据列）。

---

## 4. 验证记录

### 4.1 命令执行结果

| 命令 | 结果 | 说明 |
| --- | --- | --- |
| `./scripts/dev.sh wsl-build` | `通过` | Debug 增量构建 21 目标 0 错误 0 警告，16.2s；同时刷新 compile_commands.json |
| `./scripts/dev.sh test` | `通过` | BUILD_TESTS=ON 构建 + ctest 1/1 通过（11.0s）；测试二进制直跑汇总 `Passed: 12187 / Failed: 0` |
| `./scripts/dev.sh format --check` | `通过` | clang-format-21 检查 0 差异 |
| `clang-tidy -p build-wsl-clang source/**/*.cpp` | `未执行` | 迭代边界例行点（约 19 分钟）；本轮以 clangd 实时诊断 + 三闸门为基线，见 4.3 |
| `./scripts/dev.sh win-build` | `未执行` | 审计环境无法访问 Windows 镜像树（G:\source\projects\devpiano），见 4.3 |

### 4.2 文件统计

| 指标 | 值 |
| --- | --- |
| 源文件总数（`.cpp`，业务，不含 tests） | 43 |
| 头文件总数（`.h`，业务） | 63 |
| 测试文件（tests/ .cpp + .h） | 22 |
| 总代码行数（含测试） | 28,064 |
| 业务代码行数（不含测试） | 19,321 |
| 测试代码行数 | 8,743 |
| 测试用例数 | 62 类 / ~387 子测试（215 beginTest+runTest / 172 testCase） |
| 断言总数 | 12,187（较 AUDIT-001 的 2,921 增长 4.2 倍） |
| 最大文件（业务） | `source/MainComponent.cpp`（1,324 行）；`source/Audio/PianoSynthVoice.h`（1,184 行） |
| 最大文件（测试） | `source/tests/StyleCatalogTest.cpp`（1,695 行） |

### 4.3 未执行验证说明

- `./scripts/dev.sh win-build`：审计环境（WSL 容器）无法访问 Windows 镜像树与 MSVC 工具链，本轮标未执行。**待用户手动验证**（建议在修复 P1 后执行）。
- `clang-tidy` 全量：约 19 分钟迭代边界例行点，本轮不重复执行；以三闸门 + clangd 实时诊断（`.clangd` 已启用）为静态分析基线。**待用户手动验证**（迭代边界执行 `./scripts/dev.sh tidy --all`）。

---

## 5. 修复路线图

> 覆盖第 8 章全部未处理问题（62 项）。已暂缓 13 项不排期（维持暂缓，重开条件见第 8 章）。路线图只排期不实施，修复另开迭代。

### 5.1 立即处理（P0）

本轮无 P0。

### 5.2 当前迭代处理（P1）

- [ ] `THR-001`：AudioEngine 参数更新与音频渲染同步——setAdsr/setPianoParameters 改经原子参数快照 + 音频线程 apply（或经 midiCollector 注入控制消息），消除锁外写 voice 状态
- [ ] `THR-002`：handleExportWavClicked 的插件状态快照包进 runPluginActionWithAudioDeviceRebuild；同步修正 PluginOfflineRenderer.h 线程注释
- [ ] `SEC-001`：SettingsComponent 拆树前先 viewport.setViewedComponent(nullptr, false)；refreshTexts 树重建延后到 callAsync
- [ ] `QUAL-001`：拖放扩展名判断改用 getFileName().endsWithIgnoreCase(".devpiano.preset")
- [ ] `ERR-001`：WavExportTask 对话框关闭路径（closeButtonPressed / deleteOnClose 回调）也设置 cancelRequested；退出前确认 activeDialog 有效或置空
- [ ] `TEST-001`：PluginOperationController 抽纯函数（resolvePluginScanPath、restore 计划决策）+ 提交顺序测试

### 5.3 近期排期（P2）

- [ ] `SEC-002`：MidiFileExporter 过滤非 midi 事件；stopRecording 合并 pendingPresetEvents 后按时间戳排序
- [ ] `PERF-001`：renderPlaybackEventsIfNeeded 移调路径就地改写或复用预分配 buffer，消除每块分配
- [ ] `ARCH-001`：绑定编辑合并/落盘逻辑下沉 KeyboardMidiMapper 或 PresetFlowSupport
- [ ] `ARCH-002`：SettingsComponent 拆 .h/.cpp，公开接口收敛为构造/回调/状态查询
- [ ] `QUAL-002`：JIVE 构建辅助函数提取共享头，消除 4 文件复制
- [ ] `ERR-002`：getCustomKeyboard 判空降级或值语义持有（Release 零成本校验替代）
- [ ] `ERR-003`：KeyBindingEditDialog Unbind 路径复用统一完成路径，保证单次回调
- [ ] `TEST-002`：WavExportTask runThread 成功/取消/失败三分支测试（ScopedTempDir + 极短 take）
- [ ] `TEST-003`：PluginOfflineRenderer 无插件直调 smoke（panic 注入 + 静音收尾）
- [ ] `TEST-004`：SineSynthVoice 确定性渲染测试（音准 DFT、包络、自清）
- [ ] `TEST-005`：AppStateBuilder + SettingsSerialization 纯函数 round-trip + 损坏输入测试
- [ ] `TEST-006`：PresetFlowSupport captureCurrentState 与 id 缓存一致性测试
- [ ] `TEST-007`：PerformanceFileTest 迁移 ScopedTempDir

### 5.4 后续优化（P3）

- [ ] `SEC-003`：preset/locale 文件读取前校验大小上限
- [ ] `SEC-004`：loadPreset 版本号接受 ≤ 当前版本 + 逐字段默认填充
- [ ] `SEC-005`：SettingsSerialization/SettingsStore 数值加载钳制到合法域
- [ ] `SEC-006`：MidiTrackMergeEngine/RenderPipeline 时间戳转换 clampToInt64 收敛
- [ ] `SEC-007`：SettingsStore::readNow 枚举强转加范围校验
- [ ] `ERR-004`：SettingsStore::file() 回退路径 jassert + 启动尽早安装 logger
- [ ] `OBS-001`：initialiseFromPreset 失败路径补 DP_LOG_WARN（含路径）
- [ ] `PERF-002`：回放渲染改块游标（复用 WavFileExporter 模式）
- [ ] `PERF-003`：MIDI 导入移后台线程或加事件上限
- [ ] `PERF-004`：master limiter tanh 换多项式近似或注明设计取舍
- [ ] `PERF-005`：预设目录扫描加修改时间缓存
- [ ] `RES-001`：take 以 move/共享语义传递，消除多副本峰值
- [ ] `RES-002`：StyleCatalog applyToNode 复用对象或按树生命周期释放
- [ ] `QUAL-003`：ChannelMatrix::active 实现契约或删除字段
- [ ] `QUAL-004`：插件离线渲染实现真实 down-mix 或声道不匹配显式告警
- [ ] `QUAL-005`：软限幅抽共享 helper，两导出路径行为一致
- [ ] `QUAL-006`：删除 WavExportTask 死成员并更新 WavFileExporter.h 注释
- [ ] `QUAL-007`：确认 singleTrackOnly 无外部用户后删除
- [ ] `QUAL-008`：merge 引擎负时间戳检查前移 + 全 t=0 take 长度对齐导出语义
- [ ] `QUAL-009`：MainComponent 复用 JiveUtils.h 拆树实现
- [ ] `QUAL-010`：refreshTitles 去重 + 访问器 lambda 提取文件级辅助
- [ ] `QUAL-011`：getBuiltinToneFromUi 改名对齐实际语义
- [ ] `QUAL-012`：CJK 字体候选链统一到 DesignTokens
- [ ] `QUAL-013`：清理滞留 Phase 注释为现状描述
- [ ] `QUAL-014`：SettingsComponent toggle 保留单一写路径
- [ ] `QUAL-015`：MidiTypes.h 显式 include juce 细粒度头，消除传递依赖
- [ ] `DOC-001`：architecture.md 补 MidiTrackMergeEngine 模块章节
- [ ] `DOC-002`：roadmap 风险表 MainComponent 行数更新或改描述性表述
- [ ] `DOC-003`：project-scope 澄清多轨并轨导入与 DAW 多轨工作站的边界
- [ ] `DOC-004`：RecordingEngine.h smoothedPitchBend 注释对齐实现
- [ ] `TEST-008`：KeyboardHitMappingTest expect(true) 改可观察断言
- [ ] `TEST-009`：StyleCatalogTest 空失败消息补文案
- [ ] `TEST-010`：SettingsStoreTest 权限用例改名或补 POSIX stat 断言
- [ ] `TEST-011`：AudioEngineTest crash-only 用例补行为断言
- [ ] `TEST-012`：StyleCatalogTest 像素阈值放宽或改对比性断言
- [ ] `TEST-013`：StyleCatalogTest 全局 tokens/L&F 用 RAII 恢复
- [ ] `TEST-014`：TestRunner 类别白名单改前缀匹配或未匹配告警
- [ ] `TEST-015`：--verbose 参数实现或从帮助移除
- [ ] `TEST-016`：删除测试侧 findNodeById 副本与状态机重复测试
- [ ] `ENG-001`：多实例改为单实例并转发参数，或引入 settings 文件锁
- [ ] `ENG-002`：设置内容高度从布局树实际值读取或提共享常量
- [ ] `CMPL-001`：TestHelpers.h 迁移细粒度包含，消除 ADR-012 字面违反
- [ ] `THR-003`：MidiKeyboardState 监听线程契约注释文档化

---

## 6. 最终结论

### 6.1 当前判断

总体评级 B+。三闸门全绿、测试体系显著增强（12187 断言）、Phase 12–26 新增代码结构清晰（MidiTrackMergeEngine 纯静态、PianoSynthVoice 自包含且有 ~800 断言守护）。但快速迭代引入了 6 个 P1：3 个线程/内存安全缺陷（THR-001 锁外参数写入、THR-002 导出读插件状态无守卫、SEC-001 Viewport UAF）、1 个功能静默失效（QUAL-001 拖放预设）、1 个导出对话框悬垂（ERR-001）、1 个编排层测试空洞（TEST-001）。这些缺陷均为高概率触发场景，且 3 个集中在"参数/导出"路径上——恰是 Phase 12–26 新增代码与旧守卫机制的接缝处。

### 6.2 是否建议继续新增功能

`有条件`：三闸门与测试基础健康，但建议先在本迭代消化 6 个 P1（均为当前迭代可修复规模），再推进 Phase 27（物理演奏交互——琴盖/Una Corda/力度曲线，将新增 UI → AudioEngine 参数路径，会放大 THR-001 的竞争面）。

### 6.3 是否建议先重构 / 补测试 / 补文档

- 重构：`有条件`：无需大范围重构；ARCH-001（绑定编辑下沉）与 ARCH-002（SettingsComponent 拆 TU）应随 P2 排期处理，防止装配层回流趋势延续。
- 补测试：`是`：TEST-001（P1）优先；TEST-002~007（P2）覆盖导出/编排/序列化纯函数层，成本低收益高。
- 补文档：`是`：DOC-001（architecture.md 缺 MidiTrackMergeEngine）随本轮一并补齐；DOC-002/003 顺手修正。

### 6.4 下一步三件事

1. 修复 THR-001 + THR-002（实时参数/导出路径与音频线程同步），并执行 win-build + 全量 clang-tidy 复验（待用户手动验证项）。
2. 修复 SEC-001 + ERR-001 + QUAL-001（三个用户高频触发的 UAF/悬垂/死代码），附针对性回归测试。
3. 补 TEST-001~007 测试与 DOC-001 文档，然后按 5.3/5.4 排期消化 P2/P3。

---

## 7. 复审记录

> 每次复审追加一个小节，不覆盖旧记录。复审后必须同步更新第 0 章汇总与第 8 章问题总表。

（初次审计，暂无复审记录。）

---

## 8. 附录：问题总表（登记表）

> 第 8 章是唯一状态源。新增、关闭、暂缓、缓解任何问题，都必须更新本表。
> 编号规则：ID 前缀与领域见下表；编号为报告内唯一（每领域从 001 起）。已暂缓项按领域编号并保留风险接受原因与重开条件；已关闭项不登记（修复证据在代码与 git 历史）。跨报告引用格式 `AUDIT-XXX <ID>`。状态枚举 `未处理 / 处理中 / 已缓解 / 已暂缓 / 已关闭`。
> 承接 AUDIT-001 已暂缓 13 项（本轮复查均仍成立，状态维持，标 `AUDIT-001` 来源）。

| ID | 领域 | 问题标题 | 优先级 | 状态 | 来源 | 影响摘要 | 证据 | 风险接受原因 | 重开条件 | 下一步 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| THR-001 | 线程安全 | AudioEngine 参数更新在 Synthesiser 锁外写活跃 voice 状态（数据竞争） | P1 | 未处理 | 审计 | 演奏中拖 volume/ADSR/brightness 等 knob → handlePerformanceUiChanged → setAdsr/setPianoParameters → updateAdsrOnVoices/updatePianoParametersOnVoices 经 getVoice（持锁返回裸指针后锁释放）锁外写 voice 的 ADSR 参数与 float 成员；音频线程持 Synthesiser::lock 渲染 voice 并发读 → C++ 内存模型 UB（参数撕裂/偶发毛刺）。与 AUDIT-001 THR-001（masterGain 原子化）同类但范围更广 | `source/Audio/AudioEngine.cpp:200-213,266-283`；`source/Audio/PianoSynthVoice.h:71-77,79-83,306,413`；`source/MainComponent.cpp:1198-1201`（knob onChange 触发）；`submodules/JUCE/.../juce_Synthesiser.cpp:99-103`（getVoice 锁释放）、`:192`（渲染持锁） | - | - | 参数改原子快照或经 midiCollector 注入，音频线程侧应用 |
| THR-002 | 线程安全 | WAV 导出 Phase 1 无设备重建守卫读取活动插件状态 | P1 | 未处理 | 审计 | 插件已加载时导出 WAV（常见路径）：消息线程 snapshotPluginState → getStateInformation 与音频线程 processBlock 并发访问同一实例，违反 PluginHost.h 自身线程契约（"Any mutation path that does not go through this guard creates an immediate data race"） | `source/Recording/RecordingSessionController.cpp:188-192`；`source/Plugin/PluginHost.h:7-16`（契约原文）；`source/Recording/PluginOfflineRenderer.h:18-20`（注释声称"audio is paused"，与实际不符） | - | - | Phase 1 快照包进 runPluginActionWithAudioDeviceRebuild |
| SEC-001 | 安全 | 设置窗口语言切换/关闭时 Viewport 悬挂指针 UAF | P1 | 未处理 | 审计 | refreshTexts → buildJiveUi → safeCleanupJiveTree 销毁旧 JIVE 组件树后，Viewport::contentComp 仍指向已释放组件；setViewedComponent（deleteOrRemoveContentComp 解引用 contentComp）与析构路径 ~Viewport 均对悬挂指针调用 removeComponentListener → UB。每次语言切换/关闭设置窗口必踩，ASan 必现，Release 偶发崩溃 | `source/Settings/SettingsComponent.h:54,58-59,411-417`；`submodules/JUCE/modules/juce_gui_basics/layout/juce_Viewport.cpp:172-175,203-240` | - | - | 拆树前 viewport.setViewedComponent(nullptr, false)；树重建延后 callAsync |
| QUAL-001 | 质量 | 拖放 .devpiano.preset 扩展名判断死代码，预设拖放导入静默失效 | P1 | 未处理 | 审计 | `getFileExtension()` 只返回最后一个 '.' 之后的子串：`foo.devpiano.preset` 返回 ".preset"，两处 `ext == ".devpiano.preset"` 永不成立——isInterestedInFileDrag 拒绝拖入，filesDropped 的 handleImportPresetFile 分支不可达 | `source/MainComponent.cpp:885-889,902-921`；`submodules/JUCE/modules/juce_core/files/juce_File.cpp:684-690` | - | - | 改 getFileName().endsWithIgnoreCase(".devpiano.preset") |
| ERR-001 | 错误处理 | 导出进度框 X 关闭不触发取消，activeDialog 悬垂 UAF | P1 | 未处理 | 审计 | escapeKeyTriggersCloseButton=false 且 X 关闭不设置 cancelRequested；deleteOnClose 对话框被删除后：嵌套循环退出路径 `activeDialog->exitModalState(0)`（:174）与 timerCallback 的 getContentComponent 均对已删除对象调用 → 渲染期间点 X 即 UAF | `source/Export/WavExportTask.cpp:137-142,156-158,173-182`；`submodules/JUCE/.../juce_DialogWindow.cpp:125-129`（launchAsync → enterModalState + deleteOnClose） | - | - | X 关闭也设置 cancelRequested；activeDialog 退出路径判活/置空 |
| TEST-001 | 测试 | PluginOperationController 编排状态机零测试覆盖 | P1 | 未处理 | 审计 | 插件扫描/加载/卸载/editor/启动恢复的异步状态机（AsyncUpdater + scanStepInProgress 两步提交，306 行）零测试；每一步提交直接作用用户设置持久化。与 AUDIT-001 TEST-001（RecordingSessionController）同类缺口 | `source/Plugin/PluginOperationController.h/.cpp`；source/tests/ 全目录无引用（grep） | - | - | 抽纯函数（resolvePluginScanPath/恢复计划决策）+ 提交顺序测试 |
| SEC-002 | 安全 | MIDI 导出混入 presetChange 伪 SysEx 事件且预置事件时间轴乱序 | P2 | 未处理 | 审计 | MidiFileExporter 无条件 addEvent（presetChange 事件的 message 为默认 MidiMessage=F0 F7 空 SysEx）；stopRecording 将 pendingPresetEvents 直接 append 到已排序 events 尾部不重排。录制期间切换预置后导出 → 文件混入伪 SysEx + 负 delta 被钳 0 导致时间戳错乱 | `source/Recording/MidiFileExporter.cpp:26-37`；`source/Recording/RecordingEngine.cpp:102-105,191`；`source/Layout/PresetFlowSupport.cpp:97-98`（触发路径）；`submodules/JUCE/.../juce_MidiFile.cpp:528-531` | - | - | 导出过滤非 midi 事件；stopRecording 合并后排序 |
| PERF-001 | 性能 | 回放移调路径音频回调每块堆分配 | P2 | 未处理 | 审计 | renderPlaybackEventsIfNeeded 的 transpose 分支：栈上 MidiBuffer transposedBuffer addEvent 分配 + swapWith 与成员交换 → 每块 alloc/free 一对（transpose 启用 + 播放中有事件时），违反音频回调无分配原则 | `source/Audio/AudioEngine.cpp:355-375` | - | - | 就地改写或复用预分配 buffer |
| ARCH-001 | 架构 | MainComponent 绑定编辑合并 + 预设自动落盘业务内嵌 UI 接线 lambda | P2 | 未处理 | 审计 | initialiseUi 内 onBindingEditRequested lambda 约 60 行（查找/解绑/更新/captureCurrentState/sanitisePresetFileName/savePreset），领域规则与 UI 事件耦合，不可单测；MainComponent 已回流至 1324 行 | `source/MainComponent.cpp:640-723` | - | - | 下沉 KeyboardMidiMapper 或 PresetFlowSupport |
| ARCH-002 | 架构 | SettingsComponent.h 759 行全内联 | P2 | 未处理 | 审计 | 构造/buildJiveUi/16 通道接线/全部 rebuild 内联在头文件；消费者仅 2 个 TU（SettingsWindowManager.cpp、SettingsLayoutModelTest.cpp），拆分风险低 | `source/Settings/SettingsComponent.h:1-759` | - | - | 拆 .h/.cpp，收敛公开接口 |
| QUAL-002 | 质量 | JIVE 布局构建辅助函数在 4 个文件同构复制 | P2 | 未处理 | 审计 | node/text/button/flexRow/settingRow 及 JIVE 约定（border-width="1" 等）分散在 4 处；一处修正其余 3 处漂移 | `source/UI/jive/JiveModalDialog.cpp:15-52`、`source/UI/KeyBindingEditDialog.cpp:16-62`、`source/UI/jive/LayoutModel.cpp:9-70`、`source/Settings/jive/SettingsLayoutModel.cpp:10-62` | - | - | 提取共享头，4 处改 include |
| ERR-002 | 错误处理 | getCustomKeyboard 裸指针仅 jassert 保护 | P2 | 未处理 | 审计 | Release 构建下 JIVE 树解释失败或 custom-keyboard 节点缺失时，handleWindowFocusLost/syncUiFromSettings 等多条路径空指针解引用崩溃 | `source/MainComponentJiveAccessors.cpp:779-782` | - | - | 调用点判空降级或值语义持有 |
| ERR-003 | 错误处理 | KeyBindingEditDialog Unbind 路径双重调用 onComplete | P2 | 未处理 | 审计 | unbind 按钮 onClick 先 onComplete 再 exitModalState 未置 completed 标志；窗口删除触发 JiveDialogContent 析构 → onCancel → onComplete 第二次调用。当前回调对空结果幂等，但契约已破坏 | `source/UI/KeyBindingEditDialog.cpp:452-465`；`source/UI/jive/JiveModalDialog.cpp:146-151` | - | - | 复用统一完成路径，保证单次回调 |
| TEST-002 | 测试 | WavExportTask 后台任务本体零测试 | P2 | 未处理 | 审计 | runThread 嵌套消息循环的成功/取消/失败三分支、errorMessage 加锁读取、取消清理均无测试（buildWavExportOptions 纯函数已有覆盖） | `source/Export/WavExportTask.h/.cpp`；source/tests/ 无直接引用 | - | - | ScopedTempDir + 极短 take 三分支 smoke |
| TEST-003 | 测试 | PluginOfflineRenderer 本体零直接测试 | P2 | 未处理 | 审计 | scaleTimestamp/buildRenderEvents/addPanicMidi 有测，渲染器本体（结束静音、失败处理）仅经无插件路径间接覆盖 | `source/Recording/PluginOfflineRenderer.h/.cpp` | - | - | 无插件直调 smoke |
| TEST-004 | 测试 | SineSynthVoice 零 voice 级测试 | P2 | 未处理 | 审计 | 正弦音色 ADSR/频率精度/noteOff 自清/0 采样率护栏未锁；与 PianoSynthVoice ~800 断言不对称 | `source/Audio/SineSynthVoice.h`；source/tests/ 无直接断言 | - | - | 仿 PianoSynthVoiceTest 补确定性渲染测试 |
| TEST-005 | 测试 | AppStateBuilder + SettingsSerialization 纯函数零覆盖 | P2 | 未处理 | 审计 | 全 UI 状态拼装单一入口与通道矩阵序列化无 round-trip 测试；runtime.sampleRate<=0 分支、损坏 ValueTree 反序列化未测 | `source/Settings/AppStateBuilder.h`；`source/Settings/SettingsSerialization.h` | - | - | round-trip + 损坏输入测试 |
| TEST-006 | 测试 | PresetFlowSupport 编排零覆盖 | P2 | 未处理 | 审计 | 预设 CRUD/应用编排（applyPresetById、rename/delete/import、id 缓存一致性）全依赖手测 | `source/Layout/PresetFlowSupport.h/.cpp`；source/tests/ 无引用 | - | - | captureCurrentState 与 id 缓存抽纯补测 |
| TEST-007 | 测试 | PerformanceFileTest 未用 ScopedTempDir，固定文件名并行/残留风险 | P2 | 未处理 | 审计 | makeScratchFile 固定文件名直写系统临时目录；ctest -j 并行两进程踩同一文件；崩溃残留令下一轮 hasTempResidue 全目录扫描误报。与套件其余 6 文件约定不一致 | `source/tests/PerformanceFileTest.cpp:39-41,47-53,69-72,154-160` vs `source/tests/TestHelpers.h:14-55` | - | - | 迁移 ScopedTempDir |
| SEC-003 | 安全 | 预设/locale 文件解析无大小上限 | P3 | 未处理 | 审计 | loadPreset loadFileAsString 整读入内存；tryLoadLocaleFile 无大小/内容校验。用户可控路径放置超大文件即内存峰值风险（本地威胁面） | `source/Layout/PerformancePreset.cpp:208`；`source/Locale/LocaleManager.h:14-28` | - | - | 读取前校验大小上限 |
| SEC-004 | 安全 | loadPreset 版本号严格相等，无前向兼容 | P3 | 未处理 | 审计 | version != performancePresetFormatVersion 直接拒绝；未来格式升级后旧版应用无法读新预设且无迁移提示 | `source/Layout/PerformancePreset.cpp:229-232` | - | - | 接受 ≤ 当前版本 + 逐字段默认填充 |
| SEC-005 | 安全 | 设置/预设数值加载无钳制 | P3 | 未处理 | 审计 | valueTreeToChannelMatrix 的 velocity/outputChannel/transpose 直接 static_cast 截断；readNow 的 colourMode/noteDisplay 枚举强转无范围校验——手改 XML 注入越界值 → 异常通道映射/switch UB | `source/Settings/SettingsSerialization.cpp:31-45`；`source/Settings/SettingsStore.cpp:195-200` | - | - | 加载后 clamp 到合法域 |
| SEC-006 | 安全 | 时间戳 double→int64 极端值转换未定义行为 [推断] | P3 | 未处理 | 审计 | static_cast<int64_t>(std::round(seconds*rate)) 与 std::llround 在畸形文件（极小 PPQ + 最大变长 tick）理论上可超出 int64 → UB；正常文件远低于 2^53，实际不可达 | `source/Recording/MidiTrackMergeEngine.cpp:282,370`；`source/Recording/RenderPipeline.cpp:12-14` | - | - | clampToInt64 辅助函数收敛 |
| SEC-007 | 安全 | SettingsStore::file() 静默回退空配置 PropertiesFile | P3 | 未处理 | 审计 | getUserSettings 返回 null 时回退 Options{} 无 applicationName 的静态文件；写入静默失败仅 DP_LOG_ERROR，且该日志发生在 logger 安装前会丢失 | `source/Settings/SettingsStore.cpp:150-153` | - | - | 回退路径 jassert + 启动尽早装 logger |
| ERR-004 | 错误处理 | WavExportTask 死成员 + WavFileExporter.h 过期注释 | P3 | 未处理 | 审计 | statusLabel/progressBar SafePointer 全文无引用；头注释仍称 "built-in sine synth"，实现为 piano/sine 双音色 | `source/Export/WavExportTask.h:70-71`；`source/Recording/WavFileExporter.h:13-15` vs `WavFileExporter.cpp:24-40` | - | - | 删死成员，更新注释 |
| OBS-001 | 可观测性 | initialiseFromPreset 加载失败静默回退默认预设 | P3 | 未处理 | 审计 | loadPreset 失败无任何日志直接 fallback makeDefaultPreset——预设损坏时用户无从得知 | `source/MainComponent.cpp:260-272` | - | - | 失败路径 DP_LOG_WARN 含路径 |
| PERF-002 | 性能 | 回放渲染每块全量扫描事件向量 | P3 | 未处理 | 审计 | renderPlaybackBlock 每块从头遍历 playbackTake.events 无游标；满容量 take（~18 万事件）约 16.9M 次时间戳比较/秒压音频线程 | `source/Recording/RecordingEngine.cpp:305-330` | - | - | 排序后改块游标（复用 WavFileExporter 模式） |
| PERF-003 | 性能 | MIDI 导入在消息线程同步全量解析+排序 | P3 | 未处理 | 审计 | tryImportMidiFile 直接调 importMidiFileWithMetadata（全量解析 + stable_sort）；大文件导入 UI 冻结无进度提示 | `source/Recording/RecordingSessionController.cpp:504-506`；`source/Recording/MidiTrackMergeEngine.cpp:311,410-418` | - | - | 移后台线程或加事件上限 |
| PERF-004 | 性能 | master limiter 超阈路径每采样 std::tanh | P3 | 未处理 | 审计 | getNextAudioBlock 的 soft-knee 限幅在 |x|>0.85 时每采样 double tanh；无分配但成本可观（512×2 超阈样本 ~50µs/块）；低于阈值零开销 | `source/Audio/AudioEngine.cpp:145-166` | - | - | 多项式近似或注明设计取舍 |
| PERF-005 | 性能 | 预设回放变更在 UI 定时器路径全量磁盘扫描 | P3 | 未处理 | 审计 | drainPendingPresetChanges → applyPresetByIndex → refreshCache → scanPresetDirectory 全量读盘+JSON 解析；预设数量大时回放中切换预设造成 UI 卡顿 | `source/MainComponent.cpp:842-848`；`source/Layout/PresetFlowSupport.cpp:25-27,81-85` | - | - | 目录修改时间缓存，变化才重扫 |
| RES-001 | 资源 | take 多副本峰值内存 ~45MB | P3 | 未处理 | 审计 | stopRecording 返回成员拷贝（无 NRVO）、playbackTake 赋值拷贝、handleExportWavClicked takeCopy 拷贝——满容量 take（~15MB）停止/回放/导出瞬间 2~3 份 | `source/Recording/RecordingEngine.cpp:116,206`；`source/Recording/RecordingSessionController.cpp:175,181` | - | - | move 返回/swap + shared_ptr<const Take> 语义 |
| RES-002 | 资源 | StyleCatalog ownedStyles 每次 applyToTree 累积无释放 | P3 | 未处理 | 审计 | makeJiveObject 每次对话框打开/设置重建都新建 jive::Object 并永久持有，仅 shutdown 时 releaseOwnedStyles；长会话高频开关对话框缓慢累积 | `source/UI/jive/StyleCatalog.cpp:92-115`；`source/MainComponent.cpp:301` | - | - | applyToNode 复用对象或按树生命周期释放 |
| QUAL-003 | 质量 | ChannelMatrix::active 契约未实现 | P3 | 未处理 | 审计 | 注释宣称 inactive 时全部 pass-through，但 MidiChannelMapper 全文无任何 matrix.active 读取（仅序列化往返）；默认 true 掩盖问题，预设文件写出 false 时矩阵仍生效 | `source/Midi/MidiChannelMapper.h:18-21`；`source/Midi/MidiChannelMapper.cpp`（grep 零读取） | - | - | 实现 active 检查或删除字段与注释 |
| QUAL-004 | 质量 | 插件离线渲染 down-mix 实为截取，且两导出路径限幅行为不一致 | P3 | 未处理 | 审计 | outputChannels=jmin(...) 后逐通道 copyFrom 无混合——多输出插件导出立体声丢弃 3+ 声道、mono 插件右声道静音；且软限幅仅存在于 fallback synth 路径，插件路径无限幅（靠写入端截断），同一 take 两条路径响度行为不一致 | `source/Recording/PluginOfflineRenderer.cpp:117,163-176`；`source/Recording/WavFileExporter.cpp:134-149` | - | - | 真实 down-mix 或显式告警；限幅抽共享 helper |
| QUAL-005 | 质量 | singleTrackOnly 生产路径不可达 | P3 | 未处理 | 审计 | findNoteRichTrackIndex/singleTrackOnly 仅 options.singleTrackOnly 时调用；生产链使用默认 MidiImportOptions（mergeAllTracks=true）；约 30 行 legacy 代码仅测试覆盖 | `source/Recording/MidiTrackMergeEngine.cpp:147-184`；`source/Recording/MidiFileImporter.cpp:65-66`、`MidiFileImporter.h:15-17` | - | - | 确认无外部用户后删除 |
| QUAL-006 | 质量 | merge 引擎退化输入处理缺陷（负时间戳统计虚高 + 全 t=0 take 回放瞬时结束） | P3 | 未处理 | 审计 | 负时间戳事件先计数后丢弃，日志统计与 mergedEventCount 不符；全事件 t=0 的 take lengthSamples=0，回放首块即 ended，而导出取 lastEventEnd+1 正常渲染——回放/导出行为不一致 | `source/Recording/MidiTrackMergeEngine.cpp:341-370,431`；`source/Recording/RecordingEngine.cpp:334-343,360-372`；`source/Recording/RenderPipeline.cpp:41-45` | - | - | 负时间戳检查前移；lengthSamples 对齐导出语义 |
| QUAL-007 | 质量 | MainComponent 与 JiveUtils.h 拆树逻辑双份重复 | P3 | 未处理 | 审计 | clearJiveStyleSheets/collectJiveComponents 在 MainComponent.cpp 匿名空间与 JiveUtils.h 完全重复；修复只改一处则另一处保留旧行为 | `source/MainComponent.cpp:78-88,100-113` vs `source/UI/jive/JiveUtils.h:15-24,27-34` | - | - | MainComponent 复用 JiveUtils 版本 |
| QUAL-008 | 质量 | refreshTitles 重复条目 + 访问器 lambda 样板重复 8 处 | P3 | 未处理 | 审计 | "speed-knob" → "Playback Speed" 出现两次（:658,:661）；findItemWithID+dynamic_cast 模式在 MainComponentJiveAccessors.cpp 8 处内联重复 | `source/UI/jive/LayoutModel.cpp:658,661`；`source/MainComponentJiveAccessors.cpp:218,354,499,524,598,659,677,715` | - | - | 去重 + 提取文件级辅助 |
| QUAL-009 | 质量 | getBuiltinToneFromUi 名不副实 | P3 | 未处理 | 审计 | 实现返回 appSettings.builtinTone（UI 无音色控件），与相邻"真读 UI"的 getPianoBrightness 并列误导维护者 | `source/MainComponent.h:108`；`source/MainComponent.cpp:1086-1088` | - | - | 改名 getBuiltinToneFromSettings 或内联 |
| QUAL-010 | 质量 | CJK 字体候选链双份维护且已不一致 | P3 | 未处理 | 审计 | LookAndFeel 12 项 fallback 链 vs DesignTokens 7 项 fontconfig 探测列表；两份漂移风险 | `source/UI/DevPianoLookAndFeel.cpp:9-48`；`source/UI/jive/DesignTokens.cpp:246-268` | - | - | 统一到 DesignTokens |
| QUAL-011 | 质量 | 历史重构注释滞留（Phase 注释） | P3 | 未处理 | 审计 | "Phase 11d 删除 PluginPanel 组件类" 等描述已落地重构的注释随文件存续，与新代码不标 phase 的约定相悖 | `source/UI/PluginTypes.h:6`、`source/UI/PresetDialogs.cpp:6-8`、`source/Settings/SettingsComponent.h:18-21`、`source/UI/RecordingTypes.h:6-9` | - | - | 清理为不带 phase 的现状描述 |
| QUAL-012 | 质量 | SettingsComponent toggle 的 onClick 与 onStateChange 双重写同一属性 | P3 | 未处理 | 审计 | 每次点击 editingState.setProperty 执行两次（第二次无变化），冗余无害但语义混乱 | `source/Settings/SettingsComponent.h:204-230` | - | - | 仅保留 onStateChange |
| QUAL-013 | 质量 | MidiTypes.h 缺显式 juce include（传递包含脆弱） | P3 | 未处理 | 审计 | 使用 juce::uint8 但依赖上游传递包含；头文件自包含性破坏风险（与 ADR-012 IWYU 精神冲突） | `source/Core/MidiTypes.h`（grep 无 juce include） | - | - | 补细粒度 juce include |
| QUAL-014 | 质量 | 测试侧 findNodeById 副本与生产 helper 重复 | P3 | 未处理 | 审计 | StyleCatalogTest 匿名空间副本 vs JiveUtils.h 生产实现；测试本应验证生产 helper 本身 | `source/tests/StyleCatalogTest.cpp:83-92` vs `source/UI/jive/JiveUtils.h:158-166` | - | - | 删除副本，改用生产 helper |
| QUAL-015 | 质量 | RecordingFlow 状态机测试两份重复维护 | P3 | 未处理 | 审计 | RecordingFlowSupportTest 与 RecordingFlowStateMachineTest 的 chooseRecordingFlowCommand/getStateAfterCommand 矩阵双份，改状态机需改两处 | `source/tests/RecordingEngineTest.cpp:819-873` vs `source/tests/RecordingSessionControllerTest.cpp:106-251` | - | - | 保留一份（后者更全） |
| DOC-001 | 文档 | architecture.md 缺 MidiTrackMergeEngine 模块章节 | P3 | 未处理 | 审计 | Phase 26 新增核心引擎（多轨合并，3 文件）未收录；Recording 章节止于 MidiFileImporter | `docs/reference/architecture.md:126-140` vs `source/Recording/MidiTrackMergeEngine.h/.cpp` | - | - | 补模块章节与多轨合并管线说明 |
| DOC-002 | 文档 | roadmap 风险表 MainComponent 行数漂移 | P3 | 未处理 | 审计 | 风险表称 "当前 ~1310 行"，实测 1324 行（移除 inspector 后 -14，较 ~1310 仍存在轻微漂移） | `docs/roadmap/roadmap.md:246` vs `source/MainComponent.cpp`（1324 行） | - | - | 更新或改描述性表述 |
| DOC-003 | 文档 | project-scope "多轨超出定位" 与 Phase 26 多轨并轨能力表述冲突 | P3 | 未处理 | 审计 | scope 表称 "多轨 / 完整 DAW 工作站功能超出定位"，而 Phase 26 已实现多轨并轨导入/回放/导出；边界需澄清（并轨导入 ≠ DAW 多轨工作站） | `docs/reference/project-scope.md:57` vs `docs/roadmap/roadmap.md:223-228` | - | - | 澄清表述 |
| DOC-004 | 文档 | smoothedPitchBend 注释与实现不符 | P3 | 未处理 | 审计 | 注释称 "Initialised in startPlayback / stopPlayback"，实际 stopPlayback 不触碰该数组（仅 startPlayback 重置；行为正确，注释误导） | `source/Recording/RecordingEngine.h:132-137` vs `RecordingEngine.cpp:228,253-261` | - | - | 修正注释 |
| TEST-008 | 测试 | KeyboardHitMappingTest expect(true) 空洞断言 | P3 | 未处理 | 审计 | paint 裁剪用例只验证不崩溃，永远通过无法证伪渲染行为 | `source/tests/KeyboardHitMappingTest.cpp:163` | - | - | 改可观察断言或注明 crash-only |
| TEST-009 | 测试 | StyleCatalogTest 空失败消息断言 | P3 | 未处理 | 审计 | expect(component != nullptr, "") 多处；失败无诊断上下文 | `source/tests/StyleCatalogTest.cpp:449-452,593` | - | - | 补消息文案 |
| TEST-010 | 测试 | SettingsStoreTest 权限用例名不副实 | P3 | 未处理 | 审计 | 用例名声称验证受限权限，实际只断言 existsAsFile 与 getSize()>0，权限位从未检查 | `source/tests/SettingsStoreTest.cpp:168-186` | - | - | 改名或补 POSIX stat 断言 |
| TEST-011 | 测试 | AudioEngineTest crash-only 用例零断言 | P3 | 未处理 | 审计 | "subsequent blocks after all-notes-off are safe" 仅靠不崩溃验证，无行为契约（该上下文可断言输出为 0） | `source/tests/AudioEngineTest.cpp:265-272` | - | - | 补静音断言 |
| TEST-012 | 测试 | StyleCatalogTest 像素阈值断言对字体环境敏感 [未验证] | P3 | 未处理 | 审计 | countLightPixels 阈值 >12/>25/>50；不同 fontconfig/渲染器下字体像素数可能变化，潜在 flaky（同文件相对比例断言已规避同类问题） | `source/tests/StyleCatalogTest.cpp:1074-1076,1130,1168` | - | - | 阈值放宽或改对比性断言 |
| TEST-013 | 测试 | StyleCatalogTest 泄漏全局 tokens / LookAndFeel 状态 | P3 | 未处理 | 审计 | testDesignTokensHotReload 结束不还原 tokens；LookAndFeel::setDefaultLookAndFeel(nullptr) 全局变更——靠后续文件开头 reset 约定才不污染，约定未固化 | `source/tests/StyleCatalogTest.cpp:213-245,230,250-253` | - | - | RAII 保存/恢复 |
| TEST-014 | 测试 | TestRunner 类别白名单静默失效风险 | P3 | 未处理 | 审计 | projectCategories 硬编码 4 类别；新测试用新类别默认不运行且不报错（当前 62 类全合规，无即时问题） | `source/tests/TestRunner.cpp:133-135` | - | - | 前缀匹配或未匹配告警 |
| TEST-015 | 测试 | TestRunner --verbose 参数 no-op | P3 | 未处理 | 审计 | 解析后仅注释，帮助文案误导调用方 | `source/tests/TestRunner.cpp:57-59` | - | - | 实现或从帮助移除 |
| TEST-016 | 测试 | 全局 locale 变更依赖手工还原 | P3 | 未处理 | 审计 | devpiano::locale::activate 三次调用，进程级状态，依赖手工纪律（当前所有路径均已还原） | `source/tests/StyleCatalogTest.cpp:944,996,1017` | - | - | RAII 作用域守卫 |
| ENG-001 | 工程化 | 多实例允许 + 共享 settings 文件无跨进程保护 | P3 | 未处理 | 审计 | moreThanOneInstanceAllowed()=true 且 anotherInstanceStarted 空实现——双实例 last-write-wins 交叉写损坏设置（PropertiesFile 非跨进程安全），第二实例 --tone 参数被丢弃 | `source/Main.cpp:88-90` | - | - | 改单实例并转发参数或引入文件锁 |
| ENG-002 | 工程化 | 设置内容高度魔法数 960 双处维护 | P3 | 未处理 | 审计 | calculateSettingsContentHeight 恒返回 960 vs SettingsLayoutModel settings-root height 960；新增区段超 960 时静默截断且两处需同步改 | `source/Settings/SettingsComponent.h:730-732`；`source/Settings/jive/SettingsLayoutModel.cpp:344-345` | - | - | 从布局树读实际高度或提共享常量 |
| CMPL-001 | 决策合规 | TestHelpers.h 使用 JuceHeader.h，字面违反 ADR-012 | P3 | 未处理 | 审计 | ADR-012 决策第 1 条："所有 source/ 目录下的 .h 头文件禁止出现 #include <JuceHeader.h>"；TestHelpers.h:3 违反（测试辅助头，非业务头，级联影响有限，故 P3） | `source/tests/TestHelpers.h:3`；`docs/decisions/ADR-012-header-iwyu-and-granular-include-discipline.md:19-29` | - | - | 迁移细粒度包含 |
| THR-003 | 线程安全 | MidiKeyboardState 监听器回调依赖隐式消息线程契约 | P3 | 未处理 | 审计 | handleNoteOn → notifyMidiActivity → JIVE 树遍历；当前调用点均在消息线程（已核实），但契约未文档化，未来非消息线程注入即跨线程 UI 访问 | `source/MainComponent.cpp:806-814`；调用点 `MainComponent.cpp:637`、`KeyboardMidiMapper.cpp:171`、`MidiChannelMapper.cpp:47` | - | - | 注释契约或 MessageManagerLock 防护 |
| AUDIT-001 THR-003 | 线程安全 | MidiChannelMapper 引用成员悬垂风险 | P2 | 已暂缓 | AUDIT-001 | 构造器存储 const ChannelMatrix&/const bool&/const int&，外部对象销毁后悬垂 | `source/Midi/MidiChannelMapper.h:56-58`（本轮复查位置） | 引用对象为 MainComponent::appSettings 成员，寿命安全；reconfigure 重建 mapper | appSettings 动态分配或生命周期缩短 | 文档化生命周期契约或改值拷贝 |
| AUDIT-001 THR-004 | 线程安全 | PluginHost::getInstance 暴露裸指针 | P2 | 已暂缓 | AUDIT-001 | 返回 AudioPluginInstance* 裸指针，音频线程经它 processBlock，生命周期依赖外部协调 | `source/Plugin/PluginHost.h:64`（本轮复查位置；头文件 7-16 行线程契约注释已加强） | 生命周期由 runPluginActionWithAudioDeviceRebuild 外部协调，无并发竞争 | 引入非设备重建 guard 的插件切换路径 | 返回 Ptr 或文档化所有权契约（本轮新增 THR-002 即该契约的导出路径违反） |
| AUDIT-001 SEC-001 | 安全 | MidiChannelMapper::configForChannel 静默 clamp | P2 | 已暂缓 | AUDIT-001 | 越界 channel 参数被静默 jlimit 到 [0,15] | `source/Midi/MidiChannelMapper.cpp:10-13`（本轮复查） | 调用方均传合法 0-15 通道，越界仅理论可能 | 发现调用方传越界 channel 的实际路径 | 添加 jassert 或返回 std::optional |
| AUDIT-001 SEC-002 | 安全 | MidiFileImporter 无文件大小限制 | P2 | 已暂缓 | AUDIT-001 | 仅检查 getSize()==0，超大/恶意 MIDI 文件可导致内存耗尽 | `source/Recording/MidiFileImporter.cpp:31-39`（本轮复查） | 本地桌面应用、用户自选文件威胁面有限 | 导入超大文件出现实测内存问题 | 添加可配置大小上限 |
| AUDIT-001 SEC-003 | 安全 | MidiNoteNumber aggregate init 绕过 fromClamped | P3 | 已暂缓 | AUDIT-001 | MidiNoteNumber{200} 可绕过 clamp 保护 | `source/Core/MidiTypes.h`（本轮复查：仍为公有 aggregate） | 全项目调用点均经 fromClamped/helper 构造 | 新增绕过 fromClamped 的构造点 | 私有构造函数或 requires clause |
| AUDIT-001 SEC-004 | 安全 | 0/1-based 通道转换脆弱 | P3 | 已暂缓 | AUDIT-001 | channel 值在 0/1-based 间手工转换，缺类型系统保护 | `source/Input/KeyboardMidiMapper.cpp:165,167,183`；`source/Midi/MidiChannelMapper.cpp:26`（本轮复查各调用点 0-based 语义一致） | 当前路径正确且无缺陷报告 | 出现 0/1-based 混淆缺陷 | 统一用 MidiChannel::toZeroBased() |
| AUDIT-001 PERF-001 | 性能 | MidiFileImporter 全量内存加载 | P2 | 已暂缓 | AUDIT-001 | 整文件读入 juce::MidiFile 再转换，大文件可能数百 MB | `source/Recording/MidiFileImporter.cpp:49-62`（本轮复查） | 原缓解（仅导单轨）已随 Phase 26 默认展开全部轨道（MidiTrackMergeEngine.cpp:186-190）**失效，风险上升**；仍为本地桌面低频路径 | 导入大文件实测内存峰值过高 | 流式处理或事件数量上限（与 SEC-002 一并处理） |
| AUDIT-001 PERF-003 | 性能 | KeyboardSettings 2KB+ 固定数组 | P3 | 已暂缓 | AUDIT-001 | customKeyLabels/customKeyColours 固定 std::array 128 项 | `source/UI/KeyboardTypes.h:53,56`（本轮复查） | 持久化侧已稀疏化（SettingsStore 仅存非空 label） | 大量自定义键场景内存实测过高 | 改 std::vector 或 sparse map |
| AUDIT-001 PERF-004 | 性能 | isKeyCurrentlyDown O(n) 轮询 | P3 | 已暂缓 | AUDIT-001 | handleKeyStateChanged 每帧遍历所有 binding 查询 OS 键状态 | `source/Input/KeyboardMidiMapper.cpp:90-96`（本轮复查） | 36 次/帧消息线程开销可忽略 | 键盘轮询改高频或 binding 数大增 | std::bitset 或 unordered_set |
| AUDIT-001 ERR-016 | 错误处理 | AppStateBuilder 仅 jassert 线程守卫 | P2 | 已暂缓 | AUDIT-001 | assertMessageThreadSnapshotAccess 仅 jassert，Release 为 no-op | `source/Settings/AppStateBuilder.cpp:9-15,31,50`（本轮复查） | 本轮核查所有快照构建路径均来自消息线程 | 新增非消息线程调用方 | jassert + 错误码或 Release 保持检查 |
| AUDIT-001 ERR-017 | 错误处理 | SettingsStore scheduleSave 裸指针 API | P2 | 已暂缓 | AUDIT-001 | DebounceTimer 持有 const SettingsModel* 裸指针，timer 触发前对象析构则悬垂 | `source/Settings/SettingsStore.h:20`；`SettingsStore.cpp:355-362`（本轮复查） | 调用方均传 MainComponent::appSettings 长寿命成员 | 出现 SettingsModel 寿命短于 timer 的调用方 | shared_ptr 或文档化寿命契约 |
| AUDIT-001 QUAL-020 | 质量 | findByKeyCode 返回裸指针 | P3 | 已暂缓 | AUDIT-001 | 返回 const KeyBinding* 指向 vector 内部，修改后悬垂 | `source/Core/KeyMapTypes.h:69-77`（本轮复查） | 调用方均在同一快照内立即使用 | findByKeyCode 返回后 vector 被修改的调用方出现 | 返回 optional<reference_wrapper> 或索引 |
| AUDIT-001 QUAL-021 | 质量 | AudioEngine getMidiCollector/getKeyboardState 暴露内部可变引用 | P3 | 已暂缓 | AUDIT-001 | 返回可变引用允许外部任意修改内部 MIDI 状态 | `source/Audio/AudioEngine.h:73-78`（本轮复查，未变化） | 两个 JUCE 类型本身线程安全（内置锁/跨线程设计）；本轮未发现新增非法调用 | 外部代码直接修改内部状态造成缺陷 | 提供 const 版本或受限 API |

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
