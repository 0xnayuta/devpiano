# MIDI 文件导入与回放功能说明

> 用途：说明 devpiano 的标准 MIDI 文件（`.mid` / `.midi`）导入解析、全轨合并、回放控制、事件支持与专项测试清单。
> 当前状态：已全量实现并稳定接入回放与音频链路。
> 更新时机：MIDI 解析逻辑、事件过滤规则或导入回放交互发生变化时。

---

## 1. 概述与设计定位

devpiano 支持打开标准 MIDI 文件并在当前发声链路中回放，为用户提供练琴示范、伴奏跟弹与音色试听能力：

1. **标准格式兼容**：支持标准 MIDI Type 0（单轨多通道）与 Type 1（多轨同步）文件；
2. **多轨并轨合并与智能解析（MidiTrackMergeEngine）**：委托纯静态算法引擎将 Type 0/1 各音轨的 MIDI 播放事件合并为单一连续回放 Take，支持智能通道映射与全局元数据提取；导入路径不提供选轨模式；
3. **丰富 Channel 消息支持**：除 Note On/Off 外，完整解析并还原 **CC64 延音踏板**、**Pitch Bend 弯音** 与 **Program Change 音色切换**；
4. **导入 Take 与导出 Take 解耦**：导入的 MIDI 作为只读 Playback Take 播放，**禁止再次导出为 MIDI**（保持 Export MIDI 按钮 disabled，防止有损二次转换），但**支持离线渲染导出为 WAV 音频**；
5. **极速拖放与路径记忆**：支持从操作系统直接拖拽 `.mid` 文件到窗口即时加载播放，自动记忆最近导入路径。

---

## 2. 核心架构与主处理流程

```text
[用户点击 Import MIDI / 拖入 .mid]
    │
    ├── 0. 取消进行中的倒计时 (cancelCountIn)
    ├── 1. 若当前处于播放中，安全停止当前回放并清空当前发音
    │
    ▼
MidiFileImporter::importFileWithMetadata()
    │
    ├── 2. 读取并验证 MIDI 文件头 (Type 0 / 1, PPQ 时间基准)
    ├── 3. MidiTrackMergeEngine::mergeTracks() ──► 多轨时间线合并与通道智能路由
    ├── 4. 时间基准转换: 将 MIDI Tick 转换为绝对采样点位置 (timestampSamples)
    ├── 5. 提取 Note, CC64 Sustain, Pitch Bend, Program Change 事件
    ├── 6. 元数据解析: MidiTextDecoder 解码曲名与全局 Tempo Map
    └── 7. 组装为 RecordingTake ──► 返回 std::optional<MidiImportResult>
    │
    ▼
RecordingSessionController::replaceTakeAndStartPlayback()
    │
    ├── 8. 重置既有循环与游标: clearPlaybackLoop() + idleSeekPositionSamples.reset()
    ├── 9. 设定当前 Playback Take (canExportMidi=false, 保留曲目标题元数据)
    ├── 10. AudioEngine::armPlaybackStartPreRoll() (防 0s 音符截断)
    └── 11. RecordingEngine::startPlaybackAtTakeSample() ──► 自动开始回放 ──► 驱动虚拟键盘联动

```

---

## 3. 详细处理规则与边界设计

### 3.1 多轨并轨规则与通道映射

`MidiFileImporter` 接入了 `MidiTrackMergeEngine`：
- **全音轨合并**：Type 0/1 文件各音轨中的 MIDI 播放事件按时间顺序合并为单一 Take，支持多声部与多乐器统一回放；
- **智能通道分配**：支持 `passThrough`（原样保留通道）、`autoAssignIfSingleChannel`（单通道多轨自动分配 1-16 通道）与 `forceTrackToChannel`；
- **全轨元数据提取**：扫描所有音轨中的 Meta 事件，提取曲名、Tempo、拍号、调号并构建全局 Tempo Map；导入路径不按音符数量选轨；
- **健壮性容错**：若文件所有轨道均无 Note 事件，安全返回空结果并向 Logger 输出警告，程序不崩溃。

### 3.2 时间戳换算精度

- 根据 MIDI 文件头定义的 PPQ（Pulses Per Quarter Note）与 Tempo（默认 120 BPM，或首个 Tempo 设定），结合当前音频设备的采样率（如 44.1 kHz / 48 kHz），将每个 MIDI 事件的 Tick 准确转换为绝对采样点 `timestampSamples`；
- 回放时由 `RecordingEngine` 逐 audio block 调度，不受系统时钟抖动影响。

### 3.3 首音 0s 截断防御（Pre-roll 机制）

部分 MIDI 文件的首个音符起始时间为 0s。为防止音频设备启动瞬间的清理用 All-Notes-Off 将 0s 音符误消除，`AudioEngine` 在启动导入回放前调用 `armPlaybackStartPreRoll()`，在首个可听 block 前插入微小静音预备区，确保首音 100% 完整清晰发声。

### 3.4 状态机互斥与按钮联动
- **录制中（Recording / RecordingPaused）**：Import MIDI 按钮自动禁用，拖拽导入文件静默忽略并记录日志，防止录制与导入冲突；
- **播放中（Playing / PlayingPaused）**：Import MIDI 按钮禁用；若通过拖拽导入，系统自动安全停止当前回放，重置发音状态，载入新 Take 并从头自动开始回放；
- **导入成功后**：
  - `Record` 按钮可用（点击将放弃导入 Take 并进入录制，若开启预备拍则先执行倒计时）；
  - `Stop` 按钮可用；
  - `Play` / `Back` 按钮可用（点击 `Back` 立即从头重新播放）；
  - `Export MIDI` **严格保持 Disabled**；
  - `Export WAV` **保持 Enabled**（支持将导入的 MIDI 渲染为高质量 WAV 音频）；
  - `TimelineBar` 激活，显示 Take 总时长与当前播放时间，支持以采样点精确 Seek 与 Take-relative A/B 标记循环练习；若导入新文件，原有的 A-B 标记与 Seek 偏移自动清空重置。
### 3.5 元事件文本解码规则

标准 MIDI 文件不携带字符集信息，历史文件的轨道名/标题可能使用本地编码（GBK/CP936）甚至被制作工具按 Latin-1 反复误读。`MidiTrackMergeEngine` 统一通过 `MidiTextDecoder` 解码文本元事件，按以下顺序回退：

1. 纯 7-bit ASCII 直接构造；
2. 合法 UTF-8 原样保留，仅当整串被 Latin-1 补充字符主导时判定为历史误编码，还原原始字节后按 GBK 重新解读（可多轮解包，最多 4 轮）；
3. 每个非 ASCII 字节对都命中 GBK 码表、且不含单字节码页签名的字节流按 GBK 解码：
   - 该严格条件同时拒绝 Big5 等字节结构重叠的编码，避免产出"看似合理实则错误"的汉字；
   - **单字节码页签名**：约 91% 的 `(CP1252 重音字母 + 紧随的 ASCII 字母)` 组合恰好也是合法 GBK 字节对，因此"能解码"不足以判定为双字节文本。若某双字节字符前方紧邻 ASCII 字母/数字、且其后不再出现双字节字符，则判定该串属于单字节码页（如 `Für Elise`），交由 Windows-1252 处理；
4. 其余情况回退 Windows-1252 单字节映射，保证任何输入都不产生 `U+FFFD`。

已覆盖的实际案例：`梦中的婚礼.mid`（GBK）、`Summer.mid` 乐器名（ASCII 前缀 + GBK）、`elise.mid`（CP1252 重音）、`you.mid`（连续两轮重编码）均还原出原始标题。已知局限：Big5 与 Shift-JIS 尚无专用码表，此类文件落在 Windows-1252 兜底上，不会误报为汉字。

### 3.6 尾部残留字节容错

`juce::MidiFile::readFrom` 要求文件在最后一个块之后**不留任何字节**，否则整体返回失败——即使所有轨道内容都已解析完成。真实文件常出现此类残留（实测案例：编辑器在末尾附加 `0d 0a`）。`MidiFileImporter` 因此在该 API 返回失败时进一步检查：只要已有轨道解析出事件，即记为警告并继续导入；真正的非法文件（无任何可用轨道）仍按失败处理。

### 3.7 时间轴 Seek 与 A-B 循环跟练规则

导入的 MIDI Take 支持基于绝对采样点的无缝 Seek 与片段循环跟练：

1. **采样点精确 Seek**：
   - 播放状态下点击或拖动 `TimelineBar`，在下一个音频块边界重校准播放位置，并向全部 16 个 MIDI 通道注入 CC64(0)、CC120(0)、All-Notes-Off(0) 清理事件，消除旧音符残留；
   - 暂停或空闲状态下 Seek，将位置保存在 `idleSeekPositionSamples` 中并调用 `audioEngine.requestAllNotesOff()`，恢复播放时准确定位至该采样点起奏；
2. **A-B 循环区间与边界时序**：
   - A/B 标记以 Take 采样点保存，有效区间为半开区间 `[A, B)`；
   - 当回放跨越 B 点边界时，音频块内优先调度 16 通道发音清理，紧接着从 A 点重新起奏多通道事件，杜绝上一个循环的未结音符残留；
   - 循环期间自动抑制播放自然结束标志，确保循环平滑持续；
3. **极小区间防御**：
   - 若循环区间经播放速度与采样率缩放后小于单个音频回调块（`playbackBlockSize`），引擎保持标记但旁路循环回跳，防止单块内重复回跳导致断音风暴与 CPU 尖刺。

---

## 4. 专项手工与边界测试清单

| 用例编号 | 测试场景 | 操作步骤与验证目标 | 状态 |
|---|---|---|:---:|
| **MID-001** | 标准单轨 MIDI 导入 | 导入 `simple-notes.mid`，能听到清晰音符序列，虚拟键盘联动高亮 | [x] 已通过 |
| **MID-002** | 多轨时间线合并 | 导入 `multitrack-basic.mid`（Track 0 无音符、Track 1 含音符），确认 Track 1 音符进入合并后的 Take 并正常播放 | [x] 已通过 |
| **MID-003** | CC64 延音踏板还原 | 导入 `sustain-pedal.mid`，音符在踏板松开前持续延音，效果清晰可辨 | [x] 已通过 |
| **MID-004** | 0s 首音即时起奏 | 导入首个音符位于 0.000s 的 MIDI 文件，首音清晰完整，无吞音现象 | [x] 已通过 |
| **MID-005** | 空文件与非法文件容错 | 导入 `empty.mid` 或损坏的 `invalid.mid`，UI 提示错误，Logger 记录日志，程序不崩溃 | [x] 已通过 |
| **MID-006** | 播放中 Back 重放 | 播放到一半点击 `Back`，立即从当前 Take 最开头无缝重放 | [x] 已通过 |
| **MID-007** | 导出按钮状态边界 | 导入成功后确认 `Export MIDI` 为 disabled，`Export WAV` 为 enabled | [x] 已通过 |
| **MID-008** | 拖放即时加载 | 从 Windows 文件资源管理器拖拽 `.mid` 文件至主窗口，立即加载并播放 | [x] 已通过 |
| **MID-009** | 导入后 WAV 离线导出 | 导入 MIDI 后点击 `Export WAV`，正常渲染并生成包含该 MIDI 音乐的 WAV 文件 | [x] 已通过 |
| **MID-010** | 导入后采样点精确 Seek | 导入 MIDI 播放中拖拽时间轴至任意位置，发音在下一音频块重定向且旧音符完全释放，无悬挂音 | [ ] 待手工验证 |
| **MID-011** | 导入后多通道 A-B 循环跟练 | 在导入的多轨 MIDI 上设定 A-B 标记，播放到达 B 点时回跳至 A 点，全通道清理，无漏音与爆音 | [ ] 待手工验证 |
| **MID-012** | 导入新文件自动重置循环与游标 | 处于 A-B 循环状态下导入新 MIDI，原 A-B 标记与 Seek 偏移清空，新文件从 0 开始完整播放 | [ ] 待手工验证 |
