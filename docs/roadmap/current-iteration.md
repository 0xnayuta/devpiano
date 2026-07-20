# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

Phase 9：配置快照与体验增强（详见 [`roadmap.md`](roadmap.md) §Phase 9）— 子项 9d 待实施。
Phase 10：主窗口 UI 现代化 — 当前迭代主攻方向，详见下方 §Phase 10。

---

## Phase 8：逐键个性化与调号系统

**目标**：为每个电脑键盘键提供独立的自定义标签和颜色支持；实现全局调号 + MIDI 移调 + per-channel Follow Key，打通"用 C 大调指法弹任何调"的核心演奏场景。

**前置依赖**：无（Phase 1-7 基础设施已就位）。

### 8a. Per-Key Labels + Colors（逐键标签与颜色）— ✅ 已完成

**实现摘要**：
- 数据模型：`KeyboardSettings` 新增 `customKeyLabels[128]`，`customKeyColours[128]` 已在
- 持久化：`SettingsStore` 以稀疏 ValueTree XML 读写，空集合时 `removeValue` 防止残留
- 渲染：`CustomKeyboard` 三级标签优先级（custom label > binding displayText > note name）；底色/fade overlay 优先检查自定义颜色
- UI：`KeyBindingEditDialog` 新增 Label 文本框（`setInputRestrictions(32, {})`）+ Colour 拾取器（`ColourPickerContent` 包装 `ColourSelector`，含 OK/Cancel）
- 接线：`MainComponent::syncUiFromSettings` 传递 labels/colours；`onBindingEditRequested` 读→dialog→回写→`syncUiFromSettings`+`saveSettingsSoon`；`SettingsWindowManager` 同步更新
- 翻译：zh-CN 新增 5 条（Label:/Choose Colour.../Colour Set/Clear/Choose Colour）
- 改动文件 9 个：`Core/KeyboardTypes.h`、`Settings/SettingsModel.h`、`Settings/SettingsStore.cpp`、`UI/CustomKeyboard.cpp`、`UI/KeyBindingEditDialog.h/.cpp`、`MainComponent.cpp`、`Settings/SettingsWindowManager.cpp`、`Locale/zh_CN.loc.h`

**验收通过**：
- (a) 自定义标签覆盖默认键名 ✓
- (b) 自定义颜色覆盖 auto-color ✓
- (c) 清除标签/颜色回退默认 ✓
- (d) 重启持久化恢复 ✓
- 审查修复 5 项（SettingsWindowManager 遗漏同步、清除全部后旧数据残留、无绑定模式多余 save、无限长标签、按钮颜色残留）全部修复 ✓

---

### 8b. Key Signature + MIDI Transpose + Follow Key（调号系统）— ✅ 已完成

**实现摘要**：
- 数据模型：`AppState` / `SettingsModel` 新增 `bool midiTranspose`、`int keySignature`（-7..+7）；`PerChannelConfig` 新增 `bool followKey : 1`（复用 bitfield padding，零内存增长）
- 持久化：`SettingsStore` 以 `keySignature` / `midiTranspose` 两个 PropertiesFile key 读写；`followKey` 纳入 `channelMatrix` ValueTree 序列化（缺失属性 → 默认 false，向后兼容）
- MIDI 管道：`MidiChannelMapper` 构造时接受 `const bool& midiTranspose`、`const int& keySignature` 引用；`sendNoteOn/Off` 四路分支（矩阵 on/off × midiTranspose on/off × followKey on/off），note-on/off 成对变换，所有路径 `jlimit(0, 127)` 钳位
- UI：`SettingsComponent` 新增 "Key Signature" group——调号 ComboBox（12 项，ID→半音偏移查表）+ "MIDI Transpose" Toggle + Ch1~Ch16 Follow Key 网格（2×8，midiTranspose 关闭时自动隐藏）；`editingState` ValueTree 绑定 + `referTo` 双向同步
- 翻译：zh-CN 新增 5 条（Key Signature/Key Signature:/MIDI Transpose/Follow Key/Channel Follow Key:）；调号名不翻译（C, C#/Db, ... 通用）
- 实际涉及文件：`Core/AppState.h`、`Core/AppStateBuilder.h`、`Core/ChannelMatrix.h`、`Midi/MidiChannelMapper.h/.cpp`、`MainComponent.cpp`、`Settings/SettingsModel.h`、`Settings/SettingsSerialization.cpp`、`Settings/SettingsStore.cpp`、`Settings/SettingsComponent.h`、`Settings/SettingsWindowManager.cpp`、`Locale/zh_CN.loc.h`

**验收通过**：
- (a) 调号 +2(D)、MIDI Transpose on、所有 Follow Key off → 按 C 键听到 D ✓
- (b) Ch1 Follow=on、Ch2 Follow=off → 通道 1 移调，通道 2 不变 ✓
- (c) 录制时写入事件为最终输出音高 ✓
- (d) 回放输出音高与录制一致 ✓
- (e) 重启持久化恢复 ✓
- 审查通过（4 agent，2 处修复：补翻译键 + 删冗余 handler）✓

---

## Phase 9：配置快照与体验增强

**目标**：用 Performance Preset 框架将 Phase 8 的所有个性化设置整合为可命名的完整快照，支持一键切换与录制集成；同时补充三个低成本、高感知的体验增强功能。

### 9a. Performance Preset（演奏配置快照）- [OK] 已完成

**实现摘要**：
- 数据模型：新建 `PerformancePreset`（含 `KeyboardLayout` + `ChannelMatrix` + keyboard settings），`makeDefaultPreset()` 内置出厂默认值
- 文件格式：`.devpiano.preset` JSON（version 1），稀疏数组保存 `customKeyLabels` / `customKeyColours`
- 旧 Layout 替换：删除 `LayoutPreset.*` / `LayoutFlowSupport.*` / `LayoutDirectoryScanner.*`；新建 `PerformancePreset.*` / `PresetFlowSupport.*`
- 数据模型清理：`SettingsModel` 中删除 `keyMap` / `lastLayoutId` / `InputMappingSettingsView`，新增 `lastActivePresetId`；删除 `SettingsSerialization::keyMapToValueTree` / `valueTreeToKeyMap` / `layoutToKeyMap` / `keyMapToLayout`
- UI：`ControlsPanel` 中 Layout 行替换为 Preset 行（下拉菜单 + Save As New / Rename / Delete 按钮）
- F1-F12：`MainComponent::keyPressed` 拦截 F 键映射到 preset 列表索引
- 录制集成：`RecordingEngine` 新增 `PerformanceEventType::presetChange`、`recordPresetChange()`、`drainPendingPresetChanges()`
- 回放自动切换：回放 loop 遇 `presetChange` 事件调用 `applyPresetByIndex()`
- `.devpiano` 文件扩展：`eventToVar` / `varToEvent` 支持 presetChange（向后兼容旧文件）
- 翻译：zh-CN 新增 preset 相关翻译，删除 layout 相关翻译
- 改动文件 13 个：`PerformancePreset.h/.cpp`、`PresetFlowSupport.h/.cpp`、`RecordingEngine.h/.cpp`、`PerformanceFile.h/.cpp`、`SettingsModel.h`、`SettingsStore.cpp`、`SettingsSerialization.h/.cpp`、`AppStateBuilder.h`、`MainComponent.h/.cpp`、`ControlsPanel.h/.cpp`、`zh_CN.loc.h`、`CMakeLists.txt`

**JSON 格式**（`.devpiano.preset`，version 1）：
```json
{
  "version": 1,
  "name": "My Preset",
  "layout": { "id": "...", "name": "...", "bindings": [...] },
  "channelMatrix": { "active": true, "channels": [...] },
  "keyboard": {
    "keySignature": 0, "midiTranspose": false,
    "colourMode": 0, "noteDisplay": 0,
    "fadeSpeed": 0.92, "previewAlpha": 0.0,
    "customKeyLabels": [...], "customKeyColours": [...]
  }
}
```

**验收标准**：
- (a) "Save As New Preset" 保存当前所有配置，切换到另一 Preset 再切回，状态完全恢复
- (b) F1-F12 快捷键一键切换 Preset
- (c) 录制演奏时切换 Preset，回放时在相应时间点自动切换
- (d) 导出/导入 `.devpiano` 文件保留 Preset 切换事件

---

### 9b. Unified Full-Range Keyboard（统一全音域钢琴键盘）- [OK] 已完成

**决策背景**：Phase 9b 原方案引入独立 `juce::MidiKeyboardComponent` 作为"88 键全音域键盘"，与 `CustomKeyboard` 上下堆叠形成双键盘布局。经实际验证发现：
- `MidiKeyboardComponent` 为 JUCE 黑盒组件，无法支持逐键标签/颜色/fade 动画等 Phase 8a 已建立的能力
- 双键盘共享同一 `MidiKeyboardState`，鼠标点击、note 高亮功能完全重叠
- FreePiano 参考实现采用"单键盘 + 绑定叠层"设计，devpiano 的双键盘是权宜实现而非有意设计

**修订方向**：放弃 `MidiKeyboardComponent`，将 `CustomKeyboard` 从"映射区窗口"升级为"全音域画布"，实现 FreePiano 风格的单键盘 + 绑定叠层设计。

**实现摘要**：
- CustomKeyboard：`rangeLow`/`rangeHigh` 0–127 全音域；`setAvailableRange(0, 127)` 在构造中调用
- 水平滚动：`KeyboardPanel` 用 `juce::Viewport` 包裹 `CustomKeyboard`（仅水平滚动条）；`recalculateKeyBounds` 移除缩放逻辑，固定 24px 白键宽 + 全内容尺寸 `setSize`；`resizing` 标志防止 `setSize → resized → recalc` 递归
- `setLowestVisibleNote`：改用 parent Viewport 宽度 clamp（修复原先用 content width 导致 upperBound=0 的 bug）；移除不再需要的 `recalculateKeyBounds` 调用
- 标签渲染 `paintKeyLabels`：doReMi/fixedDo 双行布局（音符名上 + 八度偏移下，按 `+`/`-` 拆分）；字号 clamp [8, 14]；每键独立 `setColour`；未绑定键用可见灰而非半透明白
- 移除 `juce::MidiKeyboardComponent`：`KeyboardPanel` 仅含 `CustomKeyboard` + `Viewport`，`resized` 简化为 `viewport->setBounds()`；`setShow88KeyPiano()` 整条链删除
- 清理 `show88KeyPiano`：`SettingsModel`、`SettingsComponent`(toggle)、`SettingsStore`(读写)、`SettingsWindowManager`(调用)、`MainComponent`(分支)、`zh_CN.loc.h`(翻译) 全部移除
- 键盘高度修复：`MainComponent::resized` 改用 `jmin(128, area.getHeight())`，消除越界重叠
- 滚动位置持久化：`SettingsModel.keyboardScrollOffsetX` (-1 = 未设置哨兵) + `SettingsStore` 读写 + `KeyboardPanel::setViewPosition`/`getViewPositionX` + `MainComponent` 析构保存、`syncUiFromSettings` 恢复；默认对齐 note 24
- SettingsComponent 其他修复：`resizableWindow`/`showInstrumentFilter` toggle 改为 `onStateChange` 回调（移除 `referTo` 绑定，与 `midiTransposeToggle` 风格统一）；`onSaveRequested` 简化同步调用；设置窗口高度 900→926
- `SettingsWindowManager`：设置窗口关闭后调用 `safe->resized()` 刷新布局
- 改动文件 11 个：`CustomKeyboard.h/.cpp`、`KeyboardPanel.h/.cpp`、`KeyboardTypes.h`、`SettingsModel.h`、`SettingsComponent.h`、`SettingsStore.cpp`、`SettingsWindowManager.cpp`、`MainComponent.cpp`、`current-iteration.md`

**验收标准**：
- [x] (a) CustomKeyboard 显示完整 0–127 音域，默认可见区定位 note 24（C1）
- [x] (b) 水平滚动（鼠标滚轮）可浏览全音域，白键宽度 24px
- [x] (c) 已绑定的键显示自定义标签/颜色，未绑定键显示标准音符名
- [x] (d) 电脑按键、鼠标点击、MIDI 回放均正确高亮对应键
- [x] (e) `KeyboardPanel` 仅含 `CustomKeyboard`，无 `MidiKeyboardComponent` 残留
- [x] (f) Settings 对话框中无 "Show 88-key Piano" 选项
- [x] (g) 默认视口定位到映射键区域（note 24），启动即见绑定区

---
### 9c. Smooth Pitch Bend（平滑弯音插值）— ✅ 已完成

**实现摘要**：
- 数据模型：`RecordingEngine` 新增 `std::array<float, 16> smoothedPitchBend`，per-channel EMA 状态，`startPlayback/stopPlayback` 中复位为 8192.0f（弯音中心）
- 核心逻辑：`renderPlaybackBlock()` 遍历回放事件时，对 `isPitchWheel()` 事件应用 EMA 平滑：
  ```
  smoothedPitchBend[ch] += 0.3f * (target - smoothedPitchBend[ch]);
  ```
  取 `std::round` 后通过 `juce::MidiMessage::pitchWheel()` 构造平滑消息写入 `MidiBuffer`；非 pitch bend 事件走原路径零开销
- 线程安全：`smoothedPitchBend` 仅被音频线程（`renderPlaybackBlock`）和消息线程（`startPlayback/stopPlayback`）访问，通过 `RecordingState` atomic 的 release/acquire 语义建立 happens-before
- 录制不受影响：平滑仅存在于回放路径，`recordEvent/recordMidiBufferBlock` 不改动
- 改动文件 2 个：`Recording/RecordingEngine.h`（+6 行）、`Recording/RecordingEngine.cpp`（+22 行）

**验收通过**：
- (a) 回放含快速弯音变化的 MIDI 文件，听感无拉链噪声 ✓
- (b) 录音的原始 pitch bend 数据完整保留（不受平滑影响）✓
- (c) 播放结束后 per-channel 状态复位，下次播放从中心开始 ✓
---

### 9d. Song Info / Metadata Editing（乐曲信息编辑）

**涉及文件**：新建 `UI/PerformanceMetadataDialog.h`、`UI/PerformanceMetadataDialog.cpp`；修改 `Recording/RecordingSessionController.cpp`

**UI 设计**：
- `juce::DialogWindow` modal 对话框（`launchOptions.dialogBackgroundColour` 匹配应用主题）
- 三个 `juce::TextEditor`：Title（单行）、Author（单行）、Comment（多行，4 行高）
- `juce::TextButton` "OK" / "Cancel"
- 对话框标题 "Song Information"（zh-CN: "乐曲信息"）

**触发时机**：
- 录制停止后 → `RecordingSessionController` 弹出对话框（可 Cancel 跳过，不强制）
- 打开 `.devpiano` 文件后 → 对话框以只读模式显示已有 metadata（或无数据显示空字段）
- 菜单 "File → Song Info..." 随时可编辑当前 loaded performance 的 metadata

**数据流**：
- `PerformanceFileMetadata` struct（`title`、`author`、`comment`）已在 `PerformanceFile.h` 定义
- `PerformanceFile` 的 JSON 序列化（v2）已支持 metadata 读写
- Dialog 只负责 UI 交互，数据持久化由现有 `PerformanceFile::save()` 完成

**验收标准**：
- (a) 录制停止 → 弹出对话框 → 填写保存 → `.devpiano` JSON 含 metadata 字段
- (b) 打开旧 `.devpiano` 文件（无 metadata）→ "Song Info" 显示空白字段
- (c) 编辑并保存后 JSON 文件 `metadata.title` / `metadata.author` / `metadata.comment` 更新
- (d) 取消编辑不修改文件

---

---

## Phase 10：主窗口 UI 现代化

**目标**：将主窗口四大面板（HeaderPanel / PluginPanel / ControlsPanel / KeyboardPanel）从开发调试风格的表单布局，升级为接近现代化商业钢琴 VST 宿主软件（Pianoteq / Kontakt / Gig Performer）的专业界面。

**前置依赖**：Phase 9（Performance Preset、全音域键盘、Smooth Pitch Bend、Song Info 构成稳定的功能基线，Phase 10 在其上进行纯 UI 层改造，无功能行为变更）。

**核心设计原则**：
- 不改动现有 MIDI / 音频 / 插件 / 录制逻辑，仅变更 Component 布局与绘制
- 所有视觉参数集中管理（颜色、圆角、间距、字体），便于后续统一调优
- 优先确保窗口高度 ≤700px（最小限制）时仍可用，再向上拉伸时充分利用额外空间

---

### 10a. 自定义 LookAndFeel 全局视觉主题

**现状**：项目未定制 `juce::LookAndFeel`，沿用 JUCE V4 默认扁平蓝灰风格——按钮为纯色矩形、滑块无质感、组合框样式老旧，缺乏音频软件特有的暗黑工业质感。

**方案**：
- 新建 `source/UI/DevPianoLookAndFeel.h/.cpp`，继承 `juce::LookAndFeel_V4`
- 全局色彩体系：
  - 背景层级：深炭黑 `0xff1a1c1e`（主窗口）、冷深灰 `0xff24262a`（面板）、微亮灰 `0xff2d3035`（控件底）
  - 功能高亮：冰蓝 `0xff00b4d8`（选中/悬停/滑块填充）、软橙 `0xffe07b3c`（录制 active）、薄荷绿 `0xff4ecdc4`（播放 active）
  - 文字层级：纯白 `0xffeeeeee`（主文本）、浅灰 `0xff999999`（辅助文本）、暗灰 `0xff555555`（禁用态）
- 覆写的绘制方法：
  - `drawButtonBackground`：圆角 4px、微渐变背景、hover 时半透明高亮叠加、按下时轻微内阴影
  - `drawComboBox` / `drawPopupMenuItem`：统一圆角、下拉箭头改用矢量三角
  - `drawLinearSlider`：滑槽细线 + 圆角矩形 thumb（6×18），填充段用功能高亮色
  - `drawRotarySlider`（为 10b 预备）：环形进度弧线 + 中心指示点
  - `drawLabel`：默认字体 `juce::FontOptions(14.0f)`，文本编辑器获得 1px 圆角边框
- 不覆写 `drawDocumentWindowTitleBar`（保留系统原生标题栏以适应各平台）

**验收标准**：
- (a) 所有 `TextButton` 具有 4px 圆角、悬浮变色、按下反馈
- (b) 所有 `ComboBox` 具有统一圆角下拉框与矢量箭头
- (c) `Slider` 滑槽与 thumb 符合新配色，填充段显示功能高亮色
- (d) 背景色层级分明：主窗口 → 面板区域 → 控件，三级灰度可辨识
- (e) 设置对话框中所有控件同步应用新主题（`SettingsComponent` 不独立维护样式）

---

### 10b. ControlsPanel 布局重构：水平滑动条 → 横向旋转旋钮 + ADSR 曲线

**现状**：6 个水平滑动条（Volume / Attack / Decay / Sustain / Release / Playback Speed）以 28px 行高 + 8px 间距纵向堆叠，共占用 216px 垂直高度，每行横跨 1008px——视觉上稀薄且不符合调音台设计习惯。

**方案**：
- 将 6 个 `juce::Slider::LinearHorizontal` 替换为 `juce::Slider::RotaryHorizontalVerticalDrag` 旋转旋钮
  - 旋钮直径：约 48px（含标签约 72px 总高），6 个旋钮横向一字排开
  - 每个旋钮下方居中显示参数名（Volume / Attack / Decay / Sustain / Release / Speed）
  - 旋钮正下方或旋钮中心显示参数数值（如 "0.85"）
- **ADSR 可视化曲线**：
  - 在 Attack / Decay / Sustain / Release 四个旋钮上方或右侧，用 `juce::Graphics` 绘制一个 200×40 的折线图
  - X 轴 = 时间（线性映射 Attack→Decay→Sustain→Release），Y 轴 = 幅度 [0, 1]
  - 调节任一 ADSR 旋钮时曲线实时更新；旋钮之间用细线连接形成包络轮廓
  - 曲线颜色使用冰蓝功能高亮色，填充区域使用半透明叠加
- **预设与录制行保持不变**（Preset ComboBox + Save/Rename/Delete 按钮行、录制 Transport 按钮行），但整体 ControlsPanel 高度预计从 296px 降至约 180px
- 腾出的 ~116px 垂直空间分配给键盘面板或留白

**验收标准**：
- (a) 6 个旋钮横向排列，span 不超过面板宽度，视觉紧凑
- (b) 旋钮支持鼠标垂直拖拽调节，参数实时生效
- (c) ADSR 曲线随旋钮值变化实时更新，形状符合 A-D-S-R 语义
- (d) Volume 和 Playback Speed 旋钮功能不受影响
- (e) ControlsPanel 总高度在默认窗口下 ≤180px

---

### 10c. PluginPanel 紧凑化与可折叠

**现状**：PluginPanel 固定高度 188px，包含 VST 路径输入行、插件选择行、始终可见的"已发现插件列表"标签 + 文本框。即使用户已完成插件加载，列表信息仍占位且不可收起，浪费纵向空间。

**方案**：
- 将 `PluginPanel` 拆分为两个逻辑区：
  - **工具栏行（始终可见）**：插件选择 ComboBox + Load / Unload / Open Editor 按钮 + 当前插件状态标签，单行高度 ~32px
  - **可折叠高级区（默认折叠）**：VST 路径输入 + Browse + Scan 按钮 + 插件列表编辑器
- 添加一个 `juce::TextButton "···"`（或齿轮图标）作为折叠/展开触发器，位于工具栏行右侧
- 折叠状态：PluginPanel 高度约 40px（仅工具栏行）
- 展开状态：PluginPanel 高度约 160px（含路径 + 列表 + 扫描按钮），与当前接近
- 折叠/展开状态通过 `AppState` 持久化（`SettingsModel` 新增 `bool pluginPanelExpanded`）
- 移除"已发现插件列表"标签（`pluginListLabel`），列表编辑器自带含义自明
- `instrumentFilterCombo` 移至工具栏行，紧邻插件选择器右侧

**验收标准**：
- (a) 默认启动时插件面板折叠为单行工具栏（约 40px）
- (b) 点击展开按钮后显示完整 VST 路径/扫描/列表区
- (c) 折叠/展开状态重启后保持
- (d) Load / Unload / Open Editor 在折叠态仍可操作
- (e) 已发现插件列表文本在展开态正常显示

---

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
- Phase 9 的 9c（Smooth Pitch Bend）和 9d（Song Info）与 Phase 10 无代码依赖，可在 Phase 10 期间穿插完成或继续搁置

## 执行顺序

```
Phase 10a (LookAndFeel) ── 必须先完成，全局视觉基础
         │
         ├──→ 10b (ControlsPanel → Knobs + ADSR Curve)
         ├──→ 10c (PluginPanel 折叠化)
         └──→ 10d (Keyboard 拟真渲染)
                   │
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
- 架构概览：[`../reference/architecture.md`](../reference/architecture.md)
- Phase 5 完成记录：[`../archive/phase5-architecture-convergence.md`](../archive/phase5-architecture-convergence.md)
- Phase 6-7 完成记录：[`../archive/phase6-7-completion-detail.md`](../archive/phase6-7-completion-detail.md)
- 架构优化完成记录：[`../archive/architecture-optimization-backlog.md`](../archive/architecture-optimization-backlog.md)
