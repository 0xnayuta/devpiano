# Phase 4: MIDI 文件能力与 FreePiano 功能差距审计（历史归档）

> 用途：比较 devpiano 早期状态与旧 FreePiano 在 MIDI 文件、演奏数据、song 文件、导入导出、打开保存方面的差距，并记录 Phase 4 候选功能评估过程。
> 归档说明：本文档为 Phase 4 规划初期的历史差距审计与候选方案评估报告，已完成历史使命并归档。现行 MIDI 导入与回放功能规范请参考 [`docs/reference/features/midi-file-import.md`](../reference/features/midi-file-import.md)。

---

## 1. 审计目的

本轮审计回答以下问题：

1. devpiano 相对旧 FreePiano 在"MIDI 文件能力与 FreePiano 功能差距"方面已补齐哪些？
2. 还差哪些？
3. 哪些适合纳入 Phase 4 小型扩展，哪些应继续 defer？
4. Phase 4 最小推荐范围是什么？

---

## 2. Phase 4 阶段定位建议

### 为什么这轮不应继续作为 Phase 3 扩展

Phase 3（录制/回放/MIDI导出）主链路已完成，Phase 3-2 已搁置，稳定化已验证通过。当前阶段边界已明确：
- 录制 / 回放 / MIDI 导出：已有完整 MVP
- WAV 离线渲染：已有 fallback synth MVP（Phase 3-1）
- VST3 插件离线渲染：已搁置（Phase 3-2）

继续在 Phase 3 下追加"MIDI 导入"会模糊 Phase 3 的完成状态，且 MIDI 导入的用途（打开外部 MIDI 文件回放）与 Phase 3 的录制/回放闭环性质不同，独立命名更清晰。

### Phase 4 与各阶段的边界

| 阶段 | 核心目标 |
|------|---------|
| Phase 1–2 | 基础演奏、插件、键盘映射 |
| Phase 3 | 录制 / 回放 / MIDI 导出 / WAV 离线渲染 / 布局 Preset |
| **Phase 4** | **MIDI 文件导入 / 演奏数据保存与打开** |
| Phase 5 | 架构收敛与 MainComponent 瘦身（已完成，1587→606 行） |
| Phase 6 | 演奏数据持久化与播放体验增强（演奏文件保存/打开、播放速度、最近文件、拖拽、基础编辑） |
| Phase 7+ | 完整工程文件、多轨/tempo map、高级编辑（Piano roll、量化） |

### Phase 4 应该解决什么

- 打开标准 MIDI 文件（.mid）并在当前播放链路回放
- 已定义"导入 → 回放 → 导出"的 roundtrip 边界：导入的 MIDI playback take **禁止再次导出为 MIDI**；用户已有原始 `.mid` 文件，无需把导入内容再导出为 MIDI。导入 MIDI 后允许导出 WAV；VST3 插件音色离线渲染仍后置到 Phase 3-2 扩展范围。
- 提供轻量演奏文件保存/打开能力（可选，视实现成本决定）
- 记住用户最近使用的导入/导出路径（低风险体验增强）

### Phase 4 不应该解决什么

- 完整 song/project 工程文件系统（需要更全面的设计）
- 钢琴卷帘 / 事件编辑器 / 多轨编辑
- 复杂 tempo map / 拍号 / 小节网格编辑
- 旧 FreePiano `.fpm` / `.lyt` 格式的完整兼容
- VST3 插件离线渲染（Phase 3-2 仍独立 defer）
- 图形化 MIDI 编辑或量化工具

---

## 3. 审计范围

### 已阅读的 devpiano 文档

- `../../roadmap/roadmap.md`（Phase 1–4 阶段状态、当前重点）
- `../../roadmap/current-iteration.md`（Phase 4 当前迭代状态、已完成验收与剩余方向）
- `./recording-playback.md`（Phase 3 设计与实现状态、验收标准）
- `../acceptance.md`（Phase 1–3 验收状态）
- `../../issues/known-issues.md`（已知限制与待验证项）
- `../architecture.md`（当前模块边界与主链路）
- `../../decisions/ADR-002-legacy-code-as-reference-only.md`（旧代码使用原则）
- `./performance-presets.md`（Phase 9 Performance Preset 系统现状）
- `./keyboard-mapping.md`（键盘映射现状）
- `./plugin-hosting.md`（插件宿主现状）

### 已阅读的 devpiano 源码模块

- `source/Recording/RecordingEngine.h`：`PerformanceEvent`（`timestampSamples` + `message`）、`RecordingTake`（`sampleRate` + `lengthSamples` + `events`）、录制/回放状态机
- `source/Recording/MidiFileExporter.h`：`exportTakeAsMidiFile(take, destination, ppq)`，支持录制 take 导出
- `source/Recording/MidiFileImporter.*`：导入标准 MIDI 文件，自动选择 note-rich track，转换为 `RecordingTake`
- `source/Recording/WavFileExporter.*`：离线渲染链路
- `source/Export/ExportFlowSupport.*`：默认文件名生成、空 take 判断、导出选项构建
- `source/Settings/SettingsModel.h`：持久化字段（音频/性能/插件恢复/键盘布局、MIDI 导入/导出路径、主窗口尺寸）
- `source/Settings/SettingsStore.*`：XML 读写
- `source/UI/ControlsPanel.cpp`：Record / Stop / Play / Back / Import MIDI / Export MIDI / Export WAV 按钮
- `source/MainComponent.cpp`：通过 `RecordingFlowSupport` 管理录制/回放状态，包含 MIDI 导入入口、导入 playback take 与导出 take 解耦
- `source/Core/AppState.h`：运行时状态快照，无持久化演奏文件路径字段
- `source/Audio/AudioEngine.*`：音频引擎，fallback synth 存在
- `source/Layout/PerformancePreset.*`：Performance Preset 文件读写与序列化

### 已搜索或阅读的 freepiano-src/ 旧源码范围

**已确认阅读：**

- `freepiano-src/song.cpp`（完整，1575 行）：录制/回放/保存/打开，`song_event_t`（`double time + byte a/b/c/d`），固定 1M 事件缓冲区，`.fpm` 格式（magic: `"FreePianoSong"`，zlib 压缩），`.lyt` 格式（magic: `"iDreamPianoSong"`，二进制布局文件），`song_open()` / `song_save()` / `song_open_lyt()` 函数
- `freepiano-src/song.h`：事件类型定义（`SM_NOTE_ON`/`SM_NOTE_OFF`/`SM_MIDI_NOTEON`/`SM_MIDI_NOTEOFF` 等），录制/回放 API 声明
- `freepiano-src/export_wav.cpp`（663 行）：WAV 导出实现，使用 Windows `CWaveFile` 类
- `freepiano-src/export.cpp`：导出状态标志（`exporting` 布尔），非常薄
- `freepiano-src/midi.cpp`（304 行）：MIDI 输入/输出设备管理，`note_states[16][128]`，无标准 MIDI 文件读写
- `freepiano-src/gui.cpp`：文件对话框逻辑，`.fpm` / `.lyt` 扩展名处理，菜单项

**关键词搜索结果：**

- `midifile` / `MidiFile` / `SMF`：0 结果——旧 FreePiano **没有**标准 MIDI 文件导入/导出实现
- `midi.*import` / `import.*midi`：0 结果——没有 MIDI 文件导入功能
- `song_open` / `song_save`：有实现，操作的是 `.fpm` 私有格式
- `recent` / `playlist`：0 结果——没有最近文件列表或播放列表
- `file.*dialog` / `open.*file`：gui.cpp 中有 Windows 文件对话框，用于 `.fpm` / `.lyt`

### 本轮未覆盖或无法确认的范围

- 旧 FreePiano 是否有 MIDI 文件**导出**到标准 .mid 格式：无法确认，未找到 `MidiFile` 相关代码
- 旧 FreePiano 的完整 GUI 截图或功能描述：无
- 旧 FreePiano 是否支持 `.mid` 文件打开回放：**未发现**，只有 `.fpm` / `.lyt`
- 旧 FreePiano 是否有"最近文件"UI 功能：**未发现**

---

## 4. devpiano 当前能力摘要

### 录制
- **已实现**
- 依据：`RecordingEngine` + `RecordingFlowSupport` + `ControlsPanel` Record/Stop 按钮
- 说明：可录制电脑键盘产生的 MIDI 事件，存入 `RecordingTake.events`，sample-based 时间线

### 回放
- **已实现**
- 依据：`RecordingEngine::startPlayback()` / `renderPlaybackBlock()` + `ControlsPanel` Play 按钮
- 说明：录制内容可通过 fallback synth 或已加载 VST3 插件回放，事件注入 `AudioEngine` 同一 MIDI 链路

### MIDI 导出
- **已实现**
- 依据：`MidiFileExporter::exportTakeAsMidiFile()` + `ControlsPanel` Export MIDI 按钮
- 说明：可将 `RecordingTake` 导出为标准 MIDI Type 1 文件，960 PPQ，无 tempo map

### WAV 导出
- **已实现（fallback synth）**
- 依据：`WavFileExporter` + Phase 3-1 + 测试 E.1–E.9 全部通过
- 说明：离线渲染 fallback synth 输出到 WAV，支持 44100Hz / 16bit / stereo

### Fallback synth
- **已实现**
- 依据：`AudioEngine` 中的内置 synth，可独立发声
- 说明：无可用 VST3 插件时 fallback synth 作为默认发声来源

### VST3 插件宿主
- **基本实现**
- 依据：`PluginHost` + `AudioPluginInstance` + Phase 2 验收通过
- 说明：可扫描、加载、卸载 VST3 插件，驱动插件发声，支持 editor 窗口

### Performance Preset
- **已实现**
- 依据：Phase 9a + `PerformancePreset.*` + `.devpiano.preset` JSON
- 说明：内置 + 用户 preset 的保存/加载/导入/重命名/删除/启动恢复

### 当前 PerformanceEvent / timeline 模型
- **已实现**
- 依据：`RecordingEngine.h` 中 `PerformanceEvent` + `RecordingTake`
- 说明：`timestampSamples`（int64）+ `juce::MidiMessage` + `RecordingEventSource`，sample-based 时间线

### 文件打开/保存能力（演奏数据）
- **部分实现**
- 依据：`MidiFileImporter` + `MainComponent::handleImportMidiClicked()` + `SettingsModel::lastMidiImportPath`
- 说明：可打开标准 `.mid` 文件并在当前播放链路回放；导入内容作为 playback take，不作为可导出的录制 take；私有演奏文件保存/打开仍未实现

### 设置持久化能力
- **已实现**
- 依据：`SettingsModel` + `SettingsStore` + XML 持久化
- 说明：音频设备、ADSR、插件搜索路径、键盘布局、最近 MIDI 导入/导出路径和主窗口尺寸均有持久化；导出 MIDI/WAV 会复用上次导出目录

---

## 5. 旧 FreePiano 相关能力摘要

### MIDI 导入
- **未发现**
- 依据：搜索 `midifile`/`MidiFile`/`smf` 关键词，0 结果
- 说明：旧 FreePiano 没有标准 MIDI 文件（.mid/.smf）导入功能。只有 `.fpm`（私有格式）和 `.lyt`（布局文件）

### MIDI 导出
- **不确定**
- 依据：搜索 `midifile`/`MidiFile`/`export.*mid`，0 结果
- 说明：未找到标准 MIDI 文件导出的源码证据。`export.cpp` 仅含导出状态标志，不是导出格式定义。旧 FreePiano 导出能力以 WAV/MP4 为主

### Song 文件
- **确认存在**
- 依据：`song.cpp` 中 `song_open()` / `song_save()`，magic: `"FreePianoSong"`，zlib 压缩事件数组
- 说明：`.fpm` 文件是旧 FreePiano 的私有工程格式，包含 song_event_t 数组（time + a/b/c/d），zlib 压缩存储，包含 title/author/comment 元数据，包含 instrument 名称

### 工程/歌曲保存与打开
- **确认存在**
- 依据：`song_open()` / `song_save()`
- 说明：可保存/打开 `.fpm` 文件，包含完整演奏事件、keymap、setting groups、key labels、colors

### 最近文件
- **未发现**
- 依据：搜索 `recent` / `playlist`，0 结果
- 说明：旧 FreePiano 未实现最近文件列表或播放列表功能

### 录制
- **确认存在**
- 依据：`song_start_record()` / `song_stop_record()` + 固定 1M 事件缓冲区
- 说明：录制时自动快照 config（setting groups、keymap、labels、colors），以 `song_event_t` 存入缓冲区

### 回放
- **确认存在**
- 依据：`song_start_playback()` / `song_stop_playback()` + `song_update()`
- 说明：按 `song_timer`（秒级）线性扫描事件缓冲区并触发 `song_send_event()`

### 事件表示
- **确认存在**
- 依据：`song_event_t { double time; byte a; b; c; d; }`
- 说明：时间单位为秒（double），a/b/c/d 编码消息类型+通道+数据，与 `key_bind_t` 共用相同字节编码

### Tempo / tick / PPQ / track / channel
- **部分确认**
- 依据：`song_event_t.time` 为 double 秒，`SM_MIDI_NOTEON` 等 channel 字段在 a 的低 4 位
- 说明：无标准 MIDI PPQ 概念；无多轨（所有事件在单一时间线）；channel 编码在 a 的低 4 位

### 音频导出
- **确认存在**
- 依据：`export_wav.cpp`（663 行）+ `export_mp4.cpp`
- 说明：WAV 导出通过 `song_update()` 驱动实时渲染；MP4 导出也存在于旧系统

### 其他与 Phase 4 相关的用户功能
- **.lyt 布局文件**：二进制格式（magic: `"iDreamPianoSong"`），含 keymap entries（DIK 扫描码 → key_bind_t）
- **多 setting groups**：每组含 octave shift、transpose、velocity、output channel、follow_key、key_signature
- **Playback speed**：可调 `song_play_speed`
- **Auto pedal timer**：`song_auto_pedal_timer`
- **Sync/delay events**：高级事件延迟和同步机制

---

## 6. devpiano vs FreePiano 差距表

| 功能 | devpiano 当前状态 | FreePiano 旧功能状态 | 差距 | 是否适合 Phase 4 | 原因 |
|---|---|---|---|---|---|
| **MIDI 导入** | **已实现**（Phase 4-1/4-2：导入并自动选择 note-rich track） | **未发现**（无 .mid 导入） | devpiano 已超过旧实现 | 已完成 | 复用 `RecordingTake` 与回放链路 |
| **MIDI 导出** | **已实现**（MidiFileExporter） | **不确定**（无证据） | 差距：devpiano 已超过旧实现 | 不做 | 已完成 |
| **MIDI roundtrip** | **已定义边界**（导入 playback take 禁止再导出 MIDI，允许导出 WAV） | **未发现** | 差距：导入 → 回放可用；导入 → MIDI 再导出明确不作为用户功能 | **不实现 MIDI 再导出** | 用户已有原始 `.mid` 文件；Import MIDI 后 Export MIDI 保持 disabled 是预期行为；Export WAV 可用 |
| **Song 文件保存** | **未实现** | **已实现**（.fpm 私有格式） | 差距：devpiano 无任何演奏文件保存 | **可选** | 轻量 JSON 格式可行，但需设计；与 MIDI import 是不同方向 |
| **Song 文件打开** | **未实现** | **已实现**（.fpm + .lyt） | 差距：devpiano 无文件打开能力 | **不推荐** | .fpm 格式耦合旧 Windows 平台代码，不应直接迁移 |
| **最近文件** | **未实现** | **未发现** | 差距：devpiano 无最近文件列表 UI | **可选** | 属于 UI 体验增强，低成本，但非核心功能 |
| **最近导入/导出路径** | **已实现**（导入路径、导出路径均已接入） | **未发现** | devpiano 已具备轻量路径记忆 | 已完成 | Phase 4-5 已补齐导出 FileChooser 复用上次导出目录 |
| **录制后编辑** | **未实现** | **部分**（录制时快照 config） | 差距：devpiano 完全不做编辑 | **不推荐** | 编辑器复杂度高，应 defer |
| **回放控制** | **已实现基础增强**（Record/Stop/Play/Back） | **有额外**（speed、sync/delay） | 差距：devpiano 仍无 speed/sync/delay 等高级控制 | Phase 4-5 已完成基础 Back | 播放中点击 Back 会从开头重新播放 |
| **Tempo map** | **未实现** | **未发现** | 差距：无 tempo map | **不推荐** | 复杂度高，涉及文件格式变更 |
| **多轨** | **部分兼容**（自动选择 note 最多的单轨；不合并全部轨道） | **未发现**（单时间线） | 差距：无完整多轨模型 | **搁置 merge-all** | 当前“选择 note 最多的单轨”是最合适的默认模式；merge-all 单 timeline 有听感风险，后续再考虑 |
| **Velocity / channel** | **已实现**（MIDI 消息级别） | **已实现** | 无差距 | 不做 | 已完备 |
| **Sustain pedal** | **部分实现**（实时 MIDI/录制路径可承载 CC64；MIDI 文件导入暂不收集非 note 事件） | **已实现** | 差距：导入外部 MIDI 时 sustain 表现可能丢失 | **可选** | 后续可在 importer 中导入安全 channel voice 消息 |
| **WAV 导出** | **已实现**（fallback synth） | **已实现** | 无差距 | 不做 | 已完成 |
| **VST3 插件离线渲染** | **未实现**（Phase 3-2 搁置） | **不确定** | 差距：VST3 离线渲染未实现 | **应 defer** | 设计评估已完成但未验证，不应在 Phase 4 同时推进 |
| **文件拖拽打开 MIDI** | **未实现** | **未发现** | 差距：无拖拽支持 | **不推荐** | 需要额外 UI 和事件处理；非核心功能 |

---

## 7. Phase 4 候选功能清单

### 候选 1：MIDI 文件导入 MVP

- **推荐级别**：强烈推荐
- **建议阶段**：Phase 4-1
- **用户价值**：高——用户可以打开任意 .mid 文件在当前播放链路回放
- **实现成本**：低
- **风险**：低——不修改现有录制/导出链路
- **是否依赖旧 FreePiano 行为参考**：否（旧 FreePiano 无此功能）
- **是否需要修改现有数据模型**：否（`RecordingTake` 已支持导入路径）
- **是否需要新增 UI**：是（`ControlsPanel` Import MIDI 按钮 + `MainComponent` handler + FileChooser）
- **是否需要新增测试文档**：是（`./recording-playback.md` 新增包 F）
- **可能涉及的 devpiano 文件**：`source/Recording/MidiFileImporter.*`（新增）、`ControlsPanel` 新增按钮、`MainComponent` 新增 handler
- **可复用的现有模块**：`RecordingTake`、`RecordingEngine::startPlayback()`、`MidiFileExporter`（逆向理解 `juce::MidiFile` 结构）
- **验收标准**：选择一个 .mid 文件 → 导入 → 回放 → 听到音符；Stop 后 Export MIDI 保持 disabled、Export WAV 可用；Recording / Playing 期间 Import MIDI 禁用
- **不做范围**：不编辑、不做 tempo map、不做多轨、不保留原始 MIDI 结构（元事件等）

### 候选 2：MIDI import → playback roundtrip 边界

- **推荐级别**：已收敛为边界规则
- **建议阶段**：Phase 4-4 文档/测试边界收敛
- **用户价值**：中——明确导入 MIDI 是播放用途，不混入录制导出链路
- **实现成本**：低（保持当前 Export disabled 行为并记录测试）
- **风险**：低
- **是否依赖旧 FreePiano 行为参考**：否
- **是否需要修改现有数据模型**：否
- **是否需要新增 UI**：与候选 1 合并
- ** racial 建议**：保留导入 take 独立于录制 take
- **验收标准**：导入 .mid → 回放正常；Stop 后 Export MIDI 仍 disabled、Export WAV 可用；不提供“导入 MIDI → 再导出 MIDI”的用户路径

### 候选 3：最近导入/导出路径记忆

- **推荐级别**：可选
- **建议阶段**：Phase 4-1 或 Phase 4-4
- **用户价值**：中——避免每次从默认路径开始
- **实现成本**：低
- **风险**：低
- **可能涉及的 devpiano 文件**：`source/Settings/SettingsModel.h`、`source/Settings/SettingsStore.cpp`
