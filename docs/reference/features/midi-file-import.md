# MIDI 文件导入与回放功能说明

> 用途：说明 devpiano 的标准 MIDI 文件（`.mid` / `.midi`）导入解析、自动选轨、回放控制、事件支持与专项测试清单。
> 当前状态：已全量实现并稳定接入回放与音频链路。
> 更新时机：MIDI 解析逻辑、事件过滤规则或导入回放交互发生变化时。

---

## 1. 概述与设计定位

devpiano 支持打开标准 MIDI 文件并在当前发声链路中回放，为用户提供练琴示范、伴奏跟弹与音色试听能力：

1. **标准格式兼容**：支持标准 MIDI Type 0（单轨多通道）与 Type 1（多轨同步）文件；
2. **智能单轨选择**：自动分析各轨道事件，优先挑选 Note 事件密度最高的音乐主轨，自动跳过仅含 Tempo/Meta 信息的控制轨；
3. **丰富 Channel 消息支持**：除 Note On/Off 外，完整解析并还原 **CC64 延音踏板**、**Pitch Bend 弯音** 与 **Program Change 音色切换**；
4. **导入 Take 与导出 Take 解耦**：导入的 MIDI 作为只读 Playback Take 播放，**禁止再次导出为 MIDI**（保持 Export MIDI 按钮 disabled，防止有损二次转换），但**支持离线渲染导出为 WAV 音频**；
5. **极速拖放与路径记忆**：支持从操作系统直接拖拽 `.mid` 文件到窗口即时加载播放，自动记忆最近导入路径。

---

## 2. 核心架构与主处理流程

```text
[用户点击 Import MIDI / 拖入 .mid]
    │
    ▼
MidiFileImporter::importFile()
    │
    ├── 1. 读取并验证 MIDI 文件头 (Type 0 / 1, PPQ 时间基准)
    ├── 2. 遍历轨道统计 Note 数量 ──► 自动选定 Note 密度最高的轨道
    ├── 3. 时间基准转换: 将 MIDI Tick 转换为绝对采样点位置 (timestampSamples)
    ├── 4. 提取 Note, CC64 Sustain, Pitch Bend, Program Change 事件
    └── 5. 组装为 RecordingTake ──► 返回 std::optional<RecordingTake>
    │
    ▼
RecordingSessionController::handleMidiImported()
    │
    ├── 停止当前播放/录制 ──► 注入 All-Notes-Off
    ├── AudioEngine::armPlaybackStartPreRoll() (防 0s 音符截断)
    ├── 设定当前 Playback Take (isPlaybackTake=true, canExportMidi=false)
    └── RecordingEngine::startPlayback() ──► 自动开始回放 ──► 驱动虚拟键盘联动
```

---

## 3. 详细处理规则与边界设计

### 3.1 自动选轨算法
针对常见的 Type 1 多轨 MIDI 文件（例如 Track 0 仅包含拍号、速度与版权信息，Track 1/2 包含音符）：
- `MidiFileImporter` 遍历所有 Track，统计每个 Track 的 `noteOn` 事件数量；
- 选取包含 `noteOn` 数量最多的 Track 作为主解析轨；
- 若文件所有轨道均无 Note 事件，安全返回空结果并向 Logger 输出警告，程序不崩溃。

### 3.2 时间戳换算精度
- 根据 MIDI 文件头定义的 PPQ（Pulses Per Quarter Note）与 Tempo（默认 120 BPM，或首个 Tempo 设定），结合当前音频设备的采样率（如 44.1 kHz / 48 kHz），将每个 MIDI 事件的 Tick 准确转换为绝对采样点 `timestampSamples`；
- 回放时由 `RecordingEngine` 逐 audio block 调度，不受系统时钟抖动影响。

### 3.3 首音 0s 截断防御（Pre-roll 机制）
部分 MIDI 文件的首个音符起始时间为 0s。为防止音频设备启动瞬间的清理用 All-Notes-Off 将 0s 音符误消除，`AudioEngine` 在启动导入回放前调用 `armPlaybackStartPreRoll()`，在首个可听 block 前插入微小静音预备区，确保首音 100% 完整清晰发声。

### 3.4 状态机互斥与按钮联动
- **录制中（Recording）**：Import MIDI 按钮自动禁用，防止录制与导入冲突；
- **播放中（Playing）**：Import MIDI 按钮禁用；需先点击 Stop 停止当前播放，方可导入新文件；
- **导入成功后**：
  - `Record` 按钮可用（点击将放弃导入 Take 并进入录制）；
  - `Stop` 按钮可用；
  - `Play` / `Back` 按钮可用（点击 `Back` 从头重新播放）；
  - `Export MIDI` **严格保持 Disabled**；
  - `Export WAV` **保持 Enabled**（支持将导入的 MIDI 渲染为高质量 WAV 音频）。

---

## 4. 专项手工与边界测试清单

| 用例编号 | 测试场景 | 操作步骤与验证目标 | 状态 |
|---|---|---|:---:|
| **MID-001** | 标准单轨 MIDI 导入 | 导入 `simple-notes.mid`，能听到清晰音符序列，虚拟键盘联动高亮 | [x] 已通过 |
| **MID-002** | 多轨自动选轨 | 导入 `multitrack-basic.mid`（Track 0 为空，Track 1 含音符），自动选中 Track 1 并正常播放 | [x] 已通过 |
| **MID-003** | CC64 延音踏板还原 | 导入 `sustain-pedal.mid`，音符在踏板松开前持续延音，效果清晰可辨 | [x] 已通过 |
| **MID-004** | 0s 首音即时起奏 | 导入首个音符位于 0.000s 的 MIDI 文件，首音清晰完整，无吞音现象 | [x] 已通过 |
| **MID-005** | 空文件与非法文件容错 | 导入 `empty.mid` 或损坏的 `invalid.mid`，UI 提示错误，Logger 记录日志，程序不崩溃 | [x] 已通过 |
| **MID-006** | 播放中 Back 重放 | 播放到一半点击 `Back`，立即从当前 Take 最开头无缝重放 | [x] 已通过 |
| **MID-007** | 导出按钮状态边界 | 导入成功后确认 `Export MIDI` 为 disabled，`Export WAV` 为 enabled | [x] 已通过 |
| **MID-008** | 拖放即时加载 | 从 Windows 文件资源管理器拖拽 `.mid` 文件至主窗口，立即加载并播放 | [x] 已通过 |
| **MID-009** | 导入后 WAV 离线导出 | 导入 MIDI 后点击 `Export WAV`，正常渲染并生成包含该 MIDI 音乐的 WAV 文件 | [x] 已通过 |
