# 录制 / 回放 / MIDI 导出功能说明

> 用途：说明 DevPiano 录制、回放与 MIDI 导出的第一版现代化设计边界与当前 MVP 行为。
> 当前状态：**设计草案已收口；M6-1 模型骨架、M6-2 AudioEngine 最小录制边界、M6-3 最小回放（内部无 UI 入口）、M6-4 最小 UI（Record/Stop/Play 按钮与状态文本）与 M6-5 MIDI 导出已实现**。
> 读者：维护 M6 录制 / 回放 / MIDI 导出功能的开发者。
> 更新时机：录制模型、回放调度、导出格式或实现状态发生变化时。

相关文档：

- 路线图：[`../roadmap/roadmap.md`](../roadmap/roadmap.md)
- 阶段验收：[`../testing/acceptance.md`](../testing/acceptance.md)
- 专项测试：[`../testing/recording-playback.md`](../testing/recording-playback.md)
- 键盘映射：[`keyboard-mapping.md`](keyboard-mapping.md)
- 插件宿主：[`plugin-hosting.md`](plugin-hosting.md)
- 旧代码迁移边界：[`../architecture/legacy-migration.md`](../architecture/legacy-migration.md)

---

## 1. 当前主链路

录制 / 回放应接入当前 JUCE 主链路，而不是复刻旧 `song.*`。

### 1.1 电脑键盘输入

```text
JUCE KeyPress / KeyListener
  -> MainComponent::keyPressed / keyStateChanged
  -> KeyboardMidiMapper
  -> juce::MidiKeyboardState noteOn / noteOff
  -> AudioEngine::getNextAudioBlock
  -> juce::MidiBuffer
  -> PluginHost / fallback synth
  -> AudioDeviceManager
```

关键代码位置：

- `source/MainComponent.*`：装配 `AudioEngine`、`KeyboardMidiMapper`、`MidiRouter`、`PluginHost` 与 UI 面板。
- `source/Input/KeyboardMidiMapper.*`：将电脑键盘事件映射为 MIDI note on/off，并维护 held key 状态。
- `source/Audio/AudioEngine.*`：拥有 `juce::MidiKeyboardState`、`juce::MidiMessageCollector` 和每个 audio block 的 `juce::MidiBuffer`。
- `source/UI/KeyboardPanel.*`：绑定同一个 `MidiKeyboardState` 做虚拟键盘显示，不应成为录制主入口。

### 1.2 外部 MIDI 输入

```text
JUCE MidiInput callback
  -> MidiRouter::handleIncomingMidiMessage
  -> AudioEngine::getMidiCollector()
  -> MidiMessageCollector::removeNextBlockOfMessages
  -> AudioEngine::getNextAudioBlock
  -> juce::MidiBuffer
  -> PluginHost / fallback synth
```

关键代码位置：

- `source/Midi/MidiRouter.*`：打开外部 MIDI 输入，并把消息送入 `MidiMessageCollector`。
- `source/MainComponent.*`：通过 `initialiseMidiRouting()` 将 `MidiRouter` 连接到 `AudioEngine::getMidiCollector()`。
- `source/Audio/AudioEngine.*`：在 audio callback 中把 collector 中的消息取出到 block-local `MidiBuffer`。

### 1.3 发声路径

`AudioEngine::getNextAudioBlock()` 是当前实时发声的核心边界：

1. 从 `MidiMessageCollector` 取出当前 block 的实时 MIDI。
2. 通过 `MidiKeyboardState::processNextMidiBuffer()` 更新键盘状态，并注入 `MidiKeyboardState` 中待处理的键盘事件。
3. 若已加载插件，则将同一个 `MidiBuffer` 送入插件 `processBlock()`。
4. 若没有插件或插件未产生输出，则走内置 fallback synth。
5. 最后应用 master gain 并输出到音频设备。

因此，第一版回放不应绕过 `AudioEngine` 单独发声；应把已录事件重新注入同一条 MIDI/audio 路径。

---

## 2. 旧 FreePiano 行为参考

对应旧源码：`freepiano-src/song.*`、`freepiano-src/export.*`。

旧系统事件模型近似为：

```cpp
struct song_event_t {
    double time;   // 相对于录音开始的时间（秒）
    byte a;        // 消息类型 + 通道
    byte b;        // 数据字节 1
    byte c;        // 数据字节 2
    byte d;        // 数据字节 3
};
```

旧录制覆盖范围很宽：

- MIDI note/controller/program/pitch bend 等消息。
- keymap、label、color、octave、velocity、transpose 等会话状态。
- 播放、录制、停止等控制事件。
- 导出 WAV / MP4 的离线渲染路径。

旧设计的主要问题：

- **会话快照耦合过强**：回放可能覆盖用户当前布局和设置。
- **裸字节事件不可维护**：不利于与标准 MIDI 文件和 JUCE 类型衔接。
- **固定事件缓冲区**：存在硬上限，不适合现代实现。
- **时间来源混杂**：录制、回放、暂停、导出边界不够清晰。
- **平台与旧 UI 耦合**：不适合直接迁移到当前 JUCE 架构。

结论：旧代码只作为行为参考；新实现使用 JUCE 类型和项目内现代数据模型重建。

---

## 3. 第一版目标

第一版目标是恢复“演奏录制与回放”的最小闭环：

- 可开始录制。
- 可停止录制。
- 可回放录制结果。
- 录制内容包含电脑键盘和外部 MIDI 输入产生的演奏事件。
- 回放时事件按录制时间顺序进入当前发声链路。
- 已加载插件时驱动插件；未加载插件时驱动 fallback synth。
- 优先支持 MIDI 文件导出；WAV 离线渲染后置。

第一版明确不做：

- 不做音频直接录音。
- 不录制插件参数自动化。
- 不录制插件二进制状态或音色文件。
- 不做复杂编辑能力，如裁剪、拼接、量化、钢琴卷帘编辑。
- 不做 tempo map、loop、metronome、同步外部时钟。
- 不恢复旧 MP4 / 视频导出。

---

## 4. 数据模型草案

### 4.1 演奏事件

第一版内部事件以标准 MIDI 消息为核心，附带项目需要的来源和时间线信息：

```cpp
enum class RecordingEventSource {
    computerKeyboard,
    externalMidi,
    realtimeMidiBuffer,
    playback
};

struct PerformanceEvent {
    int64_t timestampSamples = 0;
    RecordingEventSource source = RecordingEventSource::computerKeyboard;
    juce::MidiMessage message;
};
```

设计原则：

- `timestampSamples` 使用录制开始后的绝对 sample 位置，便于在 audio block 中转换为 sample offset；这是录制事件唯一权威时间线。
- `juce::MidiMessage` 保存标准 MIDI 语义，避免旧 `SM_*` 裸字节模型；其内部 timestamp 不作为录制时间线使用。
- `source` 用于区分电脑键盘、外部 MIDI、已合并的实时 `MidiBuffer` 与回放事件，便于后续过滤、调试和测试。
- 第一版不强制引入多轨；所有事件在同一条时间线上。

### 4.2 录制片段

```cpp
struct RecordingTake {
    double sampleRate = 0.0;
    int64_t lengthSamples = 0;
    std::vector<PerformanceEvent> events;
};
```

设计原则：

- `RecordingTake` 是一次录制结果，不直接等同于工程文件或歌曲工程。
- `sampleRate` 记录录制时的音频采样率；回放时若设备采样率变化，需要按时间比例换算。
- 事件列表只在非 audio thread 安全边界外做结构性修改；audio callback 中避免分配、排序或持久化。

### 4.3 与 JUCE 类型的关系

- `juce::MidiMessage`：单个事件载体。
- `juce::MidiBuffer`：audio callback 中的 block-local 消息队列，负责 sample offset 调度。
- `juce::MidiMessageSequence`：适合后续编辑和 MIDI 文件导出，可由 `RecordingTake` 转换生成。
- `juce::MidiFile`：第一版 MIDI 导出的目标格式。

---

## 5. 录制设计

第一版应优先在 `AudioEngine` 附近建立录制边界，因为这里可以同时看到：

- 当前 audio block 的实时 MIDI 输入。
- 当前 block size 和 sample rate。
- 插件 / fallback synth 使用的实际 `MidiBuffer`。

建议新增独立组件，而不是继续膨胀 `MainComponent`：

```text
source/Recording/RecordingEngine.h
source/Recording/RecordingEngine.cpp
```

建议职责：

- 管理录制状态：idle / recording / playing / stopped。
- 接收 audio block 中的 MIDI 事件快照。
- 把事件转换为相对录制开始的 `timestampSamples`。
- 在停止录制时产出不可变或可复制的 `RecordingTake`。
- 暴露轻量状态给 UI，例如是否正在录制、是否有可回放片段、录制时长。

### 5.1 M6-2 AudioEngine 边界

当前最安全的 M6-2 观察点位于 `AudioEngine::getNextAudioBlock()`：

```text
midiBuffer.clear()
midiCollector.removeNextBlockOfMessages(midiBuffer, numSamples)
keyboardState.processNextMidiBuffer(midiBuffer, 0, numSamples, true)
// M6-2 recording handoff boundary lives here
plugin processBlock(...) or fallback synth.renderNextBlock(...)
```

选择这个边界的原因：

- `midiCollector.removeNextBlockOfMessages()` 之后，`midiBuffer` 已包含本 block 的外部 MIDI 输入。
- `keyboardState.processNextMidiBuffer(..., true)` 之后，`MidiKeyboardState` 中待处理的电脑键盘 note on/off 也会通过同一个 block-local `MidiBuffer` 进入后续发声路径。
- 插件 `processBlock()` 可能读取或修改 `midiBuffer`，因此录制必须发生在插件 / fallback synth 消费之前。
- 该边界能在不新增第二套 MIDI 监听链路的前提下，记录“准备交给插件 / fallback synth 的 pre-render `MidiBuffer`”。

此边界的限制：

- 经过 `MidiKeyboardState` 合并后的 `MidiBuffer` 不再携带原始来源标签；第一版应将该路径记录为 `RecordingEventSource::realtimeMidiBuffer`。
- 如果未来需要严格区分 `computerKeyboard` 与 `externalMidi`，应再增加 source tagging 机制，而不是在混合后的 `MidiBuffer` 中猜测来源。
- `RecordingEngine::recordMidiBufferBlock()` 会把符合第一版大小门限的 `MidiMessageMetadata` 转为拥有数据的 `juce::MidiMessage`，并写入 `std::vector<PerformanceEvent>`；容量耗尽或超过大小门限的消息会被丢弃并计数。
- `recordMidiBufferBlock()` 会把复制出的 `juce::MidiMessage` timestamp 归零；后续回放 / 导出只能使用 `PerformanceEvent::timestampSamples` 作为权威时间线。

M6-2 推荐调用语义：

```text
const auto blockStart = recordingEngine.getCurrentPositionSamples()
recordingEngine.recordMidiBufferBlock(midiBuffer,
                                      RecordingEventSource::realtimeMidiBuffer,
                                      blockStart)
recordingEngine.advanceRecordingPosition(numSamples)
```

当前 `AudioEngine` 已提供 `setRecordingEngine(...)`，并在该边界只于 `RecordingEngine::isRecording()` 为 true 时执行上述调用；录制实例创建、启动/停止和 UI 控制仍属于后续切片。

### 5.2 Owner / lifetime / detach 规则

第一版采用单 owner + 非拥有音频指针：

- `MainComponent` 持有 `RecordingEngine` 实例。
- `AudioEngine` 只保存 `RecordingEngine*` 非拥有指针，和当前 `PluginHost*` 注入模式一致。
- `MainComponent` 构造阶段、音频设备启动前调用 `audioEngine.setRecordingEngine(&recordingEngine)`。
- `MainComponent` 析构阶段先调用 `shutdownAudio()` 停止 audio callback，再调用 `audioEngine.setRecordingEngine(nullptr)` detach，避免 audio callback 与 detach 并发。
- `RecordingEngine` 必须声明在 `AudioEngine` 之前，使对象销毁顺序为 `AudioEngine` 先销毁、`RecordingEngine` 后销毁，避免 `AudioEngine` 析构期间悬挂引用。

后续如果要在运行中替换 recorder，必须先停止 audio callback 或引入明确同步边界；当前不支持运行中热替换 recorder。

### 5.3 Preallocation / overflow 规则

第一版采用“预分配 + 容量耗尽时丢弃新事件并计数”的策略：

- 录制开始前必须调用 `RecordingEngine::reserveEvents(expectedEventCount)`。
- `startRecording(sampleRate)` 会清空事件但保留已分配容量。
- `recordEvent()` / `recordMidiBufferBlock()` 不允许触发 `std::vector` 自动扩容；当 `events.size() >= events.capacity()` 时，当前事件会被丢弃并递增 dropped event 计数。
- `recordMidiBufferBlock()` 会在 `MidiMessageMetadata::getMessage()` 之前检查容量与消息大小，避免容量已满或大消息路径在 audio callback 中仍 materialize `juce::MidiMessage`。
- 容量耗尽时继续推进 `lengthSamples`，避免录制时间线停滞。
- `hasDroppedEvents()` / `getDroppedEventCount()` 用于停止录制后向 UI 或日志报告丢失事件；不要在 audio callback 中做 UI 提示。
- 第一版不录制大型 sysex；超过第一版实时录制大小门限的消息会被丢弃并计数，后续如需 sysex，应增加专门的非实时路径。

默认容量建议：

- 最小默认：`100 events/sec * 30 min = 180000 events`。
- 如果后续增加录制时长设置，可用 `expectedSeconds * 100` 作为普通演奏默认估算。
- 高密度 CC / pitch bend 场景应提高估算或给出“可能丢事件”提示。

`recordMidiBufferBlock()` 内部根据每个 `MidiBuffer` event 的 block-local `samplePosition` 生成绝对时间戳：

```text
timestampSamples = blockStartSamples + event.samplePosition
```

### 5.4 录制时序草案

当前已提供无 UI 的内部控制入口，用于后续手工触发或测试接入：

- `MainComponent::startInternalRecording(expectedEventCapacity)`
- `MainComponent::stopInternalRecording()`

这两个入口复用现有 audio device rebuild guard：先暂停 audio callback，再执行 `reserveEvents()` / `startRecording()` 或 `stopRecording()`，最后恢复音频设备。这样可以在没有 UI 按钮和跨线程控制模型之前，避免 message thread 与 audio callback 同时读写 `RecordingEngine`。

录制时序草案：

```text
Internal trigger starts recording
  -> RecordingEngine::reserveEvents(expectedEventCount)
  -> RecordingEngine::startRecording(sampleRate)
AudioEngine::getNextAudioBlock
  -> collect live MidiBuffer for this block
  -> keyboardState.processNextMidiBuffer(..., true)
  -> RecordingEngine::recordMidiBufferBlock(midiBuffer, realtimeMidiBuffer, blockStartSample)
  -> RecordingEngine::advanceRecordingPosition(numSamples)
Internal trigger stops recording
  -> RecordingEngine::stopRecording()
  -> RecordingTake available
```

注意事项：

- 录制层应拷贝必要 MIDI 事件，不持有 block-local `MidiBuffer` 引用。
- 不在 audio callback 中写文件、弹窗或更新 UI。
- 若需要跨线程传递状态，必须先定义单一 owner、原子状态或非实时线程 drain 策略；不要让 UI 线程和 audio thread 同时读写 `RecordingTake::events`。
- `recordMidiBufferBlock()` 是第一版边界辅助 API，不等同于完整实时安全队列；audio-thread 集成前必须明确预分配容量、容量耗尽行为和大 MIDI 消息策略。
- 当前容量耗尽行为是丢弃新事件并计数；不得在 audio callback 中扩容、弹窗或写文件。
- 停止录制时应补发/记录必要 note off 或在回放停止时执行 all-notes-off，避免悬挂音。

---

## 6. 回放设计

回放目标是把 `RecordingTake` 中到期事件重新注入 `AudioEngine` 的当前 block `MidiBuffer`。

已实现的无 UI 内部控制入口：

- `MainComponent::startInternalPlayback(const RecordingTake& take)`
- `MainComponent::stopInternalPlayback()` — 返回 `RecordingTake` 快照并发送 all-notes-off

内部时序：

```text
Internal trigger starts playback
  -> RecordingEngine::startPlayback(take, currentSampleRate)
AudioEngine::getNextAudioBlock
  -> collect live MidiBuffer
  -> RecordingEngine::renderPlaybackBlock(midiBuffer, blockStartSamples, numSamples)
  -> RecordingEngine::advancePlaybackPosition(numSamples)
  -> AudioEngine continues existing plugin / synth path
Internal trigger stops playback
  -> RecordingEngine::stopPlayback()
  -> allNotesOff() to clear hanging notes
  -> RecordingTake returned
```

关键规则：

- `renderPlaybackBlock()` 在 `AudioEngine::getNextAudioBlock()` 中于 `recordRealtimeMidiBufferIfNeeded()` 之后、插件 / synth 渲染之前被调用。
- `playbackSampleRateRatio = currentSampleRate / take.sampleRate` 用于采样率差异换算。
- `sampleOffset = eventTimestampScaled - blockStartSamples`，被 clamp 到 `[0, numSamples-1]`。
- 回放事件以相同 `MidiBuffer` 路径进入插件 / fallback synth，与实时 MIDI 合并。
- 回放不重新走 `KeyboardMidiMapper`，避免当前布局变化影响历史录制。
- 停止回放时发送 `allNotesOff(1)` 清理悬挂音。

注意事项：

- 回放事件按录制时间顺序线性扫描，不做 binary search 优化（第一版事件量 ≤ 180k）。
- `renderPlaybackBlock()` 是第一版边界辅助 API，后续可升级为索引驱动的 O(log n) 查找。
- 若 `take.sampleRate == 0` 或 `currentSampleRate == 0`，`playbackSampleRateRatio` 默认为 1.0。

---

## 7. UI 边界

第一版 UI 可以非常小：

- `Record`：开始录制。
- `Stop`：停止录制或停止回放。
- `Play`：回放最近一次录制。
- `Export MIDI`：导出最近一次录制为 `.mid`。
- 状态文本：Idle / Recording / Playing / Has Take / No Take。

建议优先放在 `ControlsPanel` 附近，但状态装配不要全部塞回 `MainComponent`：

- UI 面板只表达按钮和状态。
- `MainComponent` 只负责把 UI action 转发给录制控制接口。
- 录制状态应通过新的轻量状态结构进入 `AppState` / builder 边界，避免 UI 直接读取复杂录制对象。

---

## 8. 导出设计

### 8.1 MIDI 导出（第一优先级）

MIDI 导出建议使用 `juce::MidiFile`：

```text
RecordingTake
  -> juce::MidiMessageSequence
  -> juce::MidiFile
  -> .mid
```

第一版可采用固定 PPQ 或基于秒转换的简单 tick 方案；如果后续引入 tempo map，再升级导出模型。

导出范围：

- note on / note off。
- 外部 MIDI 输入中的标准 channel voice 消息，如 CC、pitch bend、program change，可按实现成本逐步纳入。
- 不导出插件状态、音频、布局文件或 UI 状态。

### 8.2 WAV 导出（后续）

WAV 导出需要离线渲染能力，风险高于 MIDI 导出：

- 需要构建离线 audio render loop。
- 需要处理插件是否支持离线 / 非实时渲染。
- 需要明确使用当前插件状态还是导出前冻结状态。
- 需要处理导出期间 UI 线程、音频设备和插件 editor 的关系。

因此 WAV 导出不应与第一版录制 / 回放混在同一最小切片中完成。

---

## 9. 线程与实时安全约束

实现时必须遵守：

- audio callback 中不做文件 IO。
- audio callback 中不弹窗、不访问 UI、不等待锁。
- audio callback 中尽量避免动态分配；如必须复制事件，需预分配或使用受控缓冲。
- 非 audio thread 不直接修改正在回放读取的事件数组。
- `MidiMessage` 的时间戳是应用定义的 double；进入 `MidiBuffer` 后以整数 sample offset 为准。
- 外部 MIDI、电脑键盘 UI 输入和 audio callback 时间源不同，第一版只承诺回放调度与 audio block 对齐，不承诺 DAW 级录入量化或精确输入延迟校正。

---

## 10. 实现切片建议

### M6-1：录制模型与状态

- [x] 新增 `source/Recording/RecordingEngine.*`。
- [x] 定义 `PerformanceEvent`、`RecordingTake` 和状态枚举。
- [x] 提供 start / stop / clear / hasTake / getSnapshot 等基础接口。
- [x] 明确当前 `RecordingEngine` 是单线程模型骨架，后续 audio-thread 接入前必须补齐同步和预分配策略。

### M6-2：接入实时 MIDI 录制

- [x] 明确 `AudioEngine::getNextAudioBlock()` 中的 M6-2 handoff 边界：`keyboardState.processNextMidiBuffer(..., true)` 之后、插件 / fallback synth 消费 `MidiBuffer` 之前。
- [x] 增加 `RecordingEngine::recordMidiBufferBlock()` 边界辅助 API，用 block-local sample offset 生成绝对 `timestampSamples`。
- [x] 明确混合后的 block MIDI 第一版记录为 `RecordingEventSource::realtimeMidiBuffer`，不猜测原始来源。
- [x] `AudioEngine` 已提供 `setRecordingEngine(...)`，并在录制状态下调用该边界。
- [x] 建立 owner / detach / preallocation / overflow 规则：`MainComponent` 持有，`AudioEngine` 非拥有引用，容量耗尽时丢弃新事件并计数。
- [x] 增加无 UI 的内部 start/stop helper，用于后续手工触发或测试接入录制时间线。
- [x] 验证电脑键盘能进入录制时间线。
- [~] 外部 MIDI 进入同一录制时间线仍待真实硬件或虚拟 MIDI loopback 验证。

### M6-3：最小回放

- [x] `RecordingEngine` 新增 `startPlayback(take, currentSampleRate)`、`stopPlayback()`、`renderPlaybackBlock()`、`advancePlaybackPosition()`、`isPlaying()`、`getPlaybackPositionSamples()`。
- [x] `AudioEngine::getNextAudioBlock()` 在 `recordRealtimeMidiBufferIfNeeded()` 之后、插件 / synth 渲染之前调用 `renderPlaybackEventsIfNeeded()`。
- [x] `MainComponent` 新增 `startInternalPlayback(take)` / `stopInternalPlayback()` 内部入口，stop 时发送 `allNotesOff(1)`。
- [x] 回放事件通过采样率比例换算，与实时 MIDI 共享同一 `MidiBuffer` 路径。
- [x] 已完成回放顺序与相对时间手工回归。
- [x] 已完成已加载插件 / fallback synth 回放路径手工回归。
- [x] 已完成停止回放不留下悬挂音的基础手工回归。
- [~] 采样率变化、长时间录制、容量耗尽和回放结束通知仍作为下一阶段稳定化重点。

### M6-4：最小 UI

- [x] `ControlsPanel` 新增 `recordStatusLabel`（显示 Idle / Recording / Playing）、`recordButton`（"Record"）、`playButton`（"Play"）、`stopButton`（"Stop"）。
- [x] `setRecordingState(RecordingState)` 根据状态控制按钮 enabled / disabled：Idle 时 Record 可用、Play 检查 hasTake；Recording 时 Record 不可用、Stop 可用；Playing 时 Record/Play 不可用、Stop 可用。
- [x] `MainComponent` 新增 `handleRecordClicked()` / `handlePlayClicked()` / `handleStopClicked()` 处理录音/回放/停止逻辑。
- [x] `currentTake` 存储最近一次录制的 `RecordingTake`，用于回放。

### M6-5：MIDI 导出

- [x] `source/Recording/MidiFileExporter.h/.cpp` 新增 `exportTakeAsMidiFile(take, destination, ppq)`。
- [x] 转换链路：`RecordingTake.events[]` → `juce::MidiMessageSequence`（`setTimeStamp(seconds)` 用秒级时间戳）→ `juce::MidiFile`（`setTicksPerQuarterNote(ppq)`） → `writeTo(FileOutputStream)` 输出 `.mid`。
- [x] 默认 960 PPQ，无 tempo map，秒级时间直接作为时间戳存入 `MidiMessage`。
- [x] `ControlsPanel` 新增 `exportMidiButton { "Export MIDI" }`，`setHasTake()` 时同步 `exportMidiButton.setEnabled(hasTake)`。
- [x] `MainComponent::handleExportMidiClicked()` 使用 `juce::FileChooser::launchAsync(saveMode)` 保存文件，文件名格式 `recording_<ISO8601>.mid`。
- [x] 导出成功 / 失败写入 Logger 日志。

### M6-6：WAV 离线渲染预研 / 实现

- [ ] 单独设计离线渲染链路。
- [ ] 第一切片优先支持 fallback synth 离线渲染。
- [ ] 第二切片再评估已加载 VST3 插件的离线渲染。
- [ ] 明确导出期间 UI、实时音频设备、插件 editor 和当前插件状态的边界。
- [ ] 明确失败策略：插件不支持离线渲染、目标路径无权限、空 take、用户取消保存。
- [x] 不阻塞 M6-1 到 M6-5 的上线。

### M6-7：录制 / 回放稳定化

- [x] 复核 audio callback 中访问 `RecordingEngine` 的边界，避免文件 IO、UI 操作、阻塞等待或非受控分配进入实时路径。
- [x] 将回放结束通知收敛为轻量状态传递；audio thread 只置位轻量结束标记，message thread 轮询后更新 UI / 状态。
- [x] 复核采样率变化时的回放时间缩放与播放结束判断，结束阈值改为按当前回放采样率比例缩放后的 take 长度。
- [x] 更新 `RecordingEngine.h` 中关于 “Minimal skeleton / not thread-safe” 的注释，使其准确描述当前 MVP 状态和剩余约束。

---

## 11. 验收关注点

后续实现完成后，应新增专项测试文档覆盖：

- 开始录制 / 停止录制。
- 电脑键盘 note on/off 成对录入。
- 外部 MIDI 输入录入（有硬件后验证）。
- 回放顺序与相对时间正确。
- 回放驱动已加载插件。
- 无插件时回放驱动 fallback synth。
- 停止回放不会留下悬挂音。
- 设备 sample rate / buffer size 变化后的基本行为。
- MIDI 导出文件可被 DAW 或 MIDI 工具打开。

---

## 12. 已确认的设计决策

1. **录制内容**：录制演奏 MIDI 事件，不录制音频波形。
2. **事件格式**：内部使用 `juce::MidiMessage` + 项目自有时间线元数据，不沿用旧 `SM_*` 裸字节模型。
3. **时间线**：内部优先使用 sample-based timeline，进入 `MidiBuffer` 时转换为 block-local sample offset。
4. **回放路径**：回放事件重新进入 `AudioEngine` 的现有 MIDI/audio 链路，不单独建立第二套发声链路。
5. **插件关系**：录制内容不包含插件音色或插件状态；回放使用用户当前加载的插件或 fallback synth。
6. **多轨能力**：第一版不做多轨，所有事件在同一时间线上。
7. **编辑能力**：第一版不做事件编辑、量化、裁剪或拼接。
8. **导出优先级**：优先 MIDI 文件导出；WAV 离线渲染后续独立推进；不恢复旧 MP4 导出。
