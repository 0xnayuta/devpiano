# 录制 / 回放 / MIDI 导出功能说明

> 用途：说明 DevPiano 录制、回放与 MIDI 导出的第一版现代化设计边界与当前 MVP 行为。
> 当前状态：设计草案已收口；Phase 3-3 模型骨架、Phase 3-4 AudioEngine 最小录制边界、Phase 3-5 最小回放（内部无 UI 入口）、Phase 3-6 最小 UI（Record/Stop/Play 按钮与状态文本）与 Phase 3-7 MIDI 导出已实现。包含功能说明与专项测试
> 更新时机：录制模型、回放调度、导出格式或实现状态发生变化时。

相关文档：

- 路线图：[`../roadmap/roadmap.md`](../roadmap/roadmap.md)
- 阶段验收：[`../acceptance.md`](../acceptance.md)
- 专项测试：[`./recording-playback.md`](./recording-playback.md)
- 键盘映射：[`./keyboard-mapping.md`](./keyboard-mapping.md)
- 插件宿主：[`./plugin-hosting.md`](./plugin-hosting.md)
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

- `source/MainComponent.*`：装配 `AudioEngine`、`KeyboardMidiMapper`、`PluginHost` 与 UI 面板。
- `source/Input/KeyboardMidiMapper.*`：将电脑键盘事件映射为 MIDI note on/off，并维护 held key 状态。
- `source/Audio/AudioEngine.*`：拥有 `juce::MidiKeyboardState`、`juce::MidiMessageCollector` 和每个 audio block 的 `juce::MidiBuffer`。
- `source/UI/KeyboardPanel.*`：绑定同一个 `MidiKeyboardState` 做虚拟键盘显示，不应成为录制主入口。

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
- 录制内容包含电脑键盘产生的演奏事件。
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
- `source` 用于区分电脑键盘、已合并的实时 `MidiBuffer` 与回放事件，便于后续过滤、调试和测试。
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

### 5.1 Phase 3-4 AudioEngine 边界

当前最安全的 Phase 3-4 观察点位于 `AudioEngine::getNextAudioBlock()`：

```text
midiBuffer.clear()
midiCollector.removeNextBlockOfMessages(midiBuffer, numSamples)
keyboardState.processNextMidiBuffer(midiBuffer, 0, numSamples, true)
// Phase 3-4 recording handoff boundary lives here
plugin processBlock(...) or fallback synth.renderNextBlock(...)
```

选择这个边界的原因：

- `midiCollector.removeNextBlockOfMessages()` 之后，`midiBuffer` 已包含本 block 的实时 MIDI 输入。
- `keyboardState.processNextMidiBuffer(..., true)` 之后，`MidiKeyboardState` 中待处理的电脑键盘 note on/off 也会通过同一个 block-local `MidiBuffer` 进入后续发声路径。
- 插件 `processBlock()` 可能读取或修改 `midiBuffer`，因此录制必须发生在插件 / fallback synth 消费之前。
- 该边界能在不新增第二套 MIDI 监听链路的前提下，记录“准备交给插件 / fallback synth 的 pre-render `MidiBuffer`”。

此边界的限制：

- 经过 `MidiKeyboardState` 合并后的 `MidiBuffer` 不再携带原始来源标签；第一版应将该路径记录为 `RecordingEventSource::realtimeMidiBuffer`。
- `RecordingEngine::recordMidiBufferBlock()` 会把符合第一版大小门限的 `MidiMessageMetadata` 转为拥有数据的 `juce::MidiMessage`，并写入 `std::vector<PerformanceEvent>`；容量耗尽或超过大小门限的消息会被丢弃并计数。
- `recordMidiBufferBlock()` 会把复制出的 `juce::MidiMessage` timestamp 归零；后续回放 / 导出只能使用 `PerformanceEvent::timestampSamples` 作为权威时间线。

Phase 3-4 推荐调用语义：

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
- 不导出插件状态、音频、布局文件或 UI 状态。

### 8.2 WAV 导出（Phase 3-1 MVP）

WAV 导出需要离线渲染能力，风险高于 MIDI 导出。当前阶段只设计并实现最小闭环，不恢复旧 FreePiano 的 MP4 / 视频导出，也不引入工程文件、多轨、tempo map 或复杂编辑模型。

#### MVP 目标

第一版 WAV 导出只覆盖：

- 输入：内存中的当前 `RecordingTake`。
- 输出：一个标准 `.wav` 文件。
- 渲染源：项目内 fallback synth。
- 声道：stereo。
- 采样率：优先使用当前音频设备采样率；若当前设备采样率不可用，则使用 `RecordingTake::sampleRate`；仍不可用时回退到 `44100 Hz`。
- 参数：应用当前 master gain / ADSR 设置，使导出结果与无插件时的实时回放尽量一致。
- 文件写入：使用 JUCE `WavAudioFormat` / `AudioFormatWriter`。

第一版明确不做：

- 不导出当前已加载 VST3 插件的声音。
- 不冻结、保存或恢复插件状态。
- 不依赖实时 `AudioDeviceManager` 输出设备。
- 不在 audio callback 中做导出或文件 IO。
- 不导出 MP4 / 视频。
- 不做 tempo map、metronome、loop、多轨、钢琴卷帘或事件编辑。

#### 建议模块边界

建议新增独立导出模块，避免把离线渲染流程堆入 `MainComponent`：

```text
source/Recording/WavFileExporter.h/.cpp
```

建议第一版 API 形态：

```cpp
struct WavExportOptions
{
    double sampleRate = 44100.0;
    int numChannels = 2;
    int blockSize = 512;
    float masterGain = 0.8f;
    juce::ADSR::Parameters adsr;
};

bool exportTakeAsWavFile(const RecordingTake& take,
                         const juce::File& destination,
                         const WavExportOptions& options);
```

当前 Phase 3-1a 已建立该 API 与最小 writer 创建边界；后续 Phase 3-1b 再接入真实 fallback synth 离线渲染。

`MainComponent` 的职责只应是：

- 判断是否有有效 take。
- 收集当前导出选项（采样率、ADSR、master gain）。
- 打开保存文件对话框。
- 调用导出函数并显示 / 记录结果。

离线渲染、MIDI 事件调度、音频 buffer 写入和失败处理应留在 `WavFileExporter` 内部。

#### 离线渲染链路

建议 MVP 渲染链路：

```text
RecordingTake
  -> 按目标 sampleRate 缩放 PerformanceEvent::timestampSamples
  -> 分 block 填充 juce::MidiBuffer
  -> fallback synth renderNextBlock
  -> apply master gain
  -> juce::AudioFormatWriter 写入 WAV
```

关键规则：

- `PerformanceEvent::timestampSamples` 仍是录制时间线的唯一来源。
- 若导出采样率与 take 采样率不同，使用 `targetSampleRate / take.sampleRate` 缩放事件时间和总长度。
- 导出总长度应至少覆盖 `RecordingTake::lengthSamples` 缩放后的长度，并额外保留一个短尾音窗口（例如 1-2 秒）让 ADSR release 自然结束。
- 每个 block 内只把落入 `[blockStart, blockEnd)` 的事件加入 `MidiBuffer`。
- 导出结束前追加 all-notes-off / all-sound-off 兜底，避免尾部悬挂音。
- 第一版可以线性扫描事件；若长 take 导出性能不足，再升级为事件游标。

#### 失败与边界策略

导出函数应以 `bool` 或后续 `Result` 返回失败结果，至少覆盖：

- 空 take：不导出。
- 目标文件无法创建或无写权限：失败，不覆盖原文件状态不明的路径。
- 采样率 / 声道 / block size 非法：使用安全默认值或失败。
- 写入中途失败：关闭 writer 并返回失败。
- 用户取消保存：由 UI 层处理，不调用导出函数。

#### 插件离线渲染后置

VST3 插件离线渲染放到 Phase 3-2 再评估，原因：

- 需要明确插件实例是否复用当前实时实例，还是创建独立离线实例。
- 需要处理插件 editor 打开、设备重建、插件状态冻结和线程边界。
- 部分插件可能不支持预期的非实时渲染行为。
- 失败策略和用户提示比 fallback synth 路径复杂。

因此 Phase 3-1 只要求 fallback synth WAV 导出可用；插件 WAV 导出不得阻塞该 MVP。

---

## 9. 线程与实时安全约束

实现时必须遵守：

- audio callback 中不做文件 IO。
- audio callback 中不弹窗、不访问 UI、不等待锁。
- audio callback 中尽量避免动态分配；如必须复制事件，需预分配或使用受控缓冲。
- 非 audio thread 不直接修改正在回放读取的事件数组。
- `MidiMessage` 的时间戳是应用定义的 double；进入 `MidiBuffer` 后以整数 sample offset 为准。
- 电脑键盘 UI 输入和 audio callback 时间源不同，第一版只承诺回放调度与 audio block 对齐，不承诺 DAW 级录入量化或精确输入延迟校正。

---

## 10. 实现切片建议

### Phase 3-3：录制模型与状态

- [x] 新增 `source/Recording/RecordingEngine.*`。
- [x] 定义 `PerformanceEvent`、`RecordingTake` 和状态枚举。
- [x] 提供 start / stop / clear / hasTake / getSnapshot 等基础接口。
- [x] 明确当前 `RecordingEngine` 是单线程模型骨架，后续 audio-thread 接入前必须补齐同步和预分配策略。

### Phase 3-4：接入实时 MIDI 录制

- [x] 明确 `AudioEngine::getNextAudioBlock()` 中的 Phase 3-4 handoff 边界：`keyboardState.processNextMidiBuffer(..., true)` 之后、插件 / fallback synth 消费 `MidiBuffer` 之前。
- [x] 增加 `RecordingEngine::recordMidiBufferBlock()` 边界辅助 API，用 block-local sample offset 生成绝对 `timestampSamples`。
- [x] 明确混合后的 block MIDI 第一版记录为 `RecordingEventSource::realtimeMidiBuffer`，不猜测原始来源。
- [x] `AudioEngine` 已提供 `setRecordingEngine(...)`，并在录制状态下调用该边界。
- [x] 建立 owner / detach / preallocation / overflow 规则：`MainComponent` 持有，`AudioEngine` 非拥有引用，容量耗尽时丢弃新事件并计数。
- [x] 增加无 UI 的内部 start/stop helper，用于后续手工触发或测试接入录制时间线。
- [x] 验证电脑键盘能进入录制时间线。

### Phase 3-5：最小回放

- [x] `RecordingEngine` 新增 `startPlayback(take, currentSampleRate)`、`stopPlayback()`、`renderPlaybackBlock()`、`advancePlaybackPosition()`、`isPlaying()`、`getPlaybackPositionSamples()`。
- [x] `AudioEngine::getNextAudioBlock()` 在 `recordRealtimeMidiBufferIfNeeded()` 之后、插件 / synth 渲染之前调用 `renderPlaybackEventsIfNeeded()`。
- [x] `MainComponent` 新增 `startInternalPlayback(take)` / `stopInternalPlayback()` 内部入口，stop 时发送 `allNotesOff(1)`。
- [x] 回放事件通过采样率比例换算，与实时 MIDI 共享同一 `MidiBuffer` 路径。
- [x] 已完成回放顺序与相对时间手工回归。
- [x] 已完成已加载插件 / fallback synth 回放路径手工回归。
- [x] 已完成停止回放不留下悬挂音的基础手工回归。
- [~] 采样率变化、长时间录制、容量耗尽和回放结束通知仍作为下一阶段稳定化重点。

### Phase 3-6：最小 UI

- [x] `ControlsPanel` 新增 `recordStatusLabel`（显示 Idle / Recording / Playing）、`recordButton`（"Record"）、`playButton`（"Play"）、`stopButton`（"Stop"）。
- [x] `setRecordingState(RecordingState)` 根据状态控制按钮 enabled / disabled：Idle 时 Record 可用、Play 检查 hasTake；Recording 时 Record 不可用、Stop 可用；Playing 时 Record/Play 不可用、Stop 可用。
- [x] `MainComponent` 新增 `handleRecordClicked()` / `handlePlayClicked()` / `handleStopClicked()` 处理录音/回放/停止逻辑。
- [x] `currentTake` 存储最近一次录制的 `RecordingTake`，用于回放。

### Phase 3-7：MIDI 导出

- [x] `source/Recording/MidiFileExporter.h/.cpp` 新增 `exportTakeAsMidiFile(take, destination, ppq)`。
- [x] 转换链路：`RecordingTake.events[]` → `juce::MidiMessageSequence`（`setTimeStamp(seconds)` 用秒级时间戳）→ `juce::MidiFile`（`setTicksPerQuarterNote(ppq)`） → `writeTo(FileOutputStream)` 输出 `.mid`。
- [x] 默认 960 PPQ，无 tempo map，秒级时间直接作为时间戳存入 `MidiMessage`。
- [x] `ControlsPanel` 新增 `exportMidiButton { "Export MIDI" }`，`setHasTake()` 时同步 `exportMidiButton.setEnabled(hasTake)`。
- [x] `MainComponent::handleExportMidiClicked()` 使用 `juce::FileChooser::launchAsync(saveMode)` 保存文件，文件名格式 `recording_<ISO8601>.mid`。
- [x] 导出成功 / 失败写入 Logger 日志。

### Phase 3-1：WAV 离线渲染预研 / 实现

- [x] 单独设计离线渲染链路。
- [x] 第一切片优先支持 fallback synth 离线渲染。
- [x] 第二切片再评估已加载 VST3 插件的离线渲染。
- [x] 明确导出期间 UI、实时音频设备、插件 editor 和当前插件状态的边界。
- [x] 明确失败策略：插件不支持离线渲染、目标路径无权限、空 take、用户取消保存。
- [x] 不阻塞 Phase 3-3 到 Phase 3-7 的上线。

建议实现切片：

1. **Phase 3-1a：导出模型与空实现边界**
   - [x] 新增 `source/Recording/WavFileExporter.h/.cpp`。
   - [x] 定义 `WavExportOptions` 和 `exportTakeAsWavFile(...)`。
   - [x] 暂只做参数校验、文件 writer 创建和失败返回，不接 UI。

2. **Phase 3-1b：fallback synth 离线渲染核心**
   - [x] 构建与实时 fallback synth 等价的离线 `juce::Synthesiser`。
   - [x] 按 block 将 `RecordingTake` 事件写入 `MidiBuffer`。
   - [x] 渲染到 `AudioBuffer<float>` 并写入 WAV。
   - [x] 应用 master gain / ADSR。

3. **Phase 3-1c：UI 接入**
   - [x] `ControlsPanel` 增加 `Export WAV` 按钮，启用条件与 `Export MIDI` 一致。
   - [x] `MainComponent` 只负责文件选择、导出选项收集和调用导出函数。
   - [x] 导出成功 / 失败先写 Logger；后续再补正式 UI 提示。

4. **Phase 3-1d：专项测试与边界修复**
   - [x] 新增或扩展 `docs/testing/phase3-recording-playback.md` 的 WAV 导出测试包（包 E，9 项）。
   - [x] 覆盖 fallback synth 导出、空 take 禁用、取消保存、无权限路径、导出文件可被播放器 / DAW 打开。
   - [x] 人工验证 E.1–E.9 全部通过，未发现明显 bug。

5. **Phase 3-2：VST3 插件离线渲染预研（已完成评估）**
   - 评估结果：推荐「独立离线实例 + 重置状态 + 无 editor」路径
   - 详见：[`phase3-plugin-offline-rendering.md`](phase3-plugin-offline-rendering.md)
   - 关键结论：离线实例与实时已加载实例解耦，重置为插件默认状态，离线实例不创建 editor，失败时降级到 fallback synth
   - 当前状态：设计评估已完成，待在真实 VST3 插件环境下验证「实施前的验证项」后再推进实现

### Phase 3-8：录制 / 回放稳定化

- [x] 复核 audio callback 中访问 `RecordingEngine` 的边界，避免文件 IO、UI 操作、阻塞等待或非受控分配进入实时路径。
- [x] 将回放结束通知收敛为轻量状态传递；audio thread 只置位轻量结束标记，message thread 轮询后更新 UI / 状态。
- [x] 复核采样率变化时的回放时间缩放与播放结束判断，结束阈值改为按当前回放采样率比例缩放后的 take 长度。
- [x] 更新 `RecordingEngine.h` 中关于 “Minimal skeleton / not thread-safe” 的注释，使其准确描述当前 MVP 状态和剩余约束。

---

## 11. 验收关注点

后续实现完成后，应新增专项测试文档覆盖：

- 开始录制 / 停止录制。
- 电脑键盘 note on/off 成对录入。
- 回放顺序与相对时间正确。
- 回放驱动已加载插件。
- 无插件时回放驱动 fallback synth。
- 停止回放不会留下悬挂音。
- 设备 sample rate / buffer size 变化后的基本行为。
- MIDI 导出文件可被 DAW 或 MIDI 工具打开。
- WAV 导出文件可被常见播放器或 DAW 打开。
- WAV 导出内容与 fallback synth 实时回放的音符顺序、相对时长基本一致。
- WAV 导出不依赖实时音频设备，不在 audio callback 中做文件 IO。
- WAV 导出取消保存、空 take、无权限路径不会崩溃，并有日志或 UI 反馈。

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
9. **WAV MVP 范围**：第一版 WAV 只导出当前 `RecordingTake` 的 fallback synth 音频，使用 stereo、当前设备采样率优先、当前 master gain / ADSR；不导出插件音频。
10. **WAV 插件导出后置**：VST3 插件离线渲染需要单独处理插件实例、状态冻结、editor 生命周期和失败策略，不阻塞 fallback synth WAV MVP。

---
# devpiano 录制 / 回放 / MIDI 导出测试用例

> 用途：定义 Phase 3 高级功能恢复 MVP 的专项手工回归清单。
> 更新时机：录制模型、回放调度、导出格式或 UI 控制发生变化时。

状态标记：

- [x] 已通过
- [ ] 未通过 / 未开始验证
- [~] 部分通过 / 待补充验证

相关文档：

- 阶段验收：[`acceptance.md`](acceptance.md)
- 功能说明：[`./recording-playback.md`](./recording-playback.md)
- 项目路线图：[`../roadmap/roadmap.md`](../roadmap/roadmap.md)

---

## 1. 测试目标

本测试文档覆盖 Phase 3 MVP 已恢复能力：

- Record / Stop / Play 基础闭环。
- 电脑键盘经由 `AudioEngine` pre-render `MidiBuffer` 录制边界。
- 回放事件重新进入当前插件 / fallback synth 发声路径。
- Export MIDI 输出 `.mid` 文件。
- 录制、回放、导出与键盘焦点、插件宿主、Performance Preset 的基本协同。

当前不作为 Phase 3 MVP 验收目标：

- WAV 离线渲染。
- MP4 / 视频导出。
- 复杂编辑能力（裁剪、拼接、量化、钢琴卷帘）。
- tempo map、loop、metronome、外部时钟同步。

---

## 2. 测试前提

### 环境前提

- [x] 项目可成功构建并启动。
- [x] Windows 验证构建通过：`./scripts/dev.sh win-build`。
- [x] 主窗口可见，音频设备已初始化。
- [x] `ControlsPanel` 可见 Record / Stop / Play / Export MIDI 控件。
- [x] 程序当前可发声（fallback synth 或已加载插件均可）。

### 建议测试模式

#### 模式 A：fallback synth

- 不加载插件，使用内置发声路径。
- 目的：快速验证录制 / 回放 / 导出的核心逻辑。

#### 模式 B：VST3 插件

- 加载一个已知可用的 VST3 乐器后执行同样用例。
- 目的：验证回放事件能驱动当前插件链路。

---

## 3. 基础录制闭环

### 3.1 Record 状态切换

#### 步骤

- [x] 启动程序。
- [x] 点击 `Record`。
- [x] 观察状态文本。
- [x] 不弹奏，点击 `Stop`。

#### 预期结果

- [x] 点击 `Record` 后状态显示为 Recording 或等价文本。
- [x] 程序不阻塞、不崩溃。
- [x] 点击 `Stop` 后状态回到 Idle 或等价文本。
- [x] 空录制不会导致后续 Play / Export 崩溃。

### 3.2 电脑键盘录制

#### 步骤

- [x] 点击 `Record`。
- [x] 依次弹奏 `A S D F`，每个按键保持约 0.3-0.5 秒。
- [x] 点击 `Stop`。

#### 预期结果

- [x] 录制期间按键能正常发声。
- [x] note on / note off 成对记录，不出现持续悬挂音。
- [x] 停止录制后程序仍可继续实时演奏。

### 3.3 录制期间焦点恢复

#### 步骤

- [x] 点击 `Record`。
- [x] 切换到其他窗口，再回到 DevPiano 主窗口。
- [x] 弹奏 `A/S/D`。
- [x] 点击 `Stop`。

#### 预期结果

- [x] 回到主窗口后电脑键盘仍能触发 note。
- [x] 录制状态不因窗口切换丢失。
- [x] 停止后无卡音。

---

## 4. 回放行为

### 4.1 基础回放

#### 步骤

- [x] 完成一次包含 `A S D F` 的录制。
- [x] 点击 `Play`。
- [x] 等待回放结束。

#### 预期结果

- [x] 回放能按录制顺序发声。
- [x] 音符间隔与录制时大致一致。
- [x] 回放结束后自动或手动回到 Idle。
- [x] 回放结束后没有悬挂音。

### 4.2 回放期间 Stop

#### 步骤

- [x] 录制一段较长演奏。
- [x] 点击 `Play`。
- [x] 回放中途点击 `Stop`。

#### 预期结果

- [x] 回放立即停止。
- [x] `Stop` 会发送 all-notes-off 或等价清理，避免悬挂音。
- [x] 停止后可再次点击 `Play` 回放同一 take。

### 4.3 插件链路回放

#### 步骤

- [x] 加载一个 VST3 乐器。
- [x] 完成一次录制。
- [x] 点击 `Play`。

#### 预期结果

- [x] 回放事件驱动当前插件发声。
- [x] 卸载插件后，回放可退回 fallback synth 或保持程序可用。
- [x] 插件 editor 打开时执行 Play / Stop 不崩溃。

---

## 5. MIDI 导出

### 5.1 导出当前 take

#### 步骤

- [x] 完成一次包含多个 note 的录制。
- [x] 点击 `Export MIDI`。
- [x] 在保存对话框中选择目标路径并保存 `.mid` 文件。

#### 预期结果

- [x] 导出成功时有清晰反馈或至少没有错误弹窗。
- [x] 输出文件存在且大小大于 0。
- [x] 文件扩展名为 `.mid`。
- [x] 未录制有效 take 时，`Play` 和 `Export MIDI` 按钮保持灰色禁用，无法触发导出路径。

### 5.2 外部播放器 / DAW 打开

#### 步骤

- [x] 用任意可读取 MIDI 的播放器或 DAW 打开导出的 `.mid` 文件。
- [x] 检查 note 顺序与时长。

#### 预期结果

- [x] 文件可被外部工具识别为标准 MIDI 文件。
- [x] note 顺序与录制内容一致。
- [x] 当前 MVP 不要求 tempo map、轨道命名或复杂元事件。

---

## 6. 与 Performance Preset 的协同

### 6.1 切换布局后录制

#### 步骤

- [x] 切换到另一个 Performance Preset。
- [x] 点击 `Record`。
- [x] 弹奏新布局中的几个按键。
- [x] 点击 `Stop` 并 `Play`。

#### 预期结果

- [x] 录制的是布局映射后的 MIDI note，而不是按键字符本身。
- [x] 回放不依赖当前键盘物理按键状态。
- [x] 回放不覆盖当前 Performance Preset。

### 6.2 重启后行为边界

#### 步骤

- [x] 完成一次录制。
- [x] 关闭并重启程序。

#### 预期结果

- [x] Performance Preset 可按既有规则恢复。
- [x] 当前 MVP 不要求录制 take 跨重启持久化。
- [x] 程序重启后仍可重新录制、回放和导出。

---

---

## 8. 优先测试包

### 包 A：Phase 3 MVP 快速回归

- [x] 3.1 Record 状态切换。
- [x] 3.2 电脑键盘录制。
- [x] 4.1 基础回放。
- [x] 4.2 回放期间 Stop。
- [x] 5.1 导出当前 take。

### 包 B：集成回归

- [x] 模式 B：VST3 插件链路回放。
- [x] 6.1 切换布局后录制。
- [x] 3.3 录制期间焦点恢复。
- [x] 5.2 外部播放器 / DAW 打开。


### 包 D：下一阶段 Phase 3 稳定化回归

- [x] 回放结束路径不会从 audio thread 直接执行复杂 UI / 文件 / 设置逻辑。
- [x] 设备 sample rate 变化后，旧 take 回放速度、结束时机和 Stop 状态正常。
- [x] 回放中途 Stop 后无悬挂音，且可再次 Play 同一 take。
- [~] 长时间录制或容量接近上限时，不崩溃、不阻塞 audio callback，并能记录 dropped event 状态。（当前无法准确触发容量上限，暂缓确认。）
- [x] Export MIDI 的取消保存、无权限路径、覆盖文件等失败 / 边界路径有可理解反馈或日志。

### 包 E：WAV 导出回归（Phase 3-1d）

> Phase 3-1c 已完成 UI 接入（Export WAV 按钮 + FileChooser + 调用 WavFileExporter）。
> 本包覆盖 fallback synth WAV 导出的手工验证。

#### E.1 空 take 时 Export WAV 按钮禁用

**步骤**

- [x] 启动程序后，不录制任何内容，直接观察 Export WAV 按钮状态。

**预期结果**

- [x] Export WAV 按钮为灰色禁用状态。
- [x] Export MIDI 按钮同样禁用（与 Export WAV 保持一致）。

#### E.2 正常录制后可导出 WAV

**步骤**

- [x] 点击 `Record`。
- [x] 依次弹奏 `A S D F G`，每个按键保持约 0.3–0.5 秒。
- [x] 点击 `Stop`。
- [x] 观察 Export WAV 按钮是否变为可用。

**预期结果**

- [x] 停止录制后，Export WAV 按钮立即变为可用状态。
- [x] Export MIDI 按钮同样可用。

#### E.3 WAV 保存对话框与默认文件名

**步骤**

- [x] 确保有有效 take。
- [x] 点击 `Export WAV`。
- [x] 观察保存对话框中的默认文件名格式。

**预期结果**

- [x] 对话框标题为 "Export WAV Recording" 或类似。
- [x] 默认文件名为 `recording_<ISO8601>.wav`，其中 `ISO8601` 包含日期和时间。
- [x] 文件扩展名筛选器为 `*.wav`。

#### E.4 WAV 文件成功导出并可被播放器打开

**步骤**

- [x] 完成一次包含多个 note 的录制。
- [x] 点击 `Export WAV`，选择目标路径并保存。
- [x] 用文件管理器确认文件存在且大小 > 0。
- [x] 用任意音频播放器或 DAW 打开该 `.wav` 文件。

**预期结果**

- [x] 导出过程中无弹窗错误。
- [x] 输出文件存在且文件大小大于 0。
- [x] 文件可被常见音频播放器（如 VLC、Audacity、REAPER）打开。
- [x] 播放时音符顺序与录制内容一致。
- [x] 文件声道为 stereo，采样率与目标采样率一致。

#### E.5 取消 WAV 保存对话框

**步骤**

- [x] 完成一次录制。
- [x] 点击 `Export WAV`。
- [x] 在保存对话框中点击"取消"或关闭对话框。

**预期结果**

- [x] 对话框正常关闭，无崩溃。
- [x] 无文件被创建或覆盖。
- [x] 日志中有 `[Export] WAV export cancelled by user` 或等价信息。

#### E.6 导出期间无 UI 阻塞或音频异常

**步骤**

- [x] 完成一次包含 10+ 秒演奏的录制。
- [x] 点击 `Export WAV` 并保存为较大文件（目标采样率 48000 时约数 MB）。

**预期结果**

- [x] 导出过程中主窗口保持响应，可切换窗口。
- [x] 无音频设备断开或重启。
- [x] 导出完成后无异常崩溃。

#### E.7 覆盖已有文件

**步骤**

- [x] 导出一份 WAV 文件。
- [x] 再次点击 `Export WAV`，选择同一路径，文件名与之前相同，确认覆盖。

**预期结果**

- [x] 保存对话框出现"文件已存在"警告或等价提示。
- [x] 选择覆盖后，原文件被新内容替换。
- [x] 新文件可正常播放。

#### E.8 导出文件音调与实时回放基本一致

**步骤**

- [x] 完成一次包含明确音高的演奏录制（如 C4 E4 G4）。
- [x] 用 MIDI Export 导出同一份 take 为 `.mid` 文件。
- [x] 用 WAV Export 导出同一份 take 为 `.wav` 文件。
- [x] 用 DAW 或 MIDI 播放器分别打开 `.mid` 和 `.wav`，对比音符顺序和相对时长。

**预期结果**

- [x] `.mid` 和 `.wav` 中的音符顺序一致。
- [x] `.wav` 中每个音符的时长略长于 `.mid`（因为 WAV 导出额外增加了 release tail）。
- [x] 无音符被遗漏或顺序颠倒。

#### E.9 无效采样率拒绝（边界）

**步骤**

- [x] 有有效 take 时，观察导出选项中的采样率（从音频设备或 take.sampleRate 获取）。

**预期结果**

- [x] `WavFileExporter` 对采样率 ≤ 0 的情况直接返回 false，不崩溃。
- [x] 当前实现默认 fallback 为 44100，不会传递 ≤ 0 的采样率到 `WavFileExporter`。
- [x] 若所有采样率来源均不可用，导出流程正常终止，不产生空文件或部分文件。

---

## 9. Test Run 记录

### Test Run 2026-04-28

- 构建：未记录。
- 平台：Windows 手工验证。
- 模式：Phase 3 MVP 录制 / 回放 / MIDI 导出专项回归。
- 结果：已执行项均通过，未发现明显问题。
- 备注：
  - `5.1` 中“未录制 take 时导出”的预期已修正为当前真实行为：未录制有效 take 时，`Play` 和 `Export MIDI` 按钮保持灰色禁用；该项已通过。

### Test Run 2026-04-29

- 构建：Windows MSVC 验证构建已通过。
- 平台：Windows 手工验证。
- 模式：包 D 下一阶段 Phase 3 稳定化回归。
- 结果：
  - 包 D 第 2 项：未发现明显问题。
  - 包 D 第 3 项：发现明显问题；回放中途 Stop 后有较大概率出现悬挂音，特别是在播放有声音的一瞬间点击 Stop 时容易复现；再次 Play 同一 take 正常。
  - 包 D 第 4 项：无法准确触发容量上限，暂缓确认。
  - 包 D 第 5 项：未发现明显问题。
  - 复测：Stop 悬挂音修复后，包 D 第 3 项未发现明显问题。
  - 当前结论：包 D 第 4 项保持 `[~]` 暂缓测试，等待后续可控容量上限 / 长时间录制验证方式。

### Test Run 2026-04-30

- 构建：Windows MSVC 验证构建已通过。
- 平台：Windows 手工验证。
- 模式：Phase 3-1d WAV 导出专项回归（包 E）。
- 结果：包 E 全部 9 项（E.1–E.9）均通过，未发现明显 bug。
- 备注：
  - 所有测试项均未出现明显 bug，E.1–E.9 已全部验证通过。
  - E.8 对比 WAV 与 MIDI 音符顺序一致，WAV 的 release tail 符合预期。
  - 当前 WAV 导出仅支持 fallback synth 音色，暂不支持已加载 VST3 插件音色（Phase 3-2 后置）。
  - 插件离线渲染意向已记录为未来工作项，暂不阻塞当前 MVP。
