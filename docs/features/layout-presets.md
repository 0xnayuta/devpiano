# 布局 Preset 功能说明（草案）

> 用途：记录旧 FreePiano 布局管理行为参考与现代 Preset 重建设计考虑。  
> 当前状态：**草案阶段，不实现，仅建模**。  
> 读者：需要理解布局管理历史行为、规划未来 Preset 实现方向的开发者。  
> 更新时机：设计草案有实质进展时。

相关文档：

- 旧代码迁移边界：[`../architecture/legacy-migration.md`](../architecture/legacy-migration.md)
- 键盘映射功能说明：[`keyboard-mapping.md`](keyboard-mapping.md)
- 路线图：[`../roadmap/roadmap.md`](../roadmap/roadmap.md)

---

## 1. 旧 FreePiano 布局管理行为参考

对应源码：`freepiano-src/config.*`、`freepiano-src/song.*`。

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

对应源码：`source/Core/KeyMapTypes.h`、`source/Settings/SettingsModel.h`、`source/UI/ControlsPanel.cpp`。

### 2.1 当前 KeyboardLayout 模型

```cpp
struct KeyboardLayout {
    juce::String id;          // 如 "default.freepiano.minimal"
    juce::String name;        // 如 "Default FreePiano Minimal"
    std::vector<KeyBinding> bindings;
};

struct KeyBinding {
    int keyCode;              // JUCE keyCode（标准化，与平台无关）
    juce::String displayText;
    KeyAction action;         // { type=note, trigger=keyDown, midiNote, midiChannel, velocity }
};
```

**已有能力**：
- 两个内置布局：`makeDefaultKeyboardLayout()`（36 键）、`makeFullPianoLayout()`（36 键但更高音区）
- 通过 `KeyboardMidiMapper` 使用 JUCE `KeyPress` 的稳定 keyCode
- 通过 `ControlsPanel` 的 ComboBox 切换布局
- 通过 `Save Layout` / `Reset Layout` 按钮保存和恢复当前布局

**当前限制**：
- 只支持两个内置布局，用户不能新增自定义布局
- 无 preset 导入/导出能力
- 无 per-key label 和 color UI

### 2.2 当前布局持久化模型

`SettingsModel::InputMappingSettingsView`：

```cpp
struct InputMappingSettingsView {
    juce::String layoutId;
    std::unordered_map<int, int> keyMap;  // keyCode → midiNote
};
```

`keyMap` 是从 `KeyboardLayout.bindings` 派生的运行时查找表。

---

## 3. 现代重建设计考虑（仅作建模记录，不实现）

以下为未来实现时的设计参考，**不是当前任务范围**。

### 3.1 布局 preset 文件格式

建议使用 JSON 格式，与平台无关：

```json
{
  "version": 1,
  "id": "my-custom-layout",
  "name": "My Custom Layout",
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
}
```

使用标准化 JUCE keyCode（跨平台），不嵌入 DIK 扫描码。

### 3.2 preset 与演奏设置的分离

建议 layout preset **只包含键位映射**，不包含 velocity、transpose 等演奏参数：

| 内容 | 属于 |
|---|---|
| keyCode → midiNote | Layout Preset |
| velocity、transpose、octave shift | Performance Settings（已有） |
| per-key label、color | 未来可选扩展，不在 MVP |

### 3.3 preset 注册与发现机制

用户目录下的 `.freepiano.layout` 文件自动被发现并列入 ControlsPanel 的 layout 列表。使用原生文件对话框时可选任意位置，但已保存的 preset 均归集到用户配置目录，不使用集中式注册表。

`ControlsPanel` 的 layout 列表从两个来源聚合：
1. 内置 preset（`default.freepiano.minimal`、`default.freepiano.full`）
2. 用户配置目录下的 `.freepiano.layout` 文件

### 3.4 preset 操作能力（MVP 范围）

MVP 建议只做：

- **加载**：通过原生文件对话框选择任意位置的 `.freepiano.layout` 文件并应用
- **保存当前**：将当前 `KeyboardLayout` 导出为 JSON 文件（默认到用户配置目录）
- **删除**：删除用户 preset 文件（内置不可删）

不在 MVP 范围：
- 图形化布局编辑器
- per-key label / color UI
- 多设置组

### 3.5 与录音/回放的关系

layout preset 与录制独立：
- 录制只录 MIDI 事件，不录 layout
- 回放时用户自行加载期望的 layout 和插件
- 未来可选"录制时保存 layout"作为 meta event，但不在 MVP

---

## 4. 当前迭代立场

M7-1（JSON preset 文件格式）和 M7-2（Preset 读写工具）已完成实现。

M7-3（用户目录 Preset 自动发现）正在推进：`LayoutDirectoryScanner` 已建立，`scanUserLayoutDirectory()` 在 `syncUiFromSettings()` 时被调用，内置 preset + 用户 preset 聚合逻辑已生效。

M7-4（Preset 加载）和 M7-5（Preset 保存）待实现。

---

## 5. 已实现细节（实现时间：M7-2/M7-3）

### 5.1 文件格式

JSON schema（与草案一致，无变更）：

```json
{
  "version": 1,
  "id": "my-custom-layout",
  "name": "My Custom Layout",
  "bindings": [
    {
      "keyCode": 65,
      "displayText": "A",
      "action": { "type": "note", "trigger": "keyDown", "midiNote": 60, "midiChannel": 1, "velocity": 1.0 }
    }
  ]
}
```

文件扩展名：`.freepiano.layout`。

### 5.2 目录结构

用户 layout 目录：`~/Library/Application Support/DevPiano/Layouts/`（macOS），`%APPDATA%\DevPiano\Layouts\`（Windows）。

### 5.3 发现机制

`MainComponent::syncUiFromSettings()` 调用 `devpiano::layout::scanUserLayoutDirectory()`，扫描用户配置目录下所有 `.freepiano.layout` 文件，加载为 `KeyboardLayout` 后将 ID 追加到 layout 列表。

### 5.4 实现文件

- `source/Layout/LayoutPreset.h/.cpp`：`loadLayoutPreset()` / `saveLayoutPreset()`
- `source/Layout/LayoutDirectoryScanner.h/.cpp`：`getUserLayoutDirectory()` / `scanUserLayoutDirectory()`
- `source/MainComponent.cpp`：`syncUiFromSettings()` 中聚合内置 + 用户 preset

---

## 6. 已确认的设计决策

以下问题已在本预研阶段明确，后续实现须遵循：

1. **preset 存储位置**：用户新建/保存的 preset 默认存储在用户配置目录下。导入/导出时可通过原生文件对话框选择任意位置。
2. **preset ID 冲突**：内置 preset ID 与用户 preset ID 冲突时，提醒用户重新命名。
3. **内置 preset 数量**：维持现状（`minimal` 和 `full` 两个），暂不引入更多内置 preset。
4. **preset 导入/导出 UI**：优先使用原生文件对话框（更简单），如实现困难再在 ControlsPanel 内提供导入按钮。
5. **per-key label / color**：不在 MVP 范围，暂不纳入考虑（当前 `KeyBinding.displayText` 存在但未使用）。