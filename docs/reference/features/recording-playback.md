# 演奏实时录制、回放与 MIDI 导出功能说明

> 用途：说明 devpiano 的实时演奏录制（`RecordingEngine`）、多通道 MIDI 采集、回放事件调度、标准 MIDI 导出（`MidiFileExporter`）与专项测试清单。
> 当前状态：已全量实现并稳定服务于演奏录制与回放。
> 更新时机：录制引擎边界、回放调度器或 MIDI 导出格式发生变化时。

---

## 1. 概述与设计定位

devpiano 提供了完整的“弹奏 → 录制 → 回放 → 导出 MIDI”的核心闭环：

1. **实时音频线程无锁采集**：在 `AudioEngine` 的音频处理回调中，实时无锁捕获已合并电脑键盘与通道矩阵变换的 pre-render MIDI 消息；
2. **预分配内存与溢出防御**：录制前预先分配大容量事件缓冲，录制期间实时线程**零动态内存分配（零堆分配）**，容量耗尽时以原子计数安全丢弃，不发生崩溃或阻塞；
3. **高保真同链路回放**：回放事件重新注入 `AudioEngine` 的主发声链路（驱动已加载 VST3 插件或内置物理建模钢琴），同时联动虚拟钢琴键盘高亮显示；
4. **标准 MIDI 文件导出（Type 1）**：将录制的不可变 `RecordingTake` 转换为标准 MIDI 文件（960 PPQ），供导入外部宿主（DAW）或打谱软件；
5. **全流程会话编排（`RecordingSessionController`）**：集中管理录制、停止、播放、暂停、重新从头播放与导出状态机互斥。

---

## 2. 核心架构与数据流

### 2.1 录制数据流
```text
电脑键盘按键 ──► KeyboardMidiMapper ──► MidiChannelMapper ──► MidiMessageCollector
                                                                   │
                                                                   ▼
AudioEngine::getNextAudioBlock() (实时音频回调) ◄──────────────────┘
    │
    ├── 1. midiCollector.removeNextBlockOfMessages(midiBuffer, numSamples)
    ├── 2. keyboardState.processNextMidiBuffer(midiBuffer, 0, numSamples, true)
    │
    ├── 3. [录制手传递边界] ──► RecordingEngine::recordMidiBufferBlock()
    │                          ├── 遍历当前 block 的 MidiBuffer
    │                          ├── 将事件转为 PerformanceEvent (绝对采样时间戳)
    │                          └── 存入 pre-allocated std::vector (无扩容分配)
    │
    └── 4. 交付发声 ──► VST3 processBlock() / PianoSynthVoice 模态合成
```

### 2.2 回放数据流
```text
RecordingSessionController::handlePlayClicked() ──► RecordingEngine::startPlayback()
                                                          │
                                                          ▼
AudioEngine::getNextAudioBlock() (实时音频回调)
    │
    ├── RecordingEngine::renderPlaybackBlock(midiBuffer, numSamples)
    │    ├── 扫描当前时间窗口内的待播放 PerformanceEvent
    │    ├── 将事件写入当前 block 的 block-local midiBuffer
    │    └── 触发 keyboardState.processNextMidiBuffer 驱动 UI 虚拟键盘高亮
    │
    └── 送入发声实体 ──► 插件或内置物理建模钢琴即时发声
```

---

## 3. 核心数据模型（`RecordingTake`）

`source/Recording/RecordingEngine.h` 中定义了不可变录制片段：

```cpp
struct PerformanceEvent {
    int64_t timestampSamples = 0;              // 相对录制起点的绝对采样点
    RecordingEventSource source = RecordingEventSource::computerKeyboard;
    juce::MidiMessage message;                 // 标准 JUCE MIDI 消息
};

struct RecordingTake {
    double sampleRate = 0.0;                   // 录制时的采样率
    int64_t lengthSamples = 0;                 // 录制总长度
    std::vector<PerformanceEvent> events;      // 事件序列
};
```

---

## 4. 标准 MIDI 文件导出（`MidiFileExporter`）

`source/Recording/MidiFileExporter.cpp` 将 `RecordingTake` 序列化为标准 `.mid` 文件：
- **格式规范**：标准 MIDI Type 1 文件，时间基准固定为 **960 PPQ**（Pulses Per Quarter Note）；
- **时间转换**：将 `timestampSamples` 转换为精确的 MIDI Tick（默认基准速度 120 BPM）；
- **Track 组织**：
  - Track 0：写入速度（Set Tempo: 500,000 µs/qn 对应 120 BPM）、拍号（Time Signature: 4/4）与音轨名称；
  - Track 1：写入全部 Note On/Off、CC64 延音控制器、Pitch Bend 与 Program Change 序列；
  - 尾部自动写入 `End of Track` Meta 事件。

---

## 5. 专项手工与边界测试清单

| 用例编号 | 测试场景 | 操作步骤与验证目标 | 状态 |
|---|---|---|:---:|
| **REC-001** | 基础录制与回放 | 点击 Record → 弹奏一段旋律 → 点击 Stop → 点击 Play，完整听到刚才弹奏的旋律 | [x] 已通过 |
| **REC-002** | 虚拟键盘回放联动 | 回放录音时，虚拟钢琴键盘准确随着各音符的按下与松开同步高亮与变暗 | [x] 已通过 |
| **REC-003** | 录制中切换预设 | 录制过程中按快捷键切换 Preset，回放时声音与键位在对应时刻自动切调 | [x] 已通过 |
| **REC-004** | 长时间录制与溢出保护 | 连续录制 30 分钟以上，无卡顿、无内存暴涨，停止录制后 Take 完整可用 | [x] 已通过 |
| **REC-005** | MIDI 文件导出与 DAW 验证 | 导出 MIDI 文件并在 Reaper / Cubase / Logic 等外部 DAW 中导入，音符时值与力度完全正确 | [x] 已通过 |
| **REC-006** | 空 Take 导出保护 | 未开始录制时，Export MIDI 与 Export WAV 按钮保持 disabled | [x] 已通过 |
| **REC-007** | 播放中 Stop 防悬挂音 | 播放到包含长延音的片段中途点击 Stop，所有声音立即干净切断，无残留音 | [x] 已通过 |
