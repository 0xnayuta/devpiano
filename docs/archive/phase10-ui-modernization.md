# Phase 10 Completion Record — 主窗口 UI 现代化

> 归档日期：2026-07-23
> 来源：[`../roadmap/current-iteration.md`](../roadmap/current-iteration.md) Phase 10
> 后续方向：审计修复详见 [`../audit/code-quality-audit-2026-07-20.md`](../audit/code-quality-audit-2026-07-20.md)

---

## Phase 10：主窗口 UI 现代化

**目标**：将主窗口四大面板（HeaderPanel / PluginPanel / ControlsPanel / KeyboardPanel）从开发调试风格的表单布局，升级为接近现代化商业钢琴 VST 宿主软件的专业界面。

**前置依赖**：Phase 9 构成稳定的功能基线，Phase 10 在其上进行纯 UI 层改造，无功能行为变更。

**核心设计原则**：
- 不改动现有 MIDI / 音频 / 插件 / 录制逻辑，仅变更 Component 布局与绘制
- 所有视觉参数集中管理（颜色、圆角、间距、字体），便于后续统一调优
- 优先确保窗口高度 ≤700px 时仍可用，再向上拉伸时充分利用额外空间

---

### 10a. 自定义 LookAndFeel 全局视觉主题

**状态**：已完成 (2026-07-21)。新建 `source/UI/DevPianoLookAndFeel.h/.cpp`，继承 `juce::LookAndFeel_V4`，在 `MainComponent::initialiseUi()` 中通过 `setLookAndFeel()` 安装。

**已交付**：
- 9 个公开色彩常量（`kMainBg` / `kPanelBg` / `kControlBg` / `kPrimary` / `kRecordActive` / `kPlayActive` / `kTextPrimary` / `kTextSecondary` / `kTextDisabled`），后续 10b–10f 可直接引用
- 11 个绘制方法覆写：`drawButtonBackground` (4px 圆角 + 微渐变 + hover/press 反馈)、`drawComboBox` (圆角 + 矢量三角箭头)、`drawPopupMenuItem` (暗色菜单)、`drawLinearSlider` (2px 细线滑槽 + 6×18 圆角 thumb)、`drawRotarySlider` (10b 预备)、`fillTextEditorBackground`/`drawTextEditorOutline` (2px 圆角边框)、`drawLabel` / `getLabelFont` (默认 14px)
- L&F ColourScheme + 30+ 颜色 ID 注册覆盖全部 widget 类型
- 移除 ~40 处冗余 `setColour()` 调用（KeyBindingEditDialog / PerformanceMetadataDialog / HeaderPanel / PluginPanel）
- 三个 DialogWindow 正确传播 L&F（BindingEditWindow / ColourPicker / PerformanceMetadataDialog）
- SettingsWindowManager 的 SettingsDialogWindow 从 `MainComponent` 继承 L&F
- WSL clang + Windows MSVC 双平台构建 0 error，单元测试 100% pass，格式检查 0 violations

### 10b. ControlsPanel 布局重构：水平滑动条 → 横向旋转旋钮 + ADSR 曲线

**状态**：已完成 (2026-07-21)。将 6 个 `LinearHorizontal` 滑动条替换为 `RotaryHorizontalVerticalDrag` 旋转旋钮，并在 knob 行与 preset 行之间新增 ADSR 包络曲线可视化。

**已交付**：
- `configureSlider` → `configureKnob`：RotaryHorizontalVerticalDrag 风格、TextBoxBelow (44×16)、7→5 点钟弧形（1.25π–2.75π）、value formatter、double-click 归中
- 6 个 knob 横向一字排开，每 knob 含：冰蓝弧线旋钮 → 下方文本值显示 → 下方居中标签
- ADSR 包络曲线：44px 区域，ms-scaled 四段折线 + 半透明填充 + 1.5px 冰蓝描边，旋钮拖拽时实时更新
- 值格式化：Volume/Sustain 2 位小数、Attack/Decay/Release "0.500s"、Speed "1.0x"
- `resized()` 布局：knob 行 (68px) → ADSR 曲线 (44px) → preset 行 (24px) → recording 行 (24px)，总高 174px ≤ 180px
- Public API（getters / `setValues` / callbacks）零变更，MainComponent 调用侧零改动

---

### 10c. PluginPanel 紧凑化与可折叠

**状态**：已完成 (2026-07-21)。PluginPanel 拆分为始终可见的工具栏行和可折叠高级区，折叠态高度从 188px 降至 40px。

**已交付**：
- 工具栏行（始终可见）：`pluginStatusLabel` + `pluginSelector` (180px) + `instrumentFilterCombo` (100px) + `Load`/`Unload`/`OpenEditor` 按钮 + 折叠切换 `toggleButton` ("⋮")，共 28px
- 高级区（默认折叠）：`VST3 Path` 标签 + 路径编辑器 + `Browse`/`Scan` 按钮 + 已发现插件列表编辑区，可见性由 `addChildComponent` + `setExpanded` 管控
- `setExpanded(bool)` / `getPreferredHeight()`：折叠 40px / 展开 160px
- 状态持久化：`SettingsModel::pluginPanelExpanded` → `SettingsStore` 读写，重启保持
- `pluginListLabel` 和 `pluginSelectionLabel` 移除（冗余标签）
- `MainComponent`：`resized()` 高度改为 `getPreferredHeight()` 动态获取
- 5 个高级区 widget 用 `addChildComponent` 初始不可见，避免首次折叠态时的可见性泄漏

### 10d. 虚拟钢琴键盘拟真渲染

**状态**：已完成 (2026-07-22)。白键/黑键渲染从调试风格纯色矩形升级为渐变填充 + 底部圆角 + 阴影投影 + 中部扩散高亮反馈，并支持黑键 binding label 显示。

**已交付**：
- **白键**：`0xfff0f0f0`→`0xffd8d8d8` 纵向渐变填充，底部两角 2px 圆角（`addRoundedRectangle` + bool 控制），fade 高亮从键中部向顶部扩散渐变，边框 `0xffaaaaaa`
- **黑键**：`0xff444444`→`0xff1a1a1a` 纵向渐变 + 两层阴影投影（alpha 0.08/0.05，仅侧边+底部，expand 1.0/1.5px）+ 底部圆角 + fade 中部向上渐变高亮 + 键高随 fade 线性微缩 0→2px（无 snap-back）+ 边框 `0xff333333`
- **黑键标签**：移除 `isWhite` guard，上半部绑定位标签（字体 `jmin(10, kw*0.4f)`，颜色 `0xffcccccc`），跳过音名
- 仅修改 `CustomKeyboard.cpp` 三个 `paint*` 方法，`KeyRenderState` 及鼠标/几何/动画逻辑零变更

---

### 10e. Transport 按钮图标化 + 底部状态栏

**状态**：已完成 (2026-07-22)。传输按钮从文字 TextButton 升级为矢量化 DrawableButton，新增底部 StatusBar，HeaderPanel 压缩至 36px。

**已交付**：
- **Transport 图标**：Record (⬤ 圆)/ Play (▶ 三角)/ Stop (■ 方块)/ Back (◀◀ 双三角) 4 个 `DrawableButton`，36×28px，通过 `DrawablePath` + `setFill` 上色，tooltip 显示功能名
- **底部 StatusBar**：22px，MIDI 活动绿点（`kPlayActive`）+ 插件名 + 音频设备信息 + 时间，背景 `kPanelBg.darker(0.2f)` + 顶部 1px 分隔线，独立于四大面板分配
- **HeaderPanel 压缩**：移除 `hintLabel`/`AudioStatus`/`setHintText`/`HeaderPanelStateBuilder`，标题字体 22→18pt，设置按钮改为齿轮 DrawableButton（非零则矢量合成），高度 98→36px，单行布局
- **ControlsPanel**：移除 `recordStatusLabel`，缩减传输按钮间距
- **国际化**：`statusBar.refreshTexts()` 接入 `MainComponent::refreshAllTexts()` 链，语言切换同步

---

### 10f. 布局尺寸规则调整

**状态**：已完成 (2026-07-22)。仅修改 `MainComponent::resized()`，引入 ControlsPanel ↔ KeyboardPanel 动态高度分配算法。

**已交付**：
- `maxKeyboardHeight` 128 → **200px**；新增 `minKeyboardHeight = 90px`
- 动态分配算法：固定面板后剩余高度 (`alloc`) 在 ControlsPanel 与 KeyboardPanel 间分配
  - 基准线 264px（controls 174 + keyboard 90），溢出按 **60% keyboard / 40% controls** 分配，键盘封顶 200px
  - 紧凑情况（`alloc ≤ 264`）：键盘守住 90px 底线，ControlsPanel 压缩
- 各窗口高度下键盘分配：默认 760px → 200px（cap），最小 700px 展开 → 184px，900px → 200px（cap）
- `removeFromBottom` → `removeFromTop`（语义等价，总分配填满 content）

**验收覆盖**：
- (a) 默认 760px 键盘 200px ≥ 116 ✓
- (b) 900px 键盘 200px ✓
- (c) 700px 键盘 184px ≥ 90，所有面板可见 ✓
- (d) PluginPanel 折叠态独立 ✓

---

## 已知回归项

各缺陷的完整修复记录见 [`../issues/known-issues.md`](../issues/known-issues.md) §2：
- 启动早期首音音高异常（保留 `25ms` audio warmup）
- MIDI 导入播放首音无声（playback-start pre-roll / arming）
- 辅助窗口键盘焦点冲突（restoreKeyboardFocus guard）
- Phase 6-2 播放速度控制（公式修正 + position 重校准 + atomic 化）
