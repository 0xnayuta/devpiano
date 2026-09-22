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
  - 支持鼠标点击发音与右键绑定编辑联动；
  - 编写 `QwertyViewModelTest` 专项单测并通过全量回归。
---

### Phase 34-B：Layout Group 轻量多键组与 HeldKey Identity 状态快照机制

> 目标：实现单 Preset 内 2~4 个轻量键位分组（Group）的毫秒级即时切换，同时以发音身份快照彻底封死悬挂音隐患。

- [ ] **Phase 34-B-1：引入 `KeyGroup` 数据模型与快照存储**：
  - 在 `source/Core/KeyMapTypes.h` 中引入 `struct KeyGroup { int8_t transposeOffset; int8_t octaveShift; uint8_t channel; };`；
  - `KeyboardLayout` 支持 `std::array<KeyGroup, 4>` 极简分组，不侵入其他全局预设属性。
- [ ] **Phase 34-B-2：重构 `HeldKeyTracker` 落实发音身份快照（Note-off Identity Preservation）**：
  - 按键按下（NoteOn）时，记录该键专属发音快照 `{ physicalKeyCode, soundingMidiNote, soundingMidiChannel }`；
  - 按键松开（NoteOff）时，100% 依据按下时记录的快照信息注销，与当前处于哪个 Group 完全解耦；
  - 支持快捷键（如 `Tab` 或功能键）在 Group 之间瞬时无缝切换。
- [ ] **Phase 34-B-3：Group 动态切换与防悬挂确定性测试集**：
  - 编写专项测试：按住按键 A -> 切换 Group -> 松开按键 A，断言 NoteOff 准确对应先前的发声音高与通道，无任何悬挂音残留。
---

### Phase 34-C：SustainPolicy 与 Sample-Accurate 事件级 Sync 切分踏板

> 目标：引入钢琴演奏学中的“切分踏板（Legato / Sync Pedal）”机制，消除空格键踩放时的断音空洞。

- [ ] **Phase 34-C-1：定义 `SustainPolicy` 状态模型**：
  - 定义枚举 `SustainPolicy { normal, syncPedal }`；
  - 在 `KeyboardMidiMapper` 中增加可配置的踏板策略选择，支持通过设置或 UI 切换。
- [ ] **Phase 34-C-2：实现音频块内的采样精确切分踏板时序**：
  - 当处于 `syncPedal` 且挂起切断时，松开踏板不立即释放；
  - 在下一个 NoteOn 到达时，在相同的 `sampleOffset` 处，严格按顺序生成事件：
    $$\text{CC64}(0) \longrightarrow \text{NoteOn}(\text{newNote}) \longrightarrow \text{CC64}(127)$$
  - 坚决杜绝任何物理线程 sleep，确保采样级精度与确定性。
- [ ] **Phase 34-C-3：踏板时序与连奏听感确定性测试**：
  - 编写 MIDI 事件时序测试，断言切分模式下 CC64 与 NoteOn 的严格相对偏移。

### Phase 34-D：PerformanceModifierState 瞬态 Press 修饰符（事件流变换）

> 目标：支持修饰键（如 Shift / Alt）按住期间的瞬态力度拉满或移调变换，松开后自动回弹基线。

- [ ] **Phase 34-D-1：设计 `PerformanceModifierState` 事件变换管道**：
  - 建立纯瞬态数据结构，包括当前激活的力度放大系数、临时八度偏移等；
  - 严格限定为事件变换（Event Transformation），严禁突变持久化配置。
- [ ] **Phase 34-D-2：修饰键集成与事件注入**：
  - 捕获修饰键的按下与松开状态，平滑注入 `KeyboardMidiMapper` 处理链路；
  - 编写状态恢复测试，验证松开修饰键后基线配置 100% 保持不变。

### Phase 34-E：扫描器增量持久化（Crash-safe State Persistence）与乐器端点概念收敛

> 目标：吸收官方 Host 与 Element 的生产级工程精髓，提升第三方插件容灾鲁棒性与乐器抽象纯净度。

- [ ] **Phase 34-E-1：插件扫描器的增量持久化（Crash-safe Scanner Persistence）**：
  - 在 `PluginHost` 的分批扫描中，每成功识别一个有效插件，立即增量持久化 `KnownPluginList`；
  - 若遇劣质第三方插件引发崩溃，下次启动可安全跳过已知崩溃点，避免反复卡死。
- [ ] **Phase 34-E-2：Seam-first 乐器端点（Instrument Endpoint）概念收敛**：
  - 梳理 `AudioEngine`、`RecordingEngine` 与 `PluginOfflineRenderer` 的乐器调用契约；
  - 在不破坏现有平稳运行的前提下，建立薄乐器端点概念层，消除重复的二元分支判断。

## 历史实现 Backlog

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
