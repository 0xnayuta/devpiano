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
4. **标准 MIDI 文件导出**：将完成录制后的 `RecordingTake` 快照转换为标准 MIDI 文件（单轨包含 Set Tempo 与 MIDI 事件，默认 960 PPQ），供导入外部宿主（DAW）或打谱软件；
5. **全流程会话编排（`RecordingSessionController`）**：统一录制、播放、暂停和停止；预备拍在完整 1–2 小节后的音频下拍开始，不在最后一拍起音或 UI 轮询时才启动。目标已到达但 UI 尚未轮询时，后续控制先接管已开始的录制；未开始时取消保留原 Take；
6. **A-B 跟练时间轴与采样级 Seek**：显示 Take 绝对时间，支持点击/拖拽 Seek、A/B 标记；清理旧发音后，在目标音符之前恢复目的通道的 program/bank/CC64/pitch，不重发历史 NoteOn。

---

## 2. 核心架构与数据流

### 2.1 录制数据流

```text
电脑键盘按键 ──► KeyboardMidiMapper (律动力度/微扰/手感曲线) ──► MidiChannelMapper ──► 有界 SPSC 输入
                                                                                   │
                                                                                   ▼
AudioEngine::getNextAudioBlock() (实时音频回调) ◄──────────────────────────────────┘
    │
    ├── 1. 经有界 SPSC 队列（RealtimeQueue）收集物理按键与控制器输入，排序至当前块
    │
    ├── 2. [录制事件边界] ──► RecordingEngine::recordMidiBufferBlock()
    │                          ├── 遍历当前 block 的 MidiBuffer
    │                          ├── 将事件转为 PerformanceEvent (绝对采样时间戳)
    │                          └── 存入 pre-allocated std::vector (无扩容分配，超额丢弃仍保留末尾释放)
    │
    ├── [预设切换事件] ──────► RecordingEngine::recordPresetChange(RecordedPreset) (注册 Take 内可执行快照)
    │
    └── 3. 交付发声 ──► VST3 processBlock() / BuiltinSynthesiser (内置无锁发声)
```

### 2.2 回放数据流

```text
RecordingSessionController::handlePlayClicked() ──► RecordingEngine::startPlaybackAtTakeSample()
                                                          │
                                                          ▼
AudioEngine::getNextAudioBlock() (实时音频回调)
    ├── applyPendingTransportCommands(midiBuffer) (变速 / Seek / Stop 的唯一音频块提交点)
    ├── RecordingEngine::renderPlaybackBlock(playbackVisualMidiBuffer, blockStartSamples, numSamples)
    ├── PlaybackIdentityTracker (FIFO 锁定最终输出身份；同输出最后持有者才交付 Off)
    ├── [视觉位图原子更新] (仅更新 displayNotes 原子数组，音频线程不调用 Listener)
    ├── midiBuffer.addEvents(playbackVisualMidiBuffer, 0, numSamples, 0)
    ├── [预设分段渲染] (同块预设先于 MIDI 生效，切换声学快照与 Master/Reverb)
    └── 插件或内置物理建模钢琴发声 (activeSynth 无锁调度，两预建音色银行平滑切换)

MainComponent::timerCallback() (消息线程)
    └── AudioEngine::dispatchPendingDisplayEvents() ──► 驱动 MidiKeyboardState 及 UI 定时器
```


### 2.3 时间轴跳转与 A-B 循环
- `TimelineBar` 使用 Take 采样点作为时间域，显示当前播放时间、总时长与 A/B 标记；点击或拖拽产生 Take-relative Seek；
- **Seek 跨通道清理时序**：Seek 在下一个音频块边界重校准播放位置，并在新位置事件之前对全部 16 个 MIDI 通道依次注入 CC64 Off、CC120 All-Sound-Off 与 CC123 All-Notes-Off 清理事件；暂停或空闲状态下记录 Take-relative 目标采样位置，并请求音频线程在下一块清理当前发声；
- **A/B 标记与循环时序**：A/B 标记以 Take 采样点保存，有效区间为半开区间 `[A, B)`；无效、倒置或空区间不会启用循环；
- **B 边界回跳与防悬挂**：音频回调按采样偏移处理 B 边界。边界 16 通道清理严格先于同采样点的 A 事件；整块恰好结束于 B 时，清理与回跳在下一音频块偏移 0 执行。循环期间自动抑制播放结束标志；
- **极小区间防御**：缩放后的有效区间至少需要覆盖一个当前音频回调块（`playbackBlockSize`）；更短区间保留 A/B 标记但自动旁路循环回跳，避免单块内重复回跳与清理风暴；
- **速度自适应**：0.5x–2.0x 的 setter 只发布目标值，不在消息线程修改位置或活动游标。下一音频块一致提交有效倍率与重缩放位置；纯变速保留下一未渲染事件，防止取整后重播旧 NoteOn。Take 采样点与 A/B 标记不变。
- **Stop 与结构边界**：活动 Stop 在块边界先执行跨通道 panic，再停止回放；同块 Stop 优先于待提交 Seek/变速。清除/替换/启动 Take 与暂停快照仅在 callback 已停止的守卫内执行；UI 的目标倍率与当前音频有效倍率分别读取，不以 state 原子值代替已进入 callback 的退出确认。

### 2.4 捕获、末尾与设备采样域

- **捕获暂停**：控制器在 callback 停止后终结已录身份及 CC64/66/67 状态，再冻结时间轴；暂停中新起音和后续无已录身份的 Off 排除。实际最终松键在该采样点闭合同身份的全部已捕获重起音，不等暂停/停止补救。
- **固定 Take 域**：开始时锁定 `RecordingTake::sampleRate`，设备新采样点/块偏移换算到该域，累计不足一个样本的余量不随块反复丢失；pause 不推进。prepare 同时重基准活动/暂停播放的位置和倍率，保留下一未渲染游标及同代发音快照。
- **末尾交付**：事件缩放与离线共用取整，播放有效长度包含最后事件 `+1`。终结位于整块边界时，下一块偏移 0 仍交付音频清理，再通知 UI；自动结束 UI 只更新状态，不重复发送 Stop 截断释放尾音。
- **完整预备拍**：先停机预分配/arm，`MetronomeProcessor` 音频计数给出块内起点，之前的 MIDI 排除、起点事件时间戳为 0。120 BPM 4/4 一/两小节分别为 2/4 秒；消息轮询、静音、取消/重启与 release/prepare 不重置已消耗的计时。
- **warmup**：空闲启动仍丢弃输入并静音；活动 Transport 在静音过渡时继续处理 MIDI/DSP 与时钟，节拍仍混入。新播放的 pre-roll 不被 warmup 提前消耗。
- **验证边界**：Windows CPU 生产链路、文件 readback、原生 VST3 和实际控制器/窗口见 [Phase D 实施记录](../../archive/audit-004-code-quality-fix-phases.md#phase-d-实施记录与直接验证2026-10-04)。不外推所有厂商插件、声卡热插拔或整个 callback 的无锁/无分配门禁。

---

## 3. 核心数据模型（`RecordingTake`）

`source/Recording/RecordingEngine.h` 中定义可变的数据结构；完成录制后，控制器使用其 Take 快照进行回放与导出：

```cpp
enum class RecordingEventSource : std::uint8_t { computerKeyboard, realtimeMidiBuffer, playback };
enum class PerformanceEventType : uint8_t { midi = 0, presetChange = 1 };

struct PerformanceEvent {
    std::int64_t timestampSamples = 0;              // 相对录制起点的绝对采样点
    PerformanceEventType type = PerformanceEventType::midi;
    std::uint32_t presetId = 0;                    // type == presetChange 时记录 Take 内快照表索引
    RecordingEventSource source = RecordingEventSource::computerKeyboard;
    juce::MidiMessage message;                     // type == midi 时记录标准 JUCE MIDI 消息
};

struct RecordingTake {
    double sampleRate = 0.0;                       // 录制开始时锁定的 Take 时间域
    std::int64_t lengthSamples = 0;                // 录制总长度（采样点）
    std::vector<PerformanceEvent> events;          // 事件序列
    std::vector<RecordedPreset> presets;           // 内嵌预设与声学快照表（v3）

    [[nodiscard]] bool isEmpty() const noexcept;
    [[nodiscard]] double durationSeconds() const noexcept;
};
```

---

## 4. 标准 MIDI 文件导出（`MidiFileExporter`）

`source/Recording/MidiFileExporter.cpp` 将 `RecordingTake` 序列化为标准 `.mid` 文件：
- **文件与轨道组织**：`exportTakeAsMidiFile()` 真实只调用一次 `midiFile.addTrack(sequence)` 生成单轨事件流（并非独立导引轨+音符轨的双轨结构），时间基准为传入的 PPQ（默认 **960 PPQ**）；
- **Meta 与 Tempo 设置**：在 tick 0 处写入单一 Set Tempo 消息（固定 `defaultTempoMicrosecondsPerQuarterNote = 500000` 微秒/四分音符，对应 120 BPM）；不自动合成曲目标题、拍号或调号 meta 事件；
- **事件过滤与显式流保持**：仅导出 `type == PerformanceEventType::midi` 且非 SysEx、非空的演奏消息，自动过滤 `presetChange` 事件与 SysEx。不调用 `updateMatchedPairs()`，避免为重复同音起音凭空插入额外 NoteOff；
- **数值准入与范围校验**：校验采样率处于支持范围且长度可表示；PPQ 仅接受 `1 .. 32767`；转换 tick 前校验非负与有限性；写出前确认 JUCE `int` tick 与 MIDI 4 字节 VLQ delta 均可表示，非法范围拒绝写出并保留原目标；
- **事务文件替换（TemporaryFile）**：先写入同目录 `TemporaryFile`，写入完成后 flush 并关闭输出流，校验状态成功后再调用 `overwriteTargetFileWithTemporary()` 原子替换目标文件。连续导出覆盖时确保完全替换为当前 Take 内容，不追加旧文件；取消、参数拒绝或替换失败均完整保留原目标字节并清理临时文件。
---

## 5. 专项手工与边界测试清单

| 用例编号 | 测试场景 | 操作步骤与验证目标 | 状态 |
|---|---|---|:---:|
| **REC-001** | 基础录制与回放 | 点击 Record → 弹奏一段旋律 → 点击 Stop → 点击 Play，完整听到刚才弹奏的旋律 | [x] 已通过 |
| **REC-002** | 虚拟键盘回放联动 | 回放录音时，虚拟钢琴键盘准确随着各音符的按下与松开同步高亮与变暗 | [x] 已通过 |
| **REC-003** | 录制中切换预设 | 录制过程中按快捷键切换 Preset，回放时声音与声学参数在对应时刻根据内嵌快照自动还原（含快照内的移调状态；普通预设切换不覆写全局调号） | [x] 已通过 |
| **REC-004** | 长时间录制与溢出保护 | 连续录制 30 分钟以上，无卡顿、无内存暴涨，停止录制后 Take 完整可用 | [x] 已通过 |
| **REC-005** | MIDI 文件导出与 DAW 验证 | 导出 MIDI 文件并在 Reaper / Cubase / Logic 等外部 DAW 中导入，音符时值与力度完全正确 | [x] 已通过 |
| **REC-006** | 空 Take 导出保护 | 未开始录制时，Export MIDI 与 Export WAV 按钮保持 disabled | [x] 已通过 |
| **REC-007** | 播放中 Stop 防悬挂音 | 播放到包含长延音的片段中途点击 Stop，所有声音立即干净切断，无残留音 | [x] 已通过 |
| **REC-008** | 时间轴点击与拖拽 Seek | 播放中点击或拖动时间轴，验证绝对时间位置随即更新、旧音符被清除，暂停时 Seek 后仍保持精确位置 | [x] 已通过 |
| **REC-009** | 多通道 A-B 跟练循环 | 导入多轨 MIDI，设置 A/B 并跨越 B 点循环，验证所有通道的旧发音被清除且 A 点事件在边界后重启 | [x] 已通过 |
| **REC-010** | 节拍器完整预备拍 | 120 BPM 4/4 一/两小节分别在完整 2/4 秒的目标音频下拍开始；零 UI 轮询立即 Play/Stop、取消重启、静音和半程设备重建 | [x] Windows 实际控制器/窗口与生产 CPU 块通过；声卡热插拔仍单列 |
| **REC-011** | 极小 A-B 循环区间防御 | 将 A-B 区间设为小于单音频块，播放平滑通过该区间，不发生死循环或重复清理风暴 | [ ] 待手工验证 |
