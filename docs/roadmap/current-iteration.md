# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

Phase 8：逐键个性化与调号系统（详见 [`roadmap.md`](roadmap.md) §Phase 8）。
Phase 9 计划同列于此，以便一次看清完整执行路径。

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

**前置依赖**：Phase 8（逐键标签/颜色和调号系统共同构成快照内容）。

### 9a. Performance Preset（演奏配置快照 / Setting Groups）

**涉及文件**：`Layout/LayoutPreset.h`、`Layout/LayoutPreset.cpp`（扩展或新建 `PerformancePreset` 类型）、`Layout/LayoutDirectoryScanner.h`、`Layout/LayoutFlowSupport.h`、`Layout/LayoutFlowSupport.cpp`、`UI/ControlsPanel.cpp`、`Recording/RecordingEngine.h`、`Recording/RecordingEngine.cpp`、`Recording/RecordingSessionController.cpp`、`Core/AppState.h`

**快照边界**——一份 Performance Preset 包含：
- `KeyboardLayout`（键位绑定：key→MIDI note mapping）
- `ChannelMatrix`（16 通道完整配置：outputChannel、transpose、octaveShift、velocity、program、bankMSB、sustainCC、followKey）
- `KeyboardSettings` 子集（`keySignature`、`midiTranspose`、`colourMode`、`noteDisplay`、`fadeSpeed`、`previewAlpha`、`customKeyLabels`、`customKeyColours`）
- **明确排除**：音频设备选择、插件路径、语言设置、窗口尺寸（这些是全局设置，不随 Preset 切换）

**JSON 格式**（扩展现有 Layout Preset，version 2）：
```json
{
  "version": 2,
  "name": "My Preset",
  "layout": { /* 现有 keymap 结构 */ },
  "channelMatrix": {
    "active": true,
    "channels": [ /* 16 × PerChannelConfig */ ]
  },
  "keyboard": {
    "keySignature": 0,
    "midiTranspose": false,
    "colourMode": 0,
    "noteDisplay": 0,
    "fadeSpeed": 0.92,
    "previewAlpha": 0.0,
    "customKeyLabels": [ /* 128 × string */ ],
    "customKeyColours": [ /* 128 × ARGB hex */ ]
  }
}
```

**UI 交互**：
- `ControlsPanel` 中 Preset 下拉列表支持重命名（双击行内编辑）、新建（save current as new）、删除
- 菜单栏 "Preset" 子菜单列出前 12 个 Preset，F1-F12 快捷键绑定 Preset 1-12
- 切换时 `AppState` 全量更新 + `CustomKeyboard` / `ChannelMatrix` / `ControlsPanel` repaint

**录制集成**：
- `RecordingEngine` 新增 `PerformanceEvent::Type::presetChange`，携带 preset ID（0..255）
- 录制时 `RecordingSessionController` 监听 preset 切换 → 写入事件
- 回放时 playback loop 遇到 `presetChange` → 调用 `applyPreset(id)`
- `.devpiano` JSON 格式扩展：events 数组增加 `{ "type": "presetChange", "presetId": N }` 事件

**迁移**：
- 现有 Layout Preset（version 1）首次加载时自动包装为 Performance Preset（channelMatrix 默认透传、keyboard 默认值）
- 另存为 version 2，保留原 version 1 文件不覆盖

**验收标准**：
- (a) "Save As New Preset" 保存当前所有配置，切换到另一 Preset 再切回，状态完全恢复
- (b) F1-F12 快捷键一键切换 Preset
- (c) 录制演奏时切换 Preset，回放时在相应时间点自动切换
- (d) 导出/导入 `.devpiano` 文件保留 Preset 切换事件

---

### 9b. 88-Key Full Piano Keyboard（88 键完整钢琴键盘）

**涉及文件**：`UI/KeyboardPanel.h`、`UI/KeyboardPanel.cpp`、`Settings/SettingsModel.h`、`Settings/SettingsComponent.cpp`

**实现方式**：
- `KeyboardPanel` 内部增加 `juce::MidiKeyboardComponent` 成员
- `setAvailableRange(0, 127)`、`setLowestVisibleKey(0)`、`setKeyWidth(16.0f)`
- 从 `AudioEngine` 的 `juce::MidiKeyboardState` 获取 note 状态并同步
- 与 `CustomKeyboard` 上下排列：`CustomKeyboard` 在上（电脑键盘映射区），`MidiKeyboardComponent` 在下（全音域）

**UI 控制**：
- `SettingsComponent`（GUI 页）增加 `juce::ToggleButton` "Show 88-key Piano"（默认 off，避免对仅需电脑键盘的用户增加视觉负担）
- `KeyboardPanel` 根据设置动态 `setVisible()` 控制 `MidiKeyboardComponent`

**验收标准**：
- (a) 开启后窗口底部显示 88 键钢琴键盘，水平可滚动
- (b) 电脑按键触发 note 时，对应钢琴键高亮（力度着色）
- (c) MIDI 回放时钢琴键同步高亮
- (d) 鼠标点击钢琴键发出对应 MIDI 音符（JUCE 内置行为）

---

### 9c. Smooth Pitch Bend（平滑弯音插值）

**涉及文件**：`Recording/RecordingEngine.h`、`Recording/RecordingEngine.cpp`

**实现方式**：
- `RecordingEngine` 增加 `std::array<float, 16> smoothedPitchBend` per-channel 状态，初始值 8192.0f（中心位置）
- 回放路径 `getNextAudioBlock`：遇到 pitch bend 事件时，不直接写入 target，而是
  ```
  smoothedPitchBend[ch] += 0.3f * (targetValue - smoothedPitchBend[ch]);
  ```
  （EMA 指数移动平均，alpha=0.3，约 10ms 时间常数）
- 将平滑后的 14-bit 值（round 取整）写入 `MidiBuffer`
- 录音时不应用平滑（保留原始事件精度）
- 播放停止后 per-channel 状态复位到 8192.0f

**验收标准**：
- (a) 回放含快速弯音变化的 MIDI 文件，听感无"拉链噪声"
- (b) 录音的原始 pitch bend 数据完整保留（不受平滑影响）
- (c) 播放结束后 per-channel 状态复位，下次播放从中心开始

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

## 本轮决策

- Phase 8a/8b 为 Phase 9a（Performance Preset）的必要前置——逐键标签/颜色和调号系统共同构成快照内容，不可跳过
- 88-key Piano（9b）、Smooth Pitch Bend（9c）、Song Info（9d）三者与 Phase 8 无强依赖，可与 Phase 8 并行开发，但建议集中在 Phase 9 统一收尾以降低上下文切换开销
- Phase 7-5（Metadata 编辑对话框）的搁置决策被 9d 替代——9d 的范围更窄（仅编辑 title/author/comment），UI 更简洁
- Phase 7-7（全屏模式）确认永不实现

## 执行顺序

```
Phase 8a (Labels + Colors) ✅ ──┐
                                ├──→ Phase 9a (Performance Preset)
Phase 8b (Key Sig System)    ✅ ──┘

(无依赖，可与 Phase 8 并行或 Phase 9 集中执行)
Phase 9b (88-Key Keyboard)
Phase 9c (Smooth Pitch Bend)
Phase 9d (Song Info / Metadata)
```

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
