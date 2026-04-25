# 录制 / 回放功能说明（草案）

> 用途：记录旧 FreePiano 录制/回放行为参考与现代重建设计考虑。  
> 当前状态：**草案阶段，不实现，仅建模**。  
> 读者：需要理解录制/回放历史行为、规划未来实现方向的开发者。  
> 更新时机：设计草案有实质进展时。

相关文档：

- 旧代码迁移边界：[`../architecture/legacy-migration.md`](../architecture/legacy-migration.md)
- 路线图：[`../roadmap/roadmap.md`](../roadmap/roadmap.md)

---

## 1. 旧 FreePiano 录制/回放行为参考

对应源码：`freepiano-src/song.*`、`freepiano-src/export.*`。

### 1.1 核心事件模型

旧系统以时间戳原始字节数组为基本录制单位：

```cpp
struct song_event_t {
    double time;   // 相对于录音开始的时间（秒）
    byte a;       // 消息类型 + 通道
    byte b;       // 数据字节1（如 note number）
    byte c;       // 数据字节2（如 velocity）
    byte d;       // 数据字节3
};
```

消息类型使用 `SM_*`（Song Message）编码，覆盖：

| 类别 | 消息 | 说明 |
|---|---|---|
| MIDI 标准 | `SM_MIDI_NOTEON`, `SM_MIDI_NOTEOFF`, `SM_MIDI_CONTROLLER`, `SM_MIDI_PITCH_BEND`, `SM_MIDI_PROGRAM` | 原始 MIDI 消息 |
| 系统控制 | `SM_SYSTEM` | 键盘映射/标签/颜色配置事件 |
| 设置事件 | `SM_KEY_SIGNATURE`, `SM_OCTAVE`, `SM_VELOCITY`, `SM_CHANNEL`, `SM_TRANSPOSE` | 录音时全局设置状态 |
| 播放控制 | `SM_PLAY`, `SM_RECORD`, `SM_STOP` | 播放/录制/停止指令 |
| 同步/延迟 | `sync_event_t`, `delay_event_t` | 独立于主事件流的辅助队列 |

**关键观察**：旧录制不是纯 MIDI note 录制，而是**完整会话状态快照**，包括：
- 所有按键映射（keymap）
- 所有设置组（setting groups）
- 键盘标签和颜色

### 1.2 录制行为

- `song_start_record()`：初始化录音，清空事件缓冲区，重置计时器 `song_timer = 0`
- 录音时所有 `song_send_event()` 调用会同时写入 `song_event_buffer`（最多 1M 事件）
- `SM_PLAY`、`SM_RECORD`、`SM_STOP` 本身不被录制（避免循环）
- 录音开始时会先录制当前**完整配置快照**（setting groups、keymap、labels、colors）作为 t=0 事件
- `song_stop_record()` 在末尾写入 `SM_STOP` 事件
- 支持多个输入/输出轨道（`SM_INPUT_0..3` / `SM_OUTPUT_0..3`），每个事件带有轨道标记

### 1.3 回放行为

- `song_start_playback()`：重置计时器，从缓冲区起始位置开始
- `song_update(time_elapsed)` 由音频引擎每帧调用，推进 `song_timer` 并执行到期的已录制事件
- 事件触发 `song_output_event()`，经过与录音时相同的翻译管道（包括 octave shift、transpose、follow key 等）
- 支持播放速度（`song_play_speed`），用于慢放/快放
- 播放结束后自动停止

### 1.4 导出行为

- `export_wav.*`：离线渲染，将录制内容以 WAV 格式输出（直接渲染音频，不走实时音频链路）
- `export_mp4.*`：视频导出（将音频与界面录屏合成 MP4）
- 旧导出是离线渲染，不是实时录制

### 1.5 旧设计的已知问题

- **配置快照耦合**：录音中嵌入完整 keymap 和设置状态，导致回放时强制覆盖用户当前设置
- **裸字节事件**：使用 `SM_*` 而非标准 MIDI 编码，与外部 MIDI 工具不兼容
- **最大事件数限制**：1M 事件硬上限，无动态扩展
- **时间来源不明确**：`song_timer` 由音频回调驱动，暂停/恢复语义不清晰
- **静态缓冲区**：不支持超大曲目

---

## 2. 当前原型状态

对应源码：`source/Legacy/UnusedPrototypes/SongEngine.*`。

当前原型是旧 `song.*` 的直译移植，未重新设计：

- 同样的 `song_event_t { time, a, b, c, d }` 字节数组
- 固定 1M 事件缓冲区
- `update(timeElapsed)` 驱动回放
- 没有与当前 `AudioEngine`、`KeyboardMidiMapper`、`PluginHost` 集成

**当前原型不参与主构建，不可用。**

---

## 3. 现代重建设计考虑（仅作建模记录，不实现）

以下为未来实现时的设计参考，**不是当前任务范围**。

### 3.1 录制事件模型

建议分离关注点，不使用旧"会话快照"模型：

**分层事件模型（草案）：**

```cpp
// 演奏事件（必须录制）
struct PerformanceEvent {
    double timestampSeconds;       // 绝对时间戳（秒）
    EventSource source;            // enum: computerKeyboard | externalMidi | controller
    juce::MidiMessage message;     // 标准 MIDI 消息（note on/off, CC, etc.）
    int inputIndex;                // 哪个 MIDI 输入（多设备场景）
};

// 元事件（可选录制）
struct MetaEvent {
    double timestampSeconds;
    MetaEventType type;             // layoutChange | settingsChange | marker
    juce::var payload;             // 灵活载荷
};
```

**与旧系统的区别：**
- 使用 JUCE `MidiMessage` 而非裸 `SM_*` 字节，兼容标准 MIDI 工具
- 不强制嵌入完整 keymap；只录制用户显式操作
- source 标记区分事件来源，回放时可选择性回放

### 3.2 时间基础

- 使用高精度时钟，不依赖音频回调
- 暂停/恢复语义清晰

### 3.3 回放模型

- 回放时将事件重新路由到 `AudioEngine` 的 MIDI 输入（与实时输入同一条链路）
- 不重新触发 `KeyboardMidiMapper`（避免键盘映射与录制时不一致）
- 支持选择性回放（只回放某一类 source）

### 3.4 与插件的关系

- 录制的是 **MIDI 输出事件**，不是插件音频本身
- 插件音色是"听众听到的结果"，录制的是"演奏意图"

### 3.5 导出格式

建议优先实现：

1. **MIDI 文件导出**（`juce::MidiFile`）：将演奏事件序列化为标准 MIDI Type 1 文件
2. **WAV 离线渲染**：将 MIDI 文件通过离线音频链路渲染为 WAV

暂不实现 MP4/视频合成。

---

## 4. 当前迭代立场

**本迭代（预研阶段）只建模，不实现。**

当前不接入主链路，不修改任何 `source/` 代码，不改变构建配置。

仅允许：
- 阅读旧代码，提炼行为规则
- 在本文档中记录设计草案

不得：
- 在 `source/` 中添加新的录制/回放代码
- 创建参与主构建的新类或模块
- 实现导出功能

---

## 5. 待明确的设计问题

以下问题在后续实现前需要明确：

1. **录制范围**：是否需要同时录制插件参数自动化？还是只录制 MIDI note/CC？
2. **多轨支持**：是否需要分离电脑键盘、外部 MIDI、控制器为独立轨道？
3. **文件格式**：使用标准 MIDI 文件（Type 1）还是自定义 JSON + 二进制混合格式？
4. **编辑能力**：录制后是否需要支持裁剪、拼接、量化等编辑操作？
5. **实时回放 vs 离线渲染**：是否需要实时回放，还是只做离线导出？
6. **回放时插件状态**：回放时加载的插件与录音时不同时如何处理？