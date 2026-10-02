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
4. **标准 MIDI 文件导出（Type 1）**：将完成录制后的 `RecordingTake` 快照转换为标准 MIDI 文件（960 PPQ），供导入外部宿主（DAW）或打谱软件；
5. **全流程会话编排（`RecordingSessionController`）**：集中管理录制、停止、播放、暂停、重新从头播放与导出状态机互斥；支持 1–2 小节节拍器预备拍倒计时（Count-in），倒计时期间若触发其他传输控制或 Take 替换，会安全取消倒计时，避免延迟启动录制覆盖现有 Take；
6. **A-B 跟练时间轴与采样级 Seek**：展示 Take 的绝对时间与总时长，支持点击/拖拽 Seek、A/B 循环标记与跨 16 通道发音完全清理（CC64 Off + CC120 + All Notes Off）。

---

## 2. 核心架构与数据流

### 2.1 录制数据流

```text
电脑键盘按键 ──► KeyboardMidiMapper (律动力度/微扰/手感曲线) ──► MidiChannelMapper ──► MidiMessageCollector
                                                                                   │
                                                                                   ▼
AudioEngine::getNextAudioBlock() (实时音频回调) ◄──────────────────────────────────┘
    │
    ├── 1. midiCollector.removeNextBlockOfMessages(midiBuffer, numSamples)
    ├── 2. keyboardState.processNextMidiBuffer(midiBuffer, 0, numSamples, true)
    │
    ├── 3. [录制事件边界] ──► RecordingEngine::recordMidiBufferBlock()
    │                          ├── 遍历当前 block 的 MidiBuffer
    │                          ├── 将事件转为 PerformanceEvent (绝对采样时间戳)
    │                          └── 存入 pre-allocated std::vector (无扩容分配)
    │
    ├── [预设切换事件] ──────► RecordingEngine::recordPresetChange(presetId, timestamp)
    │
    └── 4. 交付发声 ──► VST3 processBlock() / PianoSynthVoice 模态合成
```
### 2.2 回放数据流

```text
RecordingSessionController::handlePlayClicked() ──► RecordingEngine::startPlaybackAtTakeSample()
                                                          │
                                                          ▼
AudioEngine::getNextAudioBlock() (实时音频回调)
    ├── RecordingEngine::renderPlaybackBlock(playbackVisualMidiBuffer, blockStartSamples, numSamples)
    ├── keyboardState.processNextMidiBuffer(playbackVisualMidiBuffer, 0, numSamples, false)
    │    └── 同步调用 Listener（虚拟键盘高亮；当前 UI 回调线程边界见 known-issues）
    ├── midiBuffer.addEvents(playbackVisualMidiBuffer, 0, numSamples, 0)
    └── 插件或内置物理建模钢琴发声
```

### 2.3 时间轴跳转与 A-B 循环
- `TimelineBar` 使用 Take 采样点作为时间域，显示当前播放时间、总时长与 A/B 标记；点击或拖拽产生 Take-relative Seek；
- **Seek 跨通道清理时序**：Seek 在下一个音频块边界重校准播放位置，并在新位置事件之前对全部 16 个 MIDI 通道依次注入 CC64 Off、CC120 All-Sound-Off 与 CC123 All-Notes-Off 清理事件；暂停或空闲状态下记录 Take-relative 目标采样位置，并请求音频线程在下一块清理当前发声；
- **A/B 标记与循环时序**：A/B 标记以 Take 采样点保存，有效区间为半开区间 `[A, B)`；无效、倒置或空区间不会启用循环；
- **B 边界回跳与防悬挂**：音频回调按采样偏移处理 B 边界。边界 16 通道清理严格先于同采样点的 A 事件；整块恰好结束于 B 时，清理与回跳在下一音频块偏移 0 执行。循环期间自动抑制播放结束标志；
- **极小区间防御**：缩放后的有效区间至少需要覆盖一个当前音频回调块（`playbackBlockSize`）；更短区间保留 A/B 标记但自动旁路循环回跳，避免单块内重复回跳与清理风暴；
- **速度自适应**：播放速度在 0.5x–2.0x 范围内调节时，事件、循环边界与回放位置按当前速度缩放，Take 绝对采样点与 A/B 标记保持不变。
---

## 3. 核心数据模型（`RecordingTake`）

`source/Recording/RecordingEngine.h` 中定义可变的数据结构；完成录制后，控制器使用其 Take 快照进行回放与导出：

```cpp
enum class RecordingEventSource : std::uint8_t { computerKeyboard, realtimeMidiBuffer, playback };
enum class PerformanceEventType : uint8_t { midi = 0, presetChange = 1 };

struct PerformanceEvent {
    std::int64_t timestampSamples = 0;              // 相对录制起点的绝对采样点
    PerformanceEventType type = PerformanceEventType::midi;
    uint8_t presetId = 0;                          // type == presetChange 时记录预设索引
    RecordingEventSource source = RecordingEventSource::computerKeyboard;
    juce::MidiMessage message;                     // type == midi 时记录标准 JUCE MIDI 消息
};

struct RecordingTake {
    double sampleRate = 0.0;                       // 录制时的采样率
    int64_t lengthSamples = 0;                     // 录制总长度
    std::vector<PerformanceEvent> events;          // 事件序列

    [[nodiscard]] bool isEmpty() const noexcept;
    [[nodiscard]] double durationSeconds() const noexcept;
};

```

---

## 4. 标准 MIDI 文件导出（`MidiFileExporter`）

`source/Recording/MidiFileExporter.cpp` 将 `RecordingTake` 序列化为标准 `.mid` 文件：
- **格式规范**：标准 MIDI Type 1 文件，时间基准固定为 **960 PPQ**（Pulses Per Quarter Note）；
- **时间转换**：将 `timestampSamples` 转换为精确的 MIDI Tick（默认基准速度 120 BPM）；
- **数值准入**：采样率/时间戳先检查；PPQ 仅接受 `1..32767`，写出前确认 JUCE `int` tick 与 MIDI 4 字节 VLQ delta 均可表示。非法范围不进入 `roundToInt()` 或打开输出，保留已有目标。
- **已有目标保护**：先写入同目录 `TemporaryFile`，检查写入/flush 并关闭流后再替换。连续导出读取到当前 Take 的音符，不把新 MIDI 追加到旧文件；拒绝或替换失败保留原字节。
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
| **REC-008** | 时间轴点击与拖拽 Seek | 播放中点击或拖动时间轴，验证绝对时间位置随即更新、旧音符被清除，暂停时 Seek 后仍保持精确位置 | [x] 已通过 |
| **REC-009** | 多通道 A-B 跟练循环 | 导入多轨 MIDI，设置 A/B 并跨越 B 点循环，验证所有通道的旧发音被清除且 A 点事件在边界后重启 | [x] 已通过 |
| **REC-010** | 节拍器预备拍倒计时录制 | 开启 1–2 小节预备拍后点击 Record，状态提示显示倒计时并发出节拍提示音，结束后启动录制；倒计时中途点击 Stop 安全取消 | [ ] 待手工验证 |
| **REC-011** | 极小 A-B 循环区间防御 | 将 A-B 区间设为小于单音频块，播放平滑通过该区间，不发生死循环或重复清理风暴 | [ ] 待手工验证 |
