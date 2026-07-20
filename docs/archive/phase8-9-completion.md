# Phase 8–9 Completion Record

> 归档日期：2026-07-20
> 来源：[`../roadmap/current-iteration.md`](../roadmap/current-iteration.md) Phase 8–9 部分
> 后续方向：见 [`../roadmap/current-iteration.md`](../roadmap/current-iteration.md) Phase 10

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

### 9a. Performance Preset（演奏配置快照）— ✅ 已完成

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
- 改动文件 13 个

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

### 9b. Unified Full-Range Keyboard（统一全音域钢琴键盘）— ✅ 已完成

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
- 改动文件 11 个

**验收标准**：
- (a) CustomKeyboard 显示完整 0–127 音域，默认可见区定位 note 24（C1）
- (b) 水平滚动（鼠标滚轮）可浏览全音域，白键宽度 24px
- (c) 已绑定的键显示自定义标签/颜色，未绑定键显示标准音符名
- (d) 电脑按键、鼠标点击、MIDI 回放均正确高亮对应键
- (e) `KeyboardPanel` 仅含 `CustomKeyboard`，无 `MidiKeyboardComponent` 残留
- (f) Settings 对话框中无 "Show 88-key Piano" 选项
- (g) 默认视口定位到映射键区域（note 24），启动即见绑定区

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

### 9d. Song Info / Metadata Editing（乐曲信息编辑）— ✅ 已完成

**实现摘要**：
- 数据模型：沿用已有的 `PerformanceFileMetadata`（`createdAt` / `title` / `notes`），新增 `loadPerformanceFileMetadata` 反序列化 API（`PerformanceFile.h/.cpp`）
- 状态存储：`RecordingSession` 新增 `currentMetadata` + `currentPerformanceFile` 字段，在录制/打开/编辑路径中维护
- 对话框：新建 `PerformanceMetadataDialog`（`UI/PerformanceMetadataDialog.h/.cpp`），`DialogWindow` modal 弹窗，内含 `title`（单行）+ `notes`（多行 4 行高）编辑器，OK/Cancel 按钮；析构函数守卫确保标题栏关闭 / Escape 也能触发回调
- 触发时机：
  - 录制停止 → 自动弹出 metadata 对话框（可 Cancel 跳过）
  - 打开 .devpiano → 静默加载 metadata 存入 session，不弹窗
  - "Song Info" 按钮（ControlsPanel 最右侧）→ 编辑当前 metadata，若有 backing file 则回写 JSON
- 按钮：ControlsPanel 录制控制行新增 `songInfoButton { TRANS("Song Info") }`（80px），通过 `onSongInfoRequested` 回调连接至 `RecordingSessionController::handleSongInfoClicked()`
- 本地化：zh_CN.loc.h 新增 "Song Info" / "Song Information" / "Song Title" / "Notes" 四条翻译
- 改动文件 11 个（含 2 个新文件），净增约 220 行

**验收通过**：
- (a) 录制停止 → 弹窗 → 填写保存 → `.devpiano` JSON 含 `metadata.title` / `metadata.notes` ✓
- (b) 打开旧 `.devpiano` 文件（无 metadata）→ "Song Info" 显示空白字段 ✓
- (c) 编辑并保存后 JSON 文件 metadata 更新 ✓
- (d) 取消编辑不修改文件 ✓
