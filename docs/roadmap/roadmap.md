# devpiano Roadmap

> 用途：作为唯一的项目状态、阶段路线与近期重点来源。
> 更新时机：阶段目标变化、功能完成度变化、重大风险变化时。

## 1. 项目目标

devpiano 是一款基于 JUCE 的现代 C++ 电脑键盘钢琴应用。

核心替代方向：

- 旧 WASAPI / ASIO / DSound 后端 -> JUCE `AudioDeviceManager`。
- 旧 VST 加载逻辑 -> JUCE `AudioPluginFormatManager` / `AudioPluginInstance`。
- 旧 Windows 键盘输入逻辑 -> JUCE `KeyListener` / `KeyPress` + 可配置 MIDI 映射。
- 旧 GDI / 原生控件 UI -> JUCE `Component` 树。
- 旧配置系统 -> `ApplicationProperties` / `ValueTree` / 项目内状态模型。

## 2. 阶段路线图

### Phase 1：工程骨架与最小演奏 [已完成]

JUCE GUI 启动、音频设备初始化、电脑键盘触发 note on/off、虚拟钢琴键盘联动、内置 fallback synth 发声。

### Phase 2：插件系统与键盘映射 [已完成]

VST3 插件扫描 / 加载 / 卸载 / editor 窗口、键盘映射系统（可配置）+ Performance Preset、扫描 UX 增强（分片进度、失败列表可发现性）。

### Phase 3：UI 与高级功能 [已完成]

UI 拆分为头部 / 插件 / 参数 / 键盘区域、Performance Preset 系统（`.devpiano.preset` JSON / 自动发现 / CRUD）、录制 / 回放 / MIDI 导出 / WAV 离线渲染 MVP。

### Phase 4：MIDI 文件导入 [已完成]

MIDI 文件导入、自动选轨、回放、虚拟键盘可视化、最近路径记忆、主窗口尺寸自适应与恢复。

功能与测试文档：[`../reference/features/midi-file-import.md`](../reference/features/midi-file-import.md)。

### Phase 5：架构收敛与 MainComponent 瘦身 [已完成]

`MainComponent.cpp` 从 ~1587 行降至 ~750 行（Phase 5 结束时 606 行，Phase 6/7 新增功能后约 750 行）。
提取 `RecordingSessionController` / `PluginOperationController` / `SettingsWindowManager` / `AppStateBuilder`。
详细完成记录见 [`../archive/phase5-architecture-convergence.md`](../archive/phase5-architecture-convergence.md)。

### Phase 6：功能补齐——钢琴键盘、MIDI 矩阵、绑定系统、GUI 设置 [已完成]

自定义钢琴键盘（`CustomKeyboard`，支持 3 种着色 / 3 种音符显示模式）、16 通道 MIDI 矩阵（`ChannelMatrix`）、note-only 绑定编辑器。
演奏文件持久化、播放速度控制、MIDI 导入增强（CC/pitch bend/program change）、Diagnostics 层、测试夹具库。
5 项 GUI 设置控件（colourMode / noteDisplay / fadeSpeed / resizable / instrumentFilter toggle）。
详细完成记录见 [`../archive/phase6-7-completion-detail.md`](../archive/phase6-7-completion-detail.md)。

### Phase 7：VST3 离线渲染与国际化 [已完成]

VST3 离线渲染（WAV 导出 + `ExportDialog` 进度）、播放速度精确控制（Slider + atomic 线程安全）、拖放文件支持、运行时中英文语言切换（JUCE `Translation`）。
Phase 7-5（Metadata 编辑对话框）— 明确搁置（基础设施已就位，UI 无现阶段价值）。
Phase 7-7（全屏模式）— 不实现（`resizable` toggle + OS 最大化可替代）。
详细完成记录见 [`../archive/phase6-7-completion-detail.md`](../archive/phase6-7-completion-detail.md)。

### 架构优化 [已完成]

架构优化 Backlog 七项（P0/P1/P2）全部完成：最近文件列表 UI、PluginOfflineRenderer 生命周期注释、PerformanceFile Base64 序列化、Diagnostics 日志层迁移、WavExportOptions 独立头文件、SettingsComponent ValueTree::Listener、MainComponent 瘦身。
详细记录见 [`../archive/architecture-optimization-backlog.md`](../archive/architecture-optimization-backlog.md)。

**长期观察项：**
- Phase 4 边界稳定：导入 playback take 禁止 MIDI 再导出；继续搁置 Phase 4-6 merge-all。
- Phase 2 插件宿主持续稳定：低优先级观察退出阶段 Debug 告警。

### Phase 8：逐键个性化与调号系统 [已完成]

逐键自定义标签（Per-Key Labels）和颜色（Per-Key Colors），全局调号 + MIDI 移调开关。

### Phase 9：配置快照与体验增强 [已完成]

Performance Preset、88 键完整钢琴键盘、Smooth Pitch Bend、乐曲信息编辑。

### Phase 10：主窗口 UI 现代化 [已完成]

自定义 LookAndFeel 暗黑主题、旋钮化 ADSR/音量、插件面板折叠化、拟真键盘渲染、Transport 图标化、底部状态栏、动态布局尺寸规则。

### 审计问题修复 (2026-07-20 ~ 2026-07-23) [已完成]

代码质量审计 (`code-quality-audit-2026-07-20`) 发现 63 项问题，经 Phase A–F 六轮修复：

- **Phase A** — 音频稳定性：音频回调堆分配、lazy prepareToPlay、RecordingEngine recording 路径原子性
- **Phase B** — 线程安全：PluginHost 断言+文档化契约、async lambda 生命周期、preset 并发写入、离线渲染线程隔离
- **Phase C** — 模块边界：ChannelMatrix→Midi/、KeyboardTypes→UI/、AppStateBuilder→Settings/、Core/ 精简至 3 个纯数据类型文件
- **Phase D** — 工程化：CMakeLists 完整、ARCH-001~004 架构更新、DOC-001~003 文档同步、clang-format 0 violations
- **Phase E** — 测试完善：新增 AudioEngine/RecordingEngine/PluginHost/KeyboardMidiMapper 4 个测试文件，测试覆盖从 2 模块→7 模块，31 测试类 74 子测试全部通过
- **Phase F** — 剩余项评估：9 项修复（JSON 崩溃防护、录制守卫等），24 项低风险/长期项标记 Deferred

最终：0 Open · 24 Deferred · 39 Closed · 评级 A-。审计关闭，项目可安全进入功能开发阶段。

## 3. 主要风险

| 风险 | 当前判断 | 应对方向 |
|---|---|---|
| 插件生命周期复杂 | 中 | 维护专项生命周期测试，重点覆盖 editor、卸载、重扫、退出。 |
| 键盘映射边界多 | 低 | 基础映射已全量验证；Performance Preset 已补充专项回归清单。 |
| `MainComponent` 职责回流 | 低 | 已通过 Phase 5 架构收敛下沉；当前 ~750 行。 |
| 文档状态漂移 | 中 | 本文件作为唯一 roadmap；当前任务只写入 [`current-iteration.md`](current-iteration.md)。 |


## 4. 完成标准参考

阶段性验收标准见：

- [`../reference/acceptance.md`](../reference/acceptance.md)

专项测试见：

- [`../reference/features/keyboard-mapping.md`](../reference/features/keyboard-mapping.md)
- [`../reference/features/performance-presets.md`](../reference/features/performance-presets.md)
- [`../reference/features/recording-playback.md`](../reference/features/recording-playback.md)
- [`../reference/features/midi-file-import.md`](../reference/features/midi-file-import.md)
- [`../reference/features/performance-persistence.md`](../reference/features/performance-persistence.md)
- [`../reference/features/plugin-hosting.md`](../reference/features/plugin-hosting.md)
- [`../reference/features/plugin-offline-rendering.md`](../reference/features/plugin-offline-rendering.md)
- [`../reference/features/fixture-inventory.md`](../reference/features/fixture-inventory.md)

## 5. 历史实现 Backlog

- Phase 2-3 完成记录： [`../archive/phase2-3-implementation-backlog.md`](../archive/phase2-3-implementation-backlog.md)
- Phase 5 完成记录：[`../archive/phase5-architecture-convergence.md`](../archive/phase5-architecture-convergence.md)
- Phase 6-7 完成记录：[`../archive/phase6-7-completion-detail.md`](../archive/phase6-7-completion-detail.md)
- 架构优化完成记录：[`../archive/architecture-optimization-backlog.md`](../archive/architecture-optimization-backlog.md)
- Phase 8–9 完成记录：[`../archive/phase8-9-completion.md`](../archive/phase8-9-completion.md)
- Phase 10 完成记录：[`../archive/phase10-ui-modernization.md`](../archive/phase10-ui-modernization.md)
- 2026-07-20 审计报告：[`../audit/code-quality-audit-2026-07-20.md`](../audit/code-quality-audit-2026-07-20.md)
