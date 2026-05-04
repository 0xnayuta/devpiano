# Phase 6：演奏数据持久化与播放体验增强

> 用途：说明 Phase 6 各子阶段的功能设计、文件格式、行为边界与验收标准。
> 当前状态：**进行中，Phase 6-1/6-2/6-6/6-7 已完成，Phase 6-3~6-5 暂缓。**
> 读者：维护 Phase 6 功能的开发者、规划者。
> 更新时机：Phase 6 各子阶段设计变化、实现状态变化、验收结果更新时。

---

## Phase 6-6：Diagnostics 最小层 ✅ 已完成

Phase 6-6 已完成，包含以下内容：

**新增文件（4 个）：**

| 文件 | 职责 |
|------|------|
| `source/Diagnostics/DebugLog.h` | 日志宏定义（`DP_LOG_INFO`、`DP_LOG_WARN`、`DP_LOG_ERROR`、`DP_DEBUG_LOG`、`DP_TRACE_MIDI`） |
| `source/Diagnostics/DebugLog.cpp` | 日志宏实现，Debug/Release 条件编译 |
| `source/Diagnostics/MidiTrace.h` | MIDI 消息描述函数声明（`describeMidiMessage`） |
| `source/Diagnostics/MidiTrace.cpp` | MIDI 消息解析实现（note on/off、CC、pitch bend、program change、channel pressure、aftertouch、sysEx、meta、fallback） |

**接口说明：**

| 接口 | 级别 | Debug 行为 | Release 行为 |
|------|------|-----------|-------------|
| `DP_LOG_INFO(message)` | 运行日志 | DBG + Logger | Logger |
| `DP_LOG_WARN(message)` | 运行日志 | DBG + Logger | Logger |
| `DP_LOG_ERROR(message)` | 运行日志 | DBG + Logger | Logger |
| `DP_DEBUG_LOG(message)` | Debug-only | DBG + Logger | **无输出** |
| `DP_TRACE_MIDI(message, stage)` | Debug-only | DBG 带 stage 标签 | **无输出** |
| `describeMidiMessage(const MidiMessage&)` | 工具函数 | 格式化字符串 | 同左 |

**已接入的关键路径：**

| 文件 | 接入点 | 宏 |
|------|--------|----|
| `MidiRouter.cpp` | 外部 MIDI 入口 | `DP_TRACE_MIDI` |
| `MidiFileImporter.cpp` | 文件读取 + non-note 事件 | `DP_TRACE_MIDI`, `DP_LOG_INFO/WARN/ERROR`, `DP_DEBUG_LOG` |
| `RecordingEngine.cpp` | 录制/回放状态 + 丢弃告警 | `DP_DEBUG_LOG`, `DP_LOG_INFO` |
| `RecordingSessionController.cpp` | Save/Open/Import/Export/Record/Playback 全流程 | `DP_LOG_INFO/WARN/ERROR` |
| `PluginHost.cpp` | scan / load / prepare / unload | `DP_LOG_INFO/WARN/ERROR` |
| `LayoutFlowSupport.cpp` | 布局保存结果 | `DP_LOG_INFO/ERROR` |
| `PluginFlowSupport.cpp` | 无效扫描目录 | `DP_LOG_WARN` |
| `MainComponent.cpp` | 音频设备诊断 | `DP_LOG_INFO` |

**Debug / Release 边界：**

- `DP_DEBUG_LOG` 和 `DP_TRACE_MIDI` 在 `JUCE_DEBUG` 或 `DEBUG` 条件下启用，Release 下为空宏或空实现（零副作用）。
- `DP_LOG_INFO/WARN/ERROR` 在 Debug 下同时写 `DBG`（调试器输出窗口）和 `juce::Logger`；Release 下只写 `Logger`。
- `traceMidi()` 在 Release 下调用 `juce::ignoreUnused` 保证无副作用。

**字符串编码边界：**

- Diagnostics API 以 `juce::String` 为主接口，`const char*` overload 仅作为兼容层并按 UTF-8 解释。
- 业务代码应直接传 `juce::String` 或字符串字面量给 `DP_LOG_*` / `DP_TRACE_MIDI`。
- 业务代码不应再通过 `.toRawUTF8()` 把 `juce::String` 降级为裸 `const char*` 后交给 Diagnostics，避免 JUCE Debug 下把 UTF-8 误判为 ASCII 触发 `juce::String(const char*)` 断言。

**散落 debug 清理：**

已将以下文件中的 `juce::Logger::writeToLog` 全部替换为 `DP_LOG_*` 系列宏：
- `MidiFileImporter.cpp`（17 处）
- `RecordingSessionController.cpp`（25 处）
- `LayoutFlowSupport.cpp`（1 处）
- `PluginFlowSupport.cpp`（1 处）
- `MainComponent.cpp`（1 处）

---

## Phase 6-7：MIDI / Performance 测试夹具与最小回归样本库

### 任务定位

Phase 6-7 是 Phase 6 后续各阶段（6-1 演奏文件保存/打开、6-2 播放速度控制、6-5 MIDI 导入增强）以及整体 MIDI roundtrip 的**工程基础设施**。它不实现业务功能，而是为这些阶段提供统一的测试输入基准。

**前置关系：** 建议在 Phase 6-1、6-2、6-5 之前或并行完成。Phase 6-6（Diagnostics 最小层）是 Phase 6-7 的依赖基础——fixture 验证过程中产生的诊断输出依赖 `DP_LOG_*` / `DP_TRACE_MIDI` 宏。

### 背景问题

当前 Phase 6 各阶段的开发和手工验收面临以下问题：

1. **依赖临时文件**：每次验证 MIDI 导入、roundtrip、错误处理都需要手动准备 MIDI 文件或临时敲键盘录制，无法稳定复现。
2. **无基准样本**：不同开发者使用不同的 MIDI 文件，导入行为的判断标准不统一。
3. **口头复现**：bug 报告依赖"我用一个 MIDI 文件试了，不行"这类描述，无法快速定位是文件问题还是代码问题。
4. **smoke test 缺失**：每次发版或合入前没有统一的最小回归集。

Phase 6-7 的目标就是用**固定 fixture 样本库**取代临时文件和口头复现。

### 目标

- 建立固定 MIDI fixture 样本库，涵盖主流场景和边界情况。
- 建立固定 PerformanceEvent / 演奏数据 fixture（JSON 格式）。
- 为 MIDI 导入、导出、roundtrip、错误处理、回放行为提供稳定输入。
- 让后续 smoke test 和手工验收有统一依据。
- 避免每次 bug 排查都依赖临时文件和口头复现。

### 非目标

- **不实现测试代码框架**（Phase 8 之后才考虑单元测试基础设施）。
- **不实现自动化测试运行**（无 test runner、无 CI 脚本）。
- **不创建 mock 插件或 mock 音频设备**。
- **不实现 fixture 的运行时加载逻辑**（那是 Phase 6-1/6-5 的任务）。
- **不修改 source/、CMakeLists.txt 或任何业务代码**。本阶段只产出现有文档（本文档）。

### 建议目录结构

```
docs/testing/fixtures/
├── midi/
│   ├── simple-notes.mid          # 最简 note on/off 序列
│   ├── velocity-channel.mid       # 多 velocity、多 channel
│   ├── sustain-pedal.mid          # 含 CC64 sustain on/off
│   ├── multitrack-basic.mid       # 多轨（Type 1），含 track names
│   ├── tempo-change-basic.mid     # 含 meta tempo change 事件
│   ├── empty.mid                  # 零事件空文件
│   └── invalid.mid                 # 损坏/非法 MIDI 文件
└── performance/
    └── simple-performance.json    # 最小 .devpiano 结构样本（Phase 6-1 之后才有意义）
```

> **注意**：本轮不创建任何文件。仅记录未来应创建的 fixture 清单及用途。

### 建议 fixture 清单

#### MIDI fixtures

| 文件名 | 内容描述 | 预期用途 |
|--------|----------|----------|
| `simple-notes.mid` | 单轨，60/64/67 三个音符依次发声，velocity 100/80/60，时长各 0.5s，120 BPM，960 PPQ | MIDI 导入基础验证；roundtrip 往返对比基准 |
| `velocity-channel.mid` | 单轨，16 个音符跨不同 velocity(20/64/127) 和 2 个 channel(1/2) | 验证 velocity 解析、channel 分配是否正确 |
| `sustain-pedal.mid` | 单轨，含 CC64 sustain on(127) / sustain off(0)，覆盖多个音符 | Phase 6-5 增强导入验证；sustain 效果可听性 |
| `multitrack-basic.mid` | Type 1，2 个 track，track 0 含 tempo meta，track 1 含 note 事件 | 验证多轨选择逻辑（自动选有 note 的轨） |
| `tempo-change-basic.mid` | 单轨，0ms 设 tempo 120，500ms 后切换为 tempo 180 | 验证 tempo change 事件被正确跳过或不崩溃 |
| `empty.mid` | 合法 MIDI 文件头，但零 track、零事件 | 验证空文件导入不崩溃，Logger 输出警告 |
| `invalid.mid` | 非 MIDI 数据（如随机字节、"not a midi file" 文本） | 验证文件解析错误处理不崩溃，Logger 输出错误 |

#### Performance fixture

| 文件名 | 内容描述 | 预期用途 |
|--------|----------|----------|
| `simple-performance.json` | Phase 6-1 之后的最小 `.devpiano` 格式样本，含 2-3 个 note 事件 | 验证保存/打开 roundtrip 的最小基准 |

### 每个 fixture 的预期用途

| Fixture | 用途 |
|---------|------|
| `simple-notes.mid` | Phase 6-5 之前 MIDI 导入的基础验证；作为 `DP_TRACE_MIDI` 输出对照基准（ Debug 下 MIDI trace 输出 vs 预期 note 序列） |
| `sustain-pedal.mid` | Phase 6-5 增强导入的目标 fixture；手工验证延音踏板效果是否可听 |
| `multitrack-basic.mid` | 自动选轨逻辑验证；手工确认选中的轨是含 note 的轨而非 tempo track |
| `tempo-change-basic.mid` | 验证 phase4-midi-file-import.md 中"跳过 meta 事件"行为是否稳定；导入过程不因 tempo change 事件而出错 |
| `empty.mid` | 错误处理边界验证；空文件不崩溃的最小保证 |
| `invalid.mid` | 健壮性验证；损坏文件不崩溃，Logger 正确输出错误 |
| `velocity-channel.mid` | 验证 CC、velocity、channel 解析的完整性 |
| `simple-performance.json` | Phase 6-1 保存/打开 roundtrip 的最小输入；后续可在此基础上扩展 smoke test |

### 验收标准

- [ ] 文档中清晰列出 8 个 MIDI fixture + 1 个 performance fixture 的名称、描述和用途。
- [ ] 每个 fixture 的预期用途与 Phase 6-5、Phase 6-1 的功能边界对应。
- [ ] fixture 清单与 Phase 6-5 验收标准中的"导入 xxx 事件"形成一一映射。
- [ ] 明确说明本轮不创建任何 fixture 文件，仅做规划记录。
- [ ] Phase 6-7 与 Phase 6-6（Diagnostics）和 Phase 6-5（MIDI 导入增强）的关系清晰。

### 风险与边界

| 风险 | 等级 | 应对 |
|------|------|------|
| fixture 文件格式不符合预期导致验收失效 | 中 | 本轮只规划，下轮创建时需对照 JUCE `MidiFile` 解析行为验证格式 |
| fixture 覆盖不足导致边界情况漏测 | 低 | MVP 阶段只覆盖最高频场景；边界情况后续按需补充 |
| 规划过度，实际创建时发现不合理 | 低 | fixture 结构极简（MIDI 是标准格式，JSON 是 human-readable），不易有结构性错误 |
| 成为拖延 Phase 6-1/6-5 的借口 | 中 | 本轮仅文档更新，下轮实现时 fixture 创建和业务代码实现可并行推进 |

### 与 Phase 6-5 MIDI 导入增强、Phase 6-6 Diagnostics 最小层的关系

```
Phase 6-6 (Diagnostics)        Phase 6-7 (Fixtures)          Phase 6-5 (MIDI Import)
      │                              │                                │
      │  DP_TRACE_MIDI               │  固定输入基准                  │  新增事件类型
      │  输出对照                    │                                │
      └──────────────────────────────┴────────────────────────────────┘
                                     │
                      fixture 验证时用 DP_TRACE_MIDI
                      对比 MIDI 导入的实际行为

Phase 6-7 同时也是 Phase 6-1 (Save/Open) 和 Phase 6-2 (Speed) 的基础设施：
fixture 的 PerformanceEvent 数据结构是 Phase 6-1 保存/打开的直接操作对象。
```

---

## 1. 背景与目标

### 1.1 当前缺口

Phase 3 实现了录制/回放/MIDI 导出/WAV 导出的 MVP 闭环，但存在一个核心缺口：

**录制完即丢失。**

用户录制一段演奏后，`RecordingTake` 仅存在于内存中。关掉程序，数据消失。唯一间接保存方式是导出为 `.mid` 文件，但那是有损转换（sample-accurate 时间戳 → MIDI tick → 再转回 sample），且丢失 devpiano 内部元数据。

### 1.2 Phase 6 目标

填补 FreePiano 核心功能差距——录制后能保存、保存后能打开、打开后能调速播放。

Phase 6 包含六个子阶段：

| 子阶段 | 目标 | 用户价值 | 备注 |
|--------|------|---------|------|
| **Phase 6-6** | **Diagnostics 最小层** | **高（工程基础设施）** | **✅ 已完成。已接入 8 个业务文件，散落 Logger 已全部替换。** |
| **Phase 6-7** | **MIDI/Performance 测试夹具** | **高（工程基础设施）** | **✅ 已完成（文档已就绪，fixture 样本待按需创建）。** |
| **Phase 6-1** | **演奏文件保存/打开（`.devpiano`）** | **最高——当前录制完即丢失** | **✅ 已完成。** |
| Phase 6-2 | 播放速度控制（0.5x–2.0x） | 高——练琴刚需 | 暂缓（尽快做） |
| Phase 6-3 | 最近文件列表 + 拖拽打开 | 中——体验增强 | 暂缓 |
| Phase 6-4 | 基础 MIDI 编辑（delete notes） | 中——最小编辑能力 | 暂缓 |
| Phase 6-5 | MIDI 导入增强（sustain/pitch bend/program change） | 中——提升回放保真度 | 暂缓 |

---

## 2. 当前能力摘要

### 已有能力

| 能力 | 状态 | 依据 |
|------|------|------|
| 录制 | 已实现 | `RecordingEngine` + `RecordingFlowSupport` |
| 回放 | 已实现 | `RecordingEngine::startPlayback()` / `renderPlaybackBlock()` |
| MIDI 导出 | 已实现 | `MidiFileExporter::exportTakeAsMidiFile()`，标准 MIDI Type 1，960 PPQ |
| WAV 导出 | 已实现 | `WavFileExporter`，fallback synth 离线渲染 |
| MIDI 导入 | 已实现 | `MidiFileImporter`，自动选轨，回放 |
| 最近导入/导出路径 | 已实现 | `SettingsModel::lastMidiImportPath` / `lastMidiExportPath` |

### 缺失能力

| 能力 | 状态 | 影响 |
|------|------|------|
| 演奏文件保存 | ✅ 已实现 | 录制数据可持久化为 `.devpiano` |
| 演奏文件打开 | ✅ 已实现 | 可恢复之前的录制 |
| 播放速度控制 | 未实现 | 无法变速回放 |
| 最近文件列表 | 未实现 | 无法快速打开最近文件 |
| 拖拽打开 | 未实现 | 需通过按钮+FileChooser |
| 基础编辑 | 未实现 | 录制后无法修改 |
| 非 note 事件导入 | 未实现 | 外部 MIDI 回放保真度有限 |

---

## 3. `.devpiano` 文件格式设计

### 3.1 定位

`.devpiano` 是 devpiano 的**私有原生格式**，用于无损保存和恢复演奏数据。

与 `.mid` 的定位对比：

| | `.devpiano` | `.mid` |
|---|---|---|
| 定位 | 内部保存/恢复 | 外部交换 |
| 精度 | 无损（sample-accurate） | 有损（tick-based 往返转换） |
| 受众 | devpiano 自身 | 任何 DAW/播放器 |
| 元数据 | 保留 devpiano 内部语义 | 仅标准 MIDI 语义 |
| 可扩展性 | 不受 MIDI 规范限制 | 受 MIDI 规范约束 |

### 3.2 格式选择

使用 JSON 文本格式，理由：

- 可读性好，便于调试
- JUCE 内置 JSON 支持（`juce::JSON`、`juce::DynamicObject`）
- 无需额外依赖（如 protobuf、flatbuffers）
- 版本演进友好（新增字段向后兼容）
- 文件体积可接受（演奏事件量级通常在万级以下）

### 3.3 JSON Schema

```json
{
  "version": 1,
  "format": "devpiano-performance",
  "sampleRate": 44100.0,
  "lengthSamples": 2646000,
  "metadata": {
    "createdAt": "2026-05-03T12:00:00Z",
    "title": "",
    "notes": ""
  },
  "events": [
    {
      "timestampSamples": 44100,
      "source": "computerKeyboard",
      "midiData": [144, 60, 127]
    },
    {
      "timestampSamples": 88200,
      "source": "computerKeyboard",
      "midiData": [128, 60, 0]
    }
  ]
}
```

### 3.4 字段说明

#### 顶层字段

| 字段 | 类型 | 说明 |
|------|------|------|
| `version` | int | 格式版本号，当前为 `1`。未来格式变更时递增。 |
| `format` | string | 固定值 `"devpiano-performance"`，用于文件类型识别。 |
| `sampleRate` | double | 录制时的音频采样率（Hz）。回放时若设备采样率不同，需按比例换算。 |
| `lengthSamples` | int64 | 录制总长度（samples）。 |
| `metadata` | object | 元数据，见下表。 |
| `events` | array | 演奏事件数组，按 `timestampSamples` 升序排列。 |

#### metadata 字段

| 字段 | 类型 | 说明 |
|------|------|------|
| `createdAt` | string | ISO 8601 创建时间。 |
| `title` | string | 用户可选标题，默认为空。 |
| `notes` | string | 用户可选备注，默认为空。 |

#### events[] 字段

| 字段 | 类型 | 说明 |
|------|------|------|
| `timestampSamples` | int64 | 事件发生的绝对 sample 位置（相对于录制开始）。 |
| `source` | string | 事件来源，映射 `RecordingEventSource` 枚举。 |
| `midiData` | array[int] | MIDI 消息原始字节（`juce::MidiMessage::getRawData()` + `getRawDataSize()`）。 |

#### source 枚举映射

| `RecordingEventSource` | JSON 字符串 |
|------------------------|-------------|
| `computerKeyboard` | `"computerKeyboard"` |
| `externalMidi` | `"externalMidi"` |
| `realtimeMidiBuffer` | `"realtimeMidiBuffer"` |
| `playback` | `"playback"` |

### 3.5 与 RecordingTake 的映射

```text
RecordingTake                    .devpiano JSON
─────────────                    ──────────────
sampleRate                  →    sampleRate
lengthSamples               →    lengthSamples
events[].timestampSamples   →    events[].timestampSamples
events[].source             →    events[].source (enum → string)
events[].message            →    events[].midiData (raw bytes)
```

序列化方向：`RecordingTake` → JSON → 文件
反序列化方向：文件 → JSON → `RecordingTake`

### 3.6 版本策略

- `version: 1` 为初始版本。
- 未来新增字段（如 tempo map、多轨）通过新增可选字段实现，不改变 version。
- 若事件结构发生不兼容变更（如 `midiData` 编码方式变化），version 递增。
- 读取时检查 `version`，不支持的版本提示用户升级程序。

---

## 4. Phase 6-1：演奏文件保存/打开 ✅ 已完成

### 4.1 目标

实现 `RecordingTake` 的持久化保存和恢复，形成"录制 → 保存 → 打开 → 回放"闭环。

### 4.2 保存行为

- 用户点击 **Save** 按钮 → FileChooser 打开，默认文件名基于当前时间戳（如 `performance-20260503-120000.devpiano`）。
- 默认目录为上次保存路径（`SettingsModel::lastPerformanceSavePath`）。
- 将当前 `RecordingTake` 序列化为 JSON 并写入 `.devpiano` 文件。
- 保存完成后 Logger 输出文件路径和事件数。
- 无录制数据时 Save 按钮 disabled。

### 4.3 打开行为

- 用户点击 **Open** 按钮 → FileChooser 打开，文件过滤器为 `*.devpiano`。
- 默认目录为上次打开路径（`SettingsModel::lastPerformanceOpenPath`）。
- 读取文件 → 解析 JSON → 构建 `RecordingTake` → 设置为当前 take。
- 打开成功后自动开始回放（与 MIDI 导入行为一致）。
- 打开失败（格式错误、版本不支持、文件损坏）时 Logger 输出错误信息，不崩溃。

### 4.4 按钮状态

| 按钮 | 条件 | 状态 |
|------|------|------|
| Save | 有录制 take 且非 playing/recording 状态 | enabled |
| Save | 无 take 或 playing/recording 中 | disabled |
| Open | 非 playing/recording 状态 | enabled |
| Open | playing/recording 中 | disabled |

### 4.5 路径记忆

- `SettingsModel` 新增 `lastPerformanceSavePath` 和 `lastPerformanceOpenPath` 字段。
- `SettingsStore` 负责读写。
- 与现有 `lastMidiImportPath` / `lastMidiExportPath` 模式一致。

### 4.6 不做范围

- 不做自动保存。
- 不做文件加密或压缩。
- 不做 `.devpiano` 文件的拖拽打开（Phase 6-3）。
- 不做最近文件列表（Phase 6-3）。
- 不做 `.fpm` 旧格式兼容。
- 不在 `.devpiano` 中保存插件状态、布局或音频设备配置（Phase 7 完整工程文件）。

### 4.7 验收标准

- [ ] 有录制 take 时，Save 按钮 enabled；点击后 FileChooser 打开。
- [ ] 保存的 `.devpiano` 文件可被任何文本编辑器打开，内容为合法 JSON。
- [ ] 保存的 JSON 包含 `version`、`format`、`sampleRate`、`lengthSamples`、`events` 字段。
- [ ] 打开之前保存的 `.devpiano` 文件后，回放内容与原始录制一致。
- [ ] 打开后 Logger 输出文件路径和事件数。
- [ ] 打开损坏或格式错误的 `.devpiano` 文件时，Logger 输出错误信息，不崩溃。
- [ ] 无录制数据时 Save 按钮 disabled。
- [ ] Recording / Playing 期间 Save 和 Open 按钮均 disabled。
- [ ] 保存后再次打开，路径记忆生效（FileChooser 默认定位到上次目录）。

---

## 5. Phase 6-2：播放速度控制 ✅ 已完成

### 5.1 目标

支持 0.5x–2.0x 速度调节，满足练琴场景的慢速回放需求。

### 5.2 设计

- 速度范围：0.5x、0.75x、1.0x（默认）、1.25x、1.5x、2.0x。
- ControlsPanel 增加速度显示 + 增减按钮（`-` / `+`）。
- 速度变更**实时生效**（播放中调整立即改变回放速率）。
- 每次启动默认 1.0x，不持久化速度值。

### 5.3 实现要点

- `RecordingEngine.playbackSpeedMultiplier`：`std::atomic<double>`，默认值 `1.0`，范围 `0.5–2.0`。
- `RecordingEngine.scaledPlaybackLengthSamples` / `playbackPositionSamples`：均为 `std::atomic<std::int64_t>`，跨 audio 线程安全。
- `RecordingEngine.setPlaybackSpeedMultiplier(double)`：播放中切换时，先重校准 `playbackPositionSamples`（`pos × oldSpeed / newSpeed`），再更新 `scaledPlaybackLengthSamples`。
- `RecordingEngine.getPlaybackSpeedMultiplier()`：返回 `.load()` 值。
- `renderPlaybackBlock()` 中用 `combinedRatio = playbackSampleRateRatio / playbackSpeedMultiplier` 缩放时间戳，不修改原始事件；block 边界使用 `[start, end)` 半开区间，`>=` 上界是刻意的防御性守卫。
- 速度变更通过 `RecordingSessionController.handlePlaybackSpeedChange()` 同步到 engine + UI，不写 settings。
- `MainComponent` 初始化时硬编码 1.0，不从持久化存储恢复。
- ControlsPanel 速度按钮在 `recording` 状态下自动禁用（`setEnabled(false)`），`playing` 状态下保持可操作。

### 5.4 不做范围

- 不做连续变速滑块（只做离散档位）。
- 不做 pitch correction（变速同时变调是预期行为）。
- 不做 loop / A-B 循环。
- **不做速度值持久化**（每次启动均为 1.0x）。

### 5.5 验收标准

- [x] ControlsPanel 显示当前播放速度，默认 `1.00x`。
- [x] 点击 `-` / `+` 按钮可调整速度，范围 0.50x–2.00x。
- [x] 播放中调整速度，回放速率立即变化。
- [x] 每次启动默认 1.0x，不受上次退出时速度影响。
- [x] 0.5x 慢速播放时音符间隔明显拉长，无卡顿。
- [x] 2.0x 快速播放时音符间隔明显缩短，无爆音。

---

## 6. Phase 6-3：最近文件列表 + 拖拽打开

### 6.1 目标

提供最近打开的演奏文件/MIDI 文件列表（最多 10 条），并支持拖拽文件到窗口触发打开。

### 6.2 最近文件列表

- 记录最近打开的 `.devpiano` 和 `.mid` 文件路径，最多 10 条。
- 列表持久化到 `SettingsModel` / `SettingsStore`。
- UI 入口：菜单栏 File 菜单下的 Recent Files 子菜单，或 ControlsPanel 的下拉列表。
- 点击列表项直接打开对应文件。
- 列表中的文件不存在时，点击后提示并从列表移除。

### 6.3 拖拽打开

- 支持拖拽 `.devpiano` 和 `.mid` 文件到主窗口。
- 拖拽 `.devpiano` → 打开演奏文件（Phase 6-1 逻辑）。
- 拖拽 `.mid` → 导入 MIDI 文件（Phase 4 逻辑）。
- 拖拽其他文件类型 → 忽略或提示不支持。

### 6.4 不做范围

- 不做播放列表管理。
- 不做文件分类/标签。
- 不做拖拽到非窗口区域。

### 6.5 验收标准

- [ ] 打开 `.devpiano` 或 `.mid` 文件后，最近文件列表更新。
- [ ] 最近文件列表最多显示 10 条。
- [ ] 点击最近文件列表项可打开对应文件。
- [ ] 列表中文件不存在时，点击后提示并移除该项。
- [ ] 拖拽 `.devpiano` 文件到窗口可打开。
- [ ] 拖拽 `.mid` 文件到窗口可导入。
- [ ] 拖拽不支持的文件类型时无反应或提示。

---

## 7. Phase 6-4：基础 MIDI 编辑（delete notes）

### 7.1 目标

提供最小编辑能力：选中音符 → 删除。

### 7.2 设计

- 将 `RecordingTake.events` 从 `const vector` 改为可变结构（或提供删除接口）。
- UI：虚拟键盘面板支持点击选中音符（高亮显示），或提供简单的事件列表视图。
- 选中后按 Delete 键或点击 Delete 按钮删除对应事件。
- 删除操作不可撤销（MVP 阶段不做 undo/redo）。

### 7.3 不做范围

- 不做添加/移动/量化音符。
- 不做钢琴卷帘编辑器（Phase 8）。
- 不做 undo/redo。
- 不做多选/批量删除。

### 7.4 验收标准

- [ ] 打开或录制的演奏数据中，可选中单个音符。
- [ ] 选中音符后可删除，删除后回放不再包含该音符。
- [ ] 删除操作不破坏其他事件的时间线。
- [ ] 删除后可正常保存为 `.devpiano` 文件。

---

## 8. Phase 6-5：MIDI 导入增强

### 8.1 目标

导入外部 MIDI 文件时，除 note on/off 外，还导入 sustain CC64、pitch bend、program change 等安全的 channel voice 消息，提升回放保真度。

### 8.2 设计

- 扩展 `MidiFileImporter`，在收集 note on/off 的同时收集：
  - Control Change（CC64 sustain pedal、CC1 mod wheel 等常用 CC）
  - Pitch Bend
  - Program Change
- 这些事件作为 `PerformanceEvent` 存入 `RecordingTake.events`，与 note 事件共享同一时间线。
- 回放时这些事件通过 `AudioEngine` 的 MIDI 链路送入插件/fallback synth。

### 8.3 不做范围

- 不导入 SysEx 消息。
- 不导入 meta 事件（tempo、time signature 等）。
- 不导入 RPN/NRPN。
- 不做 GM 音色映射。

### 8.4 验收标准

- [ ] 导入包含 sustain CC64 的 MIDI 文件后，回放时延音踏板效果可听。
- [ ] 导入包含 pitch bend 的 MIDI 文件后，回放时弯音效果可听。
- [ ] 导入包含 program change 的 MIDI 文件后，回放时音色变化可听（依赖插件支持）。
- [ ] 导入不含这些事件的 MIDI 文件时，行为与 Phase 4 一致，无回退。
- [ ] Logger 输出导入的非 note 事件数量。

---

## 9. 风险与边界

### 9.1 Phase 6-1 风险

| 风险 | 等级 | 应对 |
|------|------|------|
| JSON 序列化大文件性能 | 低 | 演奏事件量级通常在万级以下，JSON 解析开销可接受 |
| 文件格式版本兼容 | 低 | 初始版本号为 1，新增可选字段不升版本 |
| `juce::MidiMessage` 序列化边界 | 中 | 需确认 `getRawData()` 对所有消息类型（sysex 等）的行为 |

### 9.2 Phase 6-2 风险

| 风险 | 等级 | 应对 |
|------|------|------|
| 极端速度下的音频质量 | 低 | 0.5x/2.0x 范围有限，fallback synth 和插件通常可处理 |
| 速度变更时的 glitch | 中 | 变速时可能有微小跳变，可接受 |

### 9.3 Phase 6-3 风险

| 风险 | 等级 | 应对 |
|------|------|------|
| 最近文件列表持久化体积 | 低 | 10 条路径，体积极小 |
| 拖拽与现有 UI 交互冲突 | 中 | 需处理焦点和 drop 位置边界 |

### 9.4 Phase 6-4 风险

| 风险 | 等级 | 应对 |
|------|------|------|
| 数据模型变更影响回放/导出 | 中 | 需确保删除操作不破坏事件排序和时间线 |
| UI 选中交互复杂度 | 中 | MVP 阶段只做简单选中+删除，不做复杂编辑 |

### 9.5 Phase 6-5 风险

| 风险 | 等级 | 应对 |
|------|------|------|
| 非 note 事件增加文件体积 | 低 | CC/pitch bend 事件量远少于 note 事件 |
| fallback synth 不响应 CC/pitch bend | 低 | 依赖插件支持；fallback synth 可忽略不支持的事件 |

---

## 10. 验收标准总览

### Phase 6-1：演奏文件保存/打开

- [ ] Save 按钮：有 take 且非录制/播放中时 enabled。
- [ ] 保存的 `.devpiano` 文件为合法 JSON，包含完整事件数据。
- [ ] Open 按钮：非录制/播放中时 enabled。
- [ ] 打开 `.devpiano` 文件后回放内容与原始录制一致。
- [ ] 打开损坏文件时不崩溃，Logger 输出错误。
- [ ] 路径记忆：Save/Open 均记住上次目录。

### Phase 6-2：播放速度控制

- [ ] 速度显示 + 增减按钮可用，范围 0.50x–2.00x。
- [ ] 播放中变速立即生效。
- [ ] 速度值持久化恢复。

### Phase 6-3：最近文件列表 + 拖拽打开

- [ ] 最近文件列表最多 10 条，点击可打开。
- [ ] 拖拽 `.devpiano` / `.mid` 文件到窗口可打开。

### Phase 6-4：基础 MIDI 编辑

- [ ] 可选中并删除单个音符。
- [ ] 删除后回放和保存均正确。

### Phase 6-5：MIDI 导入增强

- [ ] 导入 sustain CC64、pitch bend、program change 并回放。
- [ ] 不含这些事件的 MIDI 文件行为无回退。

### Phase 6-7：MIDI / Performance 测试夹具

- [ ] `docs/testing/fixtures/midi/` 目录下有 7 个 MIDI fixture 文件（`simple-notes.mid`、`velocity-channel.mid`、`sustain-pedal.mid`、`multitrack-basic.mid`、`tempo-change-basic.mid`、`empty.mid`、`invalid.mid`）。
- [ ] `docs/testing/fixtures/performance/` 目录下有 `simple-performance.json`（Phase 6-1 完成后才有完整结构意义）。
- [ ] 每个 fixture 文件格式合法，可被 JUCE `MidiFile` 正确读取。
- [ ] `invalid.mid` 被正确识别为非法 MIDI 文件，不导致程序崩溃。
- [ ] fixture 清单覆盖 Phase 6-5 验收标准中的所有新增事件类型（sustain CC64、pitch bend、program change）。
