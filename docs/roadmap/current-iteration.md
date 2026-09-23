# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

**Phase 34：键盘演奏交互质变与演奏表现力增强 (Keyboard Performance UX & Expressive Control) [进行中，2026-09-15 ~]**

*(注：AUDIT-003 全面代码质量审计修复于 2026-09-15 全部完成并归档，包含 Linux 无头单测 socket 溢出消除、PluginOfflineRenderer 挂载房间混响对齐、Core/AppState 纯数据单向解耦、MidiTextDecoder 双重编码预分配优化及双平台全量回归。详细完成记录见 [`../archive/audit-003-code-quality-fix-phases.md`](../archive/audit-003-code-quality-fix-phases.md)。)*

在 AUDIT-003 完成后，devpiano 的工程基座（60.2 万断言全绿、三闸门合规、Windows MSVC 验证 0 错误 0 警告）已完全夯实。
基于近期对经典项目 FreePiano 及现代开源架构生态（JUCE AudioPluginHost, Kushview Element, Surge XT, Helio, VMPK, Pianoteq）的深度调研与架构裁定，devpiano 正式确立了**“专用钢琴演奏宿主（Dedicated Piano Performance Host）而非通用 DAW”**的系统定位。
本轮迭代聚焦于电脑键盘演奏人机交互的痛点消除与演奏表现力跃升，实施 5 个阶段的阶梯式落地。

---

## 核心边界与铁律约束 (Boundaries & Iron Rules)

本轮迭代全过程必须无条件遵守以下核心边界与工程铁律：

1. **铁律 1（固定音频拓扑，坚决不向 DAW 蔓延）**：
   - 音频拓扑严格限定为 `Performance Input -> Instrument -> Master -> Output` 单向管道；
   - 严禁引入任何通用 Patchbay 节点网络、多轨 DAW 时间线或视频编解码录制栈。
2. **铁律 2（发音身份恒定原则，绝对杜绝悬挂音）**：
   - 键盘映射表切换或 Group 动态平移，绝不可破坏已发出的 NoteOn 事件；
   - NoteOff 发送时必须 100% 使用 NoteOn 触发时锁定的发音身份（Pitch, Channel）快照。
3. **铁律 3（修饰符瞬态原则，严禁污染持久化设置）**：
   - `Press` 修饰键（Shift/Alt）仅作为事件流变换（Event-time Transformation）介入，动态计算 NoteOn 力度或音高，严禁突变底层持久化配置（Settings Mutation）。
4. **铁律 4（确定性采样级踏板时序，杜绝物理时钟延迟）**：
   - `Sync` 切分踏板基于音频块（Audio Block）内的**采样点偏移（Sample Offset）与严格事件排序**实现，严禁引入 `sleep` 或真实物理时钟延迟；
   - 确保内置物理建模音源与宿主第三方 VST3 插件呈现完全一致的连奏听感。
5. **铁律 5（QWERTY 视图单一事实源，作为 Performance Map 呈现）**：
   - QWERTY Visualizer 必须直接单向消费 `KeyboardMidiMapper` 暴露的 ViewModel，严禁在 UI 侧自行维护或二次计算 MIDI 映射；
   - 严格呈现为“按键-音符/唱名映射看板”，遵循 5 行功能网格规范，杜绝低价值的 3D 拟物键盘渲染与粒子特效。
6. **铁律 6（实时音频线程无锁与零分配契约）**：
   - 所有在音频回调路径中流转的状态（Group、踏板策略、修饰状态）必须保持原子性与预分配，遵守“Zero Lock / Zero Allocation in Audio Callback”铁律。
7. **铁律 7（严格三闸门基线与双平台 MSVC 验证）**：
   - 任何阶段变更后必须满足：`./scripts/dev.sh format --check` 全绿、`./scripts/dev.sh test` 全量断言通过、Windows MSVC 纯净构建验证通过。

---

## 阶段规划详案 (Execution Roadmap)

### Phase 34-A：QWERTY Visualizer（5 行 Performance Map 声明式卡片）[已完成，2026-09-22]

> 目标：在主界面新增自适应、可折叠的 5 行电脑键盘物理映射卡片，彻底消灭初学者“电脑按键与钢琴琴键对应”的盲弹认知成本。

- [x] **Phase 34-A-1：设计并实现 QWERTY ViewModel 接口与映射快照**：
  - 在 `source/Core/QwertyModel.h` 定义只读 `QwertyKeyVisualState` 与 5 行 `QwertyViewModel`，音乐理论基础算法下沉解耦至 `source/Core/MusicTheory.h`；
  - `KeyboardMidiMapper::createQwertySnapshot` 实现只读快照生成方法，以 `layout`、`heldKeys` 及 `keySignature` 为单一事实源零堆分配同步。
- [x] **Phase 34-A-2：在 JIVE 声明式 UI 体系中构建 5 行 QWERTY 键盘网格卡片**：
  - 构建 `QwertyComponent` 原生自绘组件与 5 行 ANSI 物理键位网格（每行权重 15.0f 严格对齐）；
  - 在 `source/UI/jive/LayoutModel.cpp` 中构建声明式 `makeQwertyCardTree()`，插入于 `ControlsPanel` 与 `KeyboardArea` 之间；
  - 支持一键折叠/展开并在 `SettingsModel` 中持久化记录展开状态（`qwertyVisualizerExpanded`）。
- [x] **Phase 34-A-3：双向交互与余晖联动动画**：
  - 物理键盘按下时，QWERTY 视觉方块物理下沉并高亮，与 88 键虚拟钢琴键盘同频联动；松开后呈现 50fps 平滑模拟荧光余晖淡出；
  - 支持鼠标点击发音与右键绑定编辑联动。
- [x] **Phase 34-A-4：12 半音 Pitch Class 和声调色板与投影联动（Harmony Projection）**：
  - 在 `source/Core/MusicTheory.h` 中建立 12-TET 半音阶和声色环（`pitchClassHarmonyHues`）与对比度算法（`getContrastingTextColour`）；
  - `CustomKeyboard`（88 键钢琴）支持 `KeyColourMode::harmony`，并在设置下拉菜单中暴露；
  - `QwertyComponent` 全面接入和声调色板：静态音名/唱名呈现微妙和声音色提示，动态击键与 88 键钢琴同频绽放三和弦几何色相并平滑余晖淡出；
  - 编写 `QwertyViewModelTest` 专项单测验证色相间隔、八度同色、三全音互补及全量回归。

---

### Phase 34-B：Layout Group 轻量多键组与 HeldKey Identity 状态快照机制 [已完成，2026-09-22]

> 目标：实现单 Preset 内 2~4 个轻量键位分组（Group）的毫秒级即时切换，同时以发音身份快照彻底封死悬挂音隐患。

- [x] **Phase 34-B-1：引入 `KeyGroup` 数据模型与快照存储**：
  - 在 `source/Core/KeyMapTypes.h` 中引入 `struct KeyGroup { int8_t transposeOffset; int8_t octaveShift; uint8_t channel; juce::String name; };` 与发音计算辅助函数；
  - `KeyboardLayout` 支持 `std::array<KeyGroup, 4>` 极简分组，零破坏接入现有预设管线。
- [x] **Phase 34-B-2：重构发音身份快照（Note-off Identity Preservation）**：
  - 引入 `HeldKeyIdentity { physicalKeyCode, soundingMidiNote, soundingMidiChannel, velocity }`；
  - 按键按下（NoteOn）时，计算当前激活 Group 下的发声音高与通道并存入快照；
  - 按键松开（NoteOff）时，100% 依据按下时记录的快照信息注销，与当前 Group 解耦；孤儿键扫描机制杜绝绑定删除悬挂；
  - 支持反引号键（`` ` ``）与 UI 胶囊按钮（`qwerty-group-btn`）即时循环切组并在状态栏与 QWERTY 看板实时联动。
- [x] **Phase 34-B-3：Group 动态切换与防悬挂确定性测试集**：
  - 在 `KeyboardMidiMapperTest` 中新增 `LayoutGroupAndHeldKeyIdentityTest`，严格覆盖 Group 循环切换、按住键切组松开注销、通道覆盖注销与多 Group 异构键 Panic 释放，断言 0 悬挂音。

---

### Phase 34-C：SustainPolicy 与 Sample-Accurate 事件级 Sync 切分踏板 [已完成，2026-09-22]

> 目标：引入钢琴演奏学中的“切分踏板（Legato / Sync Pedal）”机制，消除空格键踩放时的断音空洞。

- [x] **Phase 34-C-1：定义 `SustainPolicy` 状态模型**：
  - 在 `source/Core/KeyMapTypes.h` 中定义枚举 `SustainPolicy { normal, syncPedal }`；
  - 在 `KeyboardMidiMapper` 中增加可配置的踏板策略选择，支持挂起切断状态追踪（`syncPedalCutPending`），并在 `SettingsModel` / `SettingsStore` 中持久化记录。
- [x] **Phase 34-C-2：实现音频块内的采样精确切分踏板时序**：
  - 在 `source/Audio/SyncPedalProcessor.h` 中实现无锁、零堆内存分配的切分踏板调度器；
  - 挂接进 `AudioEngine::getNextAudioBlock` 渲染管线，在同一采样点处严格按顺序生成事件：
    $$\text{CC64}(0) \longrightarrow \text{NoteOn}(\text{newNote}) \longrightarrow \text{CC64}(127)$$
  - 内置物理建模音源与 VST3 插件、录音引擎端到端对齐，彻底杜绝物理线程 sleep。
- [x] **Phase 34-C-3：踏板时序与连奏听感确定性测试**：
  - 编写 `SyncPedalTest` 专项单测，全面覆盖正常透传、采样精确相对偏移、切断挂起触发、块内多音切分与状态机整合。

---

### Phase 34-D：PerformanceModifierState 瞬态 Press 修饰符（事件流变换） [已完成，2026-09-22]

> 目标：支持修饰键（如 Shift / Alt）按住期间的瞬态力度拉满或移调变换，松开后自动回弹基线。

- [x] **Phase 34-D-1：设计 `PerformanceModifierState` 事件变换管道**：
  - 在 `source/Core/KeyMapTypes.h` 中建立纯瞬态数据管道 `PerformanceModifierState`，实现只读纯函数变换（`transformVelocity`、`transformPitch`）；
  - 严格限定为事件变换（Event Transformation），与 `KeyboardLayout` / `SettingsModel` 持久化配置彻底解耦。
- [x] **Phase 34-D-2：修饰键集成与事件注入**：
  - 在 `KeyboardMidiMapper` 中捕获 Shift/Alt/Ctrl 修饰状态，NoteOn 时将修饰后的发音身份存入快照（铁律 10 联合保障），松开修饰键后再松按键绝不悬挂；
  - QWERTY 键盘卡片实时下沉高亮点亮修饰键并展示 HUD 标签（`Shift [BOOST]`、`Alt [+8va]`）；
  - 编写 `PerformanceModifierTest` 专项单测，全面验证力度拉满、八度平移、持音中途释放修饰键防悬挂与基线配置 100% 零突变。

---

### Phase 34-E：扫描器增量持久化（Crash-safe State Persistence）与乐器端点概念收敛 [已完成，2026-09-22]

> 目标：吸收官方 Host 与 Element 的生产级工程精髓，提升第三方插件容灾鲁棒性与乐器抽象纯净度。

- [x] **Phase 34-E-1：插件扫描器的增量持久化（Crash-safe Scanner Persistence）**：
  - `PluginHost` 新增 `ScanIncrementalCallback`：`advanceVst3ScanStep()` 每发现新插件、`cancelVst3ScanSession()` 取消、`addVst3FileToKnownList()` 单文件导入均立即回调，`PluginOperationController` 随即同步写入 `knownPluginListState`，崩溃不再丢弃整轮扫描成果；
  - 扫描目标在首个插件被探测前即落盘；`beginVst3ScanSession()` 读取 dead-man's pedal 并把崩溃插件加入黑名单推迟到序列末尾，避免反复卡死在同一入口；
  - `PluginScanPersistenceTest` 覆盖 pedal 恢复、增量回调语义与未注册回调时的零副作用。
- [x] **Phase 34-E-2：Seam-first 乐器端点（Instrument Endpoint）概念收敛**：
  - 新增 `source/Audio/InstrumentEndpoint.h`：以 `resolveInstrumentEndpoint()` 无锁解析当前乐器端点（种类 / 宿主实例 / 描述 / 就绪 / 通道几何），统一 `AudioEngine` 设备准备与实时渲染、`RecordingSessionController` 离线导出中的重复判断；
  - 离线侧新增 `renderTakeThroughInstrumentEndpoint()` 端点路由，`WavExportTask` 不再自持 `offlinePlugin != nullptr ? ... : ...` 二元分支；
  - `InstrumentEndpointTest` 覆盖端点解析回落、通道几何与就绪语义、离线双路由与空 take 拒绝。


---

### Phase 34-F：跨平台实现深度收敛与 JUCE 9 原生框架利用全面升级 (Cross-Platform & JUCE 9 Convergence) [已完成，2026-09-23]

> 目标：对全库 14 个业务子模块进行系统性跨平台与 JUCE 9 框架利用深度审计，消除不必要的手写封装与自造轮子，收敛平台特化代码至最小且必要的集合。  
> 归档记录详见：[`../archive/cross-platform-and-juce9-convergence.md`](../archive/cross-platform-and-juce9-convergence.md)。

- [x] **Phase 34-F-1：源码 7-bit ASCII 规范化与废弃 AlertWindow 绘制代码清理 (`QUAL-001`, `QUAL-002`, `JUCE-003`)**：
  - `StyleCatalogTest.cpp`、`MidiChannelMapperTest.cpp` 与 `KeyboardMidiMapperTest.cpp` 消除裸中文与 Unicode 箭头，全库字符串字面量 100% 达到 Strict 7-bit ASCII 铁律，消除 Windows/MSVC 编译乱码与断言崩溃隐患；
  - `DevPianoLookAndFeel` 彻底删除对 `juce::AlertWindow` 的废弃重写方法与颜色配置；
  - 清理内化 JIVE 核心源码中残留的 `#if JUCE_MAJOR_VERSION >= 8` 历史版本宏。
- [x] **Phase 34-F-2：JUCE 9 原生合法文件名替换与运行时数据目录大小写归一 (`JUCE-001`, `PLAT-003`, `ARCH-001`)**：
  - `PerformancePreset.cpp` 以 JUCE 9 原生 `juce::File::createLegalFileName` 取代手写 ASCII 过滤轮子 `sanitisePresetFileName`，天然解锁中文与 Unicode 预设名称在 Windows/Linux 上的合法落盘；
  - `PluginHost.cpp` 与 `DevPianoLogger.cpp` 数据目录名称从小写 `"devpiano"` 统一为 `"DevPiano"`，根治 Linux 大小写敏感文件系统下的双目录分裂；
  - `PresetFlowSupport.cpp` 与 `RecordingSessionController.cpp` 直接内联调用声明式 `JiveModalDialog`，彻底删除 4 个薄转发空壳类源文件并更新 CMake 配置。
- [x] **Phase 34-F-3：Main.cpp 原生 Hook 消除与纯净跨平台化 (`PLAT-001`)**：
  - 彻底拔除 `source/Main.cpp` 中的 Windows `WNDPROC` 钩子、`AttachThreadInput` 与全局静态指针，移除 `#include <windows.h>`，顶层 Shell 跨平台纯度达到 100%；
  - 全平台统一采用 JUCE 9 原生 `DocumentWindow::activeWindowStatusChanged()` 配合 `callAsync` 延后分发 `restoreKeyboardFocus()`，窗口前台化使用 `toFront(true)` 与 `juce::Process::makeForegroundProcess()`；
  - 同步更新 `known-issues.md` 将 `PLAT-001` 转入已修复清单。
- [x] **Phase 34-F-4：WavExportTask 完全异步化与模态循环解耦 (`JUCE-002`)**：
  - `WavExportTask` 演进为现代化非阻塞异步任务模型（`startAsync(onComplete)`），彻底消除主线程嵌套消息循环 `runDispatchLoopUntil(10)` 与主线程 `Thread::sleep(10)`；
  - `RecordingSessionController::handleExportWavClicked()` 完全非阻塞化；
  - 从 `CMakeLists.txt` 中主应用 `devpiano` 编译配置中彻底移除 `JUCE_MODAL_LOOPS_PERMITTED=1` 编译宏。

---

## 历史实现 Backlog

- 跨平台实现收敛与 JUCE 9 框架深度利用阶段归档：[`../archive/cross-platform-and-juce9-convergence.md`](../archive/cross-platform-and-juce9-convergence.md)

- AUDIT-003 修复阶段归档（全面代码质量审计缺陷消除与架构对齐）：[`../archive/audit-003-code-quality-fix-phases.md`](../archive/audit-003-code-quality-fix-phases.md)
- Phase 33 完成记录（可观测性加固与生产级诊断基础设施）：[`../archive/phase33-observability-and-diagnostics-infrastructure.md`](../archive/phase33-observability-and-diagnostics-infrastructure.md)
- Phase 30 ~ 32 完成记录（古典调律、空间声学与微观机械拟真三部曲）：[`../archive/phase30-32-temperaments-spatial-mechanics.md`](../archive/phase30-32-temperaments-spatial-mechanics.md)
- Phase 29 完成记录（现实物理演奏交互与声学控制）：[`../archive/phase29-physical-voicing-and-acoustic-interaction.md`](../archive/phase29-physical-voicing-and-acoustic-interaction.md)
- Phase 28 完成记录（Devpiano 声明式 UI 基础设施深度治理与接口冻结）：[`../archive/phase28-ui-governance-and-api-freeze.md`](../archive/phase28-ui-governance-and-api-freeze.md)
- Phase 27 完成记录（JUCE 9.0.1 框架升级、UI 基础设施内化与全平台生态演进）：[`../archive/phase27-juce9-upgrade-and-ui-internalization.md`](../archive/phase27-juce9-upgrade-and-ui-internalization.md)
- ADR-014 实施归档（内化 Devpiano UI 基础设施与 JIVE 子模块退役治理）：[`../archive/adr-014-internalize-ui-infrastructure.md`](../archive/adr-014-internalize-ui-infrastructure.md)
- AUDIT-002 修复阶段归档（全量 62 项缺陷修复与质量门禁闭环）：[`../archive/audit-002-code-quality-fix-phases.md`](../archive/audit-002-code-quality-fix-phases.md)
- Phase 26 完成记录（MIDI 多轨并轨与综合时间线合并）：[`../archive/phase26-midi-multi-track-timeline-merge.md`](../archive/phase26-midi-multi-track-timeline-merge.md)
- Phase 25 完成记录（Linux 原生桌面构建与音频驱动适配）：[`../archive/phase25-linux-desktop-and-audio-path.md`](../archive/phase25-linux-desktop-and-audio-path.md)
- Post-v1.0.0 文档体系治理与打包流水线自动化完成记录：[`../guides/release-workflow.md`](../guides/release-workflow.md)
- Phase 24 完成记录（生命力与非线性动力学绽放）：[`../archive/phase24-vitality-and-dynamic-blooming.md`](../archive/phase24-vitality-and-dynamic-blooming.md)
- Phase 23 完成记录（大师级音色校准与 Pianoteq 对齐精调）：[`../archive/phase23-master-voicing-realism-calibration.md`](../archive/phase23-master-voicing-realism-calibration.md)
- Phase 22 完成记录（物理声学极致深化与机械拟真）：[`../archive/phase22-physical-modeling-acoustic-refinement.md`](../archive/phase22-physical-modeling-acoustic-refinement.md)
- Phase 21 完成记录（踏板交感共鸣与琴盖空间声学）：[`../archive/phase21-sympathetic-resonance-lid-acoustics.md`](../archive/phase21-sympathetic-resonance-lid-acoustics.md)
- Phase 11 完成记录（声明式 UI 架构）：[`../archive/phase11-declarative-ui-jive.md`](../archive/phase11-declarative-ui-jive.md)
