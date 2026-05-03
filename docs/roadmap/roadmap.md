# devpiano Roadmap

> 用途：作为唯一的项目状态、阶段路线与近期重点来源。  
> 读者：项目维护者、规划下一轮开发的人。  
> 更新时机：阶段目标变化、功能完成度变化、重大风险变化时。

## 1. 项目目标

将旧版 Windows FreePiano 重构为基于 JUCE 的现代 C++ 音频应用。

核心替代方向：

- 旧 WASAPI / ASIO / DSound 后端 -> JUCE `AudioDeviceManager`。
- 旧 VST 加载逻辑 -> JUCE `AudioPluginFormatManager` / `AudioPluginInstance`。
- 旧 Windows 键盘输入逻辑 -> JUCE `KeyListener` / `KeyPress` + 可配置 MIDI 映射。
- 旧 GDI / 原生控件 UI -> JUCE `Component` 树。
- 旧配置系统 -> `ApplicationProperties` / `ValueTree` / 项目内状态模型。

## 2. 当前状态摘要

**已完成阶段：**
- Phase 1-4（功能开发）：Phase 1-1-Phase 4 全部完成，核心功能已可用。
- Phase 5.1-5.7（架构收敛）：MainComponent 职责下沉，Phase 5-5..5-11 已完成。

**当前阶段：**
- Phase 5.8：MainComponent 瘦身已完成 5.8a-5.8e（1587→606 行），远低于 1200 行目标。
- 当前插入缺陷“启动 / 音频重建早期首音音高异常”已修复并通过人工验证；保留 `25ms` audio warmup，详见 [`../testing/known-issues.md`](../testing/known-issues.md) §2。
- 插入缺陷“MIDI 导入播放首音无声”已通过 playback-start pre-roll / arming 修复并完成人工回归；后续作为 Phase 4 MIDI import 回归项观察，详见 [`../testing/known-issues.md`](../testing/known-issues.md) §8。

**搁置项：**
- 外部 MIDI 硬件依赖验证（待硬件条件恢复）。
- VST3 插件离线渲染（Phase 3-2）后置。

## 3. 阶段路线图

### Phase 1：工程骨架与最小演奏（Phase 1-1-Phase 1-2）

状态：已完成。

关键里程碑：
- JUCE GUI 程序可启动，音频设备可初始化。
- 电脑键盘可触发 note on / note off，虚拟钢琴键盘可联动显示。
- 程序可发声，来源为内置 fallback synth 或已加载插件。

### Phase 2：插件系统与键盘映射（Phase 2）

状态：已完成。

关键里程碑：
- VST3 插件扫描、加载、卸载、editor 窗口完整。
- 键盘映射系统可配置，支持默认布局和自定义 Preset。
- 插件扫描产品化增强（失败文件明细、多目录扫描、结果持久化、状态提示）。

### Phase 3：UI 与高级功能（Phase 3）

状态：已完成。

关键里程碑：
- UI 拆分为头部、插件、参数、键盘等区域。
- 布局 Preset 系统（JSON 格式、自动发现、导入/保存/重命名/删除）。
- 录制/回放/MIDI 导出/WAV 离线渲染 MVP。

### Phase 4：MIDI 文件导入（Phase 4）

状态：已完成。

关键里程碑：
- MIDI 文件导入、自动选轨、回放、虚拟键盘可视化。
- 最近路径记忆、主窗口尺寸自适应与恢复。

功能与测试文档：[`../features/phase4-midi-file-import.md`](../features/phase4-midi-file-import.md)、[`../testing/phase4-midi-file-import.md`](../testing/phase4-midi-file-import.md)。

### Phase 5：架构收敛与 MainComponent 瘦身

状态：已完成（5.1-5.7 已完成，5.8a-5.8e 已完成，人工回归通过）。

**目标：** 将 `MainComponent.cpp` 从约 1587 行降至 1200 行以下，通过提取 helper 收敛职责。

**已完成（5.1-5.7）：**
- 录制会话状态结构化、导出流程统一、布局 CRUD 流程收敛。
- 设置窗口生命周期收敛、AppState 清理、ControlsPanel 按钮状态统一。
- MIDI 导入流程下沉。

**已完成（5.8a+5.8b+5.8c）：**
- 布局管理 handlers 提取到 `Layout/LayoutFlowSupport`（MainComponent 1587→1349 行，减少 238 行）。
- 录制/回放/MIDI 导入编排提取到 `Recording/RecordingSessionController`（MainComponent 1349→930 行，减少 419 行）。
- 插件操作提取到 `Plugin/PluginOperationController`（MainComponent 930→711 行，减少 219 行）。
- 设置窗口管理提取到 `Settings/SettingsWindowManager`（MainComponent 711→631 行，减少 80 行）。
- 状态快照构建提取到 `Core/AppStateBuilder`（MainComponent 631→606 行，减少 25 行；主要收益是边界收敛）。
- 累计减少 981 行，远低于 1200 行目标。

**后续 tech debt（低优先级，暂不执行）：**
- Phase 5.8f：AppStateBuilder 分层清理 — 已评估为低优先级 tech debt，暂不执行。详见 [`../archive/phase5-architecture-convergence.md`](../archive/phase5-architecture-convergence.md)。
- 5.8+ 后续机会（音频设备 helper、设置 flow helper、匿名 namespace 分散）— 已评估，不建议继续拆分，长期搁置到 MainComponent 再次膨胀。

5.8a-5.8e 已使 `MainComponent.cpp` 降至 606 行，已达成 1200 行以下目标。人工回归已通过，无明显回退。详细计划见 [`../archive/phase5-architecture-convergence.md`](../archive/phase5-architecture-convergence.md)。

详细完成记录见：[`../archive/phase5-architecture-convergence.md`](../archive/phase5-architecture-convergence.md)。

### Phase 6：演奏数据持久化与播放体验增强

状态：进行中（6-1 核心功能已完成）。

**目标：** 填补 FreePiano 核心功能差距——录制后能保存、保存后能打开、打开后能调速播放。

**已完成：**

- **Phase 6-1：演奏文件保存/打开**（核心）✅
  - `RecordingTake` JSON 序列化（`.devpiano` 格式），包含 events、sampleRate、lengthSamples、元数据。
  - ControlsPanel Save/Open 按钮 + FileChooser 流程。
  - `RecordingSessionController` 接入 `PerformanceFile` API。
  - UI 布局待美化（Phase 6-2 范围）。

**规划内容：**

- **Phase 6-2：播放速度控制**
- **Phase 6-2：播放速度控制**
  - 0.5x ~ 2.0x 速度调节，ControlsPanel 增加速度显示 + 增减按钮。
  - 用户价值：高——练琴刚需，FreePiano 有此功能。
- **Phase 6-3：最近文件列表 + 拖拽打开**
  - 最近打开的演奏文件/MIDI 文件列表（最多 10 条）。
  - 拖拽 `.devpiano` / `.mid` 文件到窗口触发打开。
- **Phase 6-4：基础 MIDI 编辑（delete notes）**
  - 选中音符 → 删除。需将 `RecordingTake.events` 改为可变结构。
  - 最小编辑能力：只做删除，不做添加/移动/量化。
- **Phase 6-5：MIDI 导入增强**
  - 导入 sustain CC64、pitch bend、program change，提升外部 MIDI 回放保真度。

详细功能设计与验收标准见：[`../features/phase6-performance-persistence.md`](../features/phase6-performance-persistence.md)。
专项测试见：[`../testing/phase6-performance-persistence.md`](../testing/phase6-performance-persistence.md)。
功能候选与差距分析见：[`../features/phase4-midi-file-import.md`](../features/phase4-midi-file-import.md)。

### Phase 7：完整工程文件与多轨支持（粗略规划）

状态：未开始，粗略规划。

**可能包含：**
- 完整工程文件（`.devpiano-project`）：包含演奏数据 + layout preset + 插件状态 + 音频设备配置。
- 多轨数据模型：`RecordingTake` 引入 track 概念。
- Tempo map 支持：导入/编辑/保存 tempo 变化。
- VST3 插件离线渲染（Phase 3-2 搁置项恢复）。
- Setting groups：多组独立的八度/移调/力度/通道配置。

### Phase 8：高级编辑与国际化（粗略规划）

状态：未开始，粗略规划。

**可能包含：**
- Piano roll / 事件编辑器 UI。
- 量化 / snap-to-grid。
- 多语言 UI（英文/中文）。
- MP4 视频导出。
- 自动延音踏板（auto pedal）。
- Key fade 动画、GUI 透明度等装饰功能。

## 4. 当前近期重点

优先级从高到低：

1. **Phase 6：演奏数据持久化与播放体验增强** — 规划中。
   - 核心：演奏文件保存/打开、播放速度控制、最近文件列表、拖拽打开、基础 MIDI 编辑。
   - 详见上方 Phase 6 章节。

2. **Phase 4 边界稳定**
   - 保持 Phase 4-4 边界：导入 playback take 禁止 MIDI 再导出；导入后允许 WAV 导出。
   - 继续搁置 Phase 4-6 merge-all。

3. **Phase 2 插件宿主持续稳定**
   - 低优先级持续观察退出阶段 Debug 告警。

4. **搁置项（待条件恢复）**
   - 外部 MIDI 硬件依赖验证。
   - VST3 插件离线渲染（Phase 3-2）。

## 5. 主要风险

| 风险 | 当前判断 | 应对方向 |
|---|---|---|
| 启动 / 音频重建早期首音音高异常 | 已缓解 | 已修正音频设备初始化顺序，并保留 `25ms` audio warmup；继续作为回归项观察。 |
| MIDI 导入播放首音无声 | 已修复 | 已增加 playback-start pre-roll / arming；后续触及 MIDI import / playback 启动链路时执行 Phase 4 §11.1 回归。 |
| 插件生命周期复杂 | 中 | 维护专项生命周期测试，重点覆盖 editor、卸载、重扫、退出。 |
| 外部 MIDI 硬件依赖 | 中 | 外部 MIDI 录制/回放/退出场景因无硬件暂缓；状态已记录至 [`known-issues.md`](../testing/known-issues.md)。 |
| 键盘映射边界多 | 低中 | 基础映射已全量验证；布局 preset 已补充专项回归清单。 |
| `MainComponent` 职责回流 | 低中 | 已通过 Phase 5.1-5.8e 架构收敛；布局、录制/回放/MIDI 导入、插件操作、设置窗口管理和状态快照构建已下沉到专门模块。 |
| 录制/回放实现风险 | 中 | MVP 主链路已接入；下一阶段优先收紧实时音频线程边界。 |
| 布局 Preset 实现风险 | 低 | 核心能力已完成并补充功能/测试文档。 |
| 文档状态漂移 | 中 | 本文件作为唯一 roadmap；当前任务只写入 [`current-iteration.md`](current-iteration.md)。 |

## 6. 完成标准参考

阶段性验收标准见：

- [`../testing/acceptance.md`](../testing/acceptance.md)

专项测试见：

- [`../testing/phase2-keyboard-mapping.md`](../testing/phase2-keyboard-mapping.md)
- [`../testing/phase3-layout-presets.md`](../testing/phase3-layout-presets.md)
- [`../testing/phase3-recording-playback.md`](../testing/phase3-recording-playback.md)
- [`../testing/phase4-midi-file-import.md`](../testing/phase4-midi-file-import.md)
- [`../testing/phase6-performance-persistence.md`](../testing/phase6-performance-persistence.md)
- [`../testing/phase2-plugin-host-lifecycle.md`](../testing/phase2-plugin-host-lifecycle.md)

## 7. 历史实现 Backlog

Phase 2-3 的详细实现计划与完成记录已归档至：

- [`../archive/phase2-3-implementation-backlog.md`](../archive/phase2-3-implementation-backlog.md)
