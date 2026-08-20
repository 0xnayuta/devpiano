# 演奏数据持久化（.devpiano）与回放控制功能说明

> 用途：说明 devpiano 的原生无损演奏文件格式（`.devpiano`）、`PerformanceFile` 序列化引擎、原子文件写入、播放速度平滑控制（0.5x–2.0x）与最近文件列表。
> 当前状态：已全量实现并稳定服务于演奏数据的保存、恢复与回放（Phase 6 成果）。
> 更新时机：`.devpiano` 文件格式版本、序列化协议或调速算法发生变化时。

---

## 1. 概述与设计定位

为了让用户的键盘演奏成果能够无损保存并随时恢复演练，devpiano 定义了专有的**原生演奏文件格式（`.devpiano`）**与**高保真回放控制器**：

1. **无损 Sample-Accurate 精度**：与转换为 MIDI Tick 的有损导出不同，`.devpiano` 格式直接保存录制时的绝对采样点位置（`timestampSamples`）与内部元数据，实现 100% 比特级无损还原；
2. **v2 JSON + Base64 紧凑编码**：顶层采用 Human-Readable 的 JSON 结构，密集二进制 MIDI 数据采用 Base64 编码，兼具可读性、调试友好度与体积紧凑性；
3. **原子安全写入（Atomic File Write）**：保存过程采用 `juce::TemporaryFile` 机制，先写入临时文件，全部校验成功后再原子重命名为目标文件，彻底消除因中途断电或崩溃导致文件损坏的风险；
4. **实时播放速度控制（0.5x–2.0x）**：支持在回放过程中无缝调节倍速（从慢速 0.50x 到双速 2.00x），基于原子变量无锁同步，变速时动态重校准播放游标，消除悬挂音；
5. **最近文件与拖放体验**：集成 `juce::RecentlyOpenedFilesList`（最多记录 10 个历史文件），支持拖放 `.devpiano` 文件即时加载并自动开始回放。

---

## 2. `.devpiano` 文件格式规范（Schema v2）

`.devpiano` 文件以 UTF-8 编码的 JSON 文本存储，格式版本当前固定为 `2`：

```json
{
  "version": 2,
  "format": "devpiano-performance",
  "sampleRate": 44100.0,
  "lengthSamples": 2646000,
  "metadata": {
    "createdAt": "2026-08-19T14:30:00Z",
    "title": "My Piano Sonata in C",
    "notes": "Practiced with Enhanced Modal Piano v3"
  },
  "events": [
    {
      "timestampSamples": 44100,
      "source": "computerKeyboard",
      "midiData": "kDw/"
    },
    {
      "timestampSamples": 88200,
      "source": "computerKeyboard",
      "midiData": "gDwA"
    },
    {
      "timestampSamples": 132300,
      "type": "presetChange",
      "presetId": 1
    }
  ]
}
```

### 2.1 顶层字段说明

| 字段 | 类型 | 说明 |
|---|:---:|---|
| `version` | int | 格式版本号，当前为 `2`（v1 为原始 JSON 整型数组，v2 升级为 Base64 编码并向下兼容）。 |
| `format` | string | 固定标识 `"devpiano-performance"`，用于文件类型快速校验。 |
| `sampleRate` | double | 录制时的音频采样率（Hz）。若回放设备采样率不同，系统按比例自动平移时间线。 |
| `lengthSamples` | int64 | 录制总长度（单位为采样点）。 |
| `metadata` | object | 包含 `createdAt`（ISO 8601 时间）、`title`（曲目标题）与 `notes`（备注文本）。 |
| `events` | array | 演奏事件数组，按 `timestampSamples` 严格升序排列。 |

### 2.2 事件类型支持
- **MIDI 演奏事件**：`source` 为 `"computerKeyboard"`、`"realtimeMidiBuffer"` 或 `"playback"`，`midiData` 包含经 Base64 编码的原始 MIDI 消息；
- **预设切换事件**：`type` 为 `"presetChange"`，`presetId` 记录切换的目标预设索引，回放至该时间点时自动触发界面与矩阵切调。

---

## 3. 播放速度精确控制（Speed Control）

在 `source/Recording/RecordingEngine.cpp` 中实现了线程安全的倍速回放调度器：

### 3.1 速度换算与时间步进公式
设当前设备实际处理的采样数为 $N$，播放速度倍率为 $S$（$0.5 \le S \le 2.0$）：
$$\Delta_{\text{playback}} = \text{round}(N \times S)$$
- 当 $S = 0.5$（半速）时，每渲染 1 秒音频仅推进 0.5 秒录制数据（变慢）；
- 当 $S = 2.0$（双速）时，每渲染 1 秒音频推进 2.0 秒录制数据（变快）。

### 3.2 动态变速防悬挂音重校准
在播放进行中拖动速度滑块时：
1. 速度倍率由 `std::atomic<double> playbackSpeedMultiplier` 线程安全传递；
2. 调度器立即以当前实际时间戳重新校准 `playbackPositionSamples`，并在变速跨度过大时检查并补发已过期事件的 `noteOff`，防止音符因时间线突变持续悬挂发声。

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
