# Phase 34 完成记录：键盘演奏交互质变与演奏表现力增强 (Keyboard Performance UX & Expressive Control)

> 归档日期：2026-09-23
> 前序依赖：AUDIT-003（全面代码质量审计缺陷消除与架构对齐）、Phase 30 ~ 33
> 实施范围：`source/Core/`（`QwertyModel.h`, `KeyMapTypes.h`, `MusicTheory.h`）、`source/Input/`（`KeyboardMidiMapper.cpp/.h`）、`source/Audio/`（`SyncPedalProcessor.h`, `InstrumentEndpoint.h`）、`source/UI/`（`QwertyComponent.cpp/.h`, `LayoutModel.cpp`）、`source/Plugin/`、`source/Export/`、`source/tests/`、`source/Main.cpp`
> 交付形式：阶段递进式推进，全量三闸门与全量测试套件验证闭环

---

## 1. 目标与背景

在 AUDIT-003 质量治理全面闭环后，devpiano 的工程基座（覆盖核心引擎、物理声学与 UI 全套自动化测试，零失败、三闸门合规）已完全夯实。
基于对经典开源与专业音频生态（FreePiano、JUCE AudioPluginHost、Kushview Element、Surge XT、Helio、VMPK、Pianoteq）的深度调研与架构裁定，devpiano 正式确立了**“专用钢琴演奏宿主（Dedicated Piano Performance Host）而非通用 DAW”**的系统定位。

Phase 34 聚焦于电脑键盘演奏人机交互的痛点消除与演奏表现力跃升，实施了 6 个阶段的阶梯式落地：
1. **消灭键盘盲弹认知成本**：通过 5 行 ANSI 物理键盘映射看板（QWERTY Visualizer）与 12-TET 和声调色板，直观呈现琴键映射与三和弦色相；
2. **多键位分组与彻底杜绝悬挂音**：单预设 4 组轻量 Group 毫秒级循环切换，以 `HeldKeyIdentity` 发音身份快照在物理按键与注销 NoteOff 间实现 100% 确定性注销；
3. **消除连奏断音空洞**：实现音频块内的采样精确切分踏板调度器（`SyncPedalProcessor`），消灭真实时钟延时；
4. **瞬态演奏修饰符**：Shift 力度拉满与 Alt 高八度平移，纯事件流变换管道，零配置污染；
5. **扫描器容灾与乐器端点收敛**：增量崩溃安全持久化与统一 `InstrumentEndpoint` 领域乐器抽象；
6. **跨平台与 JUCE 9 原生收敛（Phase 34-F）**：拔除 Win32 `WNDPROC` Hook 与 `<windows.h>`，异步非阻塞化 `WavExportTask`，消除 `JUCE_MODAL_LOOPS_PERMITTED`，源码 100% Strict 7-bit ASCII 规范化。

---

## 2. 核心边界与工程铁律 (Boundaries & Iron Rules)

本轮迭代全过程严格遵守并兑现了以下核心边界与工程铁律：

1. **铁律 1（固定音频拓扑，坚决不向 DAW 蔓延）**：
   - 音频拓扑严格限定为 `Performance Input -> Instrument -> Master -> Output` 单向管道；
   - 严禁引入任何通用 Patchbay 节点网络、多轨 DAW 时间线或视频编解码录制栈。
2. **铁律 2（发音身份恒定原则，绝对杜绝悬挂音）**：
   - 键盘映射表切换或 Group 动态平移，绝不可破坏已发出的 NoteOn 事件；
   - NoteOff 发送时必须 100% 使用 NoteOn 触发时锁定的发音身份（Pitch, Channel）快照注销。
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
7. **铁律 7（严格三闸门基线与全量测试闭环）**：
   - 任何阶段变更后必须满足：`./scripts/dev.sh format --check` 全绿、`./scripts/dev.sh test` 全量断言通过、编译 0 错误 0 警告。

---

## 3. 各阶段实施详案与交付成果

### Phase 34-A：QWERTY Visualizer（5 行 Performance Map 声明式卡片）[已完成，2026-09-22]

1. **设计并实现 QWERTY ViewModel 接口与映射快照**：
   - 在 `source/Core/QwertyModel.h` 定义只读 `QwertyKeyVisualState` 与 5 行 `QwertyViewModel`，音乐理论基础算法下沉解耦至 `source/Core/MusicTheory.h`；
   - `KeyboardMidiMapper::createQwertySnapshot` 实现只读快照生成方法，以 `layout`、`heldKeys` 及 `keySignature` 为单一事实源零堆分配同步。
2. **在 JIVE 声明式 UI 体系中构建 5 行 QWERTY 键盘网格卡片**：
   - 构建 `QwertyComponent` 原生自绘组件与 5 行 ANSI 物理键位网格（每行权重 15.0f 严格对齐）；
   - 在 `source/UI/jive/LayoutModel.cpp` 中构建声明式 `makeQwertyCardTree()`，插入于 `ControlsPanel` 与 `KeyboardArea` 之间；
   - 支持一键折叠/展开并在 `SettingsModel` 中持久化记录展开状态（`qwertyVisualizerExpanded`）。
3. **双向交互与余晖联动动画**：
   - 物理键盘按下时，QWERTY 视觉方块物理下沉并高亮，与 88 键虚拟钢琴键盘同频联动；松开后呈现 50fps 平滑模拟荧光余晖淡出；
   - 支持鼠标点击发音与右键绑定编辑联动。
4. **12 半音 Pitch Class 和声调色板与投影联动（Harmony Projection）**：
   - 在 `source/Core/MusicTheory.h` 中建立 12-TET 半音阶和声色环（`pitchClassHarmonyHues`）与对比度算法（`getContrastingTextColour`）；
   - `CustomKeyboard`（88 键钢琴）支持 `KeyColourMode::harmony`，并在设置下拉菜单中暴露；
   - `QwertyComponent` 全面接入和声调色板：静态音名/唱名呈现微妙和声音色提示，动态击键与 88 键钢琴同频绽放三和弦几何色相并平滑余晖淡出；
   - 编写 `QwertyViewModelTest` 专项单测验证色相间隔、八度同色、三全音互补及全量回归。

---

### Phase 34-B：Layout Group 轻量多键组与 HeldKey Identity 状态快照机制 [已完成，2026-09-22]

1. **引入 `KeyGroup` 数据模型与快照存储**：
   - 在 `source/Core/KeyMapTypes.h` 中引入 `struct KeyGroup { int8_t transposeOffset; int8_t octaveShift; uint8_t channel; juce::String name; };` 与发音计算辅助函数；
   - `KeyboardLayout` 支持 `std::array<KeyGroup, 4>` 极简分组，零破坏接入现有预设管线。
2. **重构发音身份快照（Note-off Identity Preservation）**：
   - 引入 `HeldKeyIdentity { physicalKeyCode, soundingMidiNote, soundingMidiChannel, velocity }`；
   - 按键按下（NoteOn）时，计算当前激活 Group 下的发声音高与通道并存入快照；
   - 按键松开（NoteOff）时，100% 依据按下时记录的快照信息注销，与当前 Group 解耦；孤儿键扫描机制杜绝绑定删除悬挂；
   - 支持反引号键（`` ` ``）与 UI 胶囊按钮（`qwerty-group-btn`）即时循环切组并在状态栏与 QWERTY 看板实时联动。
3. **Group 动态切换与防悬挂确定性测试集**：
   - 在 `KeyboardMidiMapperTest` 中新增 `LayoutGroupAndHeldKeyIdentityTest`，严格覆盖 Group 循环切换、按住键切组松开注销、通道覆盖注销与多 Group 异构键 Panic 释放，断言 0 悬挂音。

---

### Phase 34-C：SustainPolicy 与 Sample-Accurate 事件级 Sync 切分踏板 [已完成，2026-09-22]

1. **定义 `SustainPolicy` 状态模型**：
   - 在 `source/Core/KeyMapTypes.h` 中定义枚举 `SustainPolicy { normal, syncPedal }`；
   - 在 `KeyboardMidiMapper` 中增加可配置的踏板策略选择，支持挂起切断状态追踪（`syncPedalCutPending`），并在 `SettingsModel` / `SettingsStore` 中持久化记录。
2. **实现音频块内的采样精确切分踏板时序**：
   - 在 `source/Audio/SyncPedalProcessor.h` 中实现无锁、零堆内存分配的切分踏板调度器；
   - 挂接进 `AudioEngine::getNextAudioBlock` 渲染管线，在同一采样点处严格按顺序生成事件：
     $$\text{CC64}(0) \longrightarrow \text{NoteOn}(\text{newNote}) \longrightarrow \text{CC64}(127)$$
   - 内置物理建模音源与 VST3 插件、录音引擎端到端对齐，彻底杜绝物理线程 sleep。
3. **踏板时序与连奏听感确定性测试**：
   - 编写 `SyncPedalTest` 专项单测，全面覆盖正常透传、采样精确相对偏移、切断挂起触发、块内多音切分与状态机整合。

---

### Phase 34-D：PerformanceModifierState 瞬态 Press 修饰符（事件流变换） [已完成，2026-09-22]

1. **设计 `PerformanceModifierState` 事件变换管道**：
   - 在 `source/Core/KeyMapTypes.h` 中建立纯瞬态数据管道 `PerformanceModifierState`，实现只读纯函数变换（`transformVelocity`、`transformPitch`）；
   - 严格限定为事件变换（Event Transformation），与 `KeyboardLayout` / `SettingsModel` 持久化配置彻底解耦。
2. **修饰键集成与事件注入**：
   - 在 `KeyboardMidiMapper` 中捕获 Shift/Alt/Ctrl 修饰状态，NoteOn 时将修饰后的发音身份存入快照（铁律 2 联合保障），松开修饰键后再松按键绝不悬挂；
   - QWERTY 键盘卡片实时下沉高亮点亮修饰键并展示 HUD 标签（`Shift [BOOST]`、`Alt [+8va]`）；
   - 编写 `PerformanceModifierTest` 专项单测，全面验证力度拉满、八度平移、持音中途释放修饰键防悬挂与基线配置 100% 零突变。

---

### Phase 34-E：扫描器增量持久化（Crash-safe State Persistence）与乐器端点概念收敛 [已完成，2026-09-22]

1. **插件扫描器的增量持久化（Crash-safe Scanner Persistence）**：
   - `PluginHost` 新增 `ScanIncrementalCallback`：`advanceVst3ScanStep()` 每发现新插件、`cancelVst3ScanSession()` 取消、`addVst3FileToKnownList()` 单文件导入均立即回调，`PluginOperationController` 随即同步写入 `knownPluginListState`，崩溃不再丢弃整轮扫描成果；
   - 扫描目标在首个插件被探测前即落盘；`beginVst3ScanSession()` 读取 dead-man's pedal 并把崩溃插件加入黑名单推迟到序列末尾，避免反复卡死在同一入口；
   - `PluginScanPersistenceTest` 覆盖 pedal 恢复、增量回调语义与未注册回调时的零副作用。
2. **Seam-first 乐器端点（Instrument Endpoint）概念收敛**：
   - 新增 `source/Audio/InstrumentEndpoint.h`：以 `resolveInstrumentEndpoint()` 无锁解析当前乐器端点（种类 / 宿主实例 / 描述 / 就绪 / 通道几何），统一 `AudioEngine` 设备准备与实时渲染、`RecordingSessionController` 离线导出中的重复判断；
   - 离线侧新增 `renderTakeThroughInstrumentEndpoint()` 端点路由，`WavExportTask` 不再自持 `offlinePlugin != nullptr ? ... : ...` 二元分支；
   - `InstrumentEndpointTest` 覆盖端点解析回落、通道几何与就绪语义、离线双路由与空 take 拒绝。

---

### Phase 34-F：跨平台实现深度收敛与 JUCE 9 原生框架利用全面升级 (Cross-Platform & JUCE 9 Convergence) [已完成，2026-09-23]

1. **字符串字面量 7-bit ASCII 规范化与废弃 AlertWindow 绘制代码清理 (`QUAL-001`, `QUAL-002`, `JUCE-003`)**：
   - `StyleCatalogTest.cpp`、`MidiChannelMapperTest.cpp` 与 `KeyboardMidiMapperTest.cpp` 消除裸中文与 Unicode 箭头，全库字符串字面量 100% 达到 Strict 7-bit ASCII 铁律，消除 Windows/MSVC 编译乱码与断言崩溃隐患；
   - `DevPianoLookAndFeel` 彻底删除对 `juce::AlertWindow` 的废弃重写方法与颜色配置；
   - 清理内化 JIVE 核心源码中残留的 `#if JUCE_MAJOR_VERSION >= 8` 历史版本宏。
2. **JUCE 9 原生合法文件名替换与运行时数据目录大小写归一 (`JUCE-001`, `PLAT-003`, `ARCH-001`)**：
   - `PerformancePreset.cpp` 以 JUCE 9 原生 `juce::File::createLegalFileName` 取代手写 ASCII 过滤轮子 `sanitisePresetFileName`，天然解锁中文与 Unicode 预设名称在 Windows/Linux 上的合法落盘；
   - `PluginHost.cpp` 与 `DevPianoLogger.cpp` 数据目录名称从小写 `"devpiano"` 统一为 `"DevPiano"`，根治 Linux 大小写敏感文件系统下的双目录分裂；
   - `PresetFlowSupport.cpp` 与 `RecordingSessionController.cpp` 直接内联调用声明式 `JiveModalDialog`，彻底删除 4 个薄转发空壳类源文件并更新 CMake 配置。
3. **Main.cpp 原生 Hook 消除与纯净跨平台化 (`PLAT-001`)**：
   - 彻底拔除 `source/Main.cpp` 中的 Windows `WNDPROC` 钩子、`AttachThreadInput` 与全局静态指针，移除 `#include <windows.h>`，顶层 Shell 跨平台纯度达到 100%；
   - 全平台统一采用 JUCE 9 原生 `DocumentWindow::activeWindowStatusChanged()` 配合 `callAsync` 延后分发 `restoreKeyboardFocus()`，窗口前台化使用 `toFront(true)` 与 `juce::Process::makeForegroundProcess()`；
   - 同步更新 `known-issues.md` 将 `PLAT-001` 转入已修复清单。
4. **WavExportTask 完全异步化与模态循环解耦 (`JUCE-002`)**：
   - `WavExportTask` 演进为现代化非阻塞异步任务模型（`startAsync(onComplete)`），彻底消除主线程嵌套消息循环 `runDispatchLoopUntil(10)` 与主线程 `Thread::sleep(10)`；
   - `RecordingSessionController::handleExportWavClicked()` 完全非阻塞化；
   - 从 `CMakeLists.txt` 中主应用 `devpiano` 编译配置中彻底移除 `JUCE_MODAL_LOOPS_PERMITTED=1` 编译宏。

---

## 4. 验证与回归数据

| 验证项 | 验证手段 | 结果 |
|---|---|---|
| 代码格式合规 | `./scripts/dev.sh format --check` | 100% 格式对齐，零格式诊断 |
| 单元测试回归 | `./scripts/dev.sh test` | 70+ 测试套件，13,000+ 断言全部绿灯通过 |
| 跨平台编译 | WSL / Linux 本地 clang 构建 | 0 错误，0 警告 |
| 发音身份恒定 | `LayoutGroupAndHeldKeyIdentityTest` | 各种切组/重叠按键 0 悬挂音 |
| 踏板切分精度 | `SyncPedalTest` | 采样精确时序事件流无破音断音 |
| 字符编码铁律 | 源码全量字节扫描 | 源码 C++ 字符串字面量 100% Strict 7-bit ASCII |
