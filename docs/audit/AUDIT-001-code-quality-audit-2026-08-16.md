# devpiano 代码质量审计报告 · 2026-08-16

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
| 审计范围 | `source/` （含 15 个子目录 + tests/，104 个 .cpp/.h 文件，~18,577 行） |
| 审计日期 | `2026-08-16` |
| 审计基线 | `main` @ `b352cff`（fix: persist audio device state on default-device startup） |
| 审计人 | AI code audit（主代理核心审查 + 5 个只读 scout 并行分领域 + 手工验证） |
| 复审状态 | `初次` |

### 0.2 风险与状态汇总

| 优先级 | 合计 | 未处理 | 处理中 | 已缓解 | 已暂缓 | 已关闭 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| P0 | 0 | 0 | 0 | 0 | 0 | 0 |
| P1 | 5 | 3 | 0 | 0 | 0 | 2 |
| P2 | 29 | 16 | 0 | 0 | 9 | 4 |
| P3 | 51 | 37 | 0 | 0 | 7 | 7 |
| **合计** | 85 | 56 | 0 | 0 | 16 | 13 |

### 0.3 关键结论

- 总体评级：`B` — 核心运行时健康（0 崩溃/泄漏/静默数据损坏的 P0），线程模型经设备重建同步点设计可靠；实时线程 2 处 P1（播放结束日志 I/O、masterGain 数据竞争）已于 2026-08-16 Phase A 修复；剩余驱动因素：三闸门中 format 门禁回归失败、测试体系存在 3 个 P1 覆盖空洞、架构文档滞后于 Phase 11 新结构。
- 当前是否适合继续新增功能：`有条件` — 实时线程 P1 已修复（2026-08-16）；建议先补 3 个核心模块零测试（P1），其余 P2/P3 可与功能开发并行。
- 当前是否建议优先重构：`有条件` — 不涉及大规模重构；优先清理 P2 死代码/错误持久化路径（QUAL-001、ERR-004）。
- 最大风险：3 个核心模块（会话控制/通道矩阵/预设序列化）零测试覆盖下的回归隐患。
- 下一步最高优先级：补 TEST-001~003 三个 P1 纯逻辑测试。

### 0.4 重点发现

| ID | 优先级 | 状态 | 标题 | 当前结论 |
| --- | --- | --- | --- | --- |
| `ERR-001` | P1 | 已关闭 | 音频线程播放结束路径 DP_LOG_INFO（文件 I/O + 互斥锁） | 实时线程仅置 playbackEndedPending 标志，日志移至消息线程 checkPlaybackEnded()（2026-08-16 复审 3） |
| `THR-001` | P1 | 已关闭 | AudioEngine::masterGain 跨线程数据竞争（非原子 float） | masterGain 改 std::atomic<float>（2026-08-16 复审 3） |
| `TEST-001` | P1 | 未处理 | RecordingSessionController 零测试覆盖 | 录制/回放/导入/导出全流程状态机 646 行无任何测试 |
| `TEST-002` | P1 | 未处理 | MidiChannelMapper 零测试覆盖 | 通道路由/移调核心逻辑无测试；唯一相关测试仅测 nullptr 透传 |
| `TEST-003` | P1 | 未处理 | PerformancePreset 序列化零测试覆盖 | 预设 round-trip 无测试，格式变更可静默破坏用户数据 |
| `ENG-001` | P2 | 已关闭 | format --check 门禁回归失败（18 处违规） | 18 处已修复归零 + `.githooks/pre-commit` 防再回归（2026-08-16 复审 4） |
| `ERR-004` | P2 | 未处理 | 插件加载失败仍按成功提交恢复状态并持久化 | 下次启动反复尝试加载失败插件；UI 无失败反馈 |
| `DOC-002` | P2 | 未处理 | architecture.md 缺 Recording/Export/Layout/Diagnostics 四模块章节 | Phase 11 / RenderPipeline 新结构未收录，文档滞后 |

---

## 1. 审计范围与方法

### 1.1 审计范围

审计 `source/` 下全部业务子模块 + tests/，共 104 个文件（50 .cpp + 54 .h，~18,577 行）：

| 模块 | 路径 | 文件数 | 职责 |
| --- | --- | ---: | --- |
| 应用入口 | `source/Main.cpp` | 1 | JUCEApplication 启动、主窗口、Windows WNDPROC/焦点胶水 |
| 主装配层 | `source/MainComponent.*` + `MainComponentJiveAccessors.cpp` | 3 | 装配 JIVE UI 树，初始化音频/MIDI/插件/设置，顶层协调与访问器 |
| UI | `source/UI/`（含 `jive/`、`native/`） | 28 | JIVE 布局/样式/设计 token、CustomKeyboard、LookAndFeel、对话框、原生注入组件 |
| Core | `source/Core/` | 3 | 纯数据类型（KeyMapTypes、AppState、MidiTypes） |
| Audio | `source/Audio/` | 3 | 音频引擎、设备诊断 |
| Plugin | `source/Plugin/` | 6 | VST3 扫描/加载/卸载/editor、流程编排（PluginFlowSupport、PluginOperationController） |
| Input | `source/Input/` | 2 | 电脑键盘到 MIDI 映射 |
| Midi | `source/Midi/` | 3 | 16 通道矩阵路由（ChannelMatrix、MidiChannelMapper） |
| Recording | `source/Recording/` | 18 | 录制/回放引擎、MIDI 导入/导出、PerformanceFile、WAV 导出、离线渲染、RenderPipeline、会话控制 |
| Layout | `source/Layout/` | 4 | Performance Preset CRUD、PresetFlowSupport |
| Settings | `source/Settings/` | 10 | 设置持久化、序列化、AppStateBuilder、窗口管理 |
| Export | `source/Export/` | 5 | WAV 导出任务、导出流程支持 |
| Diagnostics | `source/Diagnostics/` | 5 | DP_LOG 宏、DevPianoLogger、MIDI trace |
| Locale | `source/Locale/` | 2 | 中文本地化、LocaleManager |
| tests | `source/tests/` | 11 | 58 个测试类 / 180 个子测试 + TestRunner |

不包括：

- `submodules/JUCE/`、`submodules/JIVE/`、`submodules/melatonin_inspector/`（禁止修改，不审）
- `scripts/`、`docs/`、构建脚本与配置文件
- 第三方依赖（JUCE 框架本身；仅用于交叉验证 API 行为）

### 1.2 审计输入

| 类型 | 路径 / 命令 |
| --- | --- |
| 代码 | `source/**/*.cpp` `source/**/*.h`（104 文件） |
| 测试 | `source/tests/*.cpp`（11 文件，58 类 / 180 子测试） |
| 架构文档 | `docs/reference/architecture.md` |
| 项目定位 | `docs/reference/project-scope.md` |
| 路线图 | `docs/roadmap/roadmap.md` + `docs/roadmap/current-iteration.md` |
| 决策记录 | `docs/decisions/` ADR-001~006 |
| 已知问题 | `docs/issues/known-issues.md` |
| 构建系统 | `CMakeLists.txt`、`.clang-tidy`、`.clang-format` |
| 构建验证 | `./scripts/dev.sh wsl-build`（通过） |
| 测试验证 | `./scripts/dev.sh test`（通过，127.77s） |
| 格式化检查 | `./scripts/dev.sh format --check`（**失败**，18 处违规） |
| Windows 验证 | `./scripts/dev.sh win-build`（通过，MSVC 镜像树） |
| 环境自检 | `./scripts/dev.sh self-check`（通过） |
| 静态分析 | `clang-tidy-21 -p build-wsl-clang`（采样 4 文件，~33 条诊断，ENG-002） |

### 1.3 严重级别定义

| 优先级 | 定义 | 期望处理 |
| --- | --- | --- |
| P0 | 崩溃、数据损坏、音频毛刺/无声、内存泄漏、线程安全缺陷 | 立即修复，阻断开发 |
| P1 | 高概率稳定性/维护性风险，影响核心路径（演奏/录制/插件） | 当前迭代修复 |
| P2 | 中等风险，影响可维护性、模块边界、测试覆盖或协作效率 | 近期排期 |
| P3 | 低风险改进：命名一致性、注释质量、const 正确性、未使用代码 | 持续跟踪或后续优化 |

> **ADR 合规映射**：违反 ADR 决策本体 → 按上表级别定级并开 `CMPL` 问题；ADR 事实性描述被证伪 → 直接修正 ADR 原文，不开问题（见 3.10 与 5.4 排期项）。

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

- 项目类型：**桌面 GUI 应用**（JUCE 框架，VST3 插件宿主，独立可执行文件）
- 核心能力：
  1. 电脑键盘触发 MIDI note → VST3 插件发声（JIVE 声明式 UI + CustomKeyboard 虚拟键盘）
  2. VST3 插件扫描 / 加载 / 卸载 / editor 窗口（分片进度、失败列表可发现性）
  3. 可配置键位映射 + Performance Preset 系统（`.devpiano.preset` JSON）
  4. 16 通道 MIDI 矩阵路由（ChannelMatrix + MidiChannelMapper）
  5. 录制 / 回放 / MIDI 导入导出 / WAV 离线渲染（RenderPipeline 统一公共管线）
  6. 逐键个性化（颜色 + 标签）+ 调号系统
  7. 中英文运行时切换（JUCE Translation）+ 运行时样式/Token 热重载（Debug）

### 2.2 技术栈与运行环境

| 类别 | 当前值 |
| --- | --- |
| 语言 | C++20 |
| 框架 | JUCE（git submodule）+ JIVE（声明式 UI，`submodules/JIVE/`）+ melatonin_inspector（Debug 组件检查） |
| 构建系统 | CMake + Ninja（WSL/Clang 21）；MSVC 19（Windows 镜像验证，`build-win-msvc`） |
| 测试框架 | JUCE UnitTest（`devpiano_tests` 目标，58 类 / 180 子测试） |
| 格式化 | clang-format-21（WebKit 基，120 列，Attach 大括号） |
| 静态分析 | clang-tidy-21（bugprone/performance/readability/modernize/clang-analyzer） |
| 音频后端 | JUCE `AudioDeviceManager` |
| 插件格式 | VST3（主路径），JUCE `AudioPluginFormatManager` |
| 数据格式 | JSON（Preset `.devpiano.preset`、录制 `.devpiano`、style_sheets/design_tokens）+ ValueTree XML（设置） |
| 开发环境 | WSL（编辑 + compile_commands.json）+ Windows/MSVC（构建验证 + 运行测试） |

### 2.3 目录与模块边界

```text
source/
├── Main.cpp                  # 应用入口（含 Windows 焦点胶水）
├── MainComponent.cpp/.h      # 主装配层（~1143 行，含访问器 include）
├── MainComponentJiveAccessors.cpp  # JIVE 面板访问器（#include 进 MainComponent.cpp）
├── UI/                       # 28 文件：JIVE 布局/样式/Token、CustomKeyboard、对话框、LookAndFeel
├── Core/                     # 3 文件：纯数据类型（零 GUI 依赖）
├── Audio/                    # 3 文件：音频引擎、设备诊断
├── Plugin/                   # 6 文件：插件宿主 + 纯函数流程编排 + 操作控制器
├── Input/                    # 2 文件：键盘→MIDI 映射
├── Midi/                     # 3 文件：通道矩阵 + 矩阵路由
├── Recording/                # 18 文件：录制/回放/导入/导出/渲染管线/会话控制
├── Layout/                   # 4 文件：Performance Preset CRUD
├── Settings/                 # 10 文件：设置持久化、序列化、状态构建、窗口管理
├── Export/                   # 5 文件：WAV 导出任务、导出流程支持
├── Diagnostics/              # 5 文件：DP_LOG 宏、Logger、MIDI trace
├── Locale/                   # 2 文件：中文本地化
└── tests/                    # 11 文件：单元测试 + TestRunner
```

边界判断：

- 清晰边界：`Core/`（纯数据，零 GUI 依赖，3 文件已收敛）、`Midi/`（独立矩阵路由）、`Export/`（独立导出任务）、`Diagnostics/`（统一日志入口，生产代码零散落输出）
- 模糊边界：`MainComponentJiveAccessors.cpp` 以 `#include` 方式并入 `MainComponent.cpp`（33.2KB 不独立成 TU，见 QUAL-014/ENG-003）；`Recording/` 18 文件内聚性（RenderPipeline 已提取公共管线，主循环保留双份属预期）
- 高复杂度热点：`MainComponent.cpp`（1143 行）、`RecordingSessionController.cpp`（646 行）、`CustomKeyboard.cpp`（634 行）、`LayoutModel.cpp`（621 行）、`StyleCatalogTest.cpp`（1426 行测试）

---

## 3. 分领域审计结果

> 本章只记录分析结论与证据摘要；具体问题必须进入第 8 章问题总表。

### 3.1 架构与模块边界

评估项：

- `MainComponent` 是否仍然承担了不属于装配层的逻辑
- FlowSupport / Controller 拆分是否彻底，是否存在循环依赖或隐性耦合
- 头文件依赖图是否合理（`#include` 深度、传递包含、前向声明使用）
- `Core/` 类型是否真正零业务逻辑、零 JUCE GUI 依赖

- 评级：`B`
- 结论：Phase 5/11 收敛成果保持良好——`PluginFlowSupport` 纯函数命名空间无成员变量（ADR-003 合规），`RecordingSessionController` / `PluginOperationController` / `SettingsWindowManager` / `AppStateBuilder` 职责边界清晰，`Core/` 收缩为 3 个纯数据类型文件且无 GUI 依赖，JIVE 迁移后 `MainComponent::resized()` 收敛为 3 行。主要遗留：(1) `MainComponentJiveAccessors.cpp` 以 `#include` 并入主文件、不独立成 TU（QUAL-014，P3）；(2) `AppStateBuilder.h` 中 `InputState::layoutId` 死字段被错误地以 `lastActivePresetId` 填充（QUAL-001，P2，潜伏误导）；(3) `MidiChannelMapper::applyTransform` 为无调用方死代码（已确认，此处不再重复开）。`MainComponent` 1143 行仍属偏高但结构已收敛（主要为 JIVE 接线与访问器转发）。
- 关联问题：`QUAL-001`、`QUAL-014`、`QUAL-013`（preset 死配置字段）、`QUAL-016`（test-only API 面）

### 3.2 代码质量与可维护性

评估项：

- RAII 与资源生命周期管理（`std::unique_ptr`、JUCE `OwnedArray`、文件句柄、插件实例）
- const 正确性（函数参数、成员函数、局部变量）
- 命名一致性（与 `docs/reference/architecture.md` 中约定是否一致）
- 注释质量（关键路径是否有意图说明，是否存在过期注释；注释遵循简体中文规范）
- 死代码 / 未使用函数 / 遗留 TODO
- 重复代码模式

- 评级：`B`
- 结论：RAII 与资源生命周期整体优秀（临时文件原子写、流 RAII、插件实例 unique_ptr 管理、析构链验证）；全仓库零 TODO/FIXME/HACK；命名与 `DP_LOG_*` 惯例一致。发现 18 项 QUAL 问题：1 项 P2（layoutId 误赋值死字段），其余 P3——包括死返回值（`stopInternalPlayback` 7 处丢弃 + 每次拷贝整个事件向量，QUAL-002）、未使用 include（QUAL-003）、跨文件重复装配（ComboBox / KeyboardSettings / PresetDialogs，QUAL-004/006/007）、误导性日志（`chooseNoteRichTrack` 声称"instead of preferred track"但偏好未实现，QUAL-009）、`MainComponent.cpp` 以 `#include` 包含 33.2KB 的 `.cpp`（QUAL-014）、`CustomKeyboard.h` 残留 Phase 6 开发步骤过期注释（QUAL-017）、怪异 lambda 初始化（QUAL-018）等。已暂缓：QUAL-019（layout 生成函数镜像重复）、QUAL-020（findByKeyCode 裸指针）、QUAL-021（getMidiCollector/getKeyboardState 可变引用）。
- 关联问题：`QUAL-001`~`QUAL-021`

### 3.3 线程安全与并发

评估项：

- JUCE `MessageManager` / `MessageThread` 正确使用：UI 操作是否仅在消息线程
- 音频回调（`AudioEngine::audioDeviceIOCallback`）是否实时安全（无锁、无分配、无 I/O）
- `std::atomic` / `CriticalSection` 使用是否正确
- 插件回调线程与 UI 线程之间的数据竞争风险

- 评级：`B-`
- 结论：线程加固保持有效——RecordingEngine 原子字段（state/currentPositionSamples/droppedEventCount/playback*）、`pendingPresetEvents` 消息线程队列、`presetChangeLock` CriticalSection、设备重建 guard（`runPluginActionWithAudioDeviceRebuild`：shutdown → action → restart）作为唯一同步点、PluginHost 线程契约（jassert + 头文件文档）均正确。**新增 1 处 P1 数据竞争**：`AudioEngine::masterGain`（AudioEngine.h:63）为普通 float，消息线程 `setMasterGain`（:231）写、音频回调尾部 `applyGain`（:208）每个 block 读——currentSampleRate/BlockSize 项（THR-002 已暂缓）仅覆盖采样率/块大小，masterGain 未被覆盖。另 2 处实时线程日志 I/O（ERR-001/002/003，见 3.6）。交叉验证：`playbackTake` 替换发生在设备暂停窗口内（startInternalPlayback 走 rebuild guard），无 use-after-free；`smoothedPitchBend` 音频线程独占访问经 isPlaying() 原子 happens-before 保护。已暂缓：THR-002（currentSampleRate/currentBlockSize 非原子，触发面仅 warmup 路径）、THR-003（MidiChannelMapper 引用成员悬垂，引用对象为 appSettings 长寿命成员）、THR-004（PluginHost::getInstance 裸指针，生命周期由设备重建外部协调 + 头文件契约）。
- 关联问题：`THR-001`、`THR-002`、`THR-003`、`THR-004`、`ERR-001`、`ERR-002`、`ERR-003`

### 3.4 安全边界

检查项：

| 检查项 | 评估 |
| --- | --- |
| 文件系统边界：文件读写路径校验（Preset 加载、MIDI 导入、设置文件） | 通过 — 存在性/空文件检查完整；临时文件原子写已覆盖 PerformanceFile 与 savePreset（AUDIT-SEC-004） |
| 插件加载安全：DLL/so 加载前校验、路径规范化 | 通过 — `normalisePluginScanPath` / `isUsablePluginScanPath` 校验扫描路径；加载走 JUCE format manager |
| 用户输入消毒：键位绑定配置解析 | 部分 — `sanitisePresetFileName` 过滤非法字符；`varToKeyBinding` 的 keyCode 无范围校验但仅影响匹配（无越界索引） |
| JSON 解析健壮性：Preset/录制/设置文件损坏或版本不兼容处理 | 通过 — 全部解析点有格式/版本校验，损坏返回 nullopt 不崩溃；交叉验证本版本 JUCE `JSON::parse` 内部自捕异常（juce_JSON.cpp:552-559），解析失败返回空 var |
| MIDI 消息有效性：note 0–127、channel 0–15、velocity 钳制 | 通过 — `MidiNoteNumber`/`MidiChannel`/`Velocity` 强类型 + clamp；`PerChannelConfig` 位域（outputChannel:4 等）天然防越界；矩阵变换 note 经 `jlimit(0,127)` |
| 缓冲区溢出：MIDI 数据数组访问、键盘状态数组边界 | 通过 — 全项目索引均带 clamp/min（channel 数组 [0,15]、labels/colours [0,128) 等） |
| 数值安全：类型转换、整数溢出、浮点精度 | 通过 — 时间戳 int64 转换带防御（scaleTimestamp 负值归零）；未发现截断导致越界的路径 |

- 结论：安全边界整体通过，无新增 SEC 问题。已暂缓维持原状（重开条件见第 8 章）：SEC-001（configForChannel 静默 clamp，调用方均传合法 0-15）、SEC-002（MidiFileImporter 无文件大小上限，本地桌面应用威胁面有限）、SEC-003（KeyMapTypes aggregate 初始化绕过 fromClamped，调用点均经 fromClamped 构造）、SEC-004（KeyboardMidiMapper 0/1-based 通道手工转换，当前路径正确无缺陷报告）。
- 关联问题：无新增（引用已暂缓项 SEC-001/002/003/004）

### 3.5 资源与性能

评估项：

- 实时音频路径的内存分配（`malloc/new` 在 audio callback 中）
- 插件实例生命周期管理（加载/卸载泄漏、editor 窗口泄漏）
- 数组/容器默认大小与增长策略（`std::vector`、`std::array`、`juce::Array`）
- MIDI 事件缓冲区上限
- 大文件处理（MIDI 文件导入、WAV 导出）的内存峰值

- 评级：`A-`
- 结论：实时路径内存纪律保持良好——`pluginBuffer` 在 prepareToPlay 预分配 32 通道（回调内仅 jassert + Release 安全网），`midiBuffer.ensureSize` 预分配，RecordingEngine 录制预 reserve 容量 + 丢弃计数，`renderPlaybackBlock` 无分配（preset 队列预 reserve）。`WavExportTask` 取消/失败路径清理已修复。无新 RES/PERF 问题；已暂缓维持原状（重开条件见第 8 章）：PERF-001（MIDI 导入全量内存加载，已缓解——现仅导入单轨道，整文件仍全量读入）、PERF-002（SettingsModel view getter 按值返回 ~4KB，调用方仅 2 处低频）、PERF-003（KeyboardTypes 固定数组，持久化侧已稀疏化）、PERF-004（isKeyCurrentlyDown O(n) 轮询，36 次/帧开销可忽略）。
- 关联问题：无新增（引用已暂缓项 PERF-001/002/003/004）

### 3.6 错误处理与可观测性

评估项：

- 异常安全性 vs JUCE 的无异常约定
- 错误传播路径：返回值 → Logger → 用户通知 是否完整
- `DevPianoLogger` 使用覆盖率（是否存在散落 `std::cout` / `DBG()`）
- 静默失败点（忽略返回值、吞异常、空 catch）

- 评级：`B-`
- 结论：日志基础设施优秀（生产代码 0 处 std::cout/printf/DBG 直用，全部走 DP_LOG_* → juce::Logger）。但存在 3 处实时线程日志 I/O（ERR-001 P1：`advancePlaybackPosition` 播放结束 `DP_LOG_INFO` 在音频线程执行，每次回放结束必触发；ERR-002 P2：回调内安全网 `DP_LOG_WARN`；ERR-003 P3：录制丢弃 `DP_DEBUG_LOG`）——`juce::Logger::writeToLog` 含互斥锁 + 潜在磁盘 I/O，实时线程阻塞/毛刺风险。另发现：插件加载失败仍按成功提交恢复状态并持久化失败插件名（ERR-004 P2）；SettingsStore 落盘失败全链路静默（ERR-005 P2，`save()` 为 void 无返回值）；启动加载 style_sheets/design_tokens 无 isVoid 校验与错误日志（ERR-006 P2）；WavFileExporter 全文件零日志（ERR-008 P2）；3 处 `catch(...)` 在已确认不抛异常的 `JSON::parse` 外是死代码（ERR-007 P3）；WavExportTask 失败残留部分文件（ERR-009 P3）；`addType` 返回值忽略（ERR-010 P3）；PresetFlowSupport rename/delete 失败仍打成功日志（ERR-011 P3）；导出后台线程无异常防护（ERR-015 P3）。
- 关联问题：`ERR-001`~`ERR-015`；已暂缓：ERR-016（AppStateBuilder 仅 jassert 线程守卫，Release 为 no-op，全部调用方为消息线程）、ERR-017（SettingsStore scheduleSave 裸指针 API，调用方均传长寿命 appSettings，无实际悬垂）。

### 3.7 测试体系

评估项：

- 现有测试覆盖的核心行为（`KeyMapTypesTest`、`MidiFileImporterTest` 等）
- 缺少测试的关键模块（音频引擎、录制引擎、插件宿主、键盘映射运行时）
- 测试可维护性（辅助函数复用、fixture 管理、magic number）
- 测试是否独立（不依赖全局状态、不依赖音频设备、不依赖文件系统副作用）
- 测试是否接入 `devpiano_tests` 目标并随 `./scripts/dev.sh test` 运行

- 评级：`C+`
- 结论：测试体系有真实且断言强度不错的覆盖（58 类 / 180 子测试，11 文件全部接入 `devpiano_tests`；StyleCatalogTest 为行为级回归而非快照测试；AudioEngineTest 全用内存 AudioBuffer 无设备依赖）。但 3 个 P1 覆盖空洞：RecordingSessionController（646 行全流程状态机）、MidiChannelMapper（通道路由/移调核心）、PerformancePreset（序列化 round-trip）**零测试**。另有 P2 问题：PerformanceFileTest 注册在 `Files` 类别被 TestRunner 默认跳过（TEST-010，.devpiano 持久化回归在默认/CI 运行中从不执行）；TestRunner 空匹配返回成功（TEST-011，拼错类别名 CI 全绿 0 测试）；类别命名三套并存且 known-issues 建议的 `--category "DevPiano"` 实际匹配 0 个项目测试（TEST-012）；AudioEngineTest 未喂入任何音符，warmup/静音断言无区分力（TEST-008）；断言空洞 `expect(true)` 多处（TEST-016）；恒真断言（TEST-017）；hasTake jassert 用例（TEST-018，修正：jassert 仅在调试器下中断，CI 无碍，属 API 契约缺陷的固化）。StyleCatalogTest 依赖进程级单例与跨文件执行顺序（TEST-013）。
- 关联问题：`TEST-001`~`TEST-020`

### 3.8 文档与配置契约

评估项：

- `docs/reference/architecture.md` 与源码模块拆分一致性
- 配置默认值漂移（`SettingsModel` 默认值与 `AppState` 初始值是否一致）
- 头文件注释与实现是否同步
- Locale 表与代码内字符串一致性（中英运行时切换无缺漏）

- 评级：`B`
- 结论：配置默认值零漂移（SettingsModel↔AppState 的 sampleRate/ADSR/masterGain/midiTranspose/keySignature 全部一致；style_sheets.json 样式键与 LayoutModel 节点一一对应；design_tokens.json 与 DesignTokens.cpp 内建回退一致；源码无硬编码中文 UI 字符串）。主要缺口：architecture.md 缺 Recording/Export/Layout/Diagnostics 四个模块章节且 Plugin 章节自相矛盾（DOC-002/003 P2）；WAV 导出进度对话框 5 个用户可见字符串走 TRANS() 但 zh_CN.loc.h 缺失译文（DOC-004 P2，中文模式下显示英文）；zh_CN.loc.h 13 个死键（DOC-005 P3）；MainComponent 行数描述漂移（DOC-001 P3）；SettingsModel 双处声明默认值（DOC-006 P3）；style_sheets.json 硬编码色值与 design_tokens.json 双事实源（DOC-007 P3）；LocaleManager.h 注释失准（DOC-008 P3）。
- 关联问题：`DOC-001`~`DOC-008`

### 3.9 工程化与构建

评估项：

- CMakeLists.txt 源文件列表完整性（是否存在未参与构建的孤立文件）
- clang-tidy 诊断清零状态
- clang-format 合规性
- 编译器警告清零状态（`-Wall -Wextra`）
- Debug / Release 构建一致性
- git 纪律：提交规范符合 AGENTS.md §4

- 评级：`B-`
- 结论：编译器 0 warning（增量构建 + 关键 TU 全新前端编译复核）；git 纪律良好（最近 20 提交全部符合 Conventional Commits）；CMake 源列表 102 项全部存在、11 个测试文件全覆盖。**回归项**：format --check 门禁被 2026-08-16 三项修复提交引入的 18 处违规重新击穿（ENG-001 P2，6 文件，其中 RenderPipeline 相关 16 处）；clang-tidy 首次实跑发现 ~33 条诊断含 1 条 bugprone 正确性风险（ENG-002 P2，`MidiChannelMapper` 构造参数 bool/int 可隐式互换）；`MainComponentJiveAccessors.cpp`（33.2KB）与 `ComboSelection.h` 不在任何 target_sources（ENG-003/004 P3）；devpiano_tests 无警告开关（ENG-005 P3）；clang-tidy GLOB 无 CONFIGURE_DEPENDS 且同一 TU 重复分析（ENG-006 P3）；`.clang-tidy` 死配置（ENG-007 P3）。
- 关联问题：`ENG-001`~`ENG-007`

### 3.10 ADR 合规审计

> 每个 ADR 逐条核对，合规状态枚举 `合规 / 部分合规 / 违反`。事实性描述被证伪 → 修正 ADR 原文（不开问题）；实现违反决策本体 → 开 `CMPL` 问题。

| ADR | 决策要点（一句话） | 审计证据（可执行检查） | 合规状态 |
| :--- | :--- | :--- | :--- |
| ADR-001 | WSL 主工作树 + Windows 镜像树 + MSVC 验证，Windows 不跨边界长期构建 | `./scripts/dev.sh win-build` 实测通过：pwsh → 镜像 `G:\source\projects\devpiano` → `windows-msvc-debug` preset → MSVC 构建成功（27.58s）；`source/` 改动仅发生在 WSL 主工作树（git 状态干净，构建产物全部在 build-*/ 与镜像树） | 合规 |
| ADR-002 | 旧 FreePiano 源码仅作迁移参考（已废止） | `freepiano-src/` 已移除；grep `freepiano\|FreePiano\|FPM\|\.fpm` 对 `source/` 零命中 | 合规 |
| ADR-003 | `PluginFlowSupport` 保持纯函数命名空间，不持成员变量 | `source/Plugin/PluginFlowSupport.h` 全文为自由函数声明（restorePluginsAtPath/tryRestoreCachedPluginList/buildStartupPluginRestorePlan 等 10 个函数），无任何成员变量；依赖经参数与 callback 显式注入 | 合规 |
| ADR-004 | JUCE `AudioDeviceManager` 作为音频设备管理主路径 | grep `WASAPI\|DirectSound\|ASIO\|XAudio` 对 `source/` 零命中；`Main.cpp` 的 `windows.h`/WNDPROC/IME hook 均为 UI 焦点与输入层平台胶水（`DevPianoWndProc` WM_SETFOCUS/WM_ACTIVATE 调度、`suppressImeForPeer`），非音频设备后端封装 | 合规 |
| ADR-005 | JUCE `AudioPluginFormatManager`/`AudioPluginInstance` 宿主，VST3 主路径 | grep `AEffect\|dispatch(\|VST2\|vst2\|steinberg` 对 `source/` 零命中；`PluginHost` 注册 `VST3PluginFormat`、`createPluginInstance()`/`prepareToPlay()`/`processBlock()` 全走 JUCE 抽象 | 合规 |
| ADR-006 | 移除外部 MIDI 设备支持，聚焦电脑键盘演奏 | grep `MidiInput\|MidiRouter\|externalMidi\|MidiInputCallback` 对 `source/` 零命中；`RecordingEventSource` 枚举仅 computerKeyboard/realtimeMidiBuffer/playback 三值 | 合规 |

- 评级：`A`
- 结论：6/6 ADR 决策本体全部合规，无 `CMPL` 问题。发现 2 处 **ADR 事实性描述被证伪**（引用链接失效，属修正原文范畴，不开问题，排期见 5.4）：ADR-001 引用的 `../development/wsl-windows-msvc-workflow.md` 与 `../getting-started/quickstart.md` 实际路径为 `docs/guides/wsl-windows-msvc-workflow.md` 与 `docs/guides/quickstart.md`；ADR-002 引用的 `../architecture/overview.md` 实际为 `docs/reference/architecture.md`。
- 关联问题：无 `CMPL`；ADR-001/002 链接修正排期见 5.4

---

## 4. 验证记录

### 4.1 命令执行结果

| 命令 | 结果 | 说明 |
| --- | --- | --- |
| `./scripts/dev.sh self-check` | 通过 | 环境自检全绿；WIN_MIRROR_DIR 未设（默认 G:\source\projects\devpiano 可见）、pwsh 自动探测成功 |
| `./scripts/dev.sh wsl-build` | 通过 | Debug 构建（linux-clang-debug preset），`ninja: no work to do.` 增量无工作，0 warning |
| `./scripts/dev.sh test` | 通过 | `devpiano_tests` 1/1 通过（127.77s），100% tests passed |
| `./scripts/dev.sh format --check` | **失败** | 18 处 clang-format 违规 / 6 文件（exit code 123），详见 ENG-001 |
| `clang-tidy-21 -p build-wsl-clang` | 未全量 | 采样 4 文件（KeyboardMidiMapper/MidiChannelMapper/RenderPipeline/StyleCatalog）得 ~33 条诊断，含 1 条 bugprone（详见 ENG-002）；全量运行排期 |
| `./scripts/dev.sh win-build` | 通过 | Windows/MSVC 镜像树验证成功（27.58s）：镜像 `G:\source\projects\devpiano`，`windows-msvc-debug` preset，MSVC 构建 0 error |
| LSP diagnostics | 通过 | clangd 实时诊断 0 issue（编译数据库由 wsl-build 刷新） |

### 4.2 文件统计

| 指标 | 值 |
| --- | --- |
| 源文件总数（`.cpp`） | 50 |
| 头文件总数（`.h`） | 54 |
| 总代码行数 | 18,577 |
| 测试用例数 | 58 个测试类 / 180 个子测试（11 个测试文件） |
| 最大文件 | `source/tests/StyleCatalogTest.cpp`（1426 行） |
| 生产最大文件 | `source/MainComponent.cpp`（1143 行） |

### 4.3 未执行验证说明

- `clang-tidy` 全量：采样 4 文件已发现 ~33 条诊断（ENG-002），全量运行与批量修复排入 P2 路线图；本报告以采样结果为证据基线。
- Windows 侧 GUI 手动验证（插件加载/导出对比等）：审计环境无法操作 Windows GUI，标 `未执行`；离线渲染导出对比验证（预期 ≤1 sample 差异）继续挂起，待用户手动验证。

---

## 5. 修复路线图

> 排期覆盖第 8 章全部 69 项未处理问题（ID 覆盖率校验见 6.5）。修复实施另开迭代，本报告只排期。

### 5.1 立即处理（P0）

- （无 P0 问题）

### 5.2 当前迭代处理（P1）

- [x] `THR-001`：`AudioEngine::masterGain` 改为 `std::atomic<float>`（或经设备重建 guard 同步写），消除音频回调/消息线程数据竞争。（已落地：`std::atomic<float>` + relaxed load/store，2026-08-16 复审 3）
- [x] `ERR-001`：`advancePlaybackPosition` 播放结束日志移出音频线程——实时线程仅置 `playbackEndedPending` 原子标志，日志移至 `RecordingSessionController::checkPlaybackEnded()`（消息线程）或预构建字符串。（已落地：日志移至 checkPlaybackEnded，字段全部原子读取，2026-08-16 复审 3）
- [ ] `TEST-001`：补 `RecordingSessionControllerTest`——覆盖 `getLastMidiExportDirectory`（.cpp:52-64）、`toRecordingFlowState`/`makeRecordingFlowStatus` 组合、`RecordingSession::isRecording/isPlaying` paused 语义（.h:36-50）、`replaceTakeAndStartPlayback` 流程（FileChooser 注入桩）。
- [ ] `TEST-002`：补 `MidiChannelMapperTest`——matrix.active=false 透传、`applyMatrixToNoteOn/Off` 通道选择、transpose 边界钳制（note+keySignature 越界）、followKey+midiTranspose 组合、noteOn/Off 对称变换。
- [ ] `TEST-003`：补 `PerformancePresetTest`——save→load round-trip（含 128 项 customKeyLabels/Colours）、`sanitisePresetFileName` 特殊字符、损坏文件返回 nullopt、formatVersion 校验。

### 5.3 近期排期（P2）

- [x] `ENG-001`：运行 `./scripts/dev.sh format` 修复 18 处违规（RenderPipeline 相关 16 处 + AudioDeviceDiagnostics.h + PerformanceFileTest.cpp），复核 `format --check` 归零；将 format 检查接入 pre-commit/CI 防再回归。（已落地：修复归零 + `.githooks/pre-commit`，2026-08-16 复审 4）
- [ ] `ERR-004`：`loadPluginByNameAndCommitState` 检查加载返回值——失败时走 `finishPluginUiAction(false)` 且不持久化失败插件名；`restorePluginByNameOnStartup` 同模式处理。
- [ ] `ERR-002`：`getNextAudioBlock` 安全网分支移除 `DP_LOG_WARN`，改为计数/标志由消息线程输出。
- [ ] `ERR-005`：`SettingsStore::save()/writeNow()` 返回 `bool`/`juce::Result`，失败时 `DP_LOG_ERROR`（含文件路径）。
- [ ] `ERR-006`：`initialiseUi`/`reloadStylesAndTokens` 的 `JSON::parse` 结果加 `isVoid()` 校验 + 失败 `DP_LOG_ERROR`。
- [ ] `ERR-008`：`WavFileExporter` 各失败分支补 `DP_LOG_ERROR`（对齐 PluginOfflineRenderer 日志粒度）。
- [ ] `QUAL-001`：删除 `InputState::layoutId` 死字段与 `AppStateBuilder.h:83` 的 `lastActivePresetId` 误赋值。
- [ ] `TEST-004`：补 SettingsStore 测试（临时 PropertiesFile round-trip + scheduleSave 合并语义）。
- [ ] `TEST-005`：补导出链纯逻辑测试（`buildWavExportOptions` 组合、`canExportTake` 边界、WAV/MIDI 头 round-trip 读回验证）。
- [ ] `TEST-006`：补 PluginHost XML round-trip（createKnownPluginListXml→restore）与 PluginPanelStateBuilder 测试。
- [ ] `TEST-007`：离屏渲染测试 CustomKeyboard 命中映射/八度切换与 AdsrCurve 拖拽钳制。
- [ ] `TEST-008`：AudioEngineTest 注入 noteOn 后断言 warmup 块内静音、warmup 后非零采样（消除"本来无声"假通过）。
- [ ] `TEST-009`：补 AudioEngine 未覆盖 API（setAdsr/armPlaybackStartPreRoll 块计数纯函数/接线）。
- [ ] `TEST-010`：PerformanceFileTest 改独立类别（如 `DevPiano/Files`）或 TestRunner 增加精确文件过滤，使 .devpiano 持久化回归进入默认运行。
- [x] `TEST-011`：TestRunner 空匹配/空注册时非零退出并输出实际测试数。（已落地：`source/tests/TestRunner.cpp` 过滤分支返回 `EXIT_FAILURE` 并输出 `Running N test(s)`；另新增 `--include-juce` 默认只跑项目测试，详见第 7 章复审记录）
- [x] `TEST-012`：统一测试类别前缀（`DevPiano/Audio`、`DevPiano/Recording`、`DevPiano/UI`），补全 4 个无类别文件，同步修正 known-issues 过滤命令。（已落地：类别统一为 `DevPiano/Core|Recording|Engine|UI`——实际引擎类别用 `DevPiano/Engine` 而非排期建议的 `DevPiano/Audio`；known-issues 缓解命令已修正，详见第 7 章复审记录）
- [x] `ENG-002`：全量运行 clang-tidy 建立基线；先批量修机械项（braces/loop-convert/qualified-auto），再处理 `bugprone-easily-swappable-parameters`（MidiChannelMapper 构造参数重排或豁免）。（已落地：4 采样文件机械项清零 + MidiChannelMapper 构造 NOLINT 豁免，2026-08-16 复审 4）
- [ ] `DOC-002`：architecture.md 补 Recording/Export/Layout/Diagnostics 四模块章节（含 RenderPipeline、WavExportTask、SettingsSerialization 等新文件）。
- [ ] `DOC-003`：architecture.md Plugin 章节更新为已收敛现状（PluginFlowSupport + PluginOperationController 已落地）。
- [ ] `DOC-004`：zh_CN.loc.h 补 5 个 WAV 导出字符串译文（Exporting.../Export cancelled./Export failed during plugin/sine rendering./Export complete.）。

### 5.4 后续优化（P3）

- [ ] `ERR-003`：`recordEvent` 丢弃日志移 stopRecording()（消息线程）统一输出。
- [ ] `ERR-007`：移除 3 处死 `catch(...)`（已确认本版本 JUCE JSON::parse 不抛异常），或改用 `JSON::parse(text, result)` Result 重载并记录行:列诊断。
- [ ] `ERR-009`：`WavExportTask` 非取消失败路径删除残留目标文件（或整体改 TemporaryFile + rename）。
- [ ] `ERR-010`：`addVst3FileToKnownList` 检查 `addType` 返回值，失败项 WARN 并按实际成功数计数。
- [ ] `ERR-011`：`getPresetDirectory`/`PresetFlowSupport` 检查 createDirectory/deleteFile 返回值；rename/delete 失败不打成功日志。
- [ ] `ERR-012`：修正 `WavExportTask.cpp:45` 注释与日志不一致（或补日志）。
- [ ] `ERR-013`：测试代码改用 DP_LOG_* 宏（PathEditorReproTest/StyleCatalogTest 直用 writeToLog 处）。
- [ ] `ERR-014`：测试中 JSON::parse 结果补 isVoid 校验并 expect（PathEditorReproTest:20、StyleCatalogTest:784）。
- [ ] `ERR-015`：`WavExportTask::run()` 内包 try-catch，捕获后设置 errorMessage + DP_LOG_ERROR + 清理部分文件。
- [ ] `QUAL-002`：`stopInternalPlayback` 改返回 void，删除 7 处 ignoreUnused 与大向量拷贝。
- [ ] `QUAL-003`：删除 MainComponent.cpp:4-5 未使用 include。
- [ ] `QUAL-004`：SettingsComponent.h ComboBox item 装配提取 `rebuildComboItems()` 供构造器与 refreshTexts() 复用。
- [ ] `QUAL-005`：合并 SettingsComponent.h 重复过时注释与拆段配置。
- [ ] `QUAL-006`：PresetDialogs.cpp `complete()` 模式上提到 `DialogContentBase`。
- [ ] `QUAL-007`：提取 `makeKeyboardSettings(view, keySignature)` 共享函数消除跨文件重复装配。
- [ ] `QUAL-008`：PerformanceFile 提取公共 `parsePerformanceFileRoot` 复用 metadata/事件解析。
- [ ] `QUAL-009`：`chooseNoteRichTrack` 实现 preferredTrack 平局语义或删除参数与"instead of"日志。
- [ ] `QUAL-010`：删除 `applyMatrixToNoteOn/Off` 未用 `originalChannel` 参数。
- [ ] `QUAL-011`：删除 `WavExportTask.cpp:45-47` 死预检查块（保留单一取消路径）。
- [ ] `QUAL-012`：PerformancePreset 移除 keySignature/midiTranspose 死配置字段（或补应用路径）。
- [ ] `QUAL-013`：成员版 `buildCurrentAppStateSnapshot` 改名消除与 core 自由函数同名混淆。
- [x] `QUAL-014`：`MainComponentJiveAccessors.cpp` 迁移为独立 TU（加入 target_sources）或改头文件方式（与 ENG-003 联动）。（已落地：独立 TU，2026-08-16 复审 4）
- [ ] `QUAL-015`：`sourceToString` 删除冗余 `default:` 分支（枚举已穷尽）。
- [ ] `QUAL-016`：逐个决策 test-only API 面（hasDroppedEvents/getLastScanFailedFiles/setLowestVisibleNote/makeFullPianoLayout/NoteRange/isValid 系列）——接入生产或删除并清理测试。
- [ ] `QUAL-017`：删除 CustomKeyboard.h 过期 Phase 6 开发步骤注释。
- [ ] `QUAL-018`：`MainComponent.cpp` adsrCurve 怪 lambda 初始化改直接 `= nullptr`。
- [ ] `TEST-013`：StyleCatalog/DesignTokens 提供 reset()，消除跨文件执行顺序依赖。
- [ ] `TEST-014`：测试 fixture/样式文件改为 `__FILE__` 相对定位或缺失时显式 skip。
- [ ] `TEST-015`：键盘状态查询抽象为可注入谓词，消除 OS 键盘依赖。
- [ ] `TEST-016`：AudioEngineTest/PluginHostTest 的 `expect(true)` 空洞断言补可观察结果校验。
- [x] `TEST-017`：MidiFileImporter velocity-channel 恒真断言拆分为独立可证伪断言。（已落地：`expect(foundVaryingVelocity, ...)` 与 `expect(foundNonDefaultChannel, ...)` 两条独立断言，见第 7 章复审记录）
- [ ] `TEST-018`：hasTake jassert 用例改为验证 RecordingSession 副本语义（Debug/Release 双配置 CI）。
- [ ] `TEST-019`：warmup 块数 magic number 改引用生产常量/注释说明。
- [ ] `TEST-020`：TestRunner --category/--name 冲突参数报错或文档化优先级。
- [ ] `DOC-001`：architecture.md 更新 MainComponent 实际行数（1143）或改描述性表述。
- [ ] `DOC-005`：清理 zh_CN.loc.h 13 个死键。
- [ ] `DOC-006`：SettingsModel 扁平成员改持有单一 KeyboardDisplaySettingsView 实例（消除双默认值）。
- [ ] `DOC-007`：style_sheets.json 硬编码色值改引用 DesignTokens（消除双事实源）。
- [ ] `DOC-008`：修正 LocaleManager.h 头注释与实际搜索目录不符。
- [x] `ENG-003`：MainComponentJiveAccessors.cpp 纳入 target_sources 或迁移为 .h（与 QUAL-014 联动）。（已落地：独立 TU）
- [x] `ENG-004`：ComboSelection.h 补入主 target_sources。
- [x] `ENG-005`：devpiano_tests 添加与主目标一致的 `-Wall -Wextra`（MSVC /W4）。（已落地：+`juce_recommended_warning_flags`，tests 既有 6 处警告修复，0 warning）
- [x] `ENG-006`：clang-tidy GLOB 加 CONFIGURE_DEPENDS，文件列表按 compile_commands 去重。（已落地：50 唯一文件归并）
- [x] `ENG-007`：删除 .clang-tidy 死 CheckOptions（readability-magic-numbers.IgnoredValues）。
- [ ] **ADR 原文修正（不开问题）**：ADR-001 引用链接更新为 `docs/guides/wsl-windows-msvc-workflow.md` 与 `docs/guides/quickstart.md`；ADR-002 引用更新为 `docs/reference/architecture.md`。

---

## 6. 最终结论

### 6.1 当前判断

devpiano 核心运行时健康：0 项 P0（无崩溃/数据损坏/静默泄漏），既有修复（音频预分配、线程同步、模块边界、测试完善、原子写入）在 2026-08-16 基线全部保持有效，且本次实测 3/4 闸门通过（wsl-build / test / win-build）+ ADR 6/6 合规。本次审计新发现 69 项问题（5 P1 / 20 P2 / 44 P3）并登记 16 项已暂缓项，评级回落至 B：主要驱动因素是实时线程 2 处 P1（日志 I/O + masterGain 竞争）、3 个核心模块测试空洞（P1）、format 门禁回归（P2）与文档滞后（P2）。无结构性架构缺陷，全部问题均可在增量迭代内收敛。

### 6.2 是否建议继续新增功能

`有条件`：可以继续，但建议先完成 5.2 的 5 项 P1（预计 1-2 个迭代）。理由：P1 中 2 项是实时线程正确性/稳定性（长期演奏/录制场景必然触达），3 项是核心模块测试空洞（新功能叠加会放大回归风险）。P2/P3 可与功能开发并行消化。

### 6.3 是否建议先重构 / 补测试 / 补文档

- 重构：`有条件` — 不需要大规模重构；优先清理 QUAL-001（错误字段赋值）、ERR-004（失败恢复态提交）、ENG-001（格式回归）、QUAL-002（死返回值大向量拷贝）。
- 补测试：`是` — 3 个 P1 空洞（RecordingSessionController / MidiChannelMapper / PerformancePreset）与 2 个 CI 静默丢覆盖点（TEST-010/011）优先。
- 补文档：`有条件` — architecture.md 四模块章节（DOC-002/003）与 WAV 导出缺译（DOC-004）优先；P3 文档项随周期清理。

### 6.4 下一步三件事

1. ~~修复实时线程 P1 对：ERR-001（播放结束日志移消息线程）+ THR-001（masterGain 改 atomic）。~~ ✅ 已于 2026-08-16 Phase A 完成（复审 3）。
2. ~~修复格式门禁回归（ENG-001：format 批量修复 + pre-commit/CI 挂钩），恢复三闸门全绿。~~ ✅ 已于 2026-08-16 Phase B 完成（复审 4）。
3. 补 3 个 P1 纯逻辑测试（TEST-001/002/003）并修复 TEST-010/011 的 CI 静默丢覆盖。

### 6.5 ID 覆盖率校验

第 8 章登记 82 项：66 项新发现（THR-001、QUAL-001~018、ERR-001~015、TEST-001~020（其中 TEST-011/012/017 已关闭）、DOC-001~008、ENG-001~007）+ 16 项已暂缓项（THR-002~004、SEC-001~004、PERF-001~004、ERR-016~017、QUAL-019~021）+ 13 项已关闭（TEST-011/012/017、THR-001、ERR-001、ENG-001~007、QUAL-014，修复证据与复审说明见第 7 章）。第 5 章路线图覆盖全部 56 项未处理（P1×3 → 5.2、P2×16 → 5.3、P3×37 → 5.4），`comm` 校验零缺失、零多余；16 项已暂缓不排期（重开条件见第 8 章）。ADR 事实性描述修正项（2 条，非问题）在 5.4 单独排期。

---

## 7. 复审记录

### 复审 1（2026-08-16，P0 TestRunner 白名单落地）

关闭 2 项：
- `TEST-011` → `已关闭`：TestRunner 空匹配/空注册改为 `EXIT_FAILURE` 并输出 `Running N test(s)`（`source/tests/TestRunner.cpp` 过滤分支），拼错 `--category`/`--name` 不再 CI 假绿。验证：`--category NotACategory` / `--name NoSuchTest` 均 exit=1。
- `TEST-012` → `已关闭`：测试类别统一为 `DevPiano/Core|Recording|Engine|UI` 前缀——33 个无类别类（RecordingEngineTest 13、KeyboardMidiMapperTest 7、AudioEngineTest 5、PluginHostTest 8）→ `DevPiano/Engine`，8 个 `devpiano` 类（StyleCatalogTest 7、PathEditorReproTest 1）→ `DevPiano/UI`；known-issues 缓解命令已修正为新默认行为说明。落地类别名与排期建议 `DevPiano/Audio` 有差异：引擎/音频层合并为 `DevPiano/Engine`（避免 `Audio` 与 JUCE `UnitTestCategories::audio` 混淆）。

关联改动（同一 P0）：TestRunner 默认仅跑项目测试（类别白名单），JUCE 库自带内部测试（~95s，含 AudioProcessorGraph 86s）改为 `--include-juce` 显式 opt-in。默认套件耗时 101.0s → 5.9s，57 类 778 断言全绿。

### 复审 2（2026-08-16，P1–P3 测试套件精简）

关闭 1 项：

- `TEST-017` → `已关闭`：MidiFileImporterTest 的 velocity-channel 恒真断言（`foundVaryingVelocity || foundNonDefaultChannel || size>0`）拆分为两条独立可证伪断言（`expect(foundVaryingVelocity, ...)` 与 `expect(foundNonDefaultChannel, ...)`）；已核实 fixture（velocity-channel.mid 含通道 2 note-on 与力度 20/64/127），两条均独立成立。

同轮结构性精简（非审计问题项，登记备查）：测试类 58 → 34（PluginHostTest 8→1、AudioEngineTest 5→2、KeyboardMidiMapperTest 7→3、KeyMapTypesTest 9→3、MidiFileImporterTest 6→2），用例 179 → 约 149；StyleCatalogTest 提取 registerRootComponentFactory/findShippedStyleSheet/findNodeById 消除 ~200 行重复，像素扫描降采样；RecordingEngineTest 提取 countMidiBufferEvents；AudioEngineTest/PluginHostTest 删除全部 `expect(true)` 空断言（保留调用序列作崩溃守护）；PathEditorReproTest 与 StyleCatalogTest 清除调试日志。默认套件 33 类 754 断言全绿，6.0s。

### 复审 3（2026-08-16，Phase A 实时线程稳定性）

关闭 2 项：

- `THR-001` → `已关闭`：`AudioEngine::masterGain` 改 `std::atomic<float>`（`AudioEngine.h:63`），写路径 `setMasterGain` 改 `store(..., std::memory_order_relaxed)`（`AudioEngine.cpp:232`），音频回调读路径改 `load(std::memory_order_relaxed)`（`AudioEngine.cpp:209`）。验证：wsl-build / test / win-build 三闸门通过。
- `ERR-001` → `已关闭`：`advancePlaybackPosition` 删除音频线程 `DP_LOG_INFO`（原 `RecordingEngine.cpp:338-340`），实时线程仅置 `playbackEndedPending` 原子标志；日志移至 `RecordingSessionController::checkPlaybackEnded()`（消息线程，`RecordingSessionController.cpp:415-417`），字段全部原子读取（`getPlaybackPositionSamples`/`getPlaybackSpeedMultiplier`）。验证：wsl-build / test / win-build 三闸门通过。

关联加固（同类实时线程竞争，同轮顺带修复）：`playbackSampleRateRatio`（普通 double，消息线程写 / 音频线程 `renderPlaybackBlock` 读）改 `std::atomic<double>` + relaxed load/store（`RecordingEngine.h:110`、`RecordingEngine.cpp` 4 处读写点）。默认套件 33 类 754 断言全绿，6.0s。

### 复审 4（2026-08-16，Phase B 工程化门禁与构建修复）

关闭 8 项：

- `ENG-001` → `已关闭`：18 处格式违规（PluginOfflineRenderer 6 / WavFileExporter 5 / RenderPipelineTest 4 / AudioDeviceDiagnostics.h / RenderPipeline.h / PerformanceFileTest.cpp）经 clang-format 修复，`format --check` 归零；新增 `.githooks/pre-commit`（仅查暂存 source 文件）+ `git config core.hooksPath .githooks` 防再回归。验证：`format --check` 0 违规。
- `ENG-002` → `已关闭`：clang-tidy 机械项批量修复——KeyboardMidiMapper braces×12、MidiChannelMapper braces×3、RenderPipeline modernize-use-ranges、StyleCatalog braces+loop-convert+qualified-auto（`--fix` 自动应用 + clang-format 复核），采样文件残留 0；`bugprone-easily-swappable-parameters`（MidiChannelMapper 构造 bool/int 引用相邻）因重排无法消除相邻性（int/bool 无论顺序都可隐式互换）、调用点仅 MainComponent 两处命名实参，采用 NOLINT 豁免并注释。验证：clang-tidy 采样文件 0 诊断、wsl-build 0 warning。
- `ENG-003` + `QUAL-014` → `已关闭`：`MainComponentJiveAccessors.cpp` 独立 TU 化——补 `#include "MainComponent.h"` + Diagnostics/Log.h + AdsrCurveComponent.h，删除 `MainComponent.cpp` 的 `#include "MainComponentJiveAccessors.cpp"`，纳入 `target_sources`（compile_commands 确认入列）。验证：wsl-build / test / win-build 全通过。
- `ENG-004` → `已关闭`：`ComboSelection.h` 补入主 target_sources（UI 段）。
- `ENG-005` → `已关闭`：devpiano_tests 对齐主目标编译选项（Clang `-Wall -Wextra`、MSVC `/W4`）+ 链接 `juce_recommended_warning_flags`；暴露的 6 处既有警告全部修复（KeyMapTypesTest char 窄化、MidiFileImporterTest importFixture 缺 static、RecordingEngineTest unused m、StyleCatalogTest 2×unused lambda capture + unused 参数，helper 删 5 处传参）。项目代码 0 warning。验证：wsl-build 0 warning、test 全绿。
- `ENG-006` → `已关闭`：clang-tidy GLOB 加 `CONFIGURE_DEPENDS`；输入文件按 compile_commands.json 的 `file` 字段提取去重（file(STRINGS) 正则，避免 string(JSON) 逐条解析性能问题），50 唯一文件 / 15 双目标重复记录归并；compile_commands 缺失时降级 GLOB 全量。
- `ENG-007` → `已关闭`：删除 `.clang-tidy` 死 CheckOptions（readability-magic-numbers.IgnoredValues，该检查已在 Checks 排除）。

关联修复（非审计问题项）：RecordingSessionController `handlePlayClicked` switch 补全 4 个未显式枚举 case（-Wswitch-enum 由 juce_recommended_warning_flags 开启，主目标既有警告）。验证：wsl-build 0 warning（项目代码）、test 33 类全绿、win-build 通过。

---

## 8. 附录：问题总表（登记表）

> 第 8 章是唯一状态源。新增、关闭、暂缓、缓解任何问题，都必须更新本表。
> 编号规则：ID 前缀与领域见下表；编号为报告内唯一（每领域从 001 起）。已暂缓项按领域编号并保留风险接受原因与重开条件；已关闭项不登记（修复证据在代码与 git 历史）。跨报告引用格式 `AUDIT-XXX <ID>`。状态枚举 `未处理 / 处理中 / 已缓解 / 已暂缓 / 已关闭`。

| ID | 领域 | 问题标题 | 优先级 | 状态 | 来源 | 影响摘要 | 证据 | 风险接受原因 | 重开条件 | 下一步 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| QUAL-001 | 质量 | InputState::layoutId 死字段被 lastActivePresetId 误赋值 | P2 | 未处理 | 审计 | 死字段全仓库无读取点；`createPersistedAppState` 用"最后激活预设名"填充"键盘布局 id"（语义错误），随后被真实 layoutId 覆盖——潜伏误导，未来若有人读取该字段将拿到错误值 | `source/Core/AppState.h:61`（字段定义）；`source/Settings/AppStateBuilder.h:83`（`layoutId = settings.lastActivePresetId` 误赋值）、`:119`（applyRuntimeInputState 正确覆盖） | - | 有人开始读取该字段 | 删除字段与 :83 赋值 |
| QUAL-002 | 质量 | stopInternalPlayback 死返回值（7 处丢弃 + 大向量拷贝） | P3 | 未处理 | 审计 | 每次停止回放拷贝整个事件向量（最大 reserve 容量）后丢弃，纯浪费 | `source/Recording/RecordingSessionController.cpp:472-480`（返回 take + getCurrentTake），丢弃点 `:181/:199/:346/:379/:414/:549/:576` | - | 大 take 停止回放出现可感知卡顿 | 改返回 void |
| QUAL-003 | 质量 | MainComponent.cpp 未使用 include | P3 | 未处理 | 审计 | 两个 include 在本文件零符号使用，增加编译依赖 | `source/MainComponent.cpp:4-5`（PluginFlowSupport.h、SettingsSerialization.h） | - | 头文件依赖被误用 | 删除 |
| QUAL-004 | 质量 | SettingsComponent ComboBox item 装配重复 | P3 | 未处理 | 审计 | 构造器与 refreshTexts() 的 addItem×3 + addItem×12 + setSelectedId 约 20 行完全重复，语言切换时再复制 | `source/Settings/SettingsComponent.h:29-31,41-43,86-97` vs `:209-212,217-221,228-240` | - | 语言切换行为漂移 | 提取 rebuildComboItems() |
| QUAL-005 | 质量 | SettingsComponent 重复过时注释 + 拆段配置 | P3 | 未处理 | 审计 | 同一注释出现两次且"unchanged"与实际拆段配置矛盾 | `source/Settings/SettingsComponent.h:131,151`（"// Diagnostics editor + Save button (unchanged)" ×2），配置拆于 :132-133 与 :152-155 | - | 注释误导维护者 | 合并配置段并保留单条注释 |
| QUAL-006 | 质量 | PresetDialogs complete() 与按钮行布局双份重复 | P3 | 未处理 | 审计 | PresetNameContent/PresetConfirmContent 的 double-callback 防护 + SafePointer 退出 8 行逐行重复（仅差 std::move）；resized() 按钮行布局重复 | `source/UI/PresetDialogs.cpp:86-95` vs `:153-162`；按钮行 `:72-78` vs `:140-146` | - | 对话框逻辑漂移 | complete 模式上提到 DialogContentBase |
| QUAL-007 | 质量 | KeyboardSettings 装配跨文件重复 | P3 | 未处理 | 审计 | colourMode/noteDisplay/fadeSpeed/keySignature/customKeyLabels/customKeyColours 装配块在两个文件逐行重复 | `source/MainComponent.cpp:1020-1027` vs `source/Settings/SettingsWindowManager.cpp:183-189` | - | 字段增改时遗漏一处 | 提取 makeKeyboardSettings 共享函数 |
| QUAL-008 | 质量 | PerformanceFile metadata/事件解析重复 | P3 | 未处理 | 审计 | loadPerformanceFileMetadata 重复 deserialiseTakeFromJson 的 JSON 解析 + format 校验（注释自认 same guard） | `source/Recording/PerformanceFile.cpp:244-275` vs `:152-174` | - | 解析语义漂移 | 提取公共 parsePerformanceFileRoot |
| QUAL-009 | 质量 | chooseNoteRichTrack preferredTrack 未实现 + 误导日志 | P3 | 未处理 | 审计 | preferredTrack 参数被 ignoreUnused（永远选音符最多轨道），但日志宣称"auto-selected track X instead of preferred track Y"——日志与事实不符 | `source/Recording/MidiFileImporter.cpp:86-99`（ignoreUnused）、`:193-195`（误导日志） | - | 用户据日志误判导入行为 | 实现平局偏好或删除参数与文案 |
| QUAL-010 | 质量 | applyMatrixToNoteOn/Off originalChannel 冗余参数 | P3 | 未处理 | 审计 | 输出通道完全由 cfg.outputChannel 决定，originalChannel 被 ignoreUnused | `source/Midi/ChannelMatrix.h:66,80`；调用点 `source/Midi/MidiChannelMapper.cpp:29/:31` | - | 签名误导 API 使用者 | 删除参数 |
| QUAL-011 | 质量 | WavExportTask 插件路径死预检查块 | P3 | 未处理 | 审计 | threadShouldExit() 预检查块无 return，赋值立即被后续渲染覆盖，与 :51-58 else 分支重复——死分支 | `source/Export/WavExportTask.cpp:45-47` | - | 取消路径语义混乱 | 删除或补 return |
| QUAL-012 | 质量 | PerformancePreset keySignature/midiTranspose 死配置字段 | P3 | 未处理 | 审计 | 字段保存/加载 round-trip 但 applyPresetData 明确不应用（注释 intentional）——永无效果的配置，格式承载死数据 | `source/Layout/PerformancePreset.h:26-27`；`PerformancePreset.cpp:251-252,310-311`（读写）；`PresetFlowSupport.cpp:112-115`（不应用 + 注释） | - | 用户误以为预设可保存调号 | 从格式移除或补应用路径 |
| QUAL-013 | 质量 | buildCurrentAppStateSnapshot 命名冲突 | P3 | 未处理 | 审计 | 成员函数与 core 自由函数同名，检索/阅读易混淆 | `source/MainComponentJiveAccessors.cpp:638`；`source/Settings/AppStateBuilder.cpp:55` | - | 误调用不同版本 | 成员版改名 |
| QUAL-015 | 质量 | sourceToString 冗余 default 分支 | P3 | 未处理 | 审计 | switch 已穷尽 3 个枚举值，default 返回与首 case 相同——未来新增枚举被静默映射，编译器 -Wswitch 无法兜底 | `source/Recording/PerformanceFile.cpp:17-19` | - | 新增 RecordingEventSource 值被静默错映射 | 删除 default 或显式 jassertfalse |
| QUAL-016 | 质量 | test-only API 面（6 处生产零消费） | P3 | 未处理 | 审计 | hasDroppedEvents/getLastScanFailedFiles/setLowestVisibleNote/makeFullPianoLayout/NoteRange/isValid 系列仅测试使用，生产无人消费 | `source/Recording/RecordingEngine.h:47`、`source/Plugin/PluginHost.h:47`、`source/UI/CustomKeyboard.h:38`（零调用）、`source/Core/KeyMapTypes.h:172`、`source/Core/MidiTypes.h:73-84,32,55,79` | - | API 面膨胀/误导 | 逐个接入生产或删除 |
| QUAL-017 | 质量 | CustomKeyboard.h 过期开发步骤注释 | P3 | 未处理 | 审计 | "Next: Group B (channel/velocity colour modes)" 描述 Phase 6 早期状态，功能早已全部完成 | `source/UI/CustomKeyboard.h:38-41`（类注释 "Steps completed: 1-4…Next: Group B"） | - | 误导新读者 | 更新为现状描述 |
| QUAL-018 | 质量 | adsrCurve 怪异 lambda 初始化 | P3 | 未处理 | 审计 | `auto* adsrCurve = []() -> AdsrCurveComponent* { return nullptr; }();` 等价于 `= nullptr`，增加噪声 | `source/MainComponent.cpp:401` | - | 可读性 | 改直接初始化 |
| ERR-002 | 错误处理 | 音频回调安全网分支内 DP_LOG_WARN | P2 | 未处理 | 审计 | getNextAudioBlock 内 pluginBuffer 契约违约安全网触发 DP_LOG_WARN（实时线程 I/O）；触发罕见但一旦触发即毛刺 | `source/Audio/AudioEngine.cpp:188` | - | prepareToPlay 契约被违反 | 回调内仅 jassert，日志移消息线程 |
| ERR-003 | 错误处理 | recordEvent 丢弃事件时音频线程 DP_DEBUG_LOG | P3 | 未处理 | 审计 | Debug 构建下实时线程日志 I/O，恰在系统过载（容量耗尽）时加剧毛刺 | `source/Recording/RecordingEngine.cpp:148` | - | 容量耗尽场景实时性恶化 | 丢弃日志移 stopRecording() |
| ERR-004 | 错误处理 | 插件加载失败仍按成功提交恢复状态并持久化失败插件名 | P2 | 未处理 | 审计 | loadPluginByName 返回值被 ignoreUnused 后无条件 commitPluginRecoveryStateAndFinishUi(true)——失败插件名被持久化为 lastPluginName，下次启动反复尝试加载失败插件，UI 无失败反馈（与 ScoutQual 的 QUAL-012 为同一问题，合并登记于此） | `source/Plugin/PluginOperationController.cpp:163-169`（loadPluginByNameAndCommitState）、`:156-158`（restorePluginByNameOnStartup 同模式）；`juce::ignoreUnused(success)` | - | 失败插件在启动时反复加载 | 失败分支不持久化 + 用户可见错误 |
| ERR-005 | 错误处理 | SettingsStore 落盘失败全链路静默 | P2 | 未处理 | 审计 | save()/writeNow() 为 void，f.saveIfNeeded() 返回值被忽略——磁盘满/只读/权限失败无日志无反馈（对比录制保存路径均有成败记录） | `source/Settings/SettingsStore.cpp:264`（saveIfNeeded 返回忽略）；`source/Settings/SettingsStore.h`（save 为 void）；`source/MainComponent.cpp:1112`（saveSettingsNow 直接调用） | - | 设置静默丢失 | save 返回 bool/Result + DP_LOG_ERROR |
| ERR-006 | 错误处理 | 启动/热重载 JSON 解析无 isVoid 校验与错误日志 | P2 | 未处理 | 审计 | design_tokens.json/style_sheets.json 损坏时静默回退默认主题，零诊断（热重载路径 844/863 有 isVoid 检查但无日志；启动路径 277-278/299-300 无检查无日志） | `source/MainComponent.cpp:277-278,299-300`（initialiseUi）、`:843-846,862-866`（reloadStylesAndTokens） | - | 样式文件损坏无法定位 | isVoid 校验 + DP_LOG_ERROR（含路径） |
| ERR-007 | 错误处理 | 3 处 catch(...) 为死代码（本版本 JUCE JSON::parse 不抛异常） | P3 | 未处理 | 审计 | 交叉验证本地 JUCE（juce_JSON.cpp:552-559）JSON::parse 内部自捕异常，解析失败返回空 var——历史修复加入的 try-catch 从不触发；其本应提供的解析诊断缺失 | `source/Recording/PerformanceFile.cpp:156,256`、`source/Layout/PerformancePreset.cpp:204`；验证 `submodules/JUCE/modules/juce_core/json/juce_JSON.cpp:552-559` | - | 升级 JUCE 版本改变解析行为 | 移除死 catch 或改 Result 重载记行:列 |
| ERR-008 | 错误处理 | WavFileExporter 全文件零日志（静默失败） | P2 | 未处理 | 审计 | 非法参数/建目录失败/openedOk 失败/writer 为空/写盘失败全部静默 return false（对比 PluginOfflineRenderer 逐分支 DP_LOG_ERROR）——sine 导出路径失败日志无痕 | `source/Recording/WavFileExporter.cpp:115-122,127-128,138-139,185-186` | - | 导出失败无法定位 | 各失败分支补 DP_LOG_ERROR |
| ERR-009 | 错误处理 | WavExportTask 非取消失败残留目标文件 | P3 | 未处理 | 审计 | 渲染失败（非取消）不清理已创建的目标 .wav（取消路径已处理），用户收到失败但残留截断文件 | `source/Export/WavExportTask.cpp:56-57,70-71`（失败分支无清理；取消分支 :53-54/:67-68 已处理） | - | 残留文件被误导入 | 失败分支删除残留（或 TemporaryFile 原子写） |
| ERR-010 | 错误处理 | PluginHost addType 返回值忽略 | P3 | 未处理 | 审计 | knownPluginList.addType(*desc) 失败时插件静默不进列表，随后按 results.size() 计数日志可能夸大 | `source/Plugin/PluginHost.cpp:145`（addType），`:149`（按 results.size() 打日志） | - | 扫描计数失真 | 检查返回值 + 按实际成功数计数 |
| ERR-011 | 错误处理 | createDirectory/deleteFile 返回值忽略；rename/delete 失败仍打成功日志 | P3 | 未处理 | 审计 | 目录创建失败导致后续 save 莫名失败；rename/delete 失败仍输出 [Preset] renamed/deleted INFO 日志——日志与事实不符 | `source/Layout/PerformancePreset.cpp:167`（createDirectory）；`source/Layout/PresetFlowSupport.cpp:232,263`（rename/delete + 成功日志） | - | 用户被成功日志误导 | 检查返回值 + 失败不打成功日志 |
| ERR-012 | 错误处理 | WavExportTask deleteFile 忽略且注释与行为不符 | P3 | 未处理 | 审计 | :45 注释声称"best-effort; log failure below"但实际日志在另一分支——该分支失败无日志 | `source/Export/WavExportTask.cpp:45` | - | 注释误导 | 补日志或修正注释 |
| ERR-013 | 错误处理 | 测试代码直用 juce::Logger::writeToLog（绕过 DP_LOG 前缀） | P3 | 未处理 | 审计 | 生产代码 0 处直用（合规），但测试 4 处绕过宏，日志格式不统一 | `source/tests/PathEditorReproTest.cpp:68,76-77,109-110,116-117`、`source/tests/StyleCatalogTest.cpp:1082-1084` | - | 测试日志无法与生产统一过滤 | 改用 DP_LOG_* |
| ERR-014 | 错误处理 | 测试中 JSON::parse 无 isVoid 校验 | P3 | 未处理 | 审计 | style_sheets.json 解析失败时测试静默空转（不报错也不断言） | `source/tests/PathEditorReproTest.cpp:20`、`source/tests/StyleCatalogTest.cpp:784`（575/710 有校验，两处遗漏） | - | 样式文件回归测试假绿 | 参照 575 模式补校验 |
| ERR-015 | 错误处理 | WavExportTask 后台线程无应用层异常防护 | P3 | 未处理 | 审计 | 渲染异常逃逸 run() 由 JUCE 线程包装兜底（juce_Thread.cpp:114-117）——Debug 仅 jassertfalse，Release 静默吞掉：errorMessage 为空、无日志、残留部分文件 | `source/Export/WavExportTask.cpp:22-88`（run()）；验证 `submodules/JUCE/modules/juce_core/threads/juce_Thread.cpp:114-117` | - | Release 下导出失败原因不可见 | run() 内 try-catch + errorMessage + 清理 |
| TEST-001 | 测试 | RecordingSessionController 零测试覆盖 | P1 | 未处理 | 审计 | 录制/回放/导入/导出全流程状态机、async FileChooser 流程、aliveFlag_ 生命周期 646 行零测试；错误分支与 UI 同步逻辑未验证 | `source/Recording/RecordingSessionController.h:24-107`（.cpp 646 行）；grep 确认 tests/ 无引用 | - | 流程状态机回归 | 补纯逻辑测试（状态组合/paused 语义/流程编排） |
| TEST-002 | 测试 | MidiChannelMapper 零测试覆盖 | P1 | 未处理 | 审计 | 通道路由 + 移调核心逻辑零覆盖；唯一相关测试只测 nullptr 透传从未实例化 mapper；ChannelMatrix 语义下落不明 | `source/Midi/MidiChannelMapper.cpp:29-77`；`source/tests/KeyboardMidiMapperTest.cpp:296-314`（仅 nullptr 测试） | - | 矩阵路由回归 | 补纯函数级测试（透传/选择/边界/对称） |
| TEST-003 | 测试 | PerformancePreset 序列化零测试覆盖 | P1 | 未处理 | 审计 | loadPreset/savePreset/scanPresetDirectory/sanitisePresetFileName 无 round-trip 覆盖；PresetFlowSupport 全部 CRUD 依赖此格式，格式变更可静默破坏用户数据 | `source/Layout/PerformancePreset.h:31-48`；tests/ 无引用 | - | 预设格式变更静默破坏 | 补 save→load round-trip + 损坏文件用例 |
| TEST-004 | 测试 | SettingsStore 零测试（含 scheduleSave 防抖合并） | P2 | 未处理 | 审计 | load/save/scheduleSave（DebounceTimer 300ms）零测试；与已暂缓项 ERR-017（scheduleSave 裸指针 API）直接相关，合并语义从未验证 | `source/Settings/SettingsStore.h:7-18` | - | 设置丢失回归 | 临时 PropertiesFile round-trip + timer 注入 |
| TEST-005 | 测试 | 导出链零测试（WavExportTask/导出器/流程支持） | P2 | 未处理 | 审计 | WavExportTask、exportTakeAsWavFile、exportTakeAsMidiFile、buildWavExportOptions/canExportTake（纯函数）全部未测；RenderPipelineTest 未触达真实写出路径 | `source/Export/WavExportTask.h:17-35`、`source/Recording/WavFileExporter.h:11-17`、`source/Recording/MidiFileExporter.h:10-12`、`source/Export/ExportFlowSupport.h:15-24` | - | 导出格式回归 | 临时目录 round-trip + 参数组合 |
| TEST-006 | 测试 | 插件操作层/XML round-trip 零覆盖 | P2 | 未处理 | 审计 | PluginOperationController、PluginFlowSupport、PluginPanelStateBuilder、PluginOfflineRenderer 零覆盖；createKnownPluginListXml→restore 持久化路径未测 | `source/tests/PluginHostTest.cpp:31-262`（仅 fresh-host 只读查询） | - | 插件恢复逻辑回归 | 补 XML round-trip + 纯逻辑层测试 |
| TEST-007 | 测试 | CustomKeyboard/AdsrCurveComponent 零测试 | P2 | 未处理 | 审计 | 键盘 hit-test/八度范围/吸附/自定义颜色渲染逻辑无测试；StyleCatalogTest 用 stub 工厂替换真实组件 | `source/UI/CustomKeyboard.cpp`、`source/UI/native/AdsrCurveComponent.cpp`；`source/tests/StyleCatalogTest.cpp:370-374`（stub） | - | 键盘交互回归 | 离屏渲染测试真实组件 |
| TEST-008 | 测试 | AudioEngineTest 未喂音符，warmup/静音断言无区分力 | P2 | 未处理 | 审计 | WarmupTest"前两块静音"与 ReleaseResourcesTest"静音"即使删除 warmup/note-off 逻辑也通过——断言无法区分"warmup 生效"与"本来无声" | `source/tests/AudioEngineTest.cpp:186-198,243-256`；头注释 :9-12 自认豁免 synth 出声 | - | warmup 回归假绿 | 注入 noteOn 后断言 warmup 内静音/后非零 |
| TEST-009 | 测试 | AudioEngine 未覆盖 API（setAdsr/armPlaybackStartPreRoll/接线） | P2 | 未处理 | 审计 | setAdsr、armPlaybackStartPreRoll、setPluginHost/setRecordingEngine 接线、recordRealtimeMidiBufferIfNeeded/renderPlaybackEventsIfNeeded 未测；calculateWarmupBlocks 纯函数无直接测试 | `source/Audio/AudioEngine.h:27-30,36,56-57`；`AudioEngine.cpp:13-22`（块计数纯函数） | - | ADSR/接线回归 | 补块计数纯函数 + ADSR 包络采样断言 |
| TEST-010 | 测试 | PerformanceFileTest 被 Files 类别默认跳过（持久化回归从不执行） | P2 | 未处理 | 审计 | PerformanceFileTest 注册在 "Files" 类别被 TestRunner 默认 skipCategories 整体跳过——.devpiano 持久化 round-trip（AUDIT-SEC-004 回归）在默认/CI 运行中从不执行 | `source/tests/PerformanceFileTest.cpp:65`；`source/tests/TestRunner.cpp:38-39`；`docs/issues/known-issues.md:77-79` | - | 原子写回归无守护 | 独立类别或精确文件过滤 |
| TEST-013 | 测试 | 跨文件进程级单例与执行顺序依赖 | P3 | 未处理 | 审计 | StyleCatalog::get()/DesignTokens::get() 被 4 文件反复覆写；PathEditorReproTest 注释显式依赖跨文件执行顺序；改链接顺序/单文件重跑可能破坏测试 | `source/tests/StyleCatalogTest.cpp:52,138,172,932-936,1019`；`source/tests/PathEditorReproTest.cpp:92-95` | - | 测试顺序脆弱 | 提供 reset() 恢复初态 |
| TEST-014 | 测试 | 测试依赖 CWD/真实文件（fixture 与样式表） | P3 | 未处理 | 审计 | MidiFileImporterTest fixture 路径 "tests/fixtures/midi/" 依赖 CTest WORKING_DIRECTORY；PathEditorReproTest/StyleCatalogTest 加载真实 style_sheets.json（CWD 或 exe 上溯）——IDE/其他工作目录运行即红 | `source/tests/MidiFileImporterTest.cpp:15-19,22`；`PathEditorReproTest.cpp:15-17`；`StyleCatalogTest.cpp:560-568` | - | 非标准工作目录测试失败 | __FILE__ 相对定位或缺失 skip |
| TEST-015 | 测试 | NoteOffOnReleaseTest 依赖真实 OS 键盘状态 | P3 | 未处理 | 审计 | KeyPress::isKeyCurrentlyDown() 查询系统键盘——headless 下恒 false 成立，真实桌面且该键被物理按住时前提失效误报 | `source/tests/KeyboardMidiMapperTest.cpp:221-247` | - | 环境相关脆弱测试 | 抽象可注入键状态谓词 |
| TEST-016 | 测试 | 断言强度不足（只调不查 expect(true)） | P3 | 未处理 | 审计 | "gain clamps >1.0"/"requestAllNotesOff does not crash"/"re-prepare after release"/"getDefaultVst3SearchPath does not crash"/"restoreKnownPluginListFromXml null safe" 仅 expect(true) 无结果校验 | `source/tests/AudioEngineTest.cpp:130-135,152-160,231-241`；`source/tests/PluginHostTest.cpp:223-230,253-260` | - | 行为回归不可见 | 补可观察结果断言 |
| TEST-018 | 测试 | hasTake jassert 用例固化 API 契约缺陷 | P3 | 未处理 | 审计 | "hasTake jasserts during recording" 仅验证 Release 不崩、零行为断言；jassert 仅在调试器下中断（juce_PlatformDefs.h:167 实现），CI 无碍；测试把"录制中读 currentTake"的契约违反固化为预期行为 | `source/tests/RecordingEngineTest.cpp:101-113`；`source/Recording/RecordingEngine.cpp:33-39`（jassert(!isRecording())）；验证 `submodules/JUCE/modules/juce_core/system/juce_PlatformDefs.h:167` | - | Debug 调试器下测试中断 | 改验证 RecordingSession 副本语义 |
| TEST-019 | 测试 | warmup 块数 magic number 与生产常量脱节 | P3 | 未处理 | 审计 | exhaustWarmup 硬编码 5 块（生产 warmupSeconds=0.025 → 44.1k/512 下 ceil=3 块）；"前两块静音"无注释关联，生产改 warmupSeconds 后测试不感知 | `source/tests/AudioEngineTest.cpp:48,186`；`source/Audio/AudioEngine.cpp:10,20-22`（warmupSeconds） | - | 生产参数漂移 | 导出 calculateWarmupBlocks 或注释说明 |
| TEST-020 | 测试 | TestRunner CLI 语义缺口（参数优先级/空匹配） | P3 | 未处理 | 审计 | --category 与 --name 同时给出时 category 静默优先；--include-files 过滤模式下不生效无提示；help 未说明优先级与空匹配行为（配合 TEST-011 静默全绿） | `source/tests/TestRunner.cpp:56-59,64-75` | - | 参数误用 | 冲突报错或文档化优先级 |
| DOC-001 | 文档 | architecture.md MainComponent 行数漂移 | P3 | 未处理 | 审计 | 文档称约 1100 行，实际 1143 行（修正后的描述再次漂移） | `docs/reference/architecture.md:47` vs `source/MainComponent.cpp`（1143 行） | - | 行数描述持续过期 | 更新或改描述性表述 |
| DOC-002 | 文档 | architecture.md 缺 Recording/Export/Layout/Diagnostics 四模块章节 | P2 | 未处理 | 审计 | 模块分层止于 Settings/UI；Phase 11 / 渲染管线提取（2026-08-16）新增的 RenderPipeline、PerformanceFile、RecordingEngine、WavExportTask、WavExportOptions、SettingsSerialization 均未收录；文档头"更新时机"未被执行 | `docs/reference/architecture.md:74-160` vs `source/Recording/`（18 文件）、`source/Export/`（5）、`source/Layout/`（4）、`source/Diagnostics/`（5） | - | 新文件结构对读者不可见 | 补四模块章节 |
| DOC-003 | 文档 | architecture.md Plugin 章节自相矛盾 | P2 | 未处理 | 审计 | Plugin 章节"后续仍可继续拆分扫描职责与实例生命周期职责"与主装配层已落地内容矛盾（PluginFlowSupport/PluginOperationController 早已拆分收敛） | `docs/reference/architecture.md:137-138` vs `:53,:59` | - | 读者误判现状 | 更新 Plugin 章节现状 |
| DOC-004 | 文档 | WAV 导出 5 个用户可见字符串缺译 | P2 | 未处理 | 审计 | TRANS() 包装的进度对话框文案 zh_CN.loc.h 无对应键——中文模式下导出时显示英文 | `source/Export/WavExportTask.cpp:28,36,44,52,56,66,70,78`（5 个字符串）vs `source/Locale/zh_CN.loc.h`（无键） | - | 中文本地化不完整 | zh_CN.loc.h 补 5 键译文 |
| DOC-005 | 文档 | zh_CN.loc.h 13 个死键 | P3 | 未处理 | 审计 | 无任何 TRANS 引用；录音/播放状态文本与 "｜" 前缀状态行属旧 UI 遗留 | `source/Locale/zh_CN.loc.h:11,12,47,60,61,62,69,70,154,199,200,207,208`（13 键） | - | 本地化表膨胀 | 删除死键 |
| DOC-006 | 文档 | SettingsModel 键盘显示默认值双处声明 | P3 | 未处理 | 审计 | KeyboardDisplaySettingsView 与扁平成员重复定义同一组默认（当前两处一致已核对），无编译期防护，存在漂移风险 | `source/Settings/SettingsModel.h:40-48` vs `:81-85` | - | 默认值漂移 | 扁平成员持有单一 View 实例 |
| DOC-007 | 文档 | style_sheets.json 硬编码色值与 tokens 双事实源 | P3 | 未处理 | 审计 | Button background #22252C=control-bg、#preset-card #181A1F=card-bg 等硬编码与 design_tokens.json 重复；#window font-size 14 与 token font-size-default 13.0 冲突；改 token 不联动样式表 | `source/UI/jive/style_sheets.json:5,14,62-63,99,112` vs `source/UI/jive/design_tokens.json:5-12,27` | - | 主题修改需改两处 | 样式表引用 DesignTokens |
| DOC-008 | 文档 | LocaleManager.h 注释与实际搜索目录不符 | P3 | 未处理 | 审计 | 注释称 "project root" 第三个搜索目录，实际为 CWD——从其它目录启动定位不到项目根 .loc | `source/Locale/LocaleManager.h:9` vs `:14-17` | - | 注释误导 | 修正注释或按注释语义实现 |
| THR-002 | 线程安全 | AudioEngine currentSampleRate/currentBlockSize 非原子 | P2 | 已暂缓 | - | prepareToPlay（消息线程）写、getNextAudioBlock（音频线程）读，无 atomic/mutex | `source/Audio/AudioEngine.h:64-65`、`AudioEngine.cpp` | 触发面收窄：getNextAudioBlock 主路径不读，仅 warmup 路径（consumeWarmupBlockIfNeeded→discardWarmupInputState）读；x86-64 对齐 double/int 实际原子 | warmup 路径扩展读取或编译优化暴露 UB | 改 std::atomic<double>/std::atomic<int> |
| THR-003 | 线程安全 | MidiChannelMapper 引用成员悬垂风险 | P2 | 已暂缓 | - | 构造器存储 const ChannelMatrix&/const bool&/const int&，外部对象销毁后引用悬垂 | `source/Midi/MidiChannelMapper.h:22-25` | 引用对象为 MainComponent::appSettings 成员（MainComponent.h 声明先于 midiChannelMapper 构造），寿命安全 | appSettings 改为动态分配或生命周期缩短 | 文档化生命周期契约或改值拷贝 |
| THR-004 | 线程安全 | PluginHost::getInstance 暴露裸指针 | P2 | 已暂缓 | - | 返回 AudioPluginInstance* 裸指针，音频线程经它调用 processBlock，生命周期依赖外部协调 | `source/Plugin/PluginHost.h:64` | 生命周期由 runPluginActionWithAudioDeviceRebuild 外部协调 + 头文件 thread-safety contract（THR-001 修复时建立） | 引入非设备重建 guard 的插件切换路径 | 返回 juce::AudioPluginInstance::Ptr 或文档化所有权契约 |
| SEC-001 | 安全 | MidiChannelMapper::configForChannel 静默 clamp | P2 | 已暂缓 | - | 越界 channel 参数被静默 jlimit 到 [0,15]，调用方无法得知错误 | `source/Midi/MidiChannelMapper.cpp:12-14` | 实际调用方（sendNoteOn/sendNoteOff，消息线程）传入值均来自合法 0-15 通道，越界仅理论可能 | 发现调用方传越界 channel 的实际路径 | 添加 jassert 或返回 std::optional |
| SEC-002 | 安全 | MidiFileImporter 无文件大小限制 | P2 | 已暂缓 | - | 仅检查 getSize()==0，超大/恶意构造的 MIDI 文件可导致内存耗尽 | `source/Recording/MidiFileImporter.cpp`（getSize 检查） | 本地桌面应用、用户自选文件的威胁面有限 | 导入超大文件出现实测内存问题 | 添加可配置文件大小上限（如 100MB） |
| SEC-003 | 安全 | KeyMapTypes aggregate init 绕过 fromClamped | P3 | 已暂缓 | - | MidiNoteNumber{200} aggregate 初始化可绕过 clamp 保护 | `source/Core/MidiTypes.h`（aggregate struct） | 全项目调用点均经 fromClamped/helper 构造，无实际越界输入 | 新增绕过 fromClamped 的构造点 | 私有构造函数或 requires clause |
| SEC-004 | 安全 | KeyboardMidiMapper 0/1-based 通道转换脆弱 | P3 | 已暂缓 | - | channel 值在 0-based/1-based 间手工转换，缺少类型系统保护 | `source/Input/KeyboardMidiMapper.cpp`（midiChannel - 1） | 当前路径正确且无缺陷报告；MidiChannel::toZeroBased() helper 已存在可低成本收编 | 出现 0/1-based 混淆缺陷报告 | 统一用 MidiChannel::toZeroBased() |
| PERF-001 | 性能 | MidiFileImporter 全量内存加载 | P2 | 已暂缓 | - | 整文件读入 juce::MidiFile 再转换，大文件（>10min 高密度）可能数百 MB 内存 | `source/Recording/MidiFileImporter.cpp` | 已缓解：现仅导入单条最丰富轨道（chooseNoteRichTrack），不再展开全部轨道；整文件仍全量读入 | 导入大文件实测内存峰值过高 | 流式处理或事件数量上限 |
| PERF-002 | 性能 | SettingsModel view getter 按值返回 128 元素数组 | P2 | 已暂缓 | - | KeyboardDisplaySettingsView 等返回含 std::array<String,128>/std::array<Colour,128> 副本，每次调用复制 ~4KB | `source/Settings/SettingsModel.h` | 调用方仅 2 处且均为低频路径（设置同步、设置窗口打开），~4KB 拷贝影响可忽略 | 调用点增多或进入高频路径 | 返回 const& 或 std::span |
| PERF-003 | 性能 | KeyboardSettings 2KB+ 固定数组 | P3 | 已暂缓 | - | customKeyLabels/customKeyColours 固定 std::array 128 项，未自定义也占满 | `source/UI/KeyboardTypes.h` | 持久化侧已稀疏化（SettingsStore 仅存非空 label） | 大量自定义键场景内存实测过高 | 改 std::vector 或 sparse map |
| PERF-004 | 性能 | KeyboardMidiMapper::isKeyCurrentlyDown O(n) 轮询 | P3 | 已暂缓 | - | handleKeyStateChanged 每帧遍历所有 binding 查询 OS 键状态 | `source/Input/KeyboardMidiMapper.cpp` | 36 次/帧在消息线程的绝对开销可忽略（heldKeys 已为 unordered_set） | 键盘轮询改高频（音频线程）或 binding 数大增 | std::bitset 或 unordered_set 替代遍历 |
| ERR-016 | 错误处理 | AppStateBuilder 仅 jassert 线程守卫 | P2 | 已暂缓 | - | buildAppStateSnapshot 用 jassert(isMessageThread())，Release 构建为 no-op | `source/Settings/AppStateBuilder.cpp`（assertMessageThreadSnapshotAccess） | 全部调用方（MainComponent timerCallback/UI 事件）均为消息线程，实际风险低 | 新增非消息线程调用方 | jassert + 错误码或 Release 保持检查 |
| ERR-017 | 错误处理 | SettingsStore scheduleSave 裸指针 API | P2 | 已暂缓 | - | DebounceTimer 持有 const SettingsModel* 裸指针，timer 触发前对象析构则悬垂 | `source/Settings/SettingsStore.h`、`SettingsStore.cpp:275-304` | 调用方均传 MainComponent::appSettings（成员寿命长于 DebounceTimer），无实际悬垂 | 出现 SettingsModel 寿命短于 timer 的调用方 | std::shared_ptr 或文档化寿命契约 |
| QUAL-019 | 质量 | makeDefaultKeyboardLayout / makeFullPianoLayout 镜像重复 | P3 | 已暂缓 | - | 两函数 binding 构建主体镜像重复（仅 base 常量与 layout id/name 不同） | `source/Core/KeyMapTypes.h` | 已提取 makeNoteBinding helper（每 binding 降至 1 行）；P3 低风险 | 新增第三种 layout 生成函数 | 参数化 base 合并两函数 |
| QUAL-020 | 质量 | findByKeyCode 返回裸指针 | P3 | 已暂缓 | - | KeyboardLayout::findByKeyCode 返回 const KeyBinding* 指向 vector 内部，修改后悬垂 | `source/Core/KeyMapTypes.h:66` | 调用方（KeyboardMidiMapper 等）均在 vector 未变更的同一线程同一快照内使用，无实际悬垂路径 | findByKeyCode 返回后 vector 被修改的调用方出现 | 返回 std::optional<std::reference_wrapper> 或索引 |
| QUAL-021 | 质量 | AudioEngine getMidiCollector/getKeyboardState 暴露内部可变引用 | P3 | 已暂缓 | - | 两方法返回可变引用，允许外部任意修改内部 MIDI 状态 | `source/Audio/AudioEngine.h:35-38` | 两个 JUCE 类型本身线程安全（MidiKeyboardState 内置 CriticalSection、MidiMessageCollector 跨线程生产/消费设计） | 外部代码直接修改内部状态造成缺陷 | 提供 const 版本或专用受限 API |

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

> 本报告无新增 RES/ARCH/OBS/CMPL 问题：资源/架构边界经核查无新增缺陷；ADR 6/6 合规。ADR-001/002 事实性描述（引用链接）被证伪 → 修正原文（排期见 5.4），不开 CMPL。
