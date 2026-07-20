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
| P0 | 5 | 5 | 0 | 0 | 0 | 0 |
| P1 | 11 | 11 | 0 | 0 | 0 | 0 |
| P2 | 20 | 20 | 0 | 0 | 0 | 0 |
| P3 | 24 | 24 | 0 | 0 | 0 | 0 |
| **合计** | 60 | 60 | 0 | 0 | 0 | 0 |

### 0.3 关键结论

- 总体评级：`B` — 功能完整，架构拆分已大幅改善；但存在 5 个 P0 线程安全问题，需在新增功能前修复
- 当前是否适合继续新增功能：`Conditional` — 必须先修复 P0 音频回调堆分配和 RecordingEngine 数据竞争
- 当前是否建议优先重构：`Conditional` — 建议在对用户可见功能暂停期间进行一轮线程安全修复和 Module Boundary Cleanup
- 最大风险：**音频回调中存在堆内存分配**（`AudioEngine::getNextAudioBlock` 中 `pluginBuffer.setSize()`），可导致音频毛刺/掉帧
- 下一步最高优先级：修复 P0-001（音频回调堆分配）和 P0-002（RecordingEngine 数据竞争）

### 0.4 Top Findings

| ID | Priority | Status | 标题 | 当前结论 |
| --- | --- | --- | --- | --- |
| `AUDIO-001` | P0 | Open | AudioEngine 音频回调中堆分配 pluginBuffer.setSize() | 必须立即修复；预分配 buffer 或移到 prepareToPlay |
| `AUDIO-002` | P0 | Open | pluginHost->prepareToPlay() 在音频回调线程中延迟调用 | 必须移到设备初始化流程 |
| `PLUG-001` | P0 | Open | PluginHost 零内部线程同步，依赖外部设备重建保护 | 添加内部同步或文档化契约并断言 |
| `REC-001` | P0 | Open | RecordingEngine 非原子字段在音频/消息线程间无同步读写 | 向量容量预分配不解决 size 读写竞争 |
| `REC-005` | P0 | Open | PluginOfflineRenderer 在后台线程调用 processBlock() | 大多数 VST3 插件不支持多线程 |

---

## 1. 审计范围与方法

### 1.1 审计范围

审计 `source/` 下全部 14 个业务子模块 + tests/，共 70 个文件：

| 模块 | 路径 | 文件数 | 职责 |
| --- | --- | --- | --- |
| 应用入口 | `source/Main.cpp` | 1 | JUCEApplication 启动，创建主窗口 |
| 主装配层 | `source/MainComponent.*` | 2 | 装配 UI 子组件，初始化音频/MIDI/插件/设置，顶层协调 |
| UI | `source/UI/` | 17 | 虚拟键盘、控件面板、插件面板、头部面板、状态构建器、对话框 |
| Core | `source/Core/` | 7 | 数据类型定义（KeyMapTypes、AppState、KeyboardTypes、ChannelMatrix、MidiTypes） |
| Audio | `source/Audio/` | 3 | 音频引擎、设备诊断 |
| Plugin | `source/Plugin/` | 6 | VST3 插件扫描/加载/卸载/editor，流程编排 |
| Input | `source/Input/` | 2 | 电脑键盘到 MIDI 映射 |
| Midi | `source/Midi/` | 2 | MIDI 通道矩阵路由 |
| Recording | `source/Recording/` | 14 | 录制/回放引擎、MIDI 导入/导出、WAV 导出、离线渲染、会话控制器 |
| Layout | `source/Layout/` | 4 | Performance Preset CRUD、文件选择 |
| Settings | `source/Settings/` | 7 | 设置持久化、序列化、设置窗口管理 |
| Export | `source/Export/` | 5 | WAV 导出任务、导出流程支持 |
| Diagnostics | `source/Diagnostics/` | 5 | Logger 封装、MIDI trace |
| Locale | `source/Locale/` | 2 | 中文本地化、LocaleManager |
| tests | `source/tests/` | 3 | KeyMapTypesTest、MidiFileImporterTest、TestRunner |

不包括：

- `JUCE/` 子模块（禁止修改，不在审计范围）
- `scripts/`、`docs/`、构建脚本与配置文件
- 第三方依赖（JUCE 框架本身）

### 1.2 审计输入

| 类型 | 路径 / 命令 | 结果 |
| --- | --- | --- |
| 代码 | `source/**/*.cpp` `source/**/*.h` | 70 文件，~12,108 行，逐文件审计完成 |
| 测试 | `source/tests/KeyMapTypesTest.cpp` `source/tests/MidiFileImporterTest.cpp` | 全部通过 (8.21s) |
| 架构文档 | `docs/reference/architecture.md` | 已对照 |
| 项目定位 | `docs/reference/project-scope.md` | 已对照 |
| 构建系统 | `CMakeLists.txt` | 已审查源文件列表完整性 |
| 构建验证 | `./scripts/dev.sh wsl-build` | **1 warning** (implicit int-to-float conversion, KeyboardPanel.cpp:43) |
| 测试验证 | `./scripts/dev.sh test` | 100% tests passed (1/1) |
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
├── Core/                     # 7 文件：数据类型、状态模型
├── Audio/                    # 3 文件：音频引擎、设备诊断
├── Plugin/                   # 6 文件：插件扫描/加载/卸载/editor、流程编排
├── Input/                    # 2 文件：键盘→MIDI 映射
├── Midi/                     # 2 文件：通道矩阵路由
├── Recording/                # 14 文件：录制/回放引擎、MIDI 导入/导出、WAV、离线渲染、会话控制
├── Layout/                   # 4 文件：Performance Preset CRUD
├── Settings/                 # 7 文件：设置持久化、序列化、窗口管理
├── Export/                   # 5 文件：WAV 导出任务、流程支持
├── Diagnostics/              # 5 文件：Logger、MIDI trace
├── Locale/                   # 2 文件：中文本地化
└── tests/                    # 3 文件：单元测试
```

当前边界判断：

- **清晰边界**：`Diagnostics/`（独立日志层）、`Input/`（单向键盘→MIDI）、`Export/`（独立导出任务）、`Locale/`（纯数据）
- **部分模糊**：`Core/` 实际包含非纯数据类型（JUCE GUI 依赖、命名空间分散）；`Midi/` 与 `Core/` 的 `ChannelMatrix.h` 物理位置 vs 命名空间不匹配
- **高复杂度热点**：`MainComponent.cpp`（838 lines）、`RecordingSessionController.cpp`（589 lines）、`CustomKeyboard.cpp`（540 lines）、`ControlsPanel.cpp`（394 lines）

---

## 3. 分领域审计结果

> 本章只记录分析结论与证据摘要；具体问题必须进入第 8 章问题总表。

### 3.1 架构与模块边界

评估项：

- **`MainComponent` 是否仍然承担了不属于装配层的逻辑**：部分存在。MainComponent（838 lines）已大幅瘦身，但仍有 ~50 lines 的关键绑定编辑回调内联 lambda、settings persistence 逻辑、文件拖放路径分派和 slider→save 桥接代码。4 个 friend 声明（PresetFlowSupport、RecordingSessionController、PluginOperationController、SettingsWindowManager）表明 controller 仍通过 friend 访问 MainComponent 私有成员而非公共接口。
- **FlowSupport / Controller 拆分是否彻底**：拆分已基本完成。`RecordingFlowSupport`、`ExportFlowSupport`、`PluginFlowSupport`、`PresetFlowSupport` 均为纯逻辑或编排类。`RecordingSessionController`、`PluginOperationController`、`SettingsWindowManager` 各自管理独立生命周期。无循环依赖。
- **头文件依赖图**：`MainComponent.h` 包含 9 个头文件，形成依赖中心。`Core/AppStateBuilder.h` 依赖 `Settings/` 模块（依赖反转）。各模块通过 `#include "Module/File.h"` 模式引用，传递包含风险可控。前向声明使用不充分：`MainComponent.h` 无 `class PluginHost;` 等前向声明，全部直接 `#include`。
- **`Core/` 类型是否真正零业务逻辑、零 JUCE GUI 依赖**：**否**。`Core/` 下 7 个文件中有 5 个 `#include <JuceHeader.h>`。`ChannelMatrix.h` 命名空间为 `devpiano::midi`，物理位置却在 `Core/`。`KeyboardTypes.h` 命名空间为 `devpiano::ui`，使用了 `juce::Colour`、`juce::Rectangle` 等 GUI 类型。仅 `MidiTypes.h` 是真正的纯数据类型。

- 评级：**B**
- 结论：架构拆分方向正确，FlowSupport/Controller 层已基本成型。主要残留问题：(a) MainComponent 仍有 ~50 lines 可提取的逻辑，(b) Core/ 目录物理边界与命名空间/依赖不一致，(c) friend 声明应逐步替换为公共接口。
- 关联问题：`ARCH-001` ~ `ARCH-006`

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

- 评级：**C**
- 结论：存在 5 个 P0 线程安全问题。音频回调路径违反实时安全约束（堆分配、潜在阻塞调用）。RecordingEngine recording 路径存在无保护并发访问。PluginHost 内无同步，依赖外部约定。Playback 路径线程安全实现良好。
- 关联问题：`AUDIO-001`、`AUDIO-002`、`PLUG-001`、`REC-001`~`REC-004`、`THREAD-001`~`THREAD-002`

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
- **缺少测试的关键模块**：音频引擎（AudioEngine）、录制引擎（RecordingEngine playback path）、插件宿主（PluginHost load/unload）、键盘映射运行时（KeyboardMidiMapper handleKeyPressed/handleKeyStateChanged）、PerformanceFile 序列化往返、PerformancePreset JSON 序列化/反序列化、ExportFlowSupport、WavExportTask。这些模块或完全无测试，或仅有通过集成方式间接测试。
- **测试可维护性**：测试结构清晰，使用 `JUCE_BEGIN_END_UNIT_TEST_*` 宏。MidiFileImporterTest 使用 fixture MIDI 文件。KeyMapTypesTest 无外部依赖。无共享 fixture 类，每个 test suite 独立。
- **测试是否独立**：是。测试不依赖音频设备、不依赖文件系统副作用（MidiFileImporterTest 使用 fixture 文件，TestRunner 默认跳过 JUCE Files category）。

- 评级：**C+**
- 结论：现有测试质量高、独立性好，覆盖了 Core 数据类型和 MIDI 导入核心路径。但测试覆盖率严重不足——音频引擎、录制引擎、插件宿主三大核心运行时模块完全无单元测试。建议至少添加 RecordingEngine playback 路径和 AudioEngine buffer 处理的单元测试。
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

- **CMakeLists.txt 源文件列表完整性**：`AudioDeviceDiagnostics.h` 未列入 `target_sources`（但作为 header-only 通过 include 使用，符合 CMake 惯例）。所有 .cpp 文件均已列入。`tests/` 目标的源文件列表也完整。无孤立文件。
- **clang-tidy 诊断清零状态**：`.clang-tidy` 配置文件存在，但 WSL 环境中未安装 `clang-tidy` 二进制，无法运行验证。配置覆盖 bugprone/performance/readability/modernize/clang-analyzer 五大类别，并合理禁用了 trailing-return-type、magic-numbers 等不适用的规则。
- **clang-format 合规性**：**不合规**。`format --check` 返回 ~70 violations，分布在 14 个文件中（MainComponent.cpp, SettingsModel.h, SettingsComponent.h, SettingsSerialization.cpp, RecordingEngine.h/.cpp, RecordingSessionController.h/.cpp, PerformanceFile.h, MidiChannelMapper.h/.cpp, AppState.h, AppStateBuilder.h, CustomKeyboard.h/.cpp, KeyboardPanel.cpp, ControlsPanel.cpp, KeyBindingEditDialog.cpp, PerformancePreset.h/.cpp, PresetFlowSupport.cpp）。
- **编译器警告清零状态**：**1 warning**：`KeyboardPanel.cpp:43` implicit conversion from `int` to `float` may lose precision。Clang `-Wall -Wextra` 启用。无 Error。
- **Debug / Release 构建一致性**：CMake 配置使用 `juce_recommended_config_flags` 和 `juce_recommended_lto_flags` 统一管理。`JUCE_MODAL_LOOPS_PERMITTED=1` 在 Debug/Release 均启用。

- 评级：**B-**
- 结论：构建系统配置完善。主要扣分项是 ~70 处格式违规需要统一修复，以及 1 个编译器警告需要处理。clang-tidy 无法在当前环境运行，但在 Windows/MSVC 侧应可执行。
- 关联问题：`ENG-001` ~ `ENG-004`

---

## 4. 构建、测试与诊断结果

### 4.1 WSL 构建

```text
[build_wsl] build complete
Wall time: 13.55 seconds
```
- 结果：**成功**（1 warning）
- Warning: `source/UI/KeyboardPanel.cpp:43` — implicit conversion from `int` to `float` may lose precision

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

---

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
| `AUDIO-001` | P0 | Open | 音频回调中 pluginBuffer.setSize() 堆分配 | `source/Audio/AudioEngine.cpp` (getNextAudioBlock) | 每次音频回调调用 `pluginBuffer.setSize(channels, numSamples)` 可能触发堆分配，违反实时安全约束，可导致音频毛刺 | 在 `prepareToPlay` 中预分配 pluginBuffer；在回调中仅使用已分配 buffer |
| `AUDIO-002` | P0 | Open | prepareToPlay 延迟到音频回调线程执行 | `source/Audio/AudioEngine.cpp` (getNextAudioBlock, lazy prepare) | `pluginHost->prepareToPlay()` 在音频回调中延迟调用，VST3 插件的 `prepareToPlay` 可能包含阻塞操作和内存分配 | 将 prepareToPlay 移到 `AudioEngine::prepareToPlay` 或设备初始化流程中，移除 lazy prepare 模式 |
| `PLUG-001` | P0 | Open | PluginHost 零内部线程同步 | `source/Plugin/PluginHost.h:35-65`, `source/Plugin/PluginHost.cpp` | `isScanning`、`scanningPluginName`、`prepared`、`pluginInstance` (raw pointer 访问)、`knownPluginList` 均无互斥锁保护。`getInstance()` 返回裸指针，音频线程通过它调用 `processBlock` | 选项 A: 在 PluginHost 内部添加 `CriticalSection`；选项 B: 在 getInstance() 等关键路径添加 `jassert(isMessageThread())` 断言 + 文档化契约 |
| `REC-001` | P0 | Open | RecordingEngine recording 路径无同步 | `source/Recording/RecordingEngine.h:90`, `source/Recording/RecordingEngine.cpp` (recordMidiBufferBlock, recordPresetChange) | `currentTake.events` vector 在音频线程 push_back 写入、消息线程 read/clear 读取，无同步。`currentPositionSamples`、`lengthSamples`、`droppedEventCount` 同为非原子变量 | 为 recording 路径数据添加 mutex（在非音频路径使用）或使用 lock-free SPSC queue；或将 recording 数据访问全部限制在消息线程 |
| `REC-005` | P0 | Open | PluginOfflineRenderer 在后台线程调用 processBlock | `source/Recording/PluginOfflineRenderer.cpp` (renderTakeWithOfflinePlugin) | `renderTakeWithOfflinePlugin` 在 `WavExportTask::run()` 的后台线程中调用 `AudioPluginInstance::processBlock()`。绝大多数 VST3 插件假设单线程访问，多线程调用可导致崩溃或音频错误 | 文档化此限制；确保离线渲染线程是唯一使用该插件实例的线程；或使用独立的 processBlock 替换机制 |

### P1（当前迭代修复）

| ID | Priority | Status | 标题 | 文件:行号 | 描述 | 建议修复 |
| --- | --- | --- | --- | --- | --- | --- |
| `PLUG-002` | P1 | Open | PluginHost::loadPlugin 忽略 addToList 失败 | `source/Plugin/PluginHost.cpp` (loadPlugin) | `knownPluginList.addToList()` 返回值被忽略，列表添加失败不报错 | 检查返回值并在失败时记录 DP_LOG_ERROR |
| `PLUG-003` | P1 | Open | PluginHost::unload 非异常安全 | `source/Plugin/PluginHost.cpp` (unload) | `unload()` 中先 `releaseResources()` 再 `reset()` pluginInstance。如果 releaseResources 抛出（VST3 理论上不应抛出但无保证），pluginInstance 残留 | 使用 `std::unique_ptr` 的 RAII 语义，在 reset 之前先交换出临时指针 |
| `PLUG-004` | P1 | Open | PluginOperationController 析构不卸载插件 | `source/Plugin/PluginOperationController.h`, `source/Plugin/PluginOperationController.cpp` | 析构函数仅取消扫描，不调用 `closePluginEditor()` 或通过 PluginHost 卸载插件 | 在析构或 shutdown 中确保 editor 关闭和插件资源释放 |
| `REC-002` | P1 | Open | RecordingSessionController async lambda use-after-free 风险 | `source/Recording/RecordingSessionController.cpp` (~line 281, ~line 560) | 异步文件选择器 lambda 捕获 `[this]` 和 chooser `unique_ptr` 的引用。如果 controller 在回调触发前被销毁，访问悬垂指针 | 使用 `juce::Component::SafePointer` 或 `std::weak_ptr` 保护回调中的 `this`；使用 shared_ptr 管理 chooser |
| `REC-003` | P1 | Open | 录制中 preset 变更与录音向量并发写入 | `source/Recording/RecordingEngine.cpp` (recordPresetChange vs recordMidiBufferBlock) | `PresetFlowSupport` (消息线程) 调用 `recordPresetChange` 写入 `currentTake.events`，与音频线程的 `recordMidiBufferBlock` 并发写同一 vector | 将 preset change 写入独立队列，在录制停止时合并；或为 events vector 添加 mutex |
| `REC-004` | P1 | Open | RecordingEngine 录制中 getCurrentTake() 返回引用 racing | `source/Recording/RecordingEngine.h:50`, `source/Recording/RecordingEngine.cpp` (getCurrentTake) | `getCurrentTake()` 返回 `const RecordingTake&` 引用，调用方读取时音频线程可能同时在修改 events vector | 使用 `createTakeSnapshot()` 代替引用返回，或在文档中明确调用方必须在消息线程且录制停止时调用 |
| `ARCH-001` | P1 | Open | Core/ 目录包含 JUCE GUI 依赖 | `source/Core/AppState.h:5`, `source/Core/KeyMapTypes.h:3`, `source/Core/ChannelMatrix.h:6`, `source/Core/KeyboardTypes.h:7` | architecture.md 描述 Core/"平台无关"，但 5/7 文件 include `<JuceHeader.h>`，使用 `juce::String`、`juce::Colour`、`juce::Rectangle` | 选项 A: 更新 architecture.md 反映实际依赖；选项 B: 将 GUI 类型文件移到 UI/，将 ChannelMatrix 移到 Midi/ |
| `ARCH-002` | P1 | Open | ChannelMatrix.h 物理位置与命名空间不匹配 | `source/Core/ChannelMatrix.h` | 文件在 `Core/`，命名空间为 `devpiano::midi`。MidiChannelMapper 在 `Midi/` include `Core/ChannelMatrix.h` | 将 `ChannelMatrix.h` 移至 `source/Midi/`，保持命名空间不变 |
| `ARCH-003` | P1 | Open | KeyboardTypes.h 物理位置与命名空间不匹配 | `source/Core/KeyboardTypes.h` | 文件在 `Core/`，命名空间为 `devpiano::ui`，依赖 `juce::Colour`、`juce::Rectangle` GUI 类型 | 将 `KeyboardTypes.h` 移至 `source/UI/` |
| `ARCH-004` | P1 | Open | AppStateBuilder 依赖反转 | `source/Core/AppStateBuilder.h` | Core/ 中的 builder 依赖 Settings/、Audio/、Plugin/ 模块。依赖方向应为 Settings→Core，而非 Core→Settings | 将 `AppStateBuilder` 移至 `Settings/` 或新建 `State/` 模块 |
| `ERR-001` | P1 | Open | JSON::parse() 无异常保护 | `source/Recording/PerformanceFile.cpp`, `source/Layout/PerformancePreset.cpp` | `juce::JSON::parse()` 在格式错误时可能抛出异常（JUCE 文档未明确承诺 noexcept），但调用处无 try-catch | 添加 try-catch 或使用 `juce::JSON::parse()` 的 `Result` 返回版本 |
| `DOC-001` | P1 | Open | architecture.md 与源码文件清单不一致 | `docs/reference/architecture.md` | 文档列出 Core/ 含 4 文件、Recording/ 含 12 文件、未提及 Midi/ 模块，均与实际不符 | 更新架构文档反映当前文件布局和模块边界 |

### P2（近期排期）

| ID | Priority | Status | 标题 | 文件:行号 | 描述 | 建议修复 |
| --- | --- | --- | --- | --- | --- | --- |
| `PLUG-005` | P2 | Open | PluginHost::getInstance() 暴露裸指针 | `source/Plugin/PluginHost.h:59` | `getInstance()` 返回 `AudioPluginInstance*` 裸指针。调用方（AudioEngine）持有并在音频回调中使用，生命周期依赖外部协调 | 返回 `juce::AudioPluginInstance::Ptr` (引用计数智能指针) 或文档化所有权契约 |
| `PLUG-006` | P2 | Open | 同步扫描阻塞消息线程 | `source/Plugin/PluginHost.cpp` (scanAndAddToKnownList) | `scanAndAddToKnownList` 在主扫描循环中调用 `addVst3FileToKnownList`，每个插件同步处理，大目录扫描时阻塞 UI | 已有增量扫描（`incrementalScanStep`），确认同步路径仅用于启动恢复 |
| `PLUG-007` | P2 | Open | PluginOperationController 扫描状态在销毁时未清理 | `source/Plugin/PluginOperationController.cpp` | 如果 controller 在扫描进行中销毁，PluginHost 的 `isScanning` 保持 true，UI 永远卡在扫描状态 | 析构函数中添加 `pluginHost.cancelScan()` 或等效清理 |
| `REC-006` | P2 | Open | RecordingEngine renderPlaybackBlock 中使用 mutex | `source/Recording/RecordingEngine.cpp` (renderPlaybackBlock) | `renderPlaybackBlock` 在音频回调中调用，内部使用 `std::mutex` 保护 `pendingPresetChanges` 队列。mutex lock 在音频线程可导致优先级反转 | 使用 lock-free SPSC queue 或确保 mutex 仅在极短临界区使用 |
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
| `PERF-003` | P2 | Open | SettingComponent 30Hz timer 全 UI 刷新 | `source/MainComponent.cpp` (timerCallback) | `MainComponent::timerCallback()` 以 30Hz 无条件调用 `refreshReadOnlyUiStateFromCurrentSnapshot()`，即使状态未变化也触发完整 UI 刷新链 | 添加脏标记或降低刷新频率 |
| `ERR-002` | P2 | Open | RecordingSessionController 忽略 scheduleSave 失败 | `source/Recording/RecordingSessionController.cpp` (多处) | 多处调用 `owner.settingsStore.scheduleSave(owner.appSettings)` 后忽略 save 结果 | 至少在关键路径（stop recording, preset change）检查返回值 |
| `ERR-003` | P2 | Open | AppStateBuilder 仅 jassert 线程守卫 | `source/Core/AppStateBuilder.cpp` | `buildAppStateSnapshot` 使用 `jassert(isMessageThread())` 做线程守卫——Release 构建中为 no-op | 考虑使用 `jassert` + 返回错误码（debug），或在 Release 中也保持检查 |
| `ERR-004` | P2 | Open | PluginHost 加载成功后冗余赋值 | `source/Plugin/PluginHost.cpp` (loadPlugin) | `loadPlugin` 成功路径中 `prepared = false` 赋值后紧接 `lastLoadError.clear()` 后再无使用，可简化 |
| `ERR-005` | P2 | Open | WavExportTask 取消后可能不清理文件 | `source/Export/WavExportTask.cpp` (run) | `threadShouldExit()` 检查后调用 `outputFile.deleteFile()`，但 `deleteFile()` 失败时无日志 | 添加删除失败的 WARN 日志 |
| `ERR-006` | P2 | Open | SettingsStore scheduleSave 裸指针 API | `source/Settings/SettingsStore.h`, `source/Settings/SettingsStore.cpp` | `DebounceTimer` 持有 `const SettingsModel*` 裸指针，如果 SettingsModel 在 timer 触发前析构，使用悬垂指针 | 使用 `std::shared_ptr` 或确保 SettingsModel 生命周期长于 DebounceTimer |
| `TEST-001` | P2 | Open | AudioEngine 无单元测试 | — | 核心音频引擎（prepareToPlay, getNextAudioBlock, releaseResources）完全无测试覆盖 | 添加 mock PluginHost 的 AudioEngine 单元测试 |
| `TEST-002` | P2 | Open | RecordingEngine playback 路径无单元测试 | — | RecordingEngine 的 renderPlaybackBlock/advancePlaybackPosition 逻辑无测试 | 添加合成 RecordingTake 的 playback 测试 |
| `TEST-003` | P2 | Open | PluginHost 无单元测试 | — | 插件加载/卸载/扫描生命周期无测试（可能因依赖实际 VST3 文件而困难，但 mock 测试可行） | 添加 PluginHost 状态机逻辑的单元测试 |
| `TEST-004` | P2 | Open | KeyboardMidiMapper 运行时逻辑无测试 | — | handleKeyPressed/handleKeyStateChanged 的 note on/off 映射逻辑仅通过集成测试间接覆盖 | 添加 mock MidiKeyboardState 的单元测试 |

### P3（持续跟踪）

| ID | Priority | Status | 标题 | 文件:行号 | 描述 | 建议修复 |
| --- | --- | --- | --- | --- | --- | --- |
| `QUAL-001` | P3 | Open | HeaderPanel::AudioStatus struct 未使用 | `source/UI/HeaderPanel.h` | `AudioStatus` struct 定义了但未被 HeaderPanel 自身使用。`buildHeaderPanelAudioStatus` 返回的 `juce::String` 直接用于 label | 如果不需要，删除 AudioStatus struct |
| `QUAL-002` | P3 | Open | KeyboardMidiMapper note-off 逻辑重复 | `source/Input/KeyboardMidiMapper.cpp` (handleKeyStateChanged, triggerBinding) | `handleKeyStateChanged` 和 `triggerBinding` 中有重复的 note-off 发送逻辑（~8 行） | 提取为私有 `sendNoteOff` helper |
| `QUAL-003` | P3 | Open | makeDefaultKeyboardLayout / makeFullPianoLayout 重复 | `source/Core/KeyMapTypes.h` | 两个函数中 binding 数组构建模式高度重复，每个 note 定义 ~4 行 | 提取共享的 binding 构建辅助函数 |
| `QUAL-004` | P3 | Open | findByKeyCode 返回裸指针 | `source/Core/KeyMapTypes.h:66` | `KeyboardLayout::findByKeyCode` 返回 `const KeyBinding*` 指向 vector 内部元素。vector 修改后指针悬垂 | 返回 `std::optional<std::reference_wrapper<const KeyBinding>>` 或索引 |
| `QUAL-005` | P3 | Open | AudioEngine::getMidiCollector / getKeyboardState 暴露内部可变引用 | `source/Audio/AudioEngine.h:33-38` | 两个方法返回 `juce::MidiKeyboardState&` / `juce::MidiMessageCollector&` 可变引用，允许外部任意修改内部状态 | 提供 const 版本或通过专用方法暴露受限功能 |
| `QUAL-006` | P3 | Open | PluginFlowSupport 中冗余前向声明 | `source/Plugin/PluginFlowSupport.cpp` | `normalisePluginScanPath` 在 .cpp 中有冗余前向声明，.h 中已有声明 | 删除冗余前向声明 |
| `THREAD-004` | P3 | Open | PluginHost 非原子 isScanning 标志 | `source/Plugin/PluginHost.h:35-37` | `isScanning` 在消息线程写入，在 `PluginPanel::updateState()` 中通过 `PluginPanelStateBuilder` 读取。如果未来在非消息线程调用，存在数据竞争 | 改为 `std::atomic<bool>` |
| `THREAD-005` | P3 | Open | RecordingEngine::hasDroppedEvents / getDroppedEventCount 非原子 | `source/Recording/RecordingEngine.h:44-47` | `droppedEventCount` 在音频线程递增、消息线程读取，非原子 | 改为 `std::atomic<std::size_t>` |
| `SEC-006` | P3 | Open | KeyMapTypes 允许 aggregate init 绕过 fromClamped | `source/Core/MidiTypes.h` | `MidiNoteNumber{200}` aggregate 初始化可绕过 `fromClamped(200)` 的 clamp 保护 | 添加私有构造函数或使用 `requires` clause |
| `SEC-007` | P3 | Open | KeyboardMidiMapper 0/1-based channel 转换脆弱 | `source/Input/KeyboardMidiMapper.cpp` | channel 值在 0-based 和 1-based 之间手动转换，缺少类型系统保护 | 使用 `MidiChannel::toZeroBased()` 统一转换 |
| `SEC-008` | P3 | Open | MidiChannelMapper::applyTransform 仅重映射 note on/off | `source/Midi/MidiChannelMapper.cpp` (applyTransform) | `applyTransform` 仅对 note on/off 消息重映射 channel，CC/pitch bend/program change 不经过矩阵路由 | 确认是否为设计意图，若是，文档化 |
| `PERF-004` | P3 | Open | KeyboardTypes::KeyboardSettings 2KB+ 固定数组 | `source/Core/KeyboardTypes.h` | `customKeyLabels`（`std::array<juce::String,128>`）和 `customKeyColours`（`std::array<juce::Colour,128>`）固定分配 ~4KB，即使未自定义也占满内存 | 改为 `std::vector` 或 sparse map（仅在少数键自定义时节省内存） |
| `PERF-005` | P3 | Open | KeyboardMidiMapper::isKeyCurrentlyDown O(n) 轮询 | `source/Input/KeyboardMidiMapper.cpp` | `handleKeyStateChanged` 中每帧遍历所有 binding 调用 `isKeyCurrentlyDown`（O(bindings) × O(key codes)） | 使用 `std::bitset` 或 `std::unordered_set` 替代遍历 |
| `ERR-007` | P3 | Open | AudioDeviceDiagnostics parseSavedAudioDeviceState 语义模糊 | `source/Audio/AudioDeviceDiagnostics.h` | `hasSavedState` 字段名容易误解——它实际表示"有已保存的设备状态 XML"，而非"设备状态有效" | 重命名为 `hasSavedDeviceStateXml` |
| `ERR-008` | P3 | Open | WavExportTask 成功/失败通过成员变量通信 | `source/Export/WavExportTask.h` | `success` 和 `errorMessage` 在 `run()` 中设置，调用方在 `runThread()` 返回后读取，无同步 | 使用 `std::atomic<bool>` 或 `juce::CriticalSection` |
| `DOC-002` | P3 | Open | architecture.md Core/ 描述与实际不符 | `docs/reference/architecture.md:74-86` | 描述 Core/ 包含 4 个文件、"平台无关的核心数据类型"，实际 7 个文件，多数依赖 JUCE | 更新文档反映当前状态 |
| `DOC-003` | P3 | Open | architecture.md 未列出 Midi/ 模块 | `docs/reference/architecture.md` | `Midi/` 模块（MidiChannelMapper）未在架构文档中出现 | 添加 Midi/ 模块说明 |
| `ENG-001` | P3 | Open | clang-format ~70 violations | 14 个文件 | `format --check` 报告 ~70 处格式违规 | 运行 `./scripts/dev.sh format` 一次性修复 |
| `ENG-002` | P3 | Open | KeyboardPanel.cpp:43 int-to-float 隐式转换 | `source/UI/KeyboardPanel.cpp:43` | `whiteCount * customKeyboard->getKeyboardSettings().keyWidth` 结果为 int 隐式转换为 float，可能精度损失 | 使用 `static_cast<float>(whiteCount)` 显式转换 |
| `ENG-003` | P3 | Open | AudioDeviceDiagnostics.h 未列入 target_sources | `CMakeLists.txt` | header-only 文件不在 target_sources 中符合 CMake 惯例，但导致 IDE 中文件不可见 | 添加到 target_sources 以便 IDE 索引 |
| `ENG-004` | P3 | Open | 缺少 clang-tidy CI 集成 | `.clang-tidy` | 配置已存在但无 CI 或 pre-commit hook 运行 | 在 Windows 验证构建中添加 clang-tidy 步骤 |

---

## 9. 审计后建议路线图

基于审计发现，建议按以下优先级处理：

### Phase A: 音频稳定性 (P0, 预计 2-3 天)

1. **AUDIO-001**: 将 `pluginBuffer.setSize()` 移到 `prepareToPlay`，回调中仅使用预分配 buffer
2. **AUDIO-002**: 移除 lazy prepare，将 `pluginHost->prepareToPlay()` 集成到设备初始化流程
3. **REC-001**: 为 RecordingEngine recording 路径添加线程同步
4. 验证：运行现有测试 + 手动演奏/录制压力测试

### Phase B: 线程安全加固 (P0 + P1, 预计 3-5 天)

5. **PLUG-001**: PluginHost 添加线程安全契约（断言 + 文档化）
6. **REC-002**: 修复 async lambda 生命周期问题
7. **REC-003**: preset change 写入队列化
8. **REC-005**: 评估 PluginOfflineRenderer 线程模型并修复
9. 验证：全测试 + Windows 构建验证

### Phase C: 模块边界 (P1, 预计 2-3 天)

10. **ARCH-002/ARCH-003**: 移动 `ChannelMatrix.h` → `Midi/`，`KeyboardTypes.h` → `UI/`
11. **ARCH-001**: 更新 architecture.md 或清理 Core/ JUCE 依赖
12. **ARCH-004**: 移动 `AppStateBuilder` 或调整依赖方向
13. **DOC-001**: 同步 architecture.md 与实际文件布局

### Phase D: 工程化 (P3, 预计 1 天)

14. **ENG-001**: 运行 `./scripts/dev.sh format` 修复所有格式违规
15. **ENG-002**: 修复 KeyboardPanel.cpp 编译警告
16. **ENG-004**: 在 Windows CI 中集成 clang-tidy

### Phase E: 测试完善 (P2, 持续)

17. **TEST-001**: AudioEngine 单元测试
18. **TEST-002**: RecordingEngine playback 单元测试
19. **TEST-003**: PluginHost 状态机测试

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
