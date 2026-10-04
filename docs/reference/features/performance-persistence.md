# 演奏数据持久化（.devpiano）与回放控制功能说明

> 用途：说明 devpiano 的原生无损演奏文件格式（`.devpiano`）、`PerformanceFile` 序列化引擎、原子文件写入、播放速度平滑控制（0.5x–2.0x）与最近文件列表。
> 当前状态：已全量实现并稳定服务于演奏数据的保存、恢复与回放。
> 更新时机：`.devpiano` 文件格式版本、序列化协议或调速算法发生变化时。

---

## 1. 概述与设计定位

为了让用户的键盘演奏成果能够无损保存并随时恢复演练，devpiano 定义了专有的**原生演奏文件格式（`.devpiano`）**与**高保真回放控制器**：

1. **无损 Sample-Accurate 精度**：与转换为 MIDI Tick 的有损导出不同，`.devpiano` 格式直接保存录制时的绝对采样点位置（`timestampSamples`）与内部元数据，实现 100% 比特级无损还原；
2. **v3 JSON + JUCE 二进制编码**：顶层保存 MIDI 与预设事件 variant、Take 内快照表；`midiData` 继续使用 `juce::MemoryBlock::toBase64Encoding()` 的专有编码。v1/v2 的纯 MIDI 文件仍可加载，旧数字预设事件明确拒绝，不用当前目录猜测原身份；
3. **事务文件替换（Transactional File Replacement）**：保存过程采用同目录 `juce::TemporaryFile`，检查文本写入与 flush 状态、关闭流后再替换目标；普通写入/替换失败保留原目标。不以此承诺断电、强杀或存储硬件故障下的完整恢复；
4. **实时播放速度控制（0.5x–2.0x）**：支持在回放过程中无缝调节倍速（从慢速 0.50x 到双速 2.00x），基于原子变量无锁同步，变速时 Take 采样点位置绝对恒定，以二分查找动态重校准播放游标；
5. **独立元数据解析（`loadPerformanceFileMetadata`）**：无需反序列化庞大事件数组即可极速读取曲名、创建时间与备注，提升文件信息查看与历史管理性能；
6. **最近文件与拖放体验**：集成 `juce::RecentlyOpenedFilesList`（最多记录 10 个历史文件），支持拖放 `.devpiano` 文件即时加载并自动开始回放。

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
      "source": "computerKeyboard",
      "midiData": "3.PxCY"
    },
    {
      "timestampSamples": 88200,
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
| `version` | int | 写出 `3`；v1/v2 只接受纯 MIDI 事件，旧数字 `presetChange` 整体拒绝并记录原因。MIDI 编码不接受任意整数数组。 |
| `format` | string | 固定标识 `"devpiano-performance"`，用于文件类型快速校验。 |
| `sampleRate` | double | 有限数值，文件准入范围为 **8000–384000 Hz**；设备采样率不同时检查缩放后的可表示范围。 |
| `lengthSamples` | int64 | 非负整数且覆盖全部事件；须在支持的设备采样率与 0.5x–2.0x 变速下可表示，并保留块长度加法余量。 |
| `metadata` | object | 包含 `createdAt`（ISO 8601 时间）、`title`（曲目标题）与 `notes`（备注文本）。 |
| `events` | array | 按采样点稳定规范化；执行时同采样预设先于 MIDI，同类型事件保留原顺序。 |
| `presets` | array | 内嵌 `RecordedPreset`：完整预设对象（含永久 UUID）和当时的可执行 `AcousticSnapshot`；预设事件槽位越界、缺失或不可执行快照均拒绝。 |

### 2.2 事件类型支持

- **MIDI 演奏事件**：`source` 为 `"computerKeyboard"`、`"realtimeMidiBuffer"` 或 `"playback"`；`midiData` 使用 `<字节数>.<JUCE 编码负载>`，例如 `3.PxCY` 表示 `90 3c 64`，`3..xC.` 表示 `80 3c 00`。编码负载内的 `.` 是合法字符，不是第二个长度分隔符；
- **预设切换事件**：音频线程按记录采样边界消费内嵌快照，随后同采样 NoteOn 使用新参数。增添、删除、重命名外部预设不改变旧演奏；外部文件缺失仍消费保存的快照。UI 只独立反映最新状态，不晚到重发声学参数或再次查目录。
- **迁移边界**：旧文件没有可追溯的“目录索引→永久身份”映射，不能可靠重建其预设事件；拒绝比静默播放另一份音色更安全。不会偷偷丢掉旧事件后仅播放剩余 MIDI。预设配置文件 v1 的名字迁移与原生演奏文件 v1/v2 的数字事件迁移是两件不同的事。
- **初始状态**：新录制在起点捕获初始预设与声学快照；预备拍期间的状态在实际录制下拍采样点 0 进入 Take。实时与内置/VST3 离线导出共用快照参数与最终持有者释放规则。

---

### 2.3 当前 Take、元数据与原生文件绑定

- `RecordingSessionController::RecordingSession` 同时管理 Take、当前元数据和 `.devpiano` backing file；新录制（含预备拍完成）及 MIDI 导入解除旧原生文件绑定，不继承旧歌曲备注。
- 原生文件通过非空加载准入后才整体提交 Take/元数据/绑定；打开失败或取消保持原会话身份。
- 成功 Save As 到 C 后，后续信息编辑仅写 C；写入失败不改旧绑定，元数据持久化失败也不替换当前元数据。
- 每次 Take 替换或成功重新绑定推进 `takeGeneration`；原生打开、保存选择器与信息弹窗的延迟结果不允许作用于另一个 Take。暂停/继续不替换 Take，也不改变文件绑定。

### 2.4 文件与时间线准入

- 原生文件/JSON 上限为 **32 MiB**，单个 MIDI 帧上限为 **1 MiB**，累计解码数据受 **32 MiB** 预算约束。文件长度、完整读取及流状态必须一致；元数据读取失败也不提交新会话。
- 在 `fromBase64Encoding()` 分配前验证正整数字节数、精确负载长度、JUCE 字符表及末尾填充位；构造 `MidiMessage` 前验证 status/data 宽度、原始 SysEx 与有界 meta VLQ。截断帧或畸形固定宽度 meta 拒绝，不交给不满足前提的框架 accessor。
- 时间戳与长度拒绝负数、分数、非数值字段及不可表示的缩放；事件必须在 `[0, lengthSamples]`，保留旧录制中恰好位于末尾的事件。乱序旧文件稳定规范化后才进入实际播放与 seek。
- 文件支持范围与通用播放器的合成时间域分开：文件准入使用上述音频采样率范围，数值层仍支持现有低采样率合成时间域；不通过重写播放/seek 测试数值掩盖失败。


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
