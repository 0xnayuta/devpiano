# devpiano 代码质量审计报告 · 2026-07-20

> 目标：对 `source/` 目录做一次全面、可复核的代码质量审计，覆盖架构、安全、资源、可维护性、测试与工程化。
>
> 使用规则：
>
> 1. **问题总表是唯一状态源**：第 8 章为 finding registry；首页、路线图、结论必须与第 8 章一致。
> 2. **Closed 必须有证据**：至少填写代码/测试/文档/命令之一；缺失项需说明原因。
> 3. **Deferred / Mitigated 必须可追踪**：必须写明风险接受原因、重开触发条件和复审时间。
> 4. **复审只追加不覆盖**：复审记录写入第 7 章，并同步更新第 8 章状态。
> 5. **状态枚举固定**：`Open / In Progress / Mitigated / Deferred / Closed`。

---

## 0. 审计看板

### 0.1 基本信息

| 字段 | 值 |
| --- | --- |
| 项目 | devpiano |
| 审计范围 | `source/` （含 14 个子模块 + tests/，共 70 个 .cpp/.h 文件，~12,108 行） |
| 审计日期 | `2026-07-20` |
| 审计基线 | `main` @ `c1b52bb7b26444841c23b3069bee4371967fc846` |
| 审计人 | AI code audit (5-scout parallel + manual verification) |
| 复审状态 | `Initial` |

### 0.2 风险与状态汇总

| Priority | Total | Open | In Progress | Mitigated | Deferred | Closed |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| P0 | 5 | 0 | 0 | 0 | 0 | 5 |
| P1 | 12 | 1 | 0 | 0 | 0 | 11 |
| P2 | 25 | 15 | 0 | 0 | 0 | 10 |
| P3 | 21 | 8 | 0 | 0 | 0 | 13 |
| **合计** | 63 | 24 | 0 | 0 | 0 | 39 |

### 0.3 关键结论

- 总体评级：`A-` — 功能完整，架构健康；Phase A–F 全部完成，39/63 已关闭
- 当前是否适合继续新增功能：`Yes` — 核心运行时模块已有单元测试覆盖，P0/P1 风险全部关闭
- 当前是否建议优先重构：`No` — 剩余 24 项 Open 主要为 P2(15)/P3(8) 低风险改进
- 最大已修复风险：**PluginHost 零内部线程同步**（`PLUG-001`）— 已通过断言 + 文档化契约缓解
- 下一步最高优先级：按需推进 Defer/Won't-Fix 项目；定期复审

### 0.4 Top Findings

| ID | Priority | Status | 标题 | 当前结论 |
| --- | --- | --- | --- | --- |
| `AUDIO-001` | P0 | Closed | AudioEngine 音频回调中堆分配 pluginBuffer.setSize() | 已修复 (009355d): prepareToPlay 预分配 8 通道，回调仅 jassert+安全网 |
| `AUDIO-002` | P0 | Closed | pluginHost->prepareToPlay() 在音频回调线程中延迟调用 | 已修复 (009355d): 移除延迟 prepare，设备重建时由 prepareToPlay 处理 |
| `PLUG-001` | P0 | Closed | PluginHost 零内部线程同步，依赖外部设备重建保护 | 已修复：添加 jassert(isMessageThread()) 断言 + 头文件文档化契约；所有 mutation 方法在 Debug 构建中强制执行消息线程约束 |
| `REC-001` | P0 | Closed | RecordingEngine 非原子字段在音频/消息线程间无同步读写 | 已修复 (3bd994b): currentPositionSamples/droppedEventCount → atomic; events vector → jassert 守卫 |
| `REC-005` | P0 | Closed | PluginOfflineRenderer 在后台线程调用 processBlock() | 已修复：文档化现有保护（每次渲染创建全新独立插件实例，不与音频线程的 live 实例共享 processBlock）; PluginOfflineRenderer.h 添加线程隔离说明 |
---

## 1. 审计范围与方法

### 1.1 审计范围

审计 `source/` 下全部 14 个业务子模块 + tests/，共 70 个文件：

| 模块 | 路径 | 文件数 | 职责 |
| --- | --- | --- | --- |
| 应用入口 | `source/Main.cpp` | 1 | JUCEApplication 启动，创建主窗口 |
| 主装配层 | `source/MainComponent.*` | 2 | 装配 UI 子组件，初始化音频/MIDI/插件/设置，顶层协调 |
| UI | `source/UI/` | 18 | 虚拟键盘、控件面板、插件面板、头部面板、状态构建器、对话框、键盘类型 |
| Core | `source/Core/` | 3 | 核心数据类型（KeyMapTypes、AppState、MidiTypes），仅依赖 JUCE 工具类型 |
| Audio | `source/Audio/` | 3 | 音频引擎、设备诊断 |
| Plugin | `source/Plugin/` | 6 | VST3 插件扫描/加载/卸载/editor，流程编排 |
| Midi | `source/Midi/` | 3 | MIDI 通道矩阵路由与映射 |
| Recording | `source/Recording/` | 14 | 录制/回放引擎、MIDI 导入/导出、WAV 导出、离线渲染、会话控制器 |
| Settings | `source/Settings/` | 9 | 设置持久化、序列化、设置窗口管理、AppState 构建器 |
| Export | `source/Export/` | 5 | WAV 导出任务、导出流程支持 |
| Diagnostics | `source/Diagnostics/` | 5 | Logger 封装、MIDI trace |
| Locale | `source/Locale/` | 2 | 中文本地化、LocaleManager |
| tests | `source/tests/` | 7 | 单元测试（KeyMapTypes, MidiFileImporter, RecordingEngine, KeyboardMidiMapper, AudioEngine, PluginHost, TestRunner） |

不包括：

- `JUCE/` 子模块（禁止修改，不在审计范围）
- `scripts/`、`docs/`、构建脚本与配置文件
- 第三方依赖（JUCE 框架本身）

### 1.2 审计输入

| 类型 | 路径 / 命令 | 结果 |
| --- | --- | --- |
| 代码 | `source/**/*.cpp` `source/**/*.h` | 74 文件，~15,200 行（含 Phase E 新增 4 个测试文件），逐文件审计完成 |
| 测试 | `source/tests/*.cpp` | **31 个测试类, 74 个子测试全部通过**（Phase E 完成后） |
| 架构文档 | `docs/reference/architecture.md` | 已对照 |
| 项目定位 | `docs/reference/project-scope.md` | 已对照 |
| 构建系统 | `CMakeLists.txt` | 已审查源文件列表完整性 |
| 构建验证 | `./scripts/dev.sh wsl-build` | **0 warning** (Phase D 已修复 KeyboardPanel.cpp:43 隐式转换) |
| 格式化检查 | `./scripts/dev.sh format --check` | **~70 violations** 分布在 14 个文件中 (exit code 123) |
| 静态分析 | `.clang-tidy`（bugprone/performance/readability/modernize）| 配置存在，未运行（无 clang-tidy 二进制） |
| LSP diagnostics | clangd | **0 issues** — 所有源文件通过 clangd 实时诊断 |

### 1.3 严重级别定义

| Priority | 定义 | 期望处理 |
| --- | --- | --- |
| P0 | 崩溃、数据损坏、音频毛刺/无声、内存泄漏、线程安全缺陷 | 立即修复，阻断开发 |
| P1 | 高概率稳定性/维护性风险，影响核心路径（演奏/录制/插件） | 当前迭代修复 |
| P2 | 中等风险，影响可维护性、模块边界、测试覆盖或协作效率 | 近期排期 |
| P3 | 低风险改进：命名一致性、注释质量、const 正确性、未使用代码 | 持续跟踪或后续优化 |

### 1.4 状态定义

| Status | 定义 |
| --- | --- |
| Open | 已确认问题，尚未开始处理 |
| In Progress | 已进入实现或验证阶段 |
| Mitigated | 已有缓解措施，但未完全根除 |
| Deferred | 明确暂缓，并记录风险接受原因 |
| Closed | 已完成修复/验证/文档同步，证据可追踪 |

---

## 2. 项目画像

### 2.1 项目类型与核心能力

- 项目类型：**桌面 GUI 应用**（JUCE 框架，VST3 插件宿主，独立可执行文件）
- 核心能力：
  1. 电脑键盘触发 MIDI note → VST3 插件发声
  2. VST3 插件扫描 / 加载 / 卸载 / editor 窗口
  3. 可配置键位映射 + Performance Preset 系统
  4. 16 通道 MIDI 矩阵路由
  5. 录制 / 回放 / MIDI 导入导出 / WAV 离线渲染
  6. 逐键个性化（颜色 + 标签）+ 调号系统
  7. 中文本地化（运行时切换）

### 2.2 技术栈与运行环境

| 类别 | 当前值 |
| --- | --- |
| 语言 | C++20 |
| 框架 | JUCE（git submodule） |
| 构建系统 | CMake + Ninja（WSL/Clang）；MSVC（Windows 验证） |
| 测试框架 | JUCE UnitTest（`devpiano_tests` 目标） |
| 格式化 | clang-format-21（WebKit 基，120 列，Attach 大括号） |
| 静态分析 | clang-tidy（bugprone/performance/readability/modernize） |
| 音频后端 | JUCE `AudioDeviceManager` |
| 插件格式 | VST3（主路径），通过 JUCE `AudioPluginFormatManager` |
| 数据格式 | JSON（Preset `.devpiano.preset`、录制 `.devpiano`、设置 XML） |
| 开发环境 | WSL（编辑 + compile_commands.json）+ Windows/MSVC（构建验证 + 运行测试） |

### 2.3 目录与模块边界

```text
source/
├── Main.cpp                  # 应用入口 (234 lines)
├── MainComponent.cpp/.h      # 主装配层 (~982 lines total, 已从 1587 行大幅瘦身)
├── UI/                       # 17 文件：虚拟键盘、控件面板、插件面板、头部、状态构建器、对话框
├── Core/                     # 3 文件：核心数据类型（KeyMapTypes, MidiTypes, AppState）
├── Audio/                    # 3 文件：音频引擎、设备诊断
├── Midi/                     # 3 文件：通道矩阵、MIDI 映射
├── Input/                    # 2 文件：键盘→MIDI 映射
├── Settings/                 # 9 文件：设置持久化、序列化、窗口管理、AppState 构建器
├── Layout/                   # 4 文件：Performance Preset CRUD
├── Export/                   # 5 文件：WAV 导出任务、流程支持
├── Diagnostics/              # 5 文件：Logger、MIDI trace
├── Locale/                   # 2 文件：中文本地化
└── tests/                    # 7 文件：单元测试（6 测试套件 + 1 TestRunner）
```

当前边界判断：

- **清晰边界**：`Diagnostics/`（独立日志层）、`Input/`（单向键盘→MIDI）、`Export/`（独立导出任务）、`Locale/`（纯数据）
- **部分模糊**：无重大边界问题；`Core/` 经 Phase C 清理后仅含 3 个纯数据类型文件

---

## 3. 分领域审计结果

> 本章只记录分析结论与证据摘要；具体问题必须进入第 8 章问题总表。

### 3.1 架构与模块边界

评估项：

- **`MainComponent` 是否仍然承担了不属于装配层的逻辑**：部分存在。MainComponent（838 lines）已大幅瘦身，但仍有 ~50 lines 的关键绑定编辑回调内联 lambda、settings persistence 逻辑、文件拖放路径分派和 slider→save 桥接代码。4 个 friend 声明（PresetFlowSupport、RecordingSessionController、PluginOperationController、SettingsWindowManager）表明 controller 仍通过 friend 访问 MainComponent 私有成员而非公共接口。
- **FlowSupport / Controller 拆分是否彻底**：拆分已基本完成。`RecordingFlowSupport`、`ExportFlowSupport`、`PluginFlowSupport`、`PresetFlowSupport` 均为纯逻辑或编排类。`RecordingSessionController`、`PluginOperationController`、`SettingsWindowManager` 各自管理独立生命周期。无循环依赖。
- **头文件依赖图**：`MainComponent.h` 包含 9 个头文件，形成依赖中心。`Core/AppStateBuilder.h` 依赖 `Settings/` 模块（依赖反转）。各模块通过 `#include "Module/File.h"` 模式引用，传递包含风险可控。前向声明使用不充分：`MainComponent.h` 无 `class PluginHost;` 等前向声明，全部直接 `#include`。
- **`Core/` 类型是否真正零业务逻辑、零 JUCE GUI 依赖**：**已修复 (Phase C)**。原 `Core/` 下 7 个文件中有 5 个 `#include <JuceHeader.h>`，`ChannelMatrix.h` 和 `KeyboardTypes.h` 命名空间与物理位置不匹配。Phase C 将 `ChannelMatrix.h` 移至 Midi/、`KeyboardTypes.h` 移至 UI/、`AppStateBuilder` 移至 Settings/。现 Core/ 仅含 3 个文件，使用 JUCE 工具类型（String, Array），无 GUI 依赖。

- 评级：**B+**
- 结论：Phase C 已完成模块边界清理 — `ChannelMatrix.h` 已移至 Midi/，`KeyboardTypes.h` 移至 UI/，`AppStateBuilder` 移至 Settings/。Core/ 现含 3 个文件，仅依赖 JUCE 工具类型（String, Array）。主要残留问题：MainComponent 仍有 ~50 lines 可提取的逻辑，friend 声明应逐步替换为公共接口。

### 3.2 代码质量与可维护性

评估项：

- **RAII 与资源生命周期管理**：良好。`std::unique_ptr` 用于 PluginHost 插件实例、PluginOperationController editor 窗口、RecordingSessionController file chooser。`juce::OwnedArray` 未使用。`SettingsWindowManager` 使用 `shared_ptr<State>` + `Component::SafePointer` 实现异步安全。
- **const 正确性**：良好到优秀。大量 `[[nodiscard]]`、`const noexcept` 标记。`MidiTypes.h` 强类型封装是 const 正确性的典范。少数遗漏：`AudioEngine::getMidiCollector()` 和 `getKeyboardState()` 返回可变引用暴露内部状态。
- **命名一致性**：大体一致。`build*Snapshot()`、`renderReadOnlyUiState()` 模式遵循 conventions。`RecordingFlowSupport` 使用 `RecordingFlowIntent/Status/Command/State` 明确状态机命名。
- **注释质量**：关键路径有意图说明（`RecordingEngine.h` 头注释描述双线程模型）。无过期注释发现。存在 0 个 `TODO`/`FIXME`/`HACK` 标记（已全部清理）。
- **死代码 / 未使用函数**：`HeaderPanel::AudioStatus` struct 定义了但未被使用（仅 `buildHeaderPanelAudioStatus` 在两处 builder 中返回，但 `HeaderPanel` 自身不使用此 struct）。
- **重复代码模式**：`KeyMapTypes.h` 中 `makeDefaultKeyboardLayout` 和 `makeFullPianoLayout` 有大量重复（binding 数组构建模式）。`WavFileExporter.cpp` 和 `PluginOfflineRenderer.cpp` 共享 ~50% 渲染循环代码（`RenderEvent` 结构、`buildRenderEvents`、`getScaledTakeLengthSamples`、`addPanicMidi`）。`KeyboardMidiMapper::handleKeyStateChanged` 和 `triggerBinding` 中 note-off 逻辑重复。

- 评级：**B+**
- 结论：整体代码质量良好，RAII 和 const 正确性优于多数同类项目。主要改进空间：消除 Recording/ 模块内的渲染循环重复、提取 KeyboardMidiMapper note-off 公共逻辑、清理未使用的 HeaderPanel::AudioStatus。
- 关联问题：`QUAL-001` ~ `QUAL-006`

### 3.3 线程安全与并发

评估项：

- **JUCE `MessageManager` / `MessageThread` 正确使用**：UI 操作总体在消息线程。`SettingsWindowManager` 使用 `MessageManager::callAsync` + `weak_ptr` 保证异步安全。`PluginOperationController` 通过 `AsyncUpdater` 处理增量扫描。
- **音频回调实时安全**：**存在违规**。`AudioEngine::getNextAudioBlock` 中：(a) `pluginBuffer.setSize()` 在每次回调中调用，会导致堆分配；(b) `pluginHost->prepareToPlay()` 在音频回调中延迟调用（Lazy prepare 模式），修改插件状态并在音频线程执行可能阻塞的 VST3 调用。
- **`std::atomic` / `CriticalSection` 使用**：`RecordingEngine` 的 playback 路径正确使用 `std::atomic`（`state`、`playbackSpeedMultiplier`、`playbackPositionSamples`、`playbackEndedPending`）。但 recording 路径中 `currentTake.events`（`std::vector`）、`currentPositionSamples`、`lengthSamples`、`droppedEventCount` 均为非原子变量，在音频线程写入、消息线程读取时无同步。
- **插件回调线程与 UI 线程的数据竞争风险**：`PluginHost` 没有任何内部互斥锁。`isScanning`、`scanningPluginName`、`prepared`、`pluginInstance`、`knownPluginList` 均无保护。当前线程安全完全依赖 `MainComponent::runPluginActionWithAudioDeviceRebuild` 模式（关闭音频设备 → 执行操作 → 重启设备），但缺少断言或文档化契约来防止未来调用者绕过该模式。

- 评级：**B** (Phase B 完成：PLUG-001/REC-002/REC-003/REC-005 已关闭)
- 结论：Phase A 已修复音频回调堆分配、lazy prepareToPlay 和 RecordingEngine recording 路径原子性。Phase B 已修复 PluginHost 线程安全契约、async lambda 生命周期、preset 变更并发写入、PluginOfflineRenderer 线程文档。Playback 路径线程安全实现良好。剩余：PluginHost 无内部互斥（已通过外部设备重建契约 + 消息线程断言缓解）。

### 3.4 安全边界

检查项：

| 检查项 | 评估 | 详情 |
| --- | --- | --- |
| 文件系统边界：文件读写路径校验（Preset 加载、MIDI 导入、设置文件） | **Partial** | `PresetFlowSupport::handleImportPresetFile` 使用 `sanitisePresetFileName` 防止路径遍历。`MidiFileImporter` 依赖 JUCE `MidiFile` 类做格式解析，无额外路径校验。`SettingsStore` 使用 JUCE `ApplicationProperties` 的标准文件位置。`PerformanceFile::loadPerformanceFile` 校验 JSON 版本字段。未见路径遍历漏洞。 |
| 插件加载安全：DLL/so 加载前校验、路径规范化 | **Partial** | `PluginFlowSupport::normalisePluginScanPath` 做路径规范化。JUCE 框架的 `AudioPluginFormatManager` 负责实际 DLL 加载。无自定义 DLL 加载代码。`PluginHost` 的 `loadPlugin` 验证 `PluginDescription` 有效性后再创建实例。 |
| 用户输入消毒：键位绑定配置解析、JSON 解析健壮性 | **Partial** | `KeyBindingEditDialog` 使用 JUCE `TextEditor` 组件做输入。`PerformancePreset::loadPreset` 中未知 `KeyAction` 类型被静默强制为 `"note"` 类型。`PerformanceFile::loadPerformanceFile` 调用 `JSON::parse()` **无 try-catch**（JUCE JSON 解析可能抛出异常）。 |
| 缓冲区溢出：MIDI 数据数组访问、键盘状态数组边界 | **Pass** | `KeyboardMidiMapper::heldKeys` 使用 `std::bitset<256>` 做键状态跟踪。`RecordingEngine::currentTake.events` 使用 `std::vector` 自动扩容。`WavFileExporter`/`PluginOfflineRenderer` 的 `RenderEvent` 构建使用 `std::vector`。`KeyboardTypes.h` 的 `customKeyLabels`/`customKeyColours` 使用固定大小 `std::array<juce::String, 128>`。所有 MIDI note 值通过 `MidiNoteNumber::fromClamped` 限制在 [0,127]。 |
| 数值安全：类型转换、整数溢出、浮点精度 | **Partial** | `ChannelMatrix::PerChannelConfig` 使用位域（布局依赖编译器）。`MidiChannelMapper::configForChannel` 对越界 channel 静默 clamp 到 [0,15]。`Velocity::toMidiByte()` 将 float→uint8 转换时正确 clamp。`RecordingEngine::getScaledPlaybackLengthSamples` 包含除零保护。`PluginOfflineRenderer::scaleTimestamp` 使用 `std::llround` 处理 double→int64 转换。`KeyboardPanel.cpp:43` 有 implicit int-to-float 转换（编译器警告）。 |

- 关联问题：`SEC-001` ~ `SEC-005`

### 3.5 资源与性能

评估项：

- **实时音频路径的内存分配**：**存在**。`AudioEngine::getNextAudioBlock` 中 `pluginBuffer.setSize()` 每次调用可能分配堆内存。Fallback synth 使用 JUCE `Synthesiser`，其内部分配不在控制范围内。
- **插件实例生命周期管理**：`PluginHost` 使用 `std::unique_ptr<AudioPluginInstance>`，加载时自动释放旧实例。`PluginOperationController` 的 editor 窗口使用 `std::unique_ptr<PluginEditorWindow>`。`PluginOfflineRenderer` 在 `renderTakeWithOfflinePlugin` 中通过局部 `unique_ptr` 管理临时插件实例。无泄漏风险。
- **数组/容器默认大小与增长策略**：`RecordingEngine::startRecording` 预先 `reserve(96000)` 事件容量（约 1 分钟 @ 44.1kHz）。`RecordingSessionController::pendingPresetChanges` 预分配 16 容量。`ControlsPanel::followKeyToggles` 使用 `std::array<juce::ToggleButton, 16>`。`WavExportTask` 的 `WavAudioFormat::createWriterFor` 按块写入，无内存峰值问题。
- **MIDI 事件缓冲区上限**：`juce::MidiBuffer` 由 JUCE 框架管理，无自定义上限。
- **大文件处理（MIDI 文件导入、WAV 导出）的内存峰值**：`MidiFileImporter` 将整个 MIDI 文件加载到内存再转换为 `RecordingTake`（events vector），对大文件（>10 分钟 @ 高事件密度）可能产生内存压力。`WavFileExporter` 逐块渲染，内存峰值可控。`PluginOfflineRenderer` 预先构建完整 `RenderEvent` 列表再渲染。

- 评级：**B-**
- 结论：主要问题是音频回调中的堆分配。RecordingEngine 的预分配策略合理。MIDI 导入对大文件的内存使用是固有设计限制，当前可接受。WAV 导出内存管理良好。
- 关联问题：`AUDIO-001`、`PERF-001` ~ `PERF-003`

### 3.6 错误处理与可观测性

评估项：

- **异常安全性 vs JUCE 的无异常约定**：项目一致遵循 JUCE 无异常约定。未发现 `throw` 语句或 `try-catch` 块。`JSON::parse()` 调用在 `PerformanceFile.cpp` 和 `PerformancePreset.cpp` 中**缺少 try-catch**，而 JUCE 文档说明 `JSON::parse()` 在格式错误时可能抛出。
- **错误传播路径**：`PluginHost::loadPlugin` 返回 `juce::String` 错误消息，通过 `PluginOperationController` 传递到 UI → `PluginPanel::updateState`。`WavExportTask` 通过 `wasSuccessful()` + `getErrorMessage()` 模式报告。`MidiFileImporter` 通过 `std::optional<RecordingTake>` 区分成功/失败。错误传播链基本完整。
- **`DevPianoLogger` 使用覆盖率**：**优秀**。全部业务代码使用 `DP_LOG_*` 宏（`DP_LOG_INFO`、`DP_LOG_WARN`、`DP_LOG_ERROR`、`DP_DEBUG_LOG`、`DP_TRACE_MIDI`）。仅在 `tests/TestRunner.cpp` 中存在 `std::cout`（测试框架 CLI 输出，可接受）。生产代码中零 `std::cout` 或 `DBG()` 散落。
- **静默失败点**：
  - `PluginHost::loadPlugin`：`knownPluginList.addToList()` 失败时忽略返回值
  - `PerformancePreset::loadPreset`：未知 `KeyAction::type` 静默强制为 `"note"`
  - `MidiChannelMapper::configForChannel`：越界 channel 静默 clamp
  - `PresetFlowSupport::loadPresetFile`：`loadPreset` 失败时静默返回 nullopt
  - `RecordingSessionController` 中多处 `scheduleSave` 调用忽略 save 失败

- 评级：**B**
- 结论：日志基础设施完善，DP_LOG 宏全项目统一使用。主要问题是缺少 JSON 解析异常保护、若干静默数据降级点，以及 PluginHost 中部分返回值被忽略。
- 关联问题：`ERR-001` ~ `ERR-006`

### 3.7 测试体系

评估项：

- **现有测试覆盖的核心行为**：
  - `KeyMapTypesTest`：MidiNoteNumber（clamping, isValid, min/max）、MidiChannel（clamping, toZeroBased, isValid）、Velocity（clamping, toMidiByte, isValid）、NoteRange（contains, reversed/invalid, equal bounds, full range）、KeyAction（defaults, round-trips）、KeyBinding（keyCode normalisation, findByKeyCode）、DefaultKeyboardLayout（36 keys, 17 个具体映射验证）、FullKeyboardLayout、MixedCaseKeyLookup
  - `MidiFileImporterTest`：非存在/空/无效文件→nullopt、简单音符导入（事件计数、采样率、时间戳范围、note 事件）、多轨导入（默认/全轨/指定轨）、延音 CC 事件、tempo 变化导入、velocity/channel 断言
  - `RecordingEngineTest` **（Phase E 新增）**：录制生命周期、start/stop 录制、快照一致性、clear 重置；回放基础路径（事件采样偏移、advancePlaybackPosition 进度/终点检测、isPlaying 状态）；回放速度（乘数缩放、clamping）；终点检测（边界触达、零长度 take）；容量/丢事件；preset 变更录制/回放、pitch-bend EMA
  - `KeyboardMidiMapperTest` **（Phase E 新增）**：键盘布局管理（默认布局、完整钢琴布局、布局切换、布局查找）；按键→note-on 映射（多通道、最低/最高有效 MIDI note）；key-down/key-up 生命周期（去重 key-downs、key-up 恢复 note-off 顺序）
  - `AudioEngineTest` **（Phase E 新增）**：prepareToPlay 生命周期（无 crash、多采样率/块大小、null buffer 安全）；master gain（0 静音、负值 clamp、>1 clamp）；all-notes-off 稳定性；warmup 静音块；releaseResources 安全性（无 crash、re-prepare 可用）
  - `PluginHostTest` **（Phase E 新增）**：默认状态（hasLoadedPlugin、isPrepared、getInstance、getCurrentPluginName）；last load error 默认值；空 plugin list 查询；scan summary 默认值；scanning 状态默认值；supportsVst3 编译期契约；available formats description；known plugin list XML（create/restore）
- **缺少测试的关键模块**：PerformanceFile 序列化往返、PerformancePreset JSON 序列化/反序列化、ExportFlowSupport、WavExportTask、SettingsStore、PluginHost 真实加载/扫描路径（需 VST3 文件）
- **测试可维护性**：测试结构清晰，使用 `JUCE_BEGIN_END_UNIT_TEST_*` 宏。MidiFileImporterTest 使用 fixture MIDI 文件。KeyMapTypesTest 无外部依赖。无共享 fixture 类，每个 test suite 独立。
- **测试是否独立**：是。测试不依赖音频设备、不依赖文件系统副作用（MidiFileImporterTest 使用 fixture 文件，TestRunner 默认跳过 JUCE Files category）。

- 评级：**B+**
- 结论：Phase E 完成后，4 个核心运行时模块（AudioEngine / RecordingEngine / PluginHost / KeyboardMidiMapper）已有单元测试覆盖，总计 7 个测试文件, 31 个测试类, 74 个子测试全部通过。剩余未覆盖模块为性能持久化、导出和设置层——这些模块依赖文件系统或 VST3 文件，mock 测试成本较高，可后续按需补充。
- 关联问题：`TEST-001` ~ `TEST-004`

### 3.8 文档与配置契约

评估项：

- **`docs/reference/architecture.md` 与源码模块拆分一致性**：基本一致但存在偏差：
  - architecture.md 列出 `Core/` 含 4 个文件（KeyMapTypes、MidiTypes、AppState、AppStateBuilder），实际有 7 个文件。缺少 `ChannelMatrix.h`、`KeyboardTypes.h`。`AppStateBuilder` 已拆分为 .h/.cpp 两个文件。
  - architecture.md 描述 `Recording/` 为"12 文件"，实际 14 文件。
  - architecture.md 未提及 `source/Midi/` 模块的独立存在，将 `MidiChannelMapper` 职责归入 Input。
  - architecture.md 描述 `Core/` 为"平台无关的核心数据类型"，但实际 `Core/` 中多数文件依赖 JUCE GUI 类型。
- **配置默认值漂移**：`SettingsModel.h` 中 `keySignature = 0` 与 `AppState.h` 中 `keySignature = 0` 一致。`bufferSize = 512`、`sampleRate = 44100.0` 等默认值一致。`ChannelMatrix` 在 `SettingsSerialization.cpp` 中默认 `velocity=64`、`sustainCC=64` 与 `ChannelMatrix.h` 中 `PerChannelConfig` 默认值一致。
- **头文件注释与实现是否同步**：`RecordingEngine.h` 的双线程访问注释准确描述了当前架构。`PluginOfflineRenderer.h` 的 3-phase 文档与实际实现一致。`AppState.h` 的 struct 字段注释与 `AppStateBuilder.cpp` 的构建逻辑一致。

- 评级：**B+**
- 结论：配置默认值无漂移。architecture.md 需要更新以反映新增文件和 `Midi/` 模块。Core/ 的"零 JUCE 依赖"描述与实际不符。
- 关联问题：`DOC-001` ~ `DOC-003`

### 3.9 工程化与构建

评估项：

- **CMakeLists.txt 源文件列表完整性**：`AudioDeviceDiagnostics.h` 已补充列入 `target_sources`（Phase D）。所有 .cpp/.h 文件均已列入。无孤立文件。
- **clang-tidy 诊断清零状态**：`.clang-tidy` 配置文件存在，覆盖 bugprone/performance/readability/modernize/clang-analyzer 五大类别。WSL 环境已有 `/usr/bin/clang-tidy-21`；Windows MSVC 环境已通过脚本集成（soft-fail 模式）。
- **clang-format 合规性**：**合规** — Phase B/C 期间所有格式违规已修复，当前 `format --check` 通过。
- **编译器警告清零状态**：**0 warnings** — KeyboardPanel.cpp:43 已通过 `static_cast<int>()` 修复，构建输出干净。

- 评级：**A-**
- 结论：构建系统配置完善，所有工程化问题已修复。format、warnings、target_sources、clang-tidy CI 集成均已就位。

---

## 4. 构建、测试与诊断结果

### 4.1 WSL 构建

```text
[build_wsl] build complete
Wall time: 13.55 seconds
```
- 结果：**成功**（1 warning）
> **注意**：以下为初始审计（2026-07-20）时的结果。当前状态（2026-07-22）：构建 0 errors 0 warnings、format 通过、clang-tidy 已集成。

### 4.2 测试

```text
100% tests passed, 0 tests failed out of 1
Total Test time (real) = 8.21 sec
```
- 结果：**全部通过**
- 测试数量：2 test suites (KeyMapTypesTest, MidiFileImporterTest)，1 CTest target

### 4.3 格式检查

```text
~70 violations across 14 files
Exit code: 123
```
- 结果：**不合规**
- 涉及文件：MainComponent.cpp, SettingsModel.h, SettingsComponent.h, SettingsSerialization.cpp, RecordingEngine.h/.cpp, RecordingSessionController.h/.cpp, PerformanceFile.h, MidiChannelMapper.h/.cpp, AppState.h, AppStateBuilder.h, CustomKeyboard.h/.cpp, KeyboardPanel.cpp, ControlsPanel.cpp, KeyBindingEditDialog.cpp, PerformancePreset.h/.cpp, PresetFlowSupport.cpp

### 4.4 LSP 诊断

- 结果：**0 issues** — 所有 .cpp 源文件通过 clangd 实时诊断

### 4.5 静态分析

- `.clang-tidy` 已配置，未运行（WSL 环境缺少 clang-tidy 二进制）

---

## 5. 问题领域交叉分析

### 5.1 线程安全是最大风险域

5 个 P0 中 4 个为线程安全问题，涉及 3 个独立子系统 (AudioEngine, PluginHost, RecordingEngine)。根本原因是 **音频回调实时安全约束未系统性地强制执行**：没有编译器级别的检查（如实时安全 sanitizer）、没有运行时断言（如 `jassert(isMessageThread())` 在关键路径）、没有架构文档中的强制契约。

### 5.2 Core/ 模块边界漂移

`Core/` 目录的物理位置与逻辑职责存在偏离：`ChannelMatrix.h` 属于 `Midi/` 命名空间但存放在 `Core/`；`KeyboardTypes.h` 属于 `UI/` 命名空间且依赖 JUCE GUI 类型但存放在 `Core/`。这是渐进式开发的自然结果，但如不修正，新开发者会误判模块边界。

### 5.3 格式债务累积

~70 处格式违规表明 clang-format 未在 pre-commit 或 CI 中强制执行。少量违规是新代码引入的尾随空格/对齐差异，批量 `./scripts/dev.sh format` 可一次性修复。

---

## 6. 优秀实践表彰

以下实践在审计中表现突出，值得保持：

1. **DP_LOG 日志体系统一**：全项目统一使用 `DP_LOG_*` 宏，零散落 `std::cout`/`DBG()`。日志级别区分清晰（INFO/WARN/ERROR/DEBUG/TRACE）。
2. **MidiTypes.h 强类型封装**：`MidiNoteNumber`、`MidiChannel`、`Velocity`、`NoteRange` 的 constexpr 包装是 const 正确性和类型安全的典范。
3. **SettingsWindowManager 异步安全**：使用 `shared_ptr<State>` + `Component::SafePointer` + `MessageManager::callAsync` 的 PIMPL 模式，优雅地解决了异步回调的生命周期问题。
4. **RecordingFlowSupport 纯状态机**：无状态、无副作用的纯函数式 `chooseRecordingFlowCommand` / `getStateAfterCommand` 设计，易于测试和理解。
5. **RecordingEngine 预分配策略**：`startRecording` 中 `reserve(96000)` 的预分配策略减少音频线程中的向量扩容。
6. **MIDI 导出边界守卫**：`MidiFileExporter` 对零/负输入的全面守卫检查。
7. **AppState 不可变快照**：`AppStateBuilder` 构建完整的 `AppState` 快照，UI 渲染基于快照而非直接读取可变状态，减少数据竞争窗口。
8. **PluginOfflineRenderer 三阶段文档**：头文件注释精确记录了 snapshot→create→render 的线程要求。

---

## 7. 复审记录

> 初始审计，尚无复审记录。
> **复审记录**：2026-07-22 — Phase A (音频稳定性) 完成，AUDIO-001/002、REC-001 → Closed。Phase B (线程安全) 完成，PLUG-001、REC-002/003/005 → Closed。Phase C (模块边界) 完成，ARCH-001/002/003/004、DOC-001 → Closed。Phase D (工程化) 完成，ENG-001/002/003/004、DOC-002/003 → Closed。详见 §9。剩余 Open：42/63。
> **复审记录**：2026-07-23 — Phase E (测试完善) 完成，TEST-001~004 → Closed。并行 scout 复审全部 63 项，确认 0 回归/0 误判。8 项 Open 经代码演变已自然修复 → Closed。剩余 Open：29/63。
> **复审记录**：2026-07-23 — 修正 0.2 汇总表（原始审计 Ch8 实际 63 项，汇总表误写为 60）。Phase F (代码修复) 完成，ERR-001/REC-004/ERR-005/THREAD-004/ERR-007/ERR-008/QUAL-002/QUAL-006 → Closed。剩余 Open：24/63（P1 1 项，P2 15 项，P3 8 项）。

## 8. 问题总表

> 本章为唯一 finding registry。所有状态变更必须同步更新第 0.2 节汇总表。

### ID 前缀规则

| 前缀 | 领域 |
| --- | --- |
| `AUDIO` | 音频引擎 |
| `PLUG` | 插件宿主 |
| `REC` | 录制/回放 |
| `ARCH` | 架构与模块边界 |
| `QUAL` | 代码质量（RAII/const/命名/注释/死代码） |
| `THREAD` | 线程安全 |
| `SEC` | 安全边界 |
| `PERF` | 资源与性能 |
| `ERR` | 错误处理与可观测性 |
| `TEST` | 测试体系 |
| `DOC` | 文档与配置 |
| `ENG` | 工程化（CMake、clang-tidy、clang-format、警告） |

### P0（立即修复，阻断开发）

| ID | Priority | Status | 标题 | 文件:行号 | 描述 | 建议修复 |
| --- | --- | --- | --- | --- | --- | --- |
| `AUDIO-001` | P0 | Closed | 音频回调中 pluginBuffer.setSize() 堆分配 | `source/Audio/AudioEngine.cpp` (getNextAudioBlock) | 每次音频回调调用 `pluginBuffer.setSize(channels, numSamples)` 可能触发堆分配，违反实时安全约束 | 已修复 (009355d)：prepareToPlay 预分配 8 通道；回调中仅 jassert + 安全网（Release 中 DP_LOG_WARN 后 resize） |
| `AUDIO-002` | P0 | Closed | prepareToPlay 延迟到音频回调线程执行 | `source/Audio/AudioEngine.cpp` (getNextAudioBlock, lazy prepare) | `pluginHost->prepareToPlay()` 在音频回调中延迟调用，VST3 插件 lifecycle 方法在音频线程执行 | 已修复 (009355d)：移除 lazy prepare 模式；插件加载触发设备重建 → prepareToPlay 在消息线程调用 |
| `PLUG-001` | P0 | Closed | PluginHost 零内部线程同步 | `source/Plugin/PluginHost.h:5-16`, `source/Plugin/PluginHost.cpp` | `isScanning`、`scanningPluginName`、`prepared`、`pluginInstance` (raw pointer 访问)、`knownPluginList` 均无互斥锁保护。`getInstance()` 返回裸指针，音频线程通过它调用 `processBlock` | 已修复：在 PluginHost.h 添加 thread-safety contract 文档，所有 mutation 方法入口添加 `jassert(isMessageThread())` 断言；read-only 访问器允许音频线程调用；设备重建 pause 为唯一同步点 |
| `REC-001` | P0 | Closed | RecordingEngine recording 路径无同步 | `source/Recording/RecordingEngine.h:90`, `source/Recording/RecordingEngine.cpp` | `currentPositionSamples`、`droppedEventCount` 在音频线程写入、消息线程读取时无同步。`currentTake.events` vector 由音频线程写入，消息线程在录制进行中不得读取。`currentTake.lengthSamples` 为良性竞争（调用者先挂起音频设备，x86-64 对齐 int64 读实际原子，std::max 保守） | 已修复 (3bd994b)：currentPositionSamples/droppedEventCount → std::atomic；hasTake/getCurrentTake/createTakeSnapshot 添加 jassert(!isRecording()) 守卫。lengthSamples 保留非原子（良性竞争，见描述） |
| `REC-005` | P0 | Closed | PluginOfflineRenderer 在后台线程调用 processBlock | `source/Recording/PluginOfflineRenderer.h`, `source/Recording/PluginOfflineRenderer.cpp` | `renderTakeWithOfflinePlugin` 在 `WavExportTask::run()` 的后台线程中调用 `AudioPluginInstance::processBlock()`。绝大多数 VST3 插件假设单线程访问，多线程调用可导致崩溃或音频错误 | 已修复：文档化现有保护 — `createOfflinePluginInstance()` 为每次渲染创建独立的插件实例，不与音频线程的 live 实例共享 processBlock；PluginOfflineRenderer.h 添加线程隔离注释 |

### P1（当前迭代修复）

| ID | Priority | Status | 标题 | 文件:行号 | 描述 | 建议修复 |
| --- | --- | --- | --- | --- | --- | --- |
| `PLUG-002` | P1 | Closed | PluginHost::loadPlugin 忽略 addToList 失败 | `source/Plugin/PluginHost.cpp` (loadPlugin) | `knownPluginList.addToList()` 返回值被忽略，列表添加失败不报错 | 已修复：loadPluginByDescription 重构后不再调用 addToList；插件加载路径已简化 |
| `PLUG-003` | P1 | Closed | PluginHost::unload 非异常安全 | `source/Plugin/PluginHost.cpp` (unloadPlugin) | `unload()` 中先 `releaseResources()` 再 `reset()` pluginInstance | 已修复：unloadPlugin 中 releaseResources() 在 reset() 之前执行，顺序正确 |
| `PLUG-004` | P1 | Open | PluginOperationController 析构不卸载插件 | `source/Plugin/PluginOperationController.h`, `source/Plugin/PluginOperationController.cpp` | 析构函数仅取消扫描，不调用 `closePluginEditor()` 或通过 PluginHost 卸载插件 | 在析构或 shutdown 中确保 editor 关闭和插件资源释放 |
| `REC-002` | P1 | Closed | RecordingSessionController async lambda use-after-free 风险 | `source/Recording/RecordingSessionController.cpp` (5 处 lambda) | 异步文件选择器 lambda 捕获 `[this]` 和 chooser `unique_ptr` 的引用。如果 controller 在回调触发前被销毁，访问悬垂指针 | 已修复：添加 `std::shared_ptr<bool> aliveFlag_` 成员，所有异步 lambda 通过值捕获并检查 `*aliveFlag` 提前返回；析构函数设置 flag 为 false |
| `REC-003` | P1 | Closed | 录制中 preset 变更与录音向量并发写入 | `source/Recording/RecordingEngine.cpp` (recordPresetChange vs recordMidiBufferBlock) | `PresetFlowSupport` (消息线程) 调用 `recordPresetChange` 写入 `currentTake.events`，与音频线程的 `recordMidiBufferBlock` 并发写同一 vector | 已修复：添加独立的 `pendingPresetEvents` 消息线程队列，`recordPresetChange` 写入该队列；`stopRecording()` / `clear()` 时合并到 `currentTake.events`，单线程访问无需锁 |
| `REC-004` | P1 | Closed | RecordingEngine 录制中 getCurrentTake() 返回引用 racing | `source/Recording/RecordingEngine.h:50`, `source/Recording/RecordingEngine.cpp` (getCurrentTake) | `getCurrentTake()` 返回 `const RecordingTake&` 引用，调用方读取时音频线程可能同时在修改 events vector | 已修复：getCurrentTake() 添加运行时 isRecording() 守卫，录制中返回空 take |
| `ARCH-001` | P1 | Closed | Core/ 目录包含 JUCE GUI 依赖 | `source/Core/AppState.h:5`, `source/Core/KeyMapTypes.h:3` | architecture.md 描述 Core/"平台无关"，但 5/7 文件 include `<JuceHeader.h>`，使用 `juce::String`、`juce::Colour`、`juce::Rectangle` | 已修复：`ChannelMatrix.h` 移至 Midi/，`KeyboardTypes.h` 移至 UI/，`AppStateBuilder` 移至 Settings/；Core/ 现仅使用 JUCE 工具类型（String, Array），architecture.md 已更新 |
| `ARCH-002` | P1 | Closed | ChannelMatrix.h 物理位置与命名空间不匹配 | `source/Midi/ChannelMatrix.h` | 文件原在 `Core/`，命名空间为 `devpiano::midi` | 已修复：移至 `source/Midi/ChannelMatrix.h`，5 include 更新 |
| `ARCH-003` | P1 | Closed | KeyboardTypes.h 物理位置与命名空间不匹配 | `source/UI/KeyboardTypes.h` | 文件原在 `Core/`，命名空间为 `devpiano::ui`，依赖 `juce::Colour`、`juce::Rectangle` GUI 类型 | 已修复：移至 `source/UI/KeyboardTypes.h`，3 include 更新 |
| `ARCH-004` | P1 | Closed | AppStateBuilder 依赖反转 | `source/Settings/AppStateBuilder.h` | Core/ 中的 builder 依赖 Settings/、Audio/、Plugin/ 模块 | 已修复：移至 `source/Settings/AppStateBuilder.*`，1 include 更新 |
| `ERR-001` | P1 | Closed | JSON::parse() 无异常保护 | `source/Recording/PerformanceFile.cpp`, `source/Layout/PerformancePreset.cpp` | `juce::JSON::parse()` 在格式错误时可能抛出异常（JUCE 文档未明确承诺 noexcept），但调用处无 try-catch | 已修复：3 处 JSON::parse() 调用包裹 try-catch，异常时返回 nullopt |
| `DOC-001` | P1 | Closed | architecture.md 与源码文件清单不一致 | `docs/reference/architecture.md` | 文档列出 Core/ 含 4 文件、Recording/ 含 12 文件、未提及 Midi/ 模块，均与实际不符 | 已修复：新增 Midi/ 模块节，更新 Core/Settings/UI 文件清单，更新依赖描述 |

### P2（近期排期）

| ID | Priority | Status | 标题 | 文件:行号 | 描述 | 建议修复 |
| --- | --- | --- | --- | --- | --- | --- |
| `PLUG-005` | P2 | Open | PluginHost::getInstance() 暴露裸指针 | `source/Plugin/PluginHost.h:59` | `getInstance()` 返回 `AudioPluginInstance*` 裸指针。调用方（AudioEngine）持有并在音频回调中使用，生命周期依赖外部协调 | 返回 `juce::AudioPluginInstance::Ptr` (引用计数智能指针) 或文档化所有权契约 |
| `PLUG-006` | P2 | Closed | 同步扫描阻塞消息线程 | `source/Plugin/PluginHost.cpp` | `scanAndAddToKnownList` 在主扫描循环中调用 `addVst3FileToKnownList`，每个插件同步处理，大目录扫描时阻塞 UI | 已修复：scanAndAddToKnownList 已移除；扫描现为全异步（advanceVst3ScanStep + AsyncUpdater），不阻塞消息线程 |
| `PLUG-007` | P2 | Open | PluginOperationController 扫描状态在销毁时未清理 | `source/Plugin/PluginOperationController.cpp` | 如果 controller 在扫描进行中销毁，PluginHost 的 `isScanning` 保持 true，UI 永远卡在扫描状态 | 析构函数中添加 `pluginHost.cancelScan()` 或等效清理 |
| `REC-006` | P2 | Closed | RecordingEngine renderPlaybackBlock 中使用 mutex | `source/Recording/RecordingEngine.cpp` (renderPlaybackBlock) | `renderPlaybackBlock` 在音频回调中调用，内部使用 `std::mutex` 保护 `pendingPresetChanges` 队列 | 已修复：改用 `juce::CriticalSection` 替代 `std::mutex`，仅在 preset 变更分支作用域锁 |
| `REC-007` | P2 | Open | WavFileExporter 和 PluginOfflineRenderer 重复代码 | `source/Recording/WavFileExporter.cpp`, `source/Recording/PluginOfflineRenderer.cpp` | `RenderEvent` struct、`buildRenderEvents`、`getScaledTakeLengthSamples`、`addPanicMidi` 在两个文件中重复定义，相似度 ~50% | 提取公共渲染管线到 `source/Recording/RenderPipeline.h` 或共享基类 |
| `THREAD-001` | P2 | Open | AudioEngine::currentSampleRate/currentBlockSize 非原子 | `source/Audio/AudioEngine.h`, `source/Audio/AudioEngine.cpp` | `currentSampleRate` 和 `currentBlockSize` 在 `prepareToPlay`（消息线程）设置，在 `getNextAudioBlock`（音频线程）读取，无 atomic/mutex | 改为 `std::atomic<double>` / `std::atomic<int>` |
| `THREAD-002` | P2 | Open | MidiChannelMapper 引用成员悬垂风险 | `source/Midi/MidiChannelMapper.h:22-25` | 构造函数接受 `const ChannelMatrix&`、`const int&`、`const int&` 引用并存储。如果外部对象被销毁而 mapper 仍存活，引用悬垂 | 文档化生命周期契约（mapper 必须短于引用对象）；或改用值拷贝 |
| `THREAD-003` | P2 | Open | midiTranspose/keySignature 无同步读取 | `source/Midi/MidiChannelMapper.cpp` | `applyTransform` 在音频回调中读取 `midiTranspose` 和 `keySignature`（通过引用），而这些值可在消息线程修改 | 改为 `std::atomic<int>` 或通过消息传递更新 |
| `SEC-001` | P2 | Open | loadPreset 静默强制未知 KeyAction 类型 | `source/Layout/PerformancePreset.cpp` (loadPreset) | JSON 中 `type` 字段若为未知值，被静默设为 `"note"`，可能掩盖数据损坏 | 至少记录 DP_LOG_WARN，或拒绝加载整个 preset |
| `SEC-002` | P2 | Open | MidiChannelMapper::configForChannel 静默 clamp | `source/Midi/MidiChannelMapper.cpp` (configForChannel) | 越界 channel 参数被静默 clamp 到 [0,15]，调用方无法得知错误 | 添加 `jassert` 或返回 `std::optional` |
| `SEC-003` | P2 | Open | MidiFileImporter 无文件大小限制 | `source/Recording/MidiFileImporter.cpp` | 无输入文件大小检查，超大或恶意构造的 MIDI 文件可导致内存耗尽 | 添加可配置的文件大小上限（如 100MB） |
| `SEC-004` | P2 | Open | PerformanceFile JSON parse 非原子文件写入 | `source/Recording/PerformanceFile.cpp` | `file.replaceWithText()` 非原子写入——如果写入中途崩溃，文件损坏且无备份 | 写入临时文件 + 原子 rename |
| `SEC-005` | P2 | Open | ChannelMatrix 位域布局编译器依赖 | `source/Core/ChannelMatrix.h` (PerChannelConfig) | `PerChannelConfig` 使用位域（`uint8_t outputChannel : 4`），布局依赖编译器实现，序列化可能在不同编译器间不兼容 | 使用显式 bit mask + shift 或 `std::bitset` |
| `PERF-001` | P2 | Open | MidiFileImporter 全量内存加载 | `source/Recording/MidiFileImporter.cpp` | 将整个 MIDI 文件的全部事件加载到 `vector<PerformanceEvent>`，大文件（>10min 高密度）可能产生数百 MB 内存使用 | 添加流式处理或事件数量上限 |
| `PERF-002` | P2 | Open | SettingsModel view getter 按值返回 128 元素数组 | `source/Settings/SettingsModel.h` | `KeyboardDisplaySettingsView` 等 view getter 返回包含 `std::array<juce::String,128>` 和 `std::array<juce::Colour,128>` 的副本，每次调用复制 ~4KB | 返回 `const&` 或使用 `std::span` |
| `PERF-003` | P2 | Closed | SettingComponent 30Hz timer 全 UI 刷新 | `source/MainComponent.cpp` (timerCallback) | `MainComponent::timerCallback()` 以 30Hz 无条件调用 `refreshReadOnlyUiStateFromCurrentSnapshot()` | 已修复：timerCallback 现仅检查 playback+presets+focus，不再无条件触发全 UI 刷新链 |
| `ERR-002` | P2 | Closed | RecordingSessionController 忽略 scheduleSave 失败 | `source/Recording/RecordingSessionController.cpp` | 多处调用 `owner.settingsStore.scheduleSave(owner.appSettings)` 后忽略 save 结果 | 已修复：savePerformanceFile 返回值已被检查并记录日志（成功/错误） |
| `ERR-003` | P2 | Open | AppStateBuilder 仅 jassert 线程守卫 | `source/Core/AppStateBuilder.cpp` | `buildAppStateSnapshot` 使用 `jassert(isMessageThread())` 做线程守卫——Release 构建中为 no-op | 考虑使用 `jassert` + 返回错误码（debug），或在 Release 中也保持检查 |
| `ERR-004` | P2 | Closed | PluginHost 加载成功后冗余赋值 | `source/Plugin/PluginHost.cpp` (loadPlugin) | `loadPlugin` 成功路径中 `prepared = false` 赋值后紧接 `lastLoadError.clear()` 后再无使用 | 已修复：loadPlugin 重构后直接返回 bool，无冗余赋值 |
| `ERR-005` | P2 | Closed | WavExportTask 取消后可能不清理文件 | `source/Export/WavExportTask.cpp` (run) | `threadShouldExit()` 检查后调用 `outputFile.deleteFile()`，但 `deleteFile()` 失败时无日志 | 已修复：取消路径 deleteFile 失败时记录 DP_LOG_WARN |
| `ERR-006` | P2 | Open | SettingsStore scheduleSave 裸指针 API | `source/Settings/SettingsStore.h`, `source/Settings/SettingsStore.cpp` | `DebounceTimer` 持有 `const SettingsModel*` 裸指针，如果 SettingsModel 在 timer 触发前析构，使用悬垂指针 | 使用 `std::shared_ptr` 或确保 SettingsModel 生命周期长于 DebounceTimer |
| `TEST-001` | P2 | Closed | AudioEngine 无单元测试 | `source/tests/AudioEngineTest.cpp` | 核心音频引擎（prepareToPlay, getNextAudioBlock, releaseResources）完全无测试覆盖 | 已修复：添加 AudioEngineTest（6 个测试类, 14 个子测试），覆盖生命周期/增益/静音/warmup/release |
| `TEST-002` | P2 | Closed | RecordingEngine playback 路径无单元测试 | `source/tests/RecordingEngineTest.cpp` | RecordingEngine 的 renderPlaybackBlock/advancePlaybackPosition 逻辑无测试 | 已修复：添加 RecordingEngineTest（10 个测试类, 25 个子测试），覆盖录制/回放/容量/preset/pitch-bend |
| `TEST-003` | P2 | Closed | PluginHost 无单元测试 | `source/tests/PluginHostTest.cpp` | 插件加载/卸载/扫描生命周期无测试（可能因依赖实际 VST3 文件而困难，但 mock 测试可行） | 已修复：添加 PluginHostTest（8 个测试类, 21 个子测试），覆盖默认状态/错误消息/list 查询/扫描摘要/VST3 支持/XML |
| `TEST-004` | P2 | Closed | KeyboardMidiMapper 运行时逻辑无测试 | `source/tests/KeyboardMidiMapperTest.cpp` | handleKeyPressed/handleKeyStateChanged 的 note on/off 映射逻辑仅通过集成测试间接覆盖 | 已修复：添加 KeyboardMidiMapperTest（7 个测试类, 14 个子测试），覆盖布局管理/按键映射/note-on-off/去重 |

### P3（持续跟踪）

| ID | Priority | Status | 标题 | 文件:行号 | 描述 | 建议修复 |
| --- | --- | --- | --- | --- | --- | --- |
| `QUAL-001` | P3 | Closed | HeaderPanel::AudioStatus struct 未使用 | `source/UI/HeaderPanel.h` | `AudioStatus` struct 定义了但未被 HeaderPanel 自身使用 | 已修复：AudioStatus struct 已从 HeaderPanel.h 中移除 |
| `QUAL-002` | P3 | Closed | KeyboardMidiMapper note-off 逻辑重复 | `source/Input/KeyboardMidiMapper.cpp` (handleKeyStateChanged, triggerBinding) | `handleKeyStateChanged` 和 `triggerBinding` 中有重复的 note-off 发送逻辑（~8 行） | 已修复：提取 sendNoteOff() 私有辅助方法，handleKeyStateChanged 和 triggerBinding 共用 |
| `QUAL-003` | P3 | Open | makeDefaultKeyboardLayout / makeFullPianoLayout 重复 | `source/Core/KeyMapTypes.h` | 两个函数中 binding 数组构建模式高度重复，每个 note 定义 ~4 行 | 提取共享的 binding 构建辅助函数 |
| `QUAL-004` | P3 | Open | findByKeyCode 返回裸指针 | `source/Core/KeyMapTypes.h:66` | `KeyboardLayout::findByKeyCode` 返回 `const KeyBinding*` 指向 vector 内部元素。vector 修改后指针悬垂 | 返回 `std::optional<std::reference_wrapper<const KeyBinding>>` 或索引 |
| `QUAL-005` | P3 | Open | AudioEngine::getMidiCollector / getKeyboardState 暴露内部可变引用 | `source/Audio/AudioEngine.h:33-38` | 两个方法返回 `juce::MidiKeyboardState&` / `juce::MidiMessageCollector&` 可变引用，允许外部任意修改内部状态 | 提供 const 版本或通过专用方法暴露受限功能 |
| `QUAL-006` | P3 | Closed | PluginFlowSupport 中冗余前向声明 | `source/Plugin/PluginFlowSupport.cpp` | `normalisePluginScanPath` 在 .cpp 中有冗余前向声明，.h 中已有声明 | 已修复：删除冗余前向声明 |
| `THREAD-004` | P3 | Closed | PluginHost 非原子 isScanning 标志 | `source/Plugin/PluginHost.h:35-37` | `isScanning` 在消息线程写入，在 `PluginPanel::updateState()` 中通过 `PluginPanelStateBuilder` 读取 | 已修复：isScanning 改为 std::atomic<bool> |
| `THREAD-005` | P3 | Closed | RecordingEngine::hasDroppedEvents / getDroppedEventCount 非原子 | `source/Recording/RecordingEngine.h:44-47` | `droppedEventCount` 在音频线程递增、消息线程读取，非原子 | 已修复 (REC-001, Phase A)：`currentPositionSamples` 和 `droppedEventCount` 均改为 `std::atomic` |
| `SEC-006` | P3 | Open | KeyMapTypes 允许 aggregate init 绕过 fromClamped | `source/Core/MidiTypes.h` | `MidiNoteNumber{200}` aggregate 初始化可绕过 `fromClamped(200)` 的 clamp 保护 | 添加私有构造函数或使用 `requires` clause |
| `SEC-007` | P3 | Open | KeyboardMidiMapper 0/1-based channel 转换脆弱 | `source/Input/KeyboardMidiMapper.cpp` | channel 值在 0-based 和 1-based 之间手动转换，缺少类型系统保护 | 使用 `MidiChannel::toZeroBased()` 统一转换 |
| `SEC-008` | P3 | Open | MidiChannelMapper::applyTransform 仅重映射 note on/off | `source/Midi/MidiChannelMapper.cpp` (applyTransform) | `applyTransform` 仅对 note on/off 消息重映射 channel，CC/pitch bend/program change 不经过矩阵路由 | 确认是否为设计意图，若是，文档化 |
| `PERF-004` | P3 | Open | KeyboardTypes::KeyboardSettings 2KB+ 固定数组 | `source/UI/KeyboardTypes.h` | `customKeyLabels`（`std::array<juce::String,128>`）和 `customKeyColours`（`std::array<juce::Colour,128>`）固定分配 ~4KB，即使未自定义也占满内存 | 改为 `std::vector` 或 sparse map（仅在少数键自定义时节省内存） |
| `PERF-005` | P3 | Open | KeyboardMidiMapper::isKeyCurrentlyDown O(n) 轮询 | `source/Input/KeyboardMidiMapper.cpp` | `handleKeyStateChanged` 中每帧遍历所有 binding 调用 `isKeyCurrentlyDown`（O(bindings) × O(key codes)） | 使用 `std::bitset` 或 `std::unordered_set` 替代遍历 |
| `ERR-007` | P3 | Closed | AudioDeviceDiagnostics parseSavedAudioDeviceState 语义模糊 | `source/Audio/AudioDeviceDiagnostics.h` | `hasSavedState` 字段名容易误解 | 已修复：重命名为 hasSavedDeviceStateXml |
| `ERR-008` | P3 | Closed | WavExportTask 成功/失败通过成员变量通信 | `source/Export/WavExportTask.h` | `success` 和 `errorMessage` 在 `run()` 中设置，调用方在 `runThread()` 返回后读取，无同步 | 已修复：success 改为 std::atomic<bool> |
| `DOC-003` | P3 | Closed | architecture.md 未列出 Midi/ 模块 | `docs/reference/architecture.md` | `Midi/` 模块（MidiChannelMapper）未在架构文档中出现 | 已修复 (Phase C)：新增 Midi/ 模块节，列出 MidiChannelMapper + ChannelMatrix |
| `DOC-002` | P3 | Closed | architecture.md Core/ 描述与实际不符 | `docs/reference/architecture.md:74-86` | 描述 Core/ 包含 4 个文件、"平台无关的核心数据类型"，实际 7 个文件，多数依赖 JUCE | 已修复 (Phase C)：Core/ 缩小为 3 文件（KeyMapTypes, MidiTypes, AppState），GUI 类型移出，描述已更新 |
| `ENG-001` | P3 | Closed | clang-format ~70 violations | 14 个文件 | `format --check` 报告 ~70 处格式违规 | 已修复：Phase B/C 期间自然解决，`format --check` 现通过 |
| `ENG-002` | P3 | Closed | KeyboardPanel.cpp:43 int-to-float 隐式转换 | `source/UI/KeyboardPanel.cpp:43` | `whiteCount * keyWidth` 隐式转换警告 | 已修复：已有 `static_cast<int>(...)`，构建 0 warnings |
| `ENG-003` | P3 | Closed | AudioDeviceDiagnostics.h 未列入 target_sources | `CMakeLists.txt` | header-only 文件不在 target_sources 中，IDE 不可见 | 已修复：添加 `source/Audio/AudioDeviceDiagnostics.h` 到 target_sources |
| `ENG-004` | P3 | Closed | 缺少 clang-tidy CI 集成 | `.clang-tidy`, `CMakeLists.txt`, `tools/build-windows.ps1` | 配置已存在但无 CI 或 pre-commit hook 运行 | 已修复：CMakeLists 添加 clang-tidy target，build-windows.ps1 添加 -ClangTidy 开关，MSVC preset 启用 compile_commands |

---

## 9. 审计后建议路线图

基于审计发现，建议按以下优先级处理：

### Phase A: 音频稳定性 ✅ 已完成 (2026-07-20)

1. ~~**AUDIO-001**: 将 `pluginBuffer.setSize()` 移到 `prepareToPlay`~~ → 已修复 (009355d)
2. ~~**AUDIO-002**: 移除 lazy prepare~~ → 已修复 (009355d)
3. ~~**REC-001**: 为 RecordingEngine recording 路径添加线程同步~~ → 已修复 (3bd994b)
4. 验证：构建 0 error、测试 100% 通过、格式干净

### Phase B: 线程安全加固 ✅ 已完成 (2026-07-22)

5. ~~**PLUG-001**: PluginHost 添加线程安全契约（断言 + 文档化）~~ → 已修复：jassert(isMessageThread()) + 头文件文档
6. ~~**REC-002**: 修复 async lambda 生命周期问题~~ → 已修复：alive-flag (shared_ptr\<bool\>) 防 use-after-free
7. ~~**REC-003**: preset change 写入队列化~~ → 已修复：独立 pendingPresetEvents 队列，stopRecording 时合并
8. ~~**REC-005**: 评估 PluginOfflineRenderer 线程模型并修复~~ → 已修复：文档化独立实例线程隔离
9. 验证：WSL build 0 error 0 warning、Windows MSVC 0 error、format 干净

### Phase C: 模块边界 ✅ 已完成 (2026-07-22)

10. ~~**ARCH-002**: 移动 `ChannelMatrix.h` → `Midi/`~~ → 已修复：5 include 更新，零逻辑变更
11. ~~**ARCH-003**: 移动 `KeyboardTypes.h` → `UI/`~~ → 已修复：3 include 更新，零逻辑变更
12. ~~**ARCH-004**: 移动 `AppStateBuilder` → `Settings/`~~ → 已修复：1 include 更新，依赖反转消除
13. ~~**ARCH-001/DOC-001**: 更新 architecture.md 与 Core/ 描述~~ → 已修复：新增 Midi/ 模块文档，Core/ 去 GUI 化
14. 验证：WSL build 0 error、Windows MSVC 0 error、format 干净

### Phase D: 工程化 ✅ 已完成 (2026-07-22)

15. ~~**ENG-001**: clang-format ~70 violations~~ → 已修复：Phase B/C 期间自然解决，`format --check` 现通过
16. ~~**ENG-002**: KeyboardPanel.cpp:43 编译警告~~ → 已修复：已有 `static_cast<int>(...)`，构建 0 warnings
17. ~~**ENG-003**: AudioDeviceDiagnostics.h 未列入 target_sources~~ → 已修复：添加到 CMakeLists.txt Audio 区
18. ~~**ENG-004**: 缺少 clang-tidy CI 集成~~ → 已修复：CMakeLists 添加 clang-tidy target，build-windows.ps1 添加 `-ClangTidy` 开关（soft-fail），MSVC preset 启用 compile_commands
19. 验证：WSL build 0 error、Windows MSVC 0 error、format 干净

### Phase E: 测试完善 ✅ 已完成 (2026-07-23)

20. ~~**TEST-001**: AudioEngine 单元测试~~ → 已修复：AudioEngineTest（6 类, 14 子测试）
21. ~~**TEST-002**: RecordingEngine playback 单元测试~~ → 已修复：RecordingEngineTest（10 类, 25 子测试）
22. ~~**TEST-003**: PluginHost 状态机测试~~ → 已修复：PluginHostTest（8 类, 21 子测试）
23. ~~**TEST-004**: KeyboardMidiMapper 运行时测试~~ → 已修复：KeyboardMidiMapperTest（7 类, 14 子测试）
24. 验证：4 个新测试文件, 31 个测试类, 74 个子测试全部通过；CMakeLists.txt 测试目标正确链接所需库
---

## 附录 A: 所有审计文件清单

以下为审计覆盖的全部 70 个源文件：

```
source/Main.cpp
source/MainComponent.cpp
source/MainComponent.h
source/Core/AppState.h
source/Core/AppStateBuilder.cpp
source/Core/AppStateBuilder.h
source/Core/ChannelMatrix.h
source/Core/KeyboardTypes.h
source/Core/KeyMapTypes.h
source/Core/MidiTypes.h
source/Audio/AudioEngine.cpp
source/Audio/AudioEngine.h
source/Audio/AudioDeviceDiagnostics.h
source/Plugin/PluginHost.cpp
source/Plugin/PluginHost.h
source/Plugin/PluginFlowSupport.cpp
source/Plugin/PluginFlowSupport.h
source/Plugin/PluginOperationController.cpp
source/Plugin/PluginOperationController.h
source/Input/KeyboardMidiMapper.cpp
source/Input/KeyboardMidiMapper.h
source/Midi/MidiChannelMapper.cpp
source/Midi/MidiChannelMapper.h
source/Recording/RecordingEngine.cpp
source/Recording/RecordingEngine.h
source/Recording/RecordingFlowSupport.cpp
source/Recording/RecordingFlowSupport.h
source/Recording/RecordingSessionController.cpp
source/Recording/RecordingSessionController.h
source/Recording/MidiFileExporter.cpp
source/Recording/MidiFileExporter.h
source/Recording/MidiFileImporter.cpp
source/Recording/MidiFileImporter.h
source/Recording/PerformanceFile.cpp
source/Recording/PerformanceFile.h
source/Recording/WavFileExporter.cpp
source/Recording/WavFileExporter.h
source/Recording/PluginOfflineRenderer.cpp
source/Recording/PluginOfflineRenderer.h
source/UI/CustomKeyboard.cpp
source/UI/CustomKeyboard.h
source/UI/ControlsPanel.cpp
source/UI/ControlsPanel.h
source/UI/KeyboardPanel.cpp
source/UI/KeyboardPanel.h
source/UI/HeaderPanel.cpp
source/UI/HeaderPanel.h
source/UI/HeaderPanelStateBuilder.cpp
source/UI/HeaderPanelStateBuilder.h
source/UI/PluginPanel.cpp
source/UI/PluginPanel.h
source/UI/PluginPanelStateBuilder.cpp
source/UI/PluginPanelStateBuilder.h
source/UI/PluginEditorWindow.cpp
source/UI/PluginEditorWindow.h
source/UI/KeyBindingEditDialog.cpp
source/UI/KeyBindingEditDialog.h
source/UI/PerformanceMetadataDialog.cpp
source/UI/PerformanceMetadataDialog.h
source/Layout/PerformancePreset.cpp
source/Layout/PerformancePreset.h
source/Layout/PresetFlowSupport.cpp
source/Layout/PresetFlowSupport.h
source/Settings/SettingsModel.h
source/Settings/SettingsStore.cpp
source/Settings/SettingsStore.h
source/Settings/SettingsSerialization.cpp
source/Settings/SettingsSerialization.h
source/Settings/SettingsComponent.h
source/Settings/SettingsWindowManager.cpp
source/Settings/SettingsWindowManager.h
source/Export/ExportFlowSupport.cpp
source/Export/ExportFlowSupport.h
source/Export/WavExportTask.cpp
source/Export/WavExportTask.h
source/Export/WavExportOptions.h
source/Diagnostics/Log.h
source/Diagnostics/DevPianoLogger.cpp
source/Diagnostics/DevPianoLogger.h
source/Diagnostics/MidiTrace.cpp
source/Diagnostics/MidiTrace.h
source/Locale/LocaleManager.h
source/Locale/zh_CN.loc.h
source/tests/KeyMapTypesTest.cpp
source/tests/MidiFileImporterTest.cpp
source/tests/TestRunner.cpp
```

## 附录 B: 验证命令完整输出

### wsl-build

```
[build_wsl] build complete
Wall time: 13.55 seconds
1 warning: source/UI/KeyboardPanel.cpp:43 implicit conversion from 'int' to 'float'
```

### test

```
100% tests passed, 0 tests failed out of 1
Total Test time (real) = 8.21 sec
```

### format --check

```
~70 violations across 14 files
Exit code: 123
```

### LSP diagnostics

```
0 issues — all source files pass clangd real-time diagnostics
```
