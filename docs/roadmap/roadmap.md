# devpiano Roadmap

> 用途：作为唯一的项目状态、阶段路线与近期重点来源。
> 更新时机：阶段目标变化、功能完成度变化、重大风险变化时。

## 1. 项目目标

devpiano 是一款基于 JUCE 的现代 C++ 电脑键盘钢琴应用。

核心替代方向：

- 旧 WASAPI / ASIO / DSound 后端 -> JUCE `AudioDeviceManager`。
- 旧 VST 加载逻辑 -> JUCE `AudioPluginFormatManager` / `AudioPluginInstance`。
- 旧 Windows 键盘输入逻辑 -> JUCE `KeyListener` / `KeyPress` + 可配置 MIDI 映射。
- 旧 GDI / 原生控件 UI -> JUCE `Component` 树 + JIVE 声明式 UI 体系。
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

`MainComponent.cpp` 从 ~1587 行降至 ~446 行。
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

JIVE 声明式 UI 框架（`juce::ValueTree` 布局 + JSON 样式表 + Flex/Grid 自适应）替代 5 个面板的硬编码 `setBounds()` 布局；melatonin_inspector 运行时检查器加速 UI 迭代反馈；`design_tokens.json` 统一 JIVE 与原生组件样式来源；`Ctrl+R` / 文件监听热重载；`MainComponent::resized()` 缩减至 3 行（JIVE FlexBox 自动响应）。`CustomKeyboard` 与 ADSR 曲线经组件工厂原生注入，业务逻辑层零改动。回归验证全部通过。

详细计划与完成记录见 [`../archive/phase11-declarative-ui-jive.md`](../archive/phase11-declarative-ui-jive.md)。

### 全面审计 (2026-08-16) [已完成]

代码质量审计（`AUDIT-001`，2026-08-16）登记 85 项全部闭环（56 项未处理全关闭，14 项已暂缓维持）；三闸门全绿 + win-build 通过 + 全量 44 源码文件 clang-tidy 0 诊断。消除音频回调堆分配与延迟 prepare、修复 `masterGain` 跨线程数据竞争、提取公共离线渲染管线 `RenderPipeline`、断言总数提升至 3100+。

审计报告见 [`../audit/AUDIT-001-code-quality-audit-2026-08-16.md`](../audit/AUDIT-001-code-quality-audit-2026-08-16.md)，Phase A–H 逐项完成记录见 [`../archive/audit-001-code-quality-fix-phases.md`](../archive/audit-001-code-quality-fix-phases.md)。

### Phase 12–14：内置物理建模钢琴音源（SineSynth → Enhanced Modal Piano v3） [已完成，2026-08-19]

将内置 fallback 正弦合成器彻底替换为**自主拥有、纯 C++、零采样依赖的物理建模钢琴音源**：
- **Phase 12（谐波钢琴 v1）**：合并实时与离线 sine 实现为共享 `SineSynthVoice`；落地 8 分音谐波加法合成 `PianoSynthVoice`、velocity 响度/亮度双映射、分音独立衰减与参数化接线；
- **Phase 13（刚性失谐与模态耗散 v2）**：引入 JOS PASP 刚性琴弦失谐公式（$f_m = m f_0 \sqrt{1 + B m^2}$）、Mutable Instruments 模态能量耗散衰减与 3 峰音板谐振器；
- **Phase 14（增强模态合成 v3）**：Magic Circle 递归振荡器（零 `std::sin`，单核 CPU ≤ 0.7%）+ 20/14/8/6 分音覆盖 + two-stage decay（双阶段衰减）+ 同音三弦微失谐拍频（beating）+ 8 峰音板主模态组（75~950 Hz）。确立为唯一默认钢琴音色，全量 3101 断言全绿。

详细技术方案与逐项完成记录见 [`../archive/phase12-14-builtin-piano-synthesis.md`](../archive/phase12-14-builtin-piano-synthesis.md)。

### Phase 15：UI 架构统一至 JIVE（声明式弹窗与设置面板重构） [已完成，2026-08-19]

将主窗口之外的传统 JUCE 手工像素排版与弹窗体系全面统一进 **JIVE 声明式 UI 框架**：
1. **通用 JiveModalDialog 基础设施**：以 JIVE ValueTree 模板驱动预设新建/重命名/删除弹窗及歌曲信息（Metadata）编辑弹窗，彻底消除手写 `resized()` 坐标计算与冗余 Content 类；
2. **设置面板声明式重构（SettingsLayoutModel）**：使用 JIVE CSS Grid（8 列 × 2 行）声明 16 通道跟随开关，`AudioDeviceSelectorComponent` 封装为 Native 注入项，彻底消灭 `SettingsComponent` 中 300+ 行手写绝对坐标；
3. **模态操作与导出进度现代化**：`WavExportTask` 导出进度接入现代化 JIVE 暗黑 ProgressBar 声明式浮层；
4. **边界稳定**：保持 `CustomKeyboard` 原生自绘内核、`PluginEditorWindow` 原生宿主与系统 `FileChooser` 的合理边界。三闸门全绿，全量 3101 断言通过。

详细技术方案与逐项完成记录见 [`../archive/phase15-declarative-dialogs-and-settings-jive.md`](../archive/phase15-declarative-dialogs-and-settings-jive.md)。

## 3. 主要风险

| 风险 | 当前判断 | 应对方向 |
|---|---|---|
| 插件生命周期复杂 | 中 | 维护专项生命周期测试，重点覆盖 editor、卸载、重扫、退出。 |
| 键盘映射边界多 | 低 | 基础映射已全量验证；Performance Preset 已补充专项回归清单。 |
| JIVE API 稳定性（198 stars，MIT） | 中 | 固定 git commit hash；持续维护单元测试回归。 |
| `MainComponent` 职责回流 | 极低 | 架构收敛至 446 行，UI 布局完全下沉至 JIVE 与独立 Controller。 |
| 文档状态漂移 | 极低 | 本文件作为唯一 roadmap；当前任务只写入 [`current-iteration.md`](current-iteration.md)。 |

## 4. 完成标准参考

阶段性验收标准见：

- [`../reference/acceptance.md`](../reference/acceptance.md)

专项测试见：

- [`../reference/features/builtin-piano-synthesis.md`](../reference/features/builtin-piano-synthesis.md)
- [`../reference/features/declarative-ui-and-theming.md`](../reference/features/declarative-ui-and-theming.md)
- [`../reference/features/midi-channel-matrix.md`](../reference/features/midi-channel-matrix.md)
- [`../reference/features/per-key-customization.md`](../reference/features/per-key-customization.md)
- [`../reference/features/internationalization.md`](../reference/features/internationalization.md)
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
- Phase 11 完成记录（声明式 UI 架构）：[`../archive/phase11-declarative-ui-jive.md`](../archive/phase11-declarative-ui-jive.md)
- 2026-08-16 审计报告：[`../audit/AUDIT-001-code-quality-audit-2026-08-16.md`](../audit/AUDIT-001-code-quality-audit-2026-08-16.md)
- AUDIT Phase A–H 完成记录：[`../archive/audit-001-code-quality-fix-phases.md`](../archive/audit-001-code-quality-fix-phases.md)
- Phase 12–14 完成记录（内置物理建模钢琴音源）：[`../archive/phase12-14-builtin-piano-synthesis.md`](../archive/phase12-14-builtin-piano-synthesis.md)
- Phase 15 完成记录（声明式弹窗与设置面板重构）：[`../archive/phase15-declarative-dialogs-and-settings-jive.md`](../archive/phase15-declarative-dialogs-and-settings-jive.md)
