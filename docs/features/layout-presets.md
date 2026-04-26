# 布局 Preset 功能说明

> 用途：说明当前已实现的布局 preset 行为、数据格式与已知边界。  
> 当前状态：**已接入主链路，支持发现、保存、导入、重命名、删除与恢复。**  
> 读者：需要理解当前布局 preset 功能行为与维护边界的开发者。  
> 更新时机：preset 行为、文件格式或 UI 交互有实质变化时。

相关文档：

- 旧代码迁移边界：[`../architecture/legacy-migration.md`](../architecture/legacy-migration.md)
- 键盘映射功能说明：[`keyboard-mapping.md`](keyboard-mapping.md)
- 专项手工测试：[`../testing/layout-presets.md`](../testing/layout-presets.md)
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
    juce::String name;        // 如 "FreePiano Minimal"
    std::vector<KeyBinding> bindings;
};

struct KeyBinding {
    int keyCode;              // JUCE keyCode（标准化，与平台无关）
    juce::String displayText;
    KeyAction action;         // { type=note, trigger=keyDown, midiNote, midiChannel, velocity }
};
```

**已有能力**：
- 两个内置布局：`makeDefaultKeyboardLayout()`（36 键，显示名 `FreePiano Minimal`）、`makeFullPianoLayout()`（36 键但更高音区，显示名 `FreePiano Full`）
- 通过 `KeyboardMidiMapper` 使用 JUCE `KeyPress` 的稳定 keyCode
- 通过 `ControlsPanel` 的 ComboBox 切换布局
- 支持将当前布局保存为用户 preset（`.freepiano.layout`）
- 支持从任意位置导入 `.freepiano.layout` 文件到用户布局目录并立即应用
- 支持对用户 preset 执行重命名（仅修改显示名称）与删除
- 启动时可根据持久化的 `layoutId` 恢复内置 preset 或用户 preset

**当前限制**：
- 仍只有两个内置布局，不提供图形化布局编辑器
- 不支持 per-key label / color UI
- 用户 preset 的稳定 `id` 仍由文件名派生，重命名仅修改显示名称，不修改文件名和内部 `id`

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

## 3. 当前数据模型与文件格式

### 3.1 布局 preset 文件格式

当前已实现的 preset 文件为 JSON 格式，与平台无关：

```json
{
  "version": 1,
  "id": "user.layout.my-custom-layout",
  "name": "My Custom Layout",
  "displayName": "My Custom Layout",
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

说明：

- `version`：当前固定为 `1`
- `id`：写入文件时保存当前 `KeyboardLayout.id`；对用户 preset 而言，运行时会以**目标文件名派生出的稳定 id**为准
- `name` / `displayName`：当前都用于保存显示名称；加载时优先使用 `displayName`
- `bindings`：只保存键位映射本身，不包含 velocity、transpose 等演奏设置

使用标准化 JUCE keyCode（跨平台），不嵌入 DIK 扫描码。

### 3.2 preset 与演奏设置的分离

当前实现中，layout preset **只包含键位映射**，不包含 velocity、transpose 等演奏参数：

| 内容 | 属于 |
|---|---|
| keyCode → midiNote | Layout Preset |
| velocity、transpose、octave shift | Performance Settings（已有） |
| per-key label、color | 未来可选扩展，不在 MVP |

### 3.3 preset 注册与发现机制

用户目录下的 `.freepiano.layout` 文件会自动被发现并列入 `ControlsPanel` 的布局列表。当前实现中，**Import** 会将外部文件复制到用户配置目录；**Save Layout** 则保存到文件对话框返回的目标路径本身，不会额外复制一份到用户配置目录。不使用集中式注册表。

`ControlsPanel` 的 layout 列表从两个来源聚合：
1. 内置 preset（`default.freepiano.minimal`、`default.freepiano.full`）
2. 用户配置目录下的 `.freepiano.layout` 文件

### 3.4 当前已实现操作

当前已实现以下能力：

- **切换**：通过下拉菜单在内置 preset 与用户 preset 之间切换
- **保存当前**：将当前 `KeyboardLayout` 保存为 `.freepiano.layout` JSON 文件（保存位置取决于用户在文件对话框中选择的目标路径）
- **导入**：通过原生文件对话框选择任意位置的 `.freepiano.layout` 文件，复制到用户布局目录并立即应用
- **重命名显示名称**：通过独立的 `Rename` 按钮修改当前选中用户 preset 在下拉菜单中的显示名称
- **删除**：删除用户 preset 文件（内置 preset 不可删除）
- **重置当前布局**：根据当前 `layoutId` 恢复该布局的默认键位映射

当前仍未实现：
- 图形化布局编辑器
- per-key label / color UI
- 多设置组

### 3.5 与录音/回放的关系

layout preset 与录制独立：
- 录制只录 MIDI 事件，不录 layout
- 回放时用户自行加载期望的 layout 和插件
- 未来可选"录制时保存 layout"作为 meta event，但不在 MVP

---

## 4. 当前行为细节

### 4.1 内置 preset

- 内置 preset 固定为两个：
  - `default.freepiano.minimal` → `FreePiano Minimal`
  - `default.freepiano.full` → `FreePiano Full`
- 下拉菜单中这两个名称是固定友好名
- 通过 `Save Layout` 将内置 preset 另存为用户 preset 时，保存文件中的默认显示名称将沿用当前布局名称，因此也会使用上述统一名称

### 4.2 用户 preset 的 `id` 与显示名称

- 用户 preset 的稳定 `id` 由目标文件名派生，格式类似：`user.layout.<sanitised-file-name>`
- 运行时和启动恢复都按 `layoutId` 工作，而不是按显示名称工作
- 用户 preset 的显示名称优先来自 JSON 的 `displayName`
- 若 `displayName` 为空，则回退到 JSON 的 `name`
- 若两者都为空，则回退到文件名（去掉 `.freepiano.layout` 后缀）

这意味着：

- **Rename** 只会改变显示名称，不会改变文件名和 `layoutId`
- **Save As** 到新文件名会生成新的用户 `id`

### 4.3 保存行为

`Save Layout` 的当前行为：

- 以当前活动布局为基础生成新的 `.freepiano.layout`
- 若当前布局已有显示名称，则优先保留该名称
- 若当前布局名称为空，才回退到目标文件名派生名称
- 保存后会立即将当前活动布局切换为这个刚保存的用户 preset

注意：

- 若保存目标就在用户布局目录中，该 preset 会被后续扫描、下拉列表、删除、重命名和启动恢复逻辑识别
- 若保存到用户布局目录之外，则该文件本身不会被 `scanUserLayoutDirectory()` 自动发现；当前会立即切换到该新 `layoutId`，但后续重启或重新扫描时不会再从用户布局目录外重新发现它
- 若该外部保存布局此前已经把 `layoutId` / `keyMap` 写入设置，启动时仍可能基于持久化 `keyMap` 回退到一个可用基础布局；这不等同于“同一个外部 user preset 被重新发现”

### 4.4 导入行为

`Import Layout` 的当前行为：

- 可从任意位置选择 `.freepiano.layout` 文件
- 导入时保留原文件名并复制到用户布局目录
- 若用户布局目录中已存在同名文件，当前导入行为会覆盖该同名文件内容
- 导入后的用户 preset `id` 以复制后的目标文件名重新派生
- 若源文件中已有 `displayName` / `name`，则保留其显示名称；仅在名称为空时回退到文件名
- 导入完成后立即应用该布局，并更新持久化的 `layoutId`

### 4.5 重命名与删除行为

- `Rename` / `Delete` 仅对用户 preset 生效
- 选中内置 preset 时，这两个按钮应保持禁用
- `Rename` 会原地重写对应 `.freepiano.layout` 文件中的 `name` 和 `displayName` 字段
- `Delete` 会根据当前选中 `layoutId` 扫描用户布局目录，找到匹配文件后删除
- 如果删除的是当前活动用户布局，程序会回退到 `FreePiano Minimal`
- 当前删除路径不会显式检查 `deleteFile()` 的返回值；在磁盘权限异常或文件被占用时，UI 回退与设置更新可能先发生，因此“文件已删掉”应视为正常环境下的预期，而不是强保证

### 4.6 启动恢复行为

- 设置持久化保存的是 `lastLayoutId`（对外经 `layoutId` 视图暴露）和 `keyMap`，而不是布局显示名称
- 启动时先根据 `layoutId` 恢复内置或用户 preset，再叠加持久化的 `keyMap`
- 对用户 preset，恢复过程只会扫描用户布局目录，并按文件派生的稳定 `id` 查找目标布局

---

## 5. 关键实现文件

- `source/Layout/LayoutPreset.h/.cpp`：`loadLayoutPreset()` / `saveLayoutPreset()`
- `source/Layout/LayoutDirectoryScanner.h/.cpp`：`getUserLayoutDirectory()` / `scanUserLayoutDirectory()` / `loadUserLayoutById()`
- `source/MainComponent.cpp`：布局保存、导入、重命名、删除、恢复与下拉同步逻辑
- `source/UI/ControlsPanel.h/.cpp`：layout 下拉框与 Save / Import / Rename / Delete / Reset 按钮
- `source/Settings/SettingsModel.h`：`layoutId -> KeyboardLayout` 的恢复逻辑
- `source/Core/KeyMapTypes.h`：内置 preset 定义与统一显示名称

---

## 6. 已知边界与后续方向

- 旧版导出的或手写的 `.freepiano.layout` 文件如果携带历史显示名称，导入后仍可能显示旧名称；当前实现优先保留文件内显式名称而不是强制规范化
- 用户 preset 的 `id` 仍与文件名绑定，因此“重命名显示名称”和“重命名文件”是两个不同概念
- 保存到用户布局目录之外的 preset 不会被自动发现，也不会在后续启动时作为同一个外部用户 preset 被重新发现；如需进入下拉列表长期管理，应保存到用户布局目录或通过 Import 导入
- 当前文档只描述已实现行为；如后续引入图形化编辑器、per-key label/color 或更复杂的 preset 元数据，应单独补充本文件
