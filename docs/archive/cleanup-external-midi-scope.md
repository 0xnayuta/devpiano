# 清理计划：移除外部 MIDI 设备支持

> 用途：作为外部 MIDI 设备相关功能代码清理的执行计划。
> 更新时机：清理执行前各步骤确认、执行中状态更新、执行完成后关闭。
> 状态：✅ 已完成（2026-07-17）
> 关联 ADR：[`../decisions/0006-remove-external-midi-support.md`](../decisions/0006-remove-external-midi-support.md)

---

## 1. 背景

项目定位为"使用电脑键盘演奏钢琴，支持 MIDI 文件读取、导入、导出，以及音频导出"的个人工具，无外部 MIDI 硬件设备。

当前代码中包含完整的 `MidiRouter` 类及其集成逻辑，用于枚举、打开、接收系统 MIDI 输入设备消息。由于无硬件，`openAllInputs()` 打开 0 个设备，**所有相关代码编译后永不执行**，属于永久死代码。

---

## 2. 清理范围（三层）

### 第一层：核心类 — 删除

| 文件 | 行数 | 操作 |
|---|---|---|
| `source/Midi/MidiRouter.h` | 32 | 删除 |
| `source/Midi/MidiRouter.cpp` | 60 | 删除 |

**清理后效果：** `source/Midi/` 目录仅保留 `MidiChannelMapper.h/.cpp` 两个文件（共享基础设施，保留）。

### 第二层：集成与状态显示 — 简化/删除

| 文件 | 当前内容 | 操作 |
|---|---|---|
| `source/MainComponent.h` | `#include "Midi/MidiRouter.h"` + `MidiRouter midiRouter` 成员 | 删除 |
| `source/MainComponent.cpp` | `setupMidi()` 中 25 行：配置 router、collector、callback + openAllInputs | 精简至仅保留 MidiChannelMapper 设置 |
| `source/MainComponent.cpp` | `shutdown()` 中清理 router 的回调和输入 | 删除 |
| `source/MainComponent.h/.cpp` | `externalMidiMessageCount` / `lastExternalMidiMessage` 成员 | 删除 |
| `source/Core/AppState.h` | `InputState{ openMidiInputCount, midiActivityCount, lastMidiMessage }` | 简化为仅保留键盘相关字段 |
| `source/Core/AppStateBuilder.h/.cpp` | `buildRuntimeInputStateSnapshot()` 接收 `MidiRouter` 参数 | 移除 MidiRouter 参数和依赖 |
| `source/UI/HeaderPanel.h/.cpp` | `MidiStatus{ openInputCount, activityCount, lastMessage }` → 显示 `"MIDI Inputs: N"` | 简化或移除外部 MIDI 状态显示 |
| `source/UI/HeaderPanelStateBuilder.h/.cpp` | 从 AppState 构建 MidiStatus | 随 HeaderPanel 同步简化 |

### 第三层：RecordingEventSource — 简化

| 文件 | 当前内容 | 操作 |
|---|---|---|
| `source/Recording/RecordingEngine.h` | `enum RecordingEventSource { computerKeyboard, externalMidi, realtimeMidiBuffer, playback }` | 移除 `externalMidi` 枚举值 |
| `source/Recording/PerformanceFile.h` | `sourceExternalMidi = "externalMidi"` 常量 | 删除常量 |
| `source/Recording/PerformanceFile.cpp` | `sourceToString()` / `stringToSource()` 中的 externalMidi 分支 | 删除分支 |

**向后兼容说明：** 追踪所有消息路径确认没有任何代码使用 `RecordingEventSource::externalMidi` 写入文件（外部 MIDI 消息实际走 `MidiMessageCollector` → 音频线程 → 标记为 `realtimeMidiBuffer`）。删除该枚举值不影响任何现有 `.devpiano` 文件。

### 附加清理

| 文件 | 内容 | 操作 |
|---|---|---|
| `source/Recording/RecordingEngine.h/.cpp` | `recordEventAtCurrentPosition()` 方法 | 删除（无任何调用者） |

---

## 3. 执行顺序

```
Step 1: 删除 MidiRouter.h/.cpp
          ↓
Step 2: 简化 MainComponent（移除 router 成员、setupMidi 中的 router 配置、shutdown 中的清理、externalMidi 计数器）
          ↓
Step 3: 简化 AppState（移除 openMidiInputCount，移除 MidiRouter 参数）
          ↓
Step 4: 简化 HeaderPanel 状态显示
          ↓
Step 5: 清理 RecordingEventSource 枚举和 PerformanceFile 序列化
          ↓
Step 6: 删除无调用的 recordEventAtCurrentPosition()
          ↓
Step 7: 运行 ./scripts/dev.sh format && ./scripts/dev.sh test
          ↓
Step 8: 更新文档
```

---

## 4. 涉及文件清单（共约 15 个文件）

- `source/Midi/MidiRouter.h` — 删除
- `source/Midi/MidiRouter.cpp` — 删除
- `source/MainComponent.h` — 修改
- `source/MainComponent.cpp` — 修改
- `source/Core/AppState.h` — 修改
- `source/Core/AppStateBuilder.h` — 修改
- `source/Core/AppStateBuilder.cpp` — 修改
- `source/UI/HeaderPanel.h` — 修改
- `source/UI/HeaderPanel.cpp` — 修改
- `source/UI/HeaderPanelStateBuilder.h` — 修改（可能）
- `source/UI/HeaderPanelStateBuilder.cpp` — 修改（可能）
- `source/Recording/RecordingEngine.h` — 修改
- `source/Recording/RecordingEngine.cpp` — 修改
- `source/Recording/PerformanceFile.h` — 修改
- `source/Recording/PerformanceFile.cpp` — 修改

---

## 5. 验证标准

- [x] WSL 编译通过，无 `MidiRouter` 相关符号残留
- [x] HeaderPanel 不再显示 `"MIDI Inputs: 0"`
- [x] `externalMidi` 枚举值在所有代码中已无引用
- [x] `recordEventAtCurrentPosition()` 已无定义和声明
- [x] 现有单元测试全部通过
- [x] 打开/保存 `.devpiano` 文件 roundtrip 测试正常（人工验证）
- [x] Windows MSVC 验证构建通过

## 6. 清理后操作

### 文档更新

- [`docs/roadmap/roadmap.md`]：无更新必要（该文档不引用 external Midi）
- [`docs/roadmap/current-iteration.md`]：无更新必要（该条目已在当前迭代范围之外）

### 额外清理

- [x] 移除 `source/Locale/zh_CN.loc.h` 中 3 条残余翻译字符串（`"MIDI Inputs: "`、`" | Activity: "`、`" | Last: "`）

### 不执行项说明

| 文档 | 原因 |
|------|------|
| `docs/archive/phase6-7-completion-detail.md` | §已取消功能 保持现有记录，无需修改 |
| `issues/known-issues.md` | 保留硬件依赖相关缺陷记录作为历史存档（不主动删除） |
| `docs/decisions/0006-remove-external-midi-support.md` | ADR 记录决策，无需修改 |
