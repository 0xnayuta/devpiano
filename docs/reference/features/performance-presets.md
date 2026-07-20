# Performance Preset 功能说明

> 用途：说明当前已实现的 Performance Preset 行为、数据格式与已知边界。包含功能说明与手工测试。
> 当前状态：已接入主链路，支持发现、新建、导入、重命名、删除、F1-F12 快捷键与录制集成。
> 更新时机：preset 行为、文件格式或 UI 交互有实质变化时。

相关文档：

- 键盘映射功能说明：[`./keyboard-mapping.md`](./keyboard-mapping.md)
- 阶段验收：[`../acceptance.md`](../acceptance.md)
- 录制回放：[`./recording-playback.md`](./recording-playback.md)
- 路线图：[`../../roadmap/roadmap.md`](../../roadmap/roadmap.md)

---

## 1. 历史参考：布局管理

Performance Preset 系统的设计参考了旧 FreePiano 的配置模型（`config.*` / `song.*`）。

### 1.1 布局在旧系统中的定位

旧系统没有独立的"布局 preset 文件"概念。布局数据有以下几种存在形式：

| 来源 | 内容 | 用途 |
|---|---|---|
| `song_open_lyt(filename)` | `.lyt` 二进制文件，含 keymap 参数 + 键盘映射 | 加载预设布局 |
| 录音文件（`song.*`） | 内嵌完整 keymap、设置组、标签、颜色快照 | 回放时恢复演奏状态 |
| config 运行时 | `keydown_map` / `keyup_map`，256 键 → 多个 `key_bind_t` | 运行时按键查找 |
| `setting_group[]` | 多组设置（每组含 octave shift、transpose、velocity 等） | 演奏中快速切换配置组 |

**关键理解**：旧系统的"布局"不是独立的 preset 文件，而是与 config 设置、key binding 系统深度耦合的整体状态。

### 1.2 旧 key_bind_t 事件模型

每个按键绑定是一个四字节消息：

```cpp
struct key_bind_t {
    byte a;  // 消息类型（如 SM_NOTE_ON、SM_MIDI_CONTROLLER）
    byte b;  // 数据1（如 note number、controller id）
    byte c;  // 数据2（如 velocity、value）
    byte d;  // 数据3
};
```

与 `song_event_t` 的字节编码完全一致——录音中嵌入的 key binding 事件使用同一格式。

### 1.3 旧 layout 文件格式（.lyt）

` song_open_lyt()` 读取二进制 `.lyt` 文件（magic: `"iDreamPianoSong"`），结构大致为：

```
header:        "iDreamPianoSong"
version:       uint32
title:         char[40]
author:        char[20]
comment:       char[256]
keymap: {
    midi_shift, velocity_right, velocity_left,
    shift_right, shift_left, voice1, voice2
}
keymap_entries: // 256 entries, 每个是 (DIK_code → key_bind_t)
```

**重大问题**：使用 DirectInput 扫描码（`DIK_*`），与平台绑定，无法跨平台使用。

### 1.4 旧系统多设置组

旧系统支持多组 `setting_group`，每组独立存储 octave shift、transpose、velocity、output_channel、follow_key、key_signature，并通过 GUI menu 提供 Add / Insert / Delete / Copy / Paste / Clear / Default 操作。

### 1.5 旧设计的已知问题

- **平台绑定**：使用 DIK 扫描码，不跨平台
- **preset 与 config 耦合**：布局与 velocity、transpose 等演奏参数混在一起，用户无法只切换键盘映射而保留自己的 velocity 偏好
- **无独立 preset 文件**：无法导出/导入单个布局
- **多设置组无标准 preset 格式**：只能通过 song 文件或手动操作传递

---

## 2. 当前实现状态

### 2.1 PerformancePreset 数据模型

```cpp
struct PerformancePreset {
    juce::String name;
    KeyboardLayout layout;         // key bindings
    ChannelMatrix channelMatrix;   // 16-channel configs
    int keySignature = 0;          // semitone offset (-7..+7)
    bool midiTranspose = false;
    KeyColourMode colourMode;      // classic / channel / velocity
    NoteDisplayMode noteDisplay;   // doReMi / fixedDo / noteName
    float fadeSpeed = 0.92f;
    float previewAlpha = 0.0f;
    std::array<juce::String, 128> customKeyLabels;
    std::array<juce::Colour, 128> customKeyColours;
};
```

对应源码：`source/Layout/PerformancePreset.h`、`source/Layout/PresetFlowSupport.h`。

### 2.2 已实现能力

- **一键切换**：通过 `ControlsPanel` 的 ComboBox 切换 preset，或按 F1-F12 快捷键
- **新建 Preset**："Save As New" 将当前完整状态（键位 + channel matrix + keyboard settings）保存为 `.devpiano.preset` JSON 文件
- **导入**：从任意位置导入 `.devpiano.preset` 文件到用户 Preset 目录并立即应用
- **重命名**：修改 preset 文件名（同时更新显示名称）
- **删除**：删除用户 preset（内置 `[Default]` 不可删除）
- **启动恢复**：根据持久化的 `lastActivePresetId` 恢复上次使用的 preset
- **录制集成**：录制时切换 preset 会写入 `presetChange` 事件，回放时自动在对应时间点切换

---

## 3. 文件格式

### 3.1 `.devpiano.preset` JSON 格式

文件扩展名：`.devpiano.preset`，格式版本当前固定为 `1`。

```json
{
  "version": 1,
  "name": "My Preset",
  "layout": {
    "id": "user.preset.my-preset",
    "name": "My Preset",
    "bindings": [
      {
        "keyCode": 65,
        "displayText": "A",
        "action": {
          "type": "note",
          "trigger": "keyDown",
          "midiNote": 60,
          "midiChannel": 1,
          "velocity": 1.0
        }
      }
    ]
  },
  "channelMatrix": {
    "active": true,
    "channels": [
      { "outputChannel": 0, "transpose": 0, "octaveShift": 0,
        "velocity": 64, "program": 0, "bankMSB": 0,
        "sustainCC": 64, "followKey": false }
    ]
  },
  "keyboard": {
    "keySignature": 0,
    "midiTranspose": false,
    "colourMode": 0,
    "noteDisplay": 0,
    "fadeSpeed": 0.92,
    "previewAlpha": 0.0,
    "customKeyLabels": [],
    "customKeyColours": []
  }
}
```

### 3.2 Preset 快照边界

一份 Performance Preset 包含：

| 内容 | 来源 |
|---|---|
| 键盘绑定（keyCode → action） | `KeyboardLayout.bindings` |
| 16 通道配置（outputChannel, transpose, octaveShift, velocity, program, bankMSB, sustainCC, followKey） | `ChannelMatrix.channels` |
| 调号（keySignature）、MIDI 移调（midiTranspose） | `AppState` + `SettingsModel` |
| 键盘渲染（colourMode, noteDisplay, fadeSpeed, previewAlpha） | `KeyboardSettings` |
| 逐键标签（customKeyLabels[128]） | `KeyboardSettings` |
| 逐键颜色（customKeyColours[128]） | `KeyboardSettings` |

**明确排除**（这些是全局设置，不随 Preset 切换）：音频设备选择、插件路径、语言设置、窗口尺寸。

### 3.3 Preset 注册与发现

Preset 存储在用户配置目录 `DevPiano/Presets/` 下，扩展名 `.devpiano.preset` 的文件会被自动发现。
`ControlsPanel` 的 preset 列表从两个来源聚合：
1. 内置 `[Default]` preset（`makeDefaultPreset()` 返回的出厂默认值，通过 `setTextWhenNothingSelected` 显示为占位文本，不可被选中/删除/重命名）
2. 用户 Preset 目录下所有 `.devpiano.preset` 文件

---

## 4. 当前行为细节

### 4.1 内置 Default Preset

- 固定一个内置 preset：`[Default]`，通过 `setTextWhenNothingSelected("Default")` 显示为 ComboBox 占位文本
- 不可被选中、删除或重命名（Rename/Delete 按钮禁用）
- 在以下场景自动应用：(a) 启动时无已持久化的用户 preset，(b) 删除当前活动 preset 后回退

### 4.2 Preset ID 与显示名称

- 用户 preset 的稳定 ID 由文件名派生：`user.preset.<sanitised-file-name>`
- 显示名称直接来自文件名（去掉 `.devpiano.preset` 后缀），或从 JSON 的 `name` 字段读取
- **Rename** 同时修改文件名和文件内 `name` 字段
- **Save As New** 生成新文件后立即切换为新的用户 preset

### 4.3 保存行为

`Save As New` 弹出一个简短的文本输入对话框，用户输入名称后：
- 名称经 `sanitisePresetFileName()` 处理（只保留字母数字、空格、连字符、下划线）
- 若同名文件已存在，弹出覆盖确认对话框
- 保存到 `DevPiano/Presets/<name>.devpiano.preset`
- 立即将该 preset 设为当前活动 preset
- 触发 `commitPreset()` → 更新 UI 和持久化 `lastActivePresetId`

### 4.4 导入行为

- 通过拖放 `.devpiano.preset` 文件到主窗口导入（无独立 Import 按钮）
- 保留原文件名复制到 Preset 目录
- 若存在同名文件则覆盖
- 导入后立即应用，刷新 dropdown，持久化

### 4.5 重命名与删除行为

- 仅对用户 preset 生效；`[Default]` 不可操作
- **Rename**：弹出文本对话框，重命名文件 + 更新 `name` 字段；若重命名为当前活动 preset，更新 ID 跟踪
- **Delete**：删除文件后发送 `juce::File::deleteFile()`；若删除的是当前活动 preset，自动回退到 `[Default]`

### 4.6 F1-F12 快捷键

- `F1` 对应 preset 列表中第 1 个用户 preset（跳过 `[Default]`）
- 跳过 `[Default]` 后的 12 个 slot 分别映射 F1-F12
- 若某个 slot 不存在 preset，按键无效果
- `MainComponent::keyPressed` 拦截 F 键 → `presetFlowSupport->applyPresetByIndex(index)`

### 4.7 启动恢复行为

- 设置持久化保存 `lastActivePresetId`
- 启动时 `initialiseFromPreset()` 调用 `applyPresetById(lastActivePresetId)`
- 若 preset ID 未找到，回退到 `[Default]`

---

## 5. 录制集成

### 5.1 presetChange 事件

`RecordingEngine` 支持 `PerformanceEventType::presetChange`：

```cpp
void recordPresetChange(uint8_t presetId, std::int64_t timestampSamples);
```

- 携带 `presetId`（0..255）和时间戳；ID 由 `PresetFlowSupport` 分配和维护
- 录制时 `PresetFlowSupport::applyPresetData()` 调用 `recordingEngine.recordPresetChange(idx, pos)`
- `drainPendingPresetChanges()` 在 message thread 上清空音频侧入队的 preset 切换通知

### 5.2 回放自动切换

回放 loop 遇到 `PerformanceEventType::presetChange` 事件时：
- 调用 `applyPresetByIndex(event.presetId)`
- 由此实现"录制时在某个时间点切换 preset → 回放时自动切换"

### 5.3 `.devpiano` 文件扩展

`PerformanceFile.h` 中 `eventToVar` / `varToEvent` 已扩展支持：

```json
{
  "timestampSamples": 123456,
  "type": "presetChange",
  "presetId": 2
}
```

旧版 `.devpiano` 文件（不含 `presetChange` 事件）仍然兼容。

---

## 6. 关键实现文件

- `source/Layout/PerformancePreset.h/.cpp`：数据模型、JSON 序列化、目录扫描、`makeDefaultPreset()`
- `source/Layout/PresetFlowSupport.h/.cpp`：Preset CRUD 编排、`commitPreset()`、录制通知
- `source/UI/ControlsPanel.h/.cpp`：preset 下拉框与 Save As New / Rename / Delete 按钮
- `source/Recording/RecordingEngine.h/.cpp`：`PerformanceEventType::presetChange`、`recordPresetChange()`、`drainPendingPresetChanges()`
- `source/Recording/PerformanceFile.h/.cpp`：`.devpiano` 文件序列化（含 `presetChange` 事件）
- `source/Settings/SettingsModel.h`：`lastActivePresetId` 持久化字段
- `source/MainComponent.h/.cpp`：F1-F12 快捷键、timer drain、preset 流接线

---

## 7. 已知边界与后续方向

- 用户 preset 的 ID 与文件名绑定，因此"重命名"会改变 preset 的稳定标识符
- `.devpiano.preset` 文件内的 `customKeyLabels` / `customKeyColours` 以完整 128 项数组存储，保证后续编辑一致性
- 若在已有 preset 的 slot 上 Save As New，新 preset 会创建一个新 ID；旧 preset 文件仍然存在，需手动清理
- 目前不支持图形化布局编辑器、per-preset 插件绑定或 preset 排序调整

---

# devpiano Performance Preset 手工测试清单
> 用途：对 Performance Preset 的保存、导入、重命名、删除、快捷键、启动恢复与录制集成进行手工验证。
> 更新时机：preset 文件格式、交互行为、发现机制或录制集成变化时。

状态标记：
- [x] 已通过
- [ ] 未通过 / 未开始验证
- [~] 部分通过 / 待补充验证

---

## 1. 基础 Preset 操作

### 1.1 Save As New Preset
- [ ] UI：进入 ControlsPanel，点击 "Save As New" 按钮，弹出文本输入对话框
- [ ] 输入有效名称（如 "My Test Preset"），确认
- [ ] 验证：下拉菜单中出现 "My Test Preset"，且当前选中该项
- [ ] 验证：`DevPiano/Presets/My_Test_Preset.devpiano.preset` 文件已创建
- [ ] 验证：修改键盘设置（如调号、逐键颜色），切换到 `[Default]`，再切回 "My Test Preset"，所有设置恢复

### 1.2 Rename Preset
- [ ] UI：选中 "My Test Preset"，点击 "Rename" 按钮
- [ ] 输入新名称 "Renamed Preset"，确认
- [ ] 验证：下拉菜单显示 "Renamed Preset"，原 "My Test Preset" 消失
- [ ] 验证：文件重命名为 `Renamed_Preset.devpiano.preset`

### 1.3 Delete Preset
- [ ] UI：选中 "Renamed Preset"，点击 "Delete" 按钮
- [ ] 验证：下拉菜单恢复为 `[Default]`，原 preset 消失
- [ ] 验证：文件已从 `DevPiano/Presets/` 删除

### 1.4 Import Preset（拖放）
- [ ] 通过外部手段准备一个 `.devpiano.preset` 文件
- [ ] 将文件拖放到主窗口（无独立 Import 按钮，仅支持拖放导入）
- [ ] 验证：主窗口出现蓝色边框反馈
- [ ] 验证：松开后 preset 被导入并应用，下拉菜单中出现并自动选中

### 1.5 内置 Default 保护
- [ ] 选中 `[Default]` 时，Rename 和 Delete 按钮应保持禁用（灰色/不可点击）
- [ ] F1-F12 不会意外触发 Default preset 操作

---

## 2. F1-F12 快捷键

### 2.1 基础切换
- [ ] 准备至少 3 个用户 preset（如 P1, P2, P3）
- [ ] 按 F1 → 验证切换到 P1，下拉菜单也同步
- [ ] 按 F2 → 验证切换到 P2
- [ ] 按 F3 → 验证切换到 P3

### 2.2 空 slot 行为
- [ ] 只有 3 个 preset 时，按 F4 → 无效果，无 crash
- [ ] 删除 P3 后，按 F3 → 无效果（不 fallback 到不存在 slot）

### 2.3 焦点场景
- [ ] 焦点在 ComboBox 或其他编辑控件时，F1-F12 仍然触发 preset 切换（不触发控件默认行为）

---

## 3. 启动恢复


- [ ] 选择一个用户 preset（非 Default），记录当前键盘/显示/通道设置
- [ ] 关闭程序
- [ ] 重新启动程序
- [ ] 验证：启动后自动恢复到之前选中的 preset，设置与关闭前一致
- [ ] 删除该 preset 文件 → 重启 → 验证：回退到 `[Default]`

---

## 4. 录制 / 回放集成

### 5.1 录制中切换 Preset
- [ ] 准备 2 个以上预设
- [ ] 点击 Record，演奏几个音
- [ ] 在录制过程中通过 F1/F2 切换 preset
- [ ] 点击 Stop
- [ ] 通过 "Save Performance" 保存为 `.devpiano` 文件
- [ ] 验证：`.devpiano` JSON 中包含 `{ "type": "presetChange", "presetId": N }` 事件

### 5.2 回放自动切换
- [ ] 加载刚保存的 `.devpiano` 文件
- [ ] 点击 Play
- [ ] 验证：在录制时切换 preset 的时间点，回放自动切换到相应 preset
- [ ] 验证：preset 切换上下文正确（键盘布局、通道矩阵等更新）

### 5.3 旧版 `.devpiano` 兼容性
- [ ] 加载不含 `presetChange` 事件的旧 `.devpiano` 文件
- [ ] 验证：正常回放，不 crash，不出现意外 preset 切换
