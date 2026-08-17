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

详细完成记录见 [`../archive/phase2-3-implementation-backlog.md`](../archive/phase2-3-implementation-backlog.md)。

### Phase 3：UI 与高级功能 [已完成]

UI 拆分为头部 / 插件 / 参数 / 键盘区域、Performance Preset 系统（`.devpiano.preset` JSON / 自动发现 / CRUD）、录制 / 回放 / MIDI 导出 / WAV 离线渲染 MVP。

详细完成记录见 [`../archive/phase2-3-implementation-backlog.md`](../archive/phase2-3-implementation-backlog.md)。

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

详细完成记录见 [`../archive/architecture-optimization-backlog.md`](../archive/architecture-optimization-backlog.md)。

**长期观察项：**
- Phase 4 边界稳定：导入 playback take 禁止 MIDI 再导出；继续搁置 Phase 4-6 merge-all。
- Phase 2 插件宿主持续稳定：低优先级观察退出阶段 Debug 告警。

### Phase 8：逐键个性化与调号系统 [已完成]

逐键自定义标签（Per-Key Labels）和颜色（Per-Key Colors），全局调号 + MIDI 移调开关。

详细完成记录见 [`../archive/phase8-9-completion.md`](../archive/phase8-9-completion.md)。

### Phase 9：配置快照与体验增强 [已完成]

Performance Preset、88 键完整钢琴键盘、Smooth Pitch Bend、乐曲信息编辑。

详细完成记录见 [`../archive/phase8-9-completion.md`](../archive/phase8-9-completion.md)。

### Phase 10：主窗口 UI 现代化 [已完成]

自定义 LookAndFeel 暗黑主题、旋钮化 ADSR/音量、插件面板折叠化、拟真键盘渲染、Transport 图标化、底部状态栏、动态布局尺寸规则。

详细完成记录见 [`../archive/phase10-ui-modernization.md`](../archive/phase10-ui-modernization.md)。

### Phase 11：声明式 UI 架构迁移（JIVE + melatonin_inspector） [已完成]

JIVE 声明式 UI 框架（`juce::ValueTree` 布局 + JSON 样式表 + Flex/Grid 自适应）替代 5 个面板的硬编码 `setBounds()` 布局；melatonin_inspector 运行时检查器加速 UI 迭代反馈；`design_tokens.json` 统一 JIVE 与原生组件样式来源；`Ctrl+R` / 文件监听热重载；`MainComponent::resized()` 缩减至 3 行（JIVE FlexBox 自动响应）。`CustomKeyboard` 与 ADSR 曲线经组件工厂原生注入，业务逻辑层零改动。回归验证（全量单元测试 / Windows MSVC 构建 / 手动回归清单 11 项）全部通过。

详细计划与完成记录见 [`../archive/phase11-declarative-ui-jive.md`](../archive/phase11-declarative-ui-jive.md)。

### 全面审计 (2026-08-16) [已完成]

代码质量审计（`AUDIT-001`，2026-08-16）登记 85 项：56 项未处理（3 P1 / 16 P2 / 37 P3，评级 B）+ 16 项已暂缓（重开条件与风险接受原因见报告第 8 章登记表）+ 13 项已关闭。修复按 **AUDIT Phase A–H** 推进，已全部完成（2026-08-17）：56 项未处理全关闭，16 项已暂缓经复核关闭 2 项（QUAL-019/PERF-002），剩余 14 项维持。

审计期间落地的主要修复：

- 音频稳定性：消除音频回调堆分配（prepareToPlay 预分配插件缓冲）、移除回调内延迟 prepare、RecordingEngine 录制路径原子化
- 线程安全：PluginHost 线程契约（断言 + 头文件文档）、异步 lambda 生命周期防护（alive-flag）、录制中 preset 并发写入队列化、离线渲染线程隔离、播放状态原子化
- 模块边界：ChannelMatrix→Midi/、KeyboardTypes→UI/、AppStateBuilder→Settings/、Core/ 精简至 3 个纯数据类型文件
- 工程化：CMakeLists 源列表完整、架构与文档同步、clang-format 清零、全量 44 文件 clang-tidy 0 诊断
- 测试完善：断言总数 754 → 2921 全绿（覆盖会话控制/通道矩阵/预设序列化/导出/设置/插件操作层），顺带修复 SettingsStore customKeyLabels/Colours 持久化读回失效 bug
- 其余修复：JSON 崩溃防护、录制守卫、原子文件写入（PerformanceFile 与 Preset 走 TemporaryFile + rename）、公共渲染管线提取（RenderPipeline）

审计报告见 [`../audit/AUDIT-001-code-quality-audit-2026-08-16.md`](../audit/AUDIT-001-code-quality-audit-2026-08-16.md)，Phase A–H 逐项完成记录见 [`../archive/audit-001-code-quality-fix-phases.md`](../archive/audit-001-code-quality-fix-phases.md)。

### Phase 12：内置物理建模钢琴音源（SineSynth → PianoSynth） [规划中]

> 目标：把当前内置 fallback 正弦合成器逐步替换为**自主拥有、纯 C++、零/极低采样依赖的物理建模钢琴音源**，使"未加载插件时的默认钢琴"成为产品核心能力而非兜底 beep。详细排期与子任务见 [`current-iteration.md`](current-iteration.md)。

**现状（代码事实）：**

- 实时路径 `AudioEngine::SimpleSineVoice`（AudioEngine.cpp:41）与离线导出路径 `OfflineSineVoice`（WavFileExporter.cpp:28）是**两份逐字重复**的 sine 实现；另加 `RecordingSessionController.cpp:202` 插件离线实例创建失败时回退 sine。任何音色替换需同时改 3 处，否则实时/导出/回退音色分叉。
- 参数接线已有同构模式：`SettingsModel::PerformanceSettingsView`（masterGain + 4 项 ADSR）→ `MainComponent::applyPerformanceSettingsToAudioEngine`（实时）+ `ExportFlowSupport::buildWavExportOptions`（离线），新音色参数沿用即可。
- 测试盲区：`AudioEngineTest.cpp` 声明 fallback 音频输出路径不测（`MidiMessageCollector` wall-clock 时序），新音色需确定性测试夹具（直接驱动 `SynthesiserVoice`，固定 sampleRate，断言频谱/能量/时长/力度单调性）。

**技术路线（JUCE 惯例，不建新抽象）：**

以 `juce::Synthesiser` + `SynthesiserVoice` 为唯一音色承载（JUCE 生态标准做法，当前代码已在使用）——不引入自定义 `BuiltinInstrument` 接口层，不建 `source/Audio/Builtin/` 多层目录。替换音色 = 换 `addSound/addVoice` 注册内容。分三阶段渐进，每阶段可独立合入、可听感回归：

- **Phase 12：音源重构 + 谐波钢琴 v1**（Level 1）——合并两份 sine 为共享 `SynthesiserVoice` 子类（三处调用点改接，零行为变化）；新增 Harmonic PianoVoice：基频 + 2~7 次谐波叠加、velocity→亮度/响度映射、分音独立衰减；参数进 `SettingsModel`。纯加法、零采样、CPU 可忽略。验收：实时/离线音色一致、确定性音色单测、听觉对比明显优于 sine。
- **Phase 13：Stiff-String Inharmonic Piano v2**（Level 2）——加入 inharmonicity（`fₙ = n·f₀·√(1+B·n²)`，B 按音区设定）、分音衰减速率建模、简单 body 共鸣滤波。仍纯解析加法，无需延迟线。
- **Phase 14：Digital Waveguide Piano v3**（Level 3，研究性）——延迟线 + 分数延迟 + loss/dispersion filter + hammer excitation，进入真正物理建模层。**决策门**：听感收益 vs CPU 预算 vs 代码复杂度，产物是否成为默认音色。

**外部参考（许可证已实地核实）：**

| 项目 | 许可证 | 用法 |
|---|---|---|
| bBpiano | PolyForm Internal Use 1.0.0（**禁止再分发**） | 仅读思想/论文式源码（如 `From PDE to PCM.md`），**不复制代码** |
| Instrudio | 无 LICENSE（默认保留所有权利） | 仅读思想 |
| STK | MIT（含非绑定回馈请求条款） | 可复用 delay/filter 类作零件库；注意其物理乐器依赖自带 rawwaves 采样，与零采样目标有张力 |
| mda-plugins-juce | MIT | 可参考 JUCE DSP 实现惯例 |

Faust 不引入 runtime（生成代码 GPL exception 面模糊，仅作研究工具）。若复用 STK/mda 代码，在 `THIRD-PARTY-NOTICES.md` 记录来源/版本/修改。

## 3. 主要风险

| 风险 | 当前判断 | 应对方向 |
|---|---|---|
| 插件生命周期复杂 | 中 | 维护专项生命周期测试，重点覆盖 editor、卸载、重扫、退出。 |
| 键盘映射边界多 | 低 | 基础映射已全量验证；Performance Preset 已补充专项回归清单。 |
| JIVE API 稳定性（198 stars，MIT） | 中 | 固定 git commit hash；Phase 11a 首次集成时验证版本兼容性。 |
| `MainComponent` 职责回流 | 极低 | Phase 5 收敛至 ~890 行；Phase 11 目标降至 ~300 行，UI 布局移至 JIVE。 |
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
- 2026-08-16 审计报告：[`../audit/AUDIT-001-code-quality-audit-2026-08-16.md`](../audit/AUDIT-001-code-quality-audit-2026-08-16.md)
- AUDIT Phase A–H 完成记录：[`../archive/audit-001-code-quality-fix-phases.md`](../archive/audit-001-code-quality-fix-phases.md)
