# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

Phase 10：主窗口 UI 现代化 — 10a 已完成，详见下方。Phase 8–9 完成记录已归档至 [`../archive/phase8-9-completion.md`](../archive/phase8-9-completion.md)。

---

## Phase 10：主窗口 UI 现代化

**目标**：将主窗口四大面板（HeaderPanel / PluginPanel / ControlsPanel / KeyboardPanel）从开发调试风格的表单布局，升级为接近现代化商业钢琴 VST 宿主软件（Pianoteq / Kontakt / Gig Performer）的专业界面。

**前置依赖**：Phase 9（Performance Preset、全音域键盘、Smooth Pitch Bend、Song Info 构成稳定的功能基线，Phase 10 在其上进行纯 UI 层改造，无功能行为变更）。

**核心设计原则**：
- 不改动现有 MIDI / 音频 / 插件 / 录制逻辑，仅变更 Component 布局与绘制
- 所有视觉参数集中管理（颜色、圆角、间距、字体），便于后续统一调优
- 优先确保窗口高度 ≤700px（最小限制）时仍可用，再向上拉伸时充分利用额外空间

---

### 10a. 自定义 LookAndFeel 全局视觉主题 ✅ 已完成

**状态**：已完成 (2026-07-21)。新建 `source/UI/DevPianoLookAndFeel.h/.cpp`，继承 `juce::LookAndFeel_V4`，在 `MainComponent::initialiseUi()` 中通过 `setLookAndFeel()` 安装。

**已交付**：
- 9 个公开色彩常量（`kMainBg` / `kPanelBg` / `kControlBg` / `kPrimary` / `kRecordActive` / `kPlayActive` / `kTextPrimary` / `kTextSecondary` / `kTextDisabled`），后续 10b–10f 可直接引用
- 11 个绘制方法覆写：`drawButtonBackground` (4px 圆角 + 微渐变 + hover/press 反馈)、`drawComboBox` (圆角 + 矢量三角箭头)、`drawPopupMenuItem` (暗色菜单)、`drawLinearSlider` (2px 细线滑槽 + 6×18 圆角 thumb)、`drawRotarySlider` (10b 预备)、`fillTextEditorBackground`/`drawTextEditorOutline` (2px 圆角边框)、`drawLabel` / `getLabelFont` (默认 14px)
- L&F ColourScheme + 30+ 颜色 ID 注册覆盖全部 widget 类型
- 移除 ~40 处冗余 `setColour()` 调用（KeyBindingEditDialog / PerformanceMetadataDialog / HeaderPanel / PluginPanel）
- 三个 DialogWindow 正确传播 L&F（BindingEditWindow / ColourPicker / PerformanceMetadataDialog）
- SettingsWindowManager 的 SettingsDialogWindow 从 `MainComponent` 继承 L&F
- WSL clang + Windows MSVC 双平台构建 0 error，单元测试 100% pass，格式检查 0 violations

### 10b. ControlsPanel 布局重构：水平滑动条 → 横向旋转旋钮 + ADSR 曲线 ✅ 已完成

**状态**：已完成 (2026-07-21)。将 6 个 `LinearHorizontal` 滑动条替换为 `RotaryHorizontalVerticalDrag` 旋转旋钮，并在 knob 行与 preset 行之间新增 ADSR 包络曲线可视化。

**已交付**：
- `configureSlider` → `configureKnob`：RotaryHorizontalVerticalDrag 风格、TextBoxBelow (44×16)、7→5 点钟弧形（1.25π–2.75π）、value formatter、double-click 归中
- 6 个 knob 横向一字排开（slot=width/6），每 knob 含：冰蓝弧线旋钮 → 下方文本值显示 → 下方居中标签
- ADSR 包络曲线：44px 区域，ms-scaled 四段折线 + 半透明填充 + 1.5px 冰蓝描边，旋钮拖拽时实时更新
- 值格式化：Volume/Sustain 2 位小数、Attack/Decay/Release "0.500s"、Speed "1.0x"
- `resized()` 布局：knob 行 (68px) → ADSR 曲线 (44px) → preset 行 (24px) → recording 行 (24px)，总高 174px ≤ 180px
- Public API（getters / `setValues` / callbacks）零变更，MainComponent 调用侧零改动

---

### 10c. PluginPanel 紧凑化与可折叠 ✅ 已完成

**状态**：已完成 (2026-07-21)。PluginPanel 拆分为始终可见的工具栏行和可折叠高级区，折叠态高度从 188px 降至 40px。

**已交付**：
- 工具栏行（始终可见）：`pluginStatusLabel` + `pluginSelector` (180px) + `instrumentFilterCombo` (100px) + `Load`/`Unload`/`OpenEditor` 按钮 + 折叠切换 `toggleButton` ("⋮")，共 28px
- 高级区（默认折叠）：`VST3 Path` 标签 + 路径编辑器 + `Browse`/`Scan` 按钮 + 已发现插件列表编辑区，可见性由 `addChildComponent` + `setExpanded` 管控
- `setExpanded(bool)` / `getPreferredHeight()`：折叠 40px / 展开 160px
- 状态持久化：`SettingsModel::pluginPanelExpanded` → `SettingsStore` 读写，重启保持
- `pluginListLabel` 和 `pluginSelectionLabel` 移除（冗余标签）
- `MainComponent`：`resized()` 高度改为 `getPreferredHeight()` 动态获取，`ControlsPanel` 高度常量从 296→174
- 5 个高级区 widget 用 `addChildComponent` 初始不可见，避免首次折叠态时的可见性泄漏

### 10d. 虚拟钢琴键盘拟真渲染

**现状**：白键为纯色 `0xffe8e8e8` 矩形 + 灰色细边框，黑键为纯色 `0xff333333` 矩形 + 灰色边框，无圆角、无阴影、无渐变——视觉上更接近调试工具而非乐器界面。

**方案**：
- **白键渲染**：
  - 纵向渐变填充：顶部微亮（`0xfff0f0f0`）→ 底部微暗（`0xffd8d8d8`），模拟琴键侧面光线反射
  - 底部两角 2px 圆角（顶部保持直角，与物理钢琴键一致）
  - 边框改用 `0xffaaaaaa`（比当前 `0xff888888` 更柔和）
  - 按键按下时（fade > 0）：叠加功能高亮色半透明渐变（从键中部向上扩散）
- **黑键渲染**：
  - 纵向渐变填充：顶部 `0xff444444` → 底部 `0xff1a1a1a`
  - 左右两侧 + 底部绘制 3px 模糊暗色阴影（用 `juce::DropShadow` 或手动多层半透明矩形 offset），使黑键浮于白键之上
  - 底部两角 2px 圆角
  - 边框改用 `0xff333333`
  - 按键按下时：叠加高亮色，键高度微缩 2px（模拟物理按压下沉）
- **黑键标签显示**：
  - 在 `paintKeyLabels` 中移除 `if (!k.isWhite) continue` 限制
  - 黑键标签绘制在键体上半部（因黑键下半部可能被白键遮挡视觉），颜色使用浅灰 `0xffcccccc`
  - 字体大小取 `jmin(10, keyWidth * 0.4f)`
- 所有绘制逻辑在 `CustomKeyboard::paintWhiteKeys` / `paintBlackKeys` / `paintKeyLabels` 中修改，不改变 `KeyRenderState` 数据结构

**验收标准**：
- (a) 白键有顶部→底部微渐变，底部 2px 圆角，边框颜色柔和
- (b) 黑键有上下渐变 + 下方/侧方阴影投影，底部圆角，视觉上浮于白键
- (c) 黑键上显示键盘映射标签（如 "W"、"E" 等）
- (d) 按键按下时白键和黑键的高亮反馈明显且美观
- (e) 水平滚动、鼠标点击、note on/off 功能不受渲染变更影响

---

### 10e. Transport 按钮图标化 + 底部状态栏

**现状**：录制/回放控制行共 12 个纯文字 `TextButton` 紧密排列（Record / Play / Stop / Back / Import MIDI / Export MIDI / Export WAV / Save / Open / Recent），视觉拥挤且不符合音频软件行业惯例（图标 > 文字）。

**方案**：
- 用 `juce::DrawableButton` 替换传输核心按钮（Record / Play / Stop / Back to Start）：
  - 使用 `juce::Drawable` 绘制简单矢量图标（圆形 = 录制、三角 = 播放、方块 = 停止、双左三角 = 回到开头）
  - 按钮尺寸缩小至 36×28，图标 16×16 居中
  - 按钮 tooltip 显示中文/英文标签
- 文件操作按钮（Save/Open/Import/Export/Recent）保留为文字 `TextButton`，但缩小字体至 12px，分组放在传输按钮右侧
- **底部状态栏**：
  - 在 `MainComponent::resized` 中，于键盘面板下方（即整个窗口最底部）分配 22px 高度作为状态栏
  - 新建简单 `StatusBar` Component（也可以是 `MainComponent` 的直接子 Label）：
    - 左侧：MIDI 活动指示灯（实心圆点，有 MIDI 事件时绿色脉冲）+ 当前 VST 插件名
    - 中间：录音/回放时间显示（如 "01:23.4"）
    - 右侧：音频设备信息（采样率 + buffer size，如 "44.1kHz / 512"）
  - 背景色使用略深于主窗口的颜色，顶部 1px 分隔线
- **HeaderPanel 精简**：
  - 移除 `hintLabel`（当前 VST 状态信息迁移至状态栏）
  - `HeaderPanel` 总高度从 98px 降至约 36px（仅标题 + 设置按钮）
  - 设置按钮改用齿轮图标（`juce::DrawableButton`），宽度从 110px 缩至 36px

**验收标准**：
- (a) 传输按钮以矢量图标显示，hover 时 tooltip 显示功能名称
- (b) 底部状态栏显示 MIDI 活动灯、插件名、时间、音频设备信息
- (c) 状态栏高度 22px，不占用四大面板的布局空间
- (d) HeaderPanel 压缩至 36px，VST 状态信息不再显示在顶部
- (e) 所有文字按钮的原有功能不受影响

---

### 10f. 布局尺寸规则调整

**现状**：四大面板均使用硬编码固定高度（98 / 188 / 296 / `jmin(128, remaining)`），窗口高度增加时键盘面板最高 128px，超出部分全部闲置——用户拉伸窗口无法获得更大键盘。

**方案**：
- **键盘高度下限保障**：
  - 将 `maxKeyboardHeight` 从 128 提高到 200
  - 新增 `minKeyboardHeight = 90`：当窗口高度不足时，键盘面板至少 90px，优先压缩 ControlsPanel 或 PluginPanel
- **动态溢出分配**：
  - 当窗口高度 > 默认 760px 时，溢出空间按比例分配给键盘面板（60%）和 ControlsPanel（40%），PluginPanel 和 HeaderPanel 保持折叠态高度不变
  - 实现方式：`MainComponent::resized` 中先分配固定最小高度给各面板，再将剩余高度（`remaining > 0`）按 `0.6 : 0.4` 追加给键盘面板和 ControlsPanel
- **最小窗口验证**：
  - 确保在 980×700 窗口下，所有控件仍可见且可操作，键盘高度不低于 90px

**验收标准**：
- (a) 默认窗口下键盘高度 ≥ 116px（与当前持平或更大）
- (b) 窗口拉伸到 900px 高度时，键盘高度达到约 200px（上限）
- (c) 最小 700px 高度窗口下，键盘高度 ≥ 90px，所有面板均可见
- (d) PluginPanel 折叠态不受高度变化影响

---

## 本轮决策

- Phase 10 为纯 UI 层改造，不涉及 MIDI / 音频 / 插件 / 录制功能变更，风险边界清晰
- 子项按视觉依赖排序：10a（LookAndFeel）是所有视觉变更的基础，须最先实施；10b/10c/10d 可在 10a 完成后并行；10e/10f 依赖前三项确定后的剩余空间来设计

## 执行顺序

```
Phase 10a (LookAndFeel) ✅ 已完成
Phase 10b (Knobs + ADSR) ✅ 已完成
Phase 10c (PluginPanel 折叠化) ✅ 已完成
         │
         ├──→ 10d (Keyboard 拟真渲染)
         └──→ 10e (Transport 图标 + StatusBar)
         └──→ 10f (布局尺寸规则调整)
```

注：10e/10f 可并行，但建议在 10b/10c/10d 完成后进行以准确掌握剩余空间。


## 已修复缺陷（回归提醒）

各缺陷的完整修复记录与回归条件见 [`../issues/known-issues.md`](../issues/known-issues.md) §2：
- 启动早期首音音高异常（保留 `25ms` audio warmup）。
- MIDI 导入播放首音无声（playback-start pre-roll / arming）。
- 辅助窗口键盘焦点冲突（restoreKeyboardFocus guard）。
- Phase 6-2 播放速度控制（公式修正 + position 重校准 + atomic 化）。

## 验证命令

代码修改后优先执行：

```bash
./scripts/dev.sh wsl-build --configure-only
./scripts/dev.sh win-build
```

涉及环境或路径问题时执行：

```bash
./scripts/dev.sh self-check
```

## 相关文档

- 项目路线图：[`roadmap.md`](roadmap.md)
- Phase 8–9 完成记录：[`../archive/phase8-9-completion.md`](../archive/phase8-9-completion.md)
- 架构概览：[`../reference/architecture.md`](../reference/architecture.md)
- Phase 5 完成记录：[`../archive/phase5-architecture-convergence.md`](../archive/phase5-architecture-convergence.md)
- Phase 6-7 完成记录：[`../archive/phase6-7-completion-detail.md`](../archive/phase6-7-completion-detail.md)
- 架构优化完成记录：[`../archive/architecture-optimization-backlog.md`](../archive/architecture-optimization-backlog.md)
