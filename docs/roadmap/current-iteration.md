# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

**ADR-014 实施计划：内化 Devpiano UI 基础设施与 JIVE 子模块退役治理 [进行中，2026-09-01 开始]**

依据 [`docs/decisions/ADR-014-internalize-ui-infrastructure-and-deprecate-jive-submodule.md`](../decisions/ADR-014-internalize-ui-infrastructure-and-deprecate-jive-submodule.md) 架构决策，devpiano 将全面退役外部 `submodules/JIVE` Git 子模块，采用**「路线 B：先提后升（Extract Minimal Closure -> Submodule Deinit -> JUCE 9.0.1 Ready）」**实施策略。

本轮迭代专项目标为：
1. 提取 JIVE 核心最小依赖闭包（约 3,800 行代码）入 `source/UI/jive/core/`，剔除 >15,000 行冗余死代码（Grid, XML, Unused Widgets, FileObserver, Perfetto）；
2. 储备高潜力未来资产入 `source/UI/jive/extensions/`（`Grid` 与 `Transitions` 动画，供后续 Preset Browser / Channel Matrix 战略按需激活）；
3. 纯化 CMake 构建配置，彻底注销 `submodules/JIVE` 子模块；
4. 修复 JUCE 9 兼容断点（`DrawableComponent` 与 `FontOptions`），完成全套门禁与 Windows MSVC 验证。

---

## 本轮子任务排期（ADR-014 UI Infrastructure Implementation Phases）

### ADR-014 Phase 0：基线测试冻结与合规归档准备 [待开始]
> 目标：确保当前在 JUCE 8.0.15 上的测试基线 100% 绿灯，建立开源合规清单与备份。

- [ ] 执行环境自检与全量测试基线验证（`./scripts/dev.sh test`，12187+ 断言全绿）
- [ ] 在项目根目录创建 `THIRD_PARTY_NOTICES.md`，记录 JIVE 原作者（James Johnson）、MIT 许可证全文与快照 Commit（`89d5787`）
- [ ] 归档并验证 `design_tokens.json` 与 `style_sheets.json` 静态资产完整性

### ADR-014 Phase 1：提取 JIVE 核心最小依赖闭包（`source/UI/jive/core/`） [待开始]
> 目标：精准内化核心闭包代码，剥离死代码，完成命名空间与头文件依赖迁移。

- [ ] **创建目录结构**：
  - `source/UI/jive/core/`（基础核心运行时）
  - `source/UI/jive/extensions/`（战略储备扩展：`grid/`, `kinetics/`）
- [ ] **迁移 Core 基础层**（保留文件头 MIT 版权声明）：
  - 属性系统：`BoxModel`、`Property`、`PropertyBehaviours`、`Object`、`ReferenceCountedValueTreeWrapper`
  - 几何与交互：`BorderRadii`、`BorderWidth`、`Length`、`Orientation`、`Event`、`InteractionState`
  - 变体转换：`FlexVariantConverters`、`MiscVariantConverters`、`VariantConvertion`
- [ ] **迁移 Layout 核心层**：
  - 解释与节点：`Interpreter`、`GuiItem`、`GuiItemDecorator`、`CommonGuiItem`、`ContainerItem`、`ComponentFactory`
  - 弹性排版：`FlexContainer`、`FlexItem`、`LayoutStrategy`、`Display`、`Overflow`
  - 基础块项：`BlockContainer`、`BlockItem`（简化版）
- [ ] **迁移核心控件包装与样式引擎**：
  - 控件：`Button`、`ComboBox`、`Slider`、`ProgressBar`、`Text`（精简测量版）
  - 样式与画布：`StyleSheet`（精简版）、`Colours`、`Fill`、`Shadow`、`BackgroundCanvas`
- [ ] **归档战略扩展（KEEP-LATER）**：
  - 将 `GridContainer`、`GridItem`、`GridVariantConverters` 归入 `source/UI/jive/extensions/grid/`（暂不加入默认编译）
  - 将 `Transitions`、`Easing` 归入 `source/UI/jive/extensions/kinetics/`
- [ ] **清理与重定向头文件包含**：
  - 全局重定向 `source/` 下各调用点至本地头文件（`#include "UI/jive/core/..."`）
  - 消除 `jive::` 外部模块包含，调整命名空间为项目统一的 `devpiano::ui::jive`（或 `jive` 别名过渡）

### ADR-014 Phase 2：注销 JIVE Git 子模块与 CMakeLists 纯化 [待开始]
> 目标：彻底从工程构建体系中移除 `submodules/JIVE`，实现完全自包含构建。

- [ ] **CMakeLists.txt 纯化**：
  - 移除 `add_subdirectory(submodules/JIVE)`
  - 移除 target 链接中的 `jive::jive_layouts`、`jive::jive_style_sheets`、`jive::jive_core`
  - 将 `source/UI/jive/core/` 源码加入 `devpiano` 与 `devpiano_tests` 目标编译清单
- [ ] **Git Submodule 退役**：
  - 执行 `git submodule deinit -f submodules/JIVE`
  - 移除 `.gitmodules` 中的 JIVE 条目（或保留注释归档）
- [ ] **构建与测试验证**：
  - 运行 `./scripts/dev.sh wsl-build --configure-only` 刷新 `compile_commands.json`
  - 运行 `./scripts/dev.sh test` 确保 100% 单元测试在无子模块环境下通过

### ADR-014 Phase 3：JUCE 9.0.1 兼容性修复与三闸门全量验证 [待开始]
> 目标：消除已知的 JUCE 9 API 断点，确保代码库完全具备随时升级 JUCE 9.0.1 的技术状态。

- [ ] **适配 JUCE 9 UI API**：
  - 检查并重构 `TextComponent` 与 `FontUtilities` 文本宽度测量，全面使用 `juce::GlyphArrangement` 与 `juce::FontOptions`
  - 适配 `Drawable` 包装机制（JUCE 9 `DrawableComponent` 规范）
  - 确认 `StyleCatalog` 与 `Object` 的 `var` 深度值比对行为稳定
- [ ] **UI 交互全量功能回归**：
  - 主窗口 88 键拟真键盘、自绘包络、工具栏状态响应
  - 插件面板高度折叠/展开动画与重新排版（`layOutChildren`）
  - 全局模态弹窗（新建/重命名预设、删除确认、歌曲信息、WAV 导出进度条）
  - 设置窗口（音频设备、调号、通道矩阵跟随开关）
  - 运行时中英文切换与 Token 热重载
- [ ] **全套门禁闭环**：
  - 代码格式合规：`./scripts/dev.sh format --check`
  - 单元测试套件：`./scripts/dev.sh test`
  - Windows MSVC 验证构建：`./scripts/dev.sh win-build`
  - 增量静态检查：`./scripts/dev.sh tidy`

---

## 后续规划路线（Upcoming Backlog）

- **Phase 27：现实物理演奏交互与声学控制（Physical Voicing & Realistic Acoustic Interaction）**：
  - **琴盖开合度交互控制（Lid Position）**：在 JIVE UI 界面接入 Full Open / Half Stick / Closed 3 态直观选择，无缝切换底层已实现的 `lidAcoustics` 多级高频滚降与近场反射；
  - **弱音/移位踏板物理拟真（Una Corda / Soft Pedal，CC 67）**：在 `PianoSynthVoice` 中模拟击弦机右移 3 弦敲 2 弦与毛毡侧面软化物理机理，支持 CC 67 踏板信号与 UI 软踏板状态点亮；
  - **触键力度曲线（Touch Velocity Curve）**：在 `KeyboardMidiMapper` / Input 层提供 Standard / Light / Heavy / Wide Dynamic 4 种配重手感映射，自适应薄膜/机械轴/MIDI 键盘；
  - **配置持久化与预设系统联动**：将琴盖位置、Una Corda 状态与触键曲线完整纳入 `SettingsModel`、`SettingsSerialization` 与 `PerformancePreset`（`.devpiano.preset` JSON）。
- **JUCE 9.0.1 正式切换与 Release 打包验证**。

---

## 历史实现 Backlog

- AUDIT-002 修复阶段归档（全量 62 项缺陷修复与质量门禁闭环）：[`../archive/audit-002-code-quality-fix-phases.md`](../archive/audit-002-code-quality-fix-phases.md)
- Phase 26 完成记录（MIDI 多轨并轨与综合时间线合并）：[`../archive/phase26-midi-multi-track-timeline-merge.md`](../archive/phase26-midi-multi-track-timeline-merge.md)
- Phase 25 完成记录（Linux 原生桌面构建与音频驱动适配）：[`../archive/phase25-linux-desktop-and-audio-path.md`](../archive/phase25-linux-desktop-and-audio-path.md)
- Post-v1.0.0 文档体系治理与打包流水线自动化完成记录：[`../guides/release-workflow.md`](../guides/release-workflow.md)
- Phase 24 完成记录（生命力与非线性动力学绽放）：[`../archive/phase24-vitality-and-dynamic-blooming.md`](../archive/phase24-vitality-and-dynamic-blooming.md)
- Phase 23 完成记录（大师级音色校准与 Pianoteq 对齐精调）：[`../archive/phase23-master-voicing-realism-calibration.md`](../archive/phase23-master-voicing-realism-calibration.md)
- Phase 22 完成记录（物理声学极致深化与机械拟真）：[`../archive/phase22-physical-modeling-acoustic-refinement.md`](../archive/phase22-physical-modeling-acoustic-refinement.md)
- Phase 21 完成记录（踏板交感共鸣与琴盖空间声学）：[`../archive/phase21-sympathetic-resonance-lid-acoustics.md`](../archive/phase21-sympathetic-resonance-lid-acoustics.md)
- Phase 20 完成记录（微观物理动力学：纵向波先驱声与击键混沌微扰）：[`../archive/phase20-longitudinal-ping-micro-variation.md`](../archive/phase20-longitudinal-ping-micro-variation.md)
- Phase 19 完成记录（立体声音板共鸣箱与同音三弦微动力学）：[`../archive/phase19-stereo-modal-soundboard.md`](../archive/phase19-stereo-modal-soundboard.md)
- Phase 18 完成记录（88 键物理参数化与微观相位色散）：[`../archive/phase18-per-note-voicing-micro-phases.md`](../archive/phase18-per-note-voicing-micro-phases.md)
- Phase 17 完成记录（真实物理打击感钢琴音源重构）：[`../archive/phase17-physical-strike-hammer-piano.md`](../archive/phase17-physical-strike-hammer-piano.md)
- Phase 16 完成记录（虚拟键盘局部脏矩形重绘与预设覆盖确认）：[`../archive/phase16-keyboard-dirty-repaint-preset-confirm.md`](../archive/phase16-keyboard-dirty-repaint-preset-confirm.md)
- Phase 15 完成记录（声明式弹窗与设置面板重构）：[`../archive/phase15-declarative-dialogs-and-settings-jive.md`](../archive/phase15-declarative-dialogs-and-settings-jive.md)
- Phase 12–14 完成记录（内置物理建模钢琴音源三部曲）：[`../archive/phase12-14-builtin-piano-synthesis.md`](../archive/phase12-14-builtin-piano-synthesis.md)
- AUDIT-001 修复阶段归档：[`../archive/audit-001-code-quality-fix-phases.md`](../archive/audit-001-code-quality-fix-phases.md)
- Phase 11 完成记录（声明式 UI 架构）：[`../archive/phase11-declarative-ui-jive.md`](../archive/phase11-declarative-ui-jive.md)
