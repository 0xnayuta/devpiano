# 录制 / 回放功能设计（第一版草案）

> 用途：说明 DevPiano 录制、回放与导出的第一版现代化设计边界。
> 当前状态：**设计草案已收口，尚未实现**。
> 读者：后续实现 M6 录制 / 回放 / 导出功能的开发者。
> 更新时机：录制模型、回放调度、导出格式或实现状态发生变化时。

相关文档：

- 路线图：[`../roadmap/roadmap.md`](../roadmap/roadmap.md)
- 阶段验收：[`../testing/acceptance.md`](../testing/acceptance.md)
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
2. 通过 `MidiKeyboardState::processNextMidiBuffer()` 更新键盘状态 / fallback synth 状态。
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
    playback
};

struct PerformanceEvent {
    int64_t timestampSamples = 0;
    RecordingEventSource source = RecordingEventSource::computerKeyboard;
    juce::MidiMessage message;
};
```

设计原则：

- `timestampSamples` 使用录制开始后的绝对 sample 位置，便于在 audio block 中转换为 sample offset。
- `juce::MidiMessage` 保存标准 MIDI 语义，避免旧 `SM_*` 裸字节模型。
- `source` 用于区分电脑键盘与外部 MIDI，便于后续过滤、调试和测试。
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

录制时序草案：

```text
User clicks Record
  -> RecordingEngine::start(sampleRate)
AudioEngine::getNextAudioBlock
  -> collect live MidiBuffer for this block
  -> RecordingEngine::recordBlock(midiBuffer, blockStartSample, numSamples)
User clicks Stop
  -> RecordingEngine::stop()
  -> RecordingTake available
```

注意事项：

- 录制层应拷贝必要 MIDI 事件，不持有 block-local `MidiBuffer` 引用。
- 不在 audio callback 中写文件、弹窗或更新 UI。
- 若需要跨线程传递状态，使用简单原子状态或消息线程快照。
- 停止录制时应补发/记录必要 note off 或在回放停止时执行 all-notes-off，避免悬挂音。

---

## 6. 回放设计

回放目标是把 `RecordingTake` 中到期事件重新注入 `AudioEngine` 的当前 block `MidiBuffer`。

建议时序：

```text
User clicks Play
  -> RecordingEngine::startPlayback(take, currentSampleRate)
AudioEngine::getNextAudioBlock
  -> collect live MidiBuffer
  -> RecordingEngine::renderPlaybackBlock(blockStartSample, numSamples, midiBuffer)
  -> AudioEngine continues existing plugin / synth path
Playback reaches end
  -> RecordingEngine enters stopped state
```

关键规则：

- 回放事件应以 `MidiBuffer::addEvent(message, sampleOffset)` 加入当前 block。
- `sampleOffset` 必须限制在 `0..numSamples-1`。
- 若当前设备采样率不同于录制采样率，应按秒级时间换算到当前 sample timeline。
- 回放不重新走 `KeyboardMidiMapper`，避免当前布局变化影响历史录制。
- 回放可与实时输入混合，但第一版可以先明确为“播放时仍允许实时输入，二者合并进入同一 `MidiBuffer`”。
- 停止、seek 或播放结束时应发送 all-notes-off 或清理 held notes。

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

- 新增 `source/Recording/RecordingEngine.*`。
- 定义 `PerformanceEvent`、`RecordingTake` 和状态枚举。
- 提供 start / stop / clear / hasTake / getSnapshot 等基础接口。

### M6-2：接入实时 MIDI 录制

- 在 `AudioEngine::getNextAudioBlock()` 中把当前 block 的 `MidiBuffer` 快照交给 `RecordingEngine`。
- 先记录进入 audio block 的 MIDI 消息，不单独记录 UI key event。
- 验证电脑键盘与外部 MIDI 都能进入同一录制时间线。

### M6-3：最小回放

- `RecordingEngine` 根据播放位置把到期事件写回当前 block `MidiBuffer`。
- 使用当前插件 / fallback synth 发声路径。
- 停止或播放结束时清理悬挂音。

### M6-4：最小 UI

- 增加 Record / Stop / Play 按钮与状态文本。
- 确保没有录制片段时 Play 不可用或有清晰提示。

### M6-5：MIDI 导出

- 将 `RecordingTake` 转换为 `juce::MidiFile`。
- 使用文件对话框保存 `.mid`。
- 失败时给出明确错误提示。

### M6-6：WAV 离线渲染预研 / 实现

- 单独设计离线渲染链路。
- 不阻塞 M6-1 到 M6-5 的上线。

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
