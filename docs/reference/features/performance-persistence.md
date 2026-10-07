# 演奏数据持久化（.devpiano）与回放控制功能说明

> 用途：说明 devpiano 的原生无损演奏文件格式（`.devpiano`）、`PerformanceFile` 序列化引擎、原子文件写入、播放速度平滑控制（0.5x–2.0x）与最近文件列表。
> 当前状态：已全量实现并稳定服务于演奏数据的保存、恢复与回放。
> 更新时机：`.devpiano` 文件格式版本、序列化协议或调速算法发生变化时。

---

## 1. 概述与设计定位

为了让用户的键盘演奏成果能够无损保存并随时恢复演练，devpiano 定义了专有的**原生演奏文件格式（`.devpiano`）**与**高保真回放控制器**：

1. **采样时间线保存**：直接保存 `timestampSamples`、原始 MIDI 帧与内嵌声学快照，不经 MIDI tick 量化；保留可准入整数时间域的事件位置，不承诺第三方插件音频逐比特相同；
2. **v3 JSON 与 JUCE 编码**：保存采样率、长度、元数据、内嵌 `presets` 与稳定排序事件。`midiData` 使用 `MemoryBlock::toBase64Encoding()` 的十进制长度前缀和专有六位字符负载，不是通用 RFC 4648 Base64；演奏与独立元数据读取都只接受当前整数版本 `3`，不迁移旧格式；
3. **事务文件替换**：先写同目录 TemporaryFile，检查 flush/状态并关闭流，成功后替换目标；普通写出/替换失败保留原目标，仅清理自有临时文件，不承诺断电或强杀下的存储完整性；
4. **实时播放速度控制（0.5x–2.0x）**：消息线程发布有界命令，音频块入口一致更新倍率和缩放位置，保留下一未渲染事件游标；Seek 的目的状态恢复与纯变速分开，不用旧游标重算重播起音；
5. **独立元数据读取**：`loadPerformanceFileMetadata()` 读取有界完整 JSON，但不解码 MIDI 帧或构造 RecordingTake；不宣称流式跳过 JSON 事件数组；
6. **最近文件与拖放体验**：集成 `juce::RecentlyOpenedFilesList`（最多记录 10 个历史文件），支持拖放 `.devpiano` 文件即时加载并自动开始回放。
7. **歌曲备注输入与提交**：Song Information 的 Notes 使用可编辑、多行 `NotesEditor`，回车插入换行；确认通过生产控制器提交并更新绑定文件，取消保持会话元数据和文件字节不变。诊断列表继续使用只读 `ListEditor`。

---

## 2. `.devpiano` 文件格式规范（Schema v3）

文件采用 UTF-8 JSON，当前写出版本为 `3`。`presetId` 是本 Take 的 `presets` 表槽位，不是目录索引：

```json
{
  "version": 3,
  "format": "devpiano-performance",
  "sampleRate": 44100.0,
  "lengthSamples": 2646000,
  "metadata": {
    "createdAt": "2026-08-19T14:30:00Z",
    "title": "My Piano Sonata in C",
    "notes": "Practiced with Enhanced Modal Piano v3"
  },
  "presets": [
    {
      "preset": {
        "version": 2,
        "uuid": "963c9500-38d7-4e10-8025-2e61d4830084",
        "name": "Recorded Piano"
      },
      "acoustic": {
        "builtinTone": "piano",
        "masterGain": 0.7,
        "adsr": { "attack": 0.01, "decay": 0.2, "sustain": 0.8, "release": 0.3 },
        "brightness": 0.5,
        "hammerHardness": 0.5,
        "resonance": 0.5,
        "lidPosition": 0,
        "temperament": "equal",
        "referencePitchA4": 440.0,
        "soundPerspective": "player",
        "reverbSpace": "chamber",
        "reverbWet": 0.0,
        "pedalNoiseLevel": 0.6,
        "feltAgeingAmount": 0.0,
        "unaCorda": false,
        "sustainPolicy": "normal",
        "transposeEnabled": false,
        "transposeOffset": 0,
        "channelFollowKeyMask": 65023
      }
    }
  ],
  "events": [
    {
      "timestampSamples": 44100,
      "type": "midi",
      "source": "computerKeyboard",
      "midiData": "3.PxCY"
    },
    {
      "timestampSamples": 88200,
      "type": "midi",
      "source": "computerKeyboard",
      "midiData": "3..xC."
    },
    {
      "timestampSamples": 132300,
      "type": "presetChange",
      "presetId": 0
    }
  ]
}
```

### 2.1 顶层字段说明

| 字段 | 类型 | 说明 |
|---|:---:|---|
| `version` | int | 仅接受当前整数版本 `3`；其他版本或非整数版本拒绝。演奏与独立元数据读取共用版本准入，不截断大整数或浮点版本。 |
| `format` | string | 固定标识 `"devpiano-performance"`，用于文件格式标识快速校验。 |
| `sampleRate` | double | 有限正数值，文件准入范围严格为 **8000.0 – 384000.0 Hz**；支持非设备采样率时间域的缩放与回放。 |
| `lengthSamples` | int64 | 非负整数且覆盖全部事件；须在支持的采样率与 0.5x–2.0x 变速下乘积/加法均整数可表示，保留末尾 `+1` 采样点与块余量。 |
| `metadata` | object | 包含 `createdAt`（ISO 8601 时间戳字符串）、`title`（曲目标题）与 `notes`（多行备注文本）。 |
| `events` | array | 按时间戳稳定规范化，同采样点保留输入相对顺序；不能把原生文件的顺序规则等同于 MIDI 多轨并轨的事件优先级。 |
| `presets` | array | 必须存在的内嵌 `RecordedPreset` 独立快照表：包含完整预设对象 `"preset"`（格式版本 2、永久 UUID、键位布局与通道矩阵）及当时由音频引擎捕获的不可变可执行 `"acoustic"`（`AcousticSnapshot`）；纯 MIDI Take 可使用空数组，事件 `presetId` 必须严格位于 `[0, presets.size())` 范围内。 |

### 2.2 事件类型支持

- **MIDI 演奏事件（`type: "midi"`）**：必须显式提供事件类型，缺失或空类型不作 MIDI 推断。`source` 为 `"computerKeyboard"`、`"realtimeMidiBuffer"` 或 `"playback"`；`midiData` 使用 `<字节数>.<JUCE 编码负载>`，例如 `3.PxCY` 表示 `90 3c 64`，`3..xC.` 表示 `80 3c 00`。编码负载内的 `.` 是合法字符，不是第二个长度分隔符。每个 MIDI 帧在解析与分配前校验负载长度一致性、合法状态字节及有效 SysEx / Meta 边界；
- **预设切换事件（`type: "presetChange"`）**：`presetId` 为该 Take 内嵌 `presets` 数组的槽位索引。音频线程在事件采样点切换声学快照与移调状态，同采样 NoteOn 立即使用新参数。由于音色与参数已完整自包含在文件内部，外部增删、重命名预设或预设文件缺失均不影响历史演奏回放；
- **非当前格式拒绝**：v1/v2、未来版本和非整数版本均不准入，不执行历史数据迁移或目录索引猜测。加载失败保持原文件字节和当前会话；当前 v3 的可选元数据缺省仍返回空元数据；
- **初始录制状态捕获**：新录制在起点自动捕获初始预设与声学快照（槽位 0）；开启节拍器预备拍时，预备拍期间的设置在实际录制下拍（采样点 0）进入 Take。实时与离线 WAV 导出完全共享相同的快照数据。

---

### 2.3 当前 Take、元数据与原生文件绑定

- **所有权与解绑生命周期**：`RecordingSessionController::RecordingSession` 同时管理 `RecordingTake`、当前元数据 `currentMetadata`、绑定磁盘文件 `currentPerformanceFile` 与代际计数器 `takeGeneration`：
  - **新录制**：调用 `detachForNewRecording()`，清空 Take，解除原文件绑定（`currentPerformanceFile = File()`），重置元数据，递增 `takeGeneration`；
  - **录制完成提交**：调用 `commitRecordedTake(newTake)`，更新 Take，保持未绑定状态（`currentPerformanceFile = File()`），递增 `takeGeneration`，开启 `canExportMidi`；
  - **MIDI 导入**：调用 `commitImportedMidi(newTake, songTitle)`，更新 Take，解除原文件绑定，设置新歌曲标题，递增 `takeGeneration`，关闭 `canExportMidi`；
  - **原生文件打开**：调用 `openFromFile(file)`，在通过文件大小、完整读取、JSON 结构、准入约束与元数据校验后，整体提交 Take 并绑定 `currentPerformanceFile = file`，递增 `takeGeneration`；加载失败或取消保持原会话状态；
  - **另存为（Save As）**：调用 `saveToFile(newFile, expectedGeneration)`，代际一致且事务保存成功后，将 backing file 重新绑定为 `newFile`，递增 `takeGeneration`；
  - **元数据编辑**：代际一致时先准备新 metadata；若有绑定文件且 Take 有效，事务落盘成功后才提交内存。写入失败保持原内存和文件；未绑定时仅提交内存。
- **跨 Take 与延迟操作隔离**：通过 `takeGeneration` 强校验，无论系统原生文件选择器的延迟返回，还是元数据编辑弹窗的延迟确认，均不得作用于已被替换的下一代 Take，绝不发生 A 文件被 B 的内容或元数据误写改写。

### 2.4 文件与时间线准入

- **文件容量与内存预算**：原生文件与 JSON 最大上限均为 **32 MiB**（`maxPerformanceFileSizeBytes`），单个 MIDI 帧原始数据上限为 **1 MiB**，解码累计 MIDI 字节受 **32 MiB** 预算约束。文件流读取时校验 `streamLength == fileSize` 且无短读；
- **编码校验与解析安全**：分配前校验正整数字节前缀、JUCE 专有负载字符及末尾填充位；构造 MidiMessage 前校验状态/数据字节与有界 Meta VLQ，畸形或截断帧拒绝。不用通用 Base64 decoder 替代该协议。
- **时间线数值准入**：采样率严格限制在 8000–384000 Hz；`lengthSamples` 与事件 `timestampSamples` 拒绝负数、非数值及不可表示的整数缩放；事件采样戳必须位于 `[0, lengthSamples]`；
- **预设与绑定合法性校验**：落盘与加载时，校验全部 `presetChange` 事件的 `presetId` 均在 `take.presets` 范围内；校验全部内嵌预设绑定的触发动作为 `keyDown`（显式 keyUp 或未知动作拒绝保存与加载）；
- **乱序规范化与时间域独立**：当前格式的乱序事件在准入时通过 `std::stable_sort` 稳定规范化时间线，同采样事件保留原语义顺序；文件准入的音频采样率范围与通用播放器内部的测试时间域独立，不以窄化测试掩盖真实设备范围。


## 3. 播放速度精确控制（Speed Control）

在 `source/Recording/RecordingEngine.cpp` 中实现了线程安全的倍速回放调度器：

### 3.1 速度换算与时间步进公式

设当前设备实际处理的采样数为 $N$，播放速度倍率为 $S$（$0.5 \le S \le 2.0$）：
$$\Delta_{\text{playback}} = \text{round}(N \times S)$$
- 当 $S = 0.5$（半速）时，每渲染 1 秒音频仅推进 0.5 秒录制数据（变慢）；
- 当 $S = 2.0$（双速）时，每渲染 1 秒音频推进 2.0 秒录制数据（变快）。

### 3.2 动态变速游标与时间线保持

在播放进行中、暂停或停止状态下调整速度倍率时：
1. 速度倍率由 `std::atomic<double> playbackSpeedMultiplier` 线程安全传递，严格限制在 `[0.5, 2.0]` 范围内；
2. **Take 绝对位置守恒**：在 `playing`、`playingPaused` 或保留游标的 `stopped` 状态下，调度器依公式重算当前设备采样位置：
   $$P_{\text{new}} = \text{round}\left(P_{\text{old}} \times \frac{S_{\text{old}}}{S_{\text{new}}}\right)$$
   保证换算回不可变 Take 的采样点位置（`getPlaybackPositionInTakeSamples()`）严格保持不变；
3. **二分查找精确定位**：播放状态下立即调用 `resetPlaybackEventCursor`，借助 `std::ranges::lower_bound` 二分查找首个 $\ge P_{\text{new}}$ 的事件索引，防止时间线重映射引起跳音、吞音或历史事件重复触发；
4. **发音完全清理**：在时间轴主动 Seek 或 A-B 循环回跳时，引擎批量向 16 个 MIDI 通道注入 CC64(0)、CC120(0) 与 All-Notes-Off(0) 清理事件，彻底杜绝悬挂音。
---

## 4. 专项手工与边界测试清单

| 用例编号 | 测试场景 | 操作步骤与验证目标 | 状态 |
|---|---|---|:---:|
| **PRF-001** | 保存与打开无损往返 | 录制一段复杂演奏 → 保存为 `.devpiano` → 重新打开，音符顺序、音高与节奏与原演奏 100% 一致 | [x] 已通过 |
| **PRF-002** | 元数据保存与查看 | 在保存前编辑曲目标题与备注，打开后在 Song Information 弹窗中完整还原该元数据 | [x] 已通过 |
| **PRF-003** | 损坏文件防御 | 用文本编辑器故意破坏 `.devpiano` 的 JSON 结构并尝试打开，Logger 报错，程序不崩溃 | [x] 已通过 |
| **PRF-004** | 播放中实时倍速调节 | 回放时在 0.5x、1.0x、1.5x、2.0x 之间快速来回拖动滑块，音符速度平滑变化，无爆音、无卡死 | [x] 已通过 |
| **PRF-005** | 最近文件列表联动 | 成功打开或保存 `.devpiano` 文件后，最近文件菜单顶部自动追加该文件路径，点击可再次打开 | [x] 已通过 |
| **PRF-006** | 拖放即时回放 | 将 `.devpiano` 文件拖入主窗口，立即自动解析并开始回放 | [x] 已通过 |
| **PRF-007** | 预设切换事件回放 | 在录制中按 F2 切换预设并继续弹奏，保存并打开后，回放到达对应时间点自动切换为 F2 预设 | [x] 已通过 |
| **PRF-008** | 独立元数据极速加载 | 调用 `loadPerformanceFileMetadata` 读取大体积 `.devpiano` 文件，极速返回歌曲标题与创建时间，无需解包解析完整 events 事件流 | [x] 已通过 |
| **PRF-009** | 暂停与停止状态下变速位置守恒 | 在暂停或停止状态下拖动速度滑块调节倍速，重新播放时依然严格从原来的 Take 绝对采样点起奏，无任何游标漂移 | [x] 已通过 |
| **PRF-010** | Take 替换后的旧文件保护 | 打开 A，导入/录制 B 后编辑信息；A 原字节保持，未保存的 B 不绑定 A | [x] Windows 文件/实际导入信息界面验证通过 |
| **PRF-011** | Save As 重新绑定 | 当前 B Save As 到 C，再编辑信息；C 包含 B 事件和新元数据，A 不变 | [x] Windows 真实文件消费者验证通过 |
| **PRF-012** | 失败与延迟结果隔离 | 失败打开/保存保持当前身份；切换 Take 后提交旧信息或保存结果，当前文件不被改写 | [x] Windows 真实文件消费者验证通过 |
| **PRF-013** | 生产 Notes 键入/确认/取消 | 在实际 Info 窗口键入两行并确认，会话和绑定文件均保存；再次修改后取消，两者保持原值，诊断列表仍只读 | [x] Windows 实际窗口与文件字节验证通过 |
