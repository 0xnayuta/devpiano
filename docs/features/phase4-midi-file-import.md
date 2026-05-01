# Phase 4: MIDI 文件能力与 FreePiano 功能差距审计

> 用途：比较 devpiano 当前状态与旧 FreePiano 在 MIDI 文件、演奏数据、song 文件、导入导出、打开保存方面的差距，并筛选适合 Phase 4 的小型扩展候选。
> 当前状态：Phase 4 已进入实现与验收阶段；本文同时记录差距审计、已实现状态、剩余计划和边界。
> 读者：需要理解 Phase 4 阶段边界的开发者、规划者。

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
- 外部 MIDI 硬件验证：因无硬件暂缓

继续在 Phase 3 下追加"MIDI 导入"会模糊 Phase 3 的完成状态，且 MIDI 导入的用途（打开外部 MIDI 文件回放）与 Phase 3 的录制/回放闭环性质不同，独立命名更清晰。

### Phase 4 与各阶段的边界

| 阶段 | 核心目标 |
|------|---------|
| Phase 1–2 | 基础演奏、插件、键盘映射 |
| Phase 3 | 录制 / 回放 / MIDI 导出 / WAV 离线渲染 / 布局 Preset |
| **Phase 4** | **MIDI 文件导入 / 演奏数据保存与打开** |
| Phase 5+ | 架构收敛、复杂编辑、多轨、tempo map、完整工程系统 |

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

- `docs/roadmap/roadmap.md`（Phase 1–4 阶段状态、当前重点）
- `docs/roadmap/current-iteration.md`（Phase 4 当前迭代状态、已完成验收与剩余方向）
- `docs/features/phase3-recording-playback.md`（Phase 3 设计与实现状态、验收标准）
- `docs/testing/acceptance.md`（Phase 1–3 验收状态）
- `docs/testing/known-issues.md`（已知限制与待验证项）
- `docs/architecture/overview.md`（当前模块边界与主链路）
- `docs/decisions/0002-legacy-code-as-reference-only.md`（旧代码使用原则）
- `docs/features/phase3-layout-presets.md`（Phase 3 preset 系统现状）

- `docs/features/phase2-keyboard-mapping.md`（键盘映射现状）

- `docs/features/phase2-plugin-hosting.md`（插件宿主现状）

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
- `source/Layout/LayoutPreset.*`：preset 文件读写

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
- 说明：可录制电脑键盘 + 外部 MIDI 输入产生的 MIDI 事件，存入 `RecordingTake.events`，sample-based 时间线

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

### 外部 MIDI 输入
- **已实现（代码通路）**
- 依据：`MidiRouter` + `MidiMessageCollector` + `AudioEngine` 链路
- 说明：外部 MIDI 设备可驱动插件发声；验证因无硬件暂缓（`known-issues.md` §1）

### 布局 Preset
- **已实现**
- 依据：Phase 3 + `LayoutPreset.*` + `.freepiano.layout` JSON
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
| **外部 MIDI 输入录制** | **已实现**（代码通路） | **已实现** | 无差距（验证因硬件暂缓） | 不做 | 已接入，硬件限制非代码问题 |
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
- **是否需要新增测试文档**：是（`docs/testing/phase3-recording-playback.md` 新增包 F）
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
- **是否需要新增测试文档**：是（覆盖导入 playback take 不可再导出 MIDI、可导出 WAV）
- **可能涉及的 devpiano 文件**：同候选 1
- **可复用的现有模块**：`RecordingEngine::startPlayback()` + 当前导出按钮 enable/disable 状态流
- **验收标准**：导入 .mid → 回放正常；Stop 后 Export MIDI 仍 disabled、Export WAV 可用；不提供“导入 MIDI → 再导出 MIDI”的用户路径
- **不做范围**：不把导入 playback take 转成可再导出的 MIDI 录制 take；VST3 插件音色离线渲染仍归 Phase 3-2 后续计划

### 候选 3：最近导入/导出路径记忆

- **推荐级别**：可选（推荐与候选 1 同轮完成）
- **建议阶段**：Phase 4-1 或 Phase 4-4
- **用户价值**：中——避免每次从默认路径开始
- **实现成本**：低
- **风险**：低
- **是否依赖旧 FreePiano 行为参考**：否
- **是否需要修改现有数据模型**：是（`SettingsModel` 新增字段）
- **是否需要新增 UI**：是（持久化路径，下次 FileChooser 默认定位）
- **是否需要新增测试文档**：否（手工验证即可）
- **可能涉及的 devpiano 文件**：`SettingsModel.h`（新字段）、`SettingsStore.*`（读写）、`MainComponent` 或 `ExportFlowSupport`（使用路径）
- **可复用的现有模块**：`SettingsModel` / `SettingsStore` 已有持久化框架
- **验收标准**：导入一个 .mid 文件后，再次点击 Import MIDI 时 FileChooser 默认定位于上次导入的目录
- **不做范围**：不实现最近文件列表 UI（只记路径）

### 候选 4：轻量演奏文件保存

- **推荐级别**：可选（不推荐作为 Phase 4 优先项）
- **建议阶段**：Phase 4 后续或 Phase 5+
- **用户价值**：中——保存当前演奏数据
- **实现成本**：中（需要设计文件格式和 UI）
- **风险**：中
- **是否依赖旧 FreePiano 行为参考**：部分参考（`RecordingTake` 可直接序列化）
- **是否需要修改现有数据模型**：否（`RecordingTake` 已完备）
- **是否需要新增 UI**：是（Save Performance 按钮 + FileChooser）
- **是否需要新增测试文档**：是
- **可能涉及的 devpiano 文件**：`source/Recording/PerformanceFile.*`（新增）
- **可复用的现有模块**：`RecordingTake` + JSON 序列化
- **验收标准**：保存当前 take 为 `.devpiano-performance.json`，包含所有 events
- **不做范围**：不做完整工程文件（不含 layout/plugin 状态）

### 候选 5：轻量演奏文件打开

- **推荐级别**：可选（不推荐作为 Phase 4 优先项）
- **建议阶段**：Phase 4 后续或 Phase 5+（与候选 4 配套）
- **用户价值**：中
- **实现成本**：中
- **风险**：中
- **是否依赖旧 FreePiano 行为参考**：部分参考
- **是否需要修改现有数据模型**：否
- **是否需要新增 UI**：是（Open Performance 按钮）
- **是否需要新增测试文档**：是
- **可能涉及的 devpiano 文件**：同候选 4
- **可复用的现有模块**：`RecordingTake` + `RecordingEngine`
- **验收标准**：打开 `.devpiano-performance.json` 并回放
- **不做范围**：同候选 4

### 候选 6：MIDI 导入错误提示增强

- **推荐级别**：推荐（与候选 1 同轮完成成本低）
- **建议阶段**：Phase 4-1
- **用户价值**：中——帮助用户理解导入失败原因
- **实现成本**：低
- **风险**：低
- **是否依赖旧 FreePiano 行为参考**：否
- **是否需要修改现有数据模型**：否
- **是否需要新增 UI**：是（错误信息显示）
- **是否需要新增测试文档**：是（覆盖空文件、无 note、多轨文件等边界）
- **可能涉及的 devpiano 文件**：`source/Recording/MidiFileImporter.*`
- **可复用的现有模块**：同候选 1
- **验收标准**：导入空 .mid 时提示"文件不含可演奏事件"；导入多轨文件时提示"已导入第 1 轨，其他轨忽略"
- **不做范围**：不做复杂的错误恢复或自动修复

### 候选 7：回放控制小增强

- **推荐级别**：可选
- **建议阶段**：Phase 4-4 或 Phase 4 后续
- **用户价值**：低~中
- **实现成本**：低
- **风险**：低
- **是否依赖旧 FreePiano 行为参考**：部分参考（song_play_speed）
- **是否需要修改现有数据模型**：否
- **是否需要新增 UI**：是（已新增 `Back` 按钮）
- **是否需要新增测试文档**：否
- **可能涉及的 devpiano 文件**：`ControlsPanel`、`MainComponent`
- **可复用的现有模块**：现有 stop/start playback 路径
- **验收标准**：回放中途点击 `Back` 后，从当前 take 开头重新播放
- **不做范围**：播放速度调整、loop

### 候选 8：MIDI 导入后输出目标选择

- **推荐级别**：不推荐（应 defer）
- **建议阶段**：Phase 5+
- **用户价值**：中
- **实现成本**：中（需要输出路由 UI）
- **风险**：中——涉及 audio engine 路由变更
- **是否依赖旧 FreePiano 行为参考**：否
- **是否需要修改现有数据模型**：可能（需要 output target 枚举）
- **是否需要新增 UI**：是
- **是否需要新增测试文档**：是
- **可能涉及的 devpiano 文件**：`AudioEngine` / `MidiRouter`
- **可复用的现有模块**：现有 fallback synth / plugin 发声路径
- **验收标准**：导入后选择输出到 fallback synth / 已加载 plugin / MIDI output
- **不做范围**：外部 MIDI output 路由在 Phase 4 不做

**原因**：当前架构中导入的 MIDI 通过 `RecordingEngine::startPlayback()` + `AudioEngine::getNextAudioBlock()` 路由到当前已加载插件或 fallback synth，是隐式选择。"显式输出目标"需要对 playback 路径做较大扩展，不适合 Phase 4。

### 候选 9：旧 FreePiano song 文件兼容

- **推荐级别**：不推荐（应 defer 或不做）
- **建议阶段**：Phase 5+ 或明确不做
- **用户价值**：低~中（用户可能有历史 .fpm 文件）
- **实现成本**：高（.fpm 格式复杂，zlib 压缩，版本兼容）
- **风险**：高（容易引入旧平台代码）
- **是否依赖旧 FreePiano 行为参考**：是（必须）
- **是否需要修改现有数据模型**：是（需要支持 .fpm 事件格式）
- **是否需要新增 UI**：是
- **是否需要新增测试文档**：是
- **可能涉及的 devpiano 文件**：大量
- **可复用的现有模块**：无
- **验收标准**：可打开旧 .fpm 文件并回放
- **不做范围**：编辑 .fpm 文件、写入 .fpm 格式

**原因**：.fpm 格式是私有格式，耦合旧 Windows 平台代码（zlib、fread/fwrite、DIK 扫描码），完整兼容成本极高。应 defer 或明确不做，优先使用标准 MIDI 文件作为交互格式。

### 候选 10：完整 MIDI 编辑器 / 钢琴卷帘 / 多轨编辑

- **推荐级别**：不推荐（应 defer）
- **建议阶段**：M10+
- **用户价值**：高（长期）
- **实现成本**：极高
- **风险**：高
- **是否依赖旧 FreePiano 行为参考**：部分
- **是否需要修改现有数据模型**：是（需要多轨、tempo map）
- **是否需要新增 UI**：是（复杂）
- **是否需要新增测试文档**：是（大量）
- **可能涉及的 devpiano 文件**：大量
- **可复用的现有模块**：部分（`RecordingTake` 事件存储可复用）
- **验收标准**：可编辑 note 位置、velocity，添加/删除 note，显示多轨
- **不做范围**：量化、midi learn、automation

**原因**：编辑器复杂度远超 Phase 4 范围，应作为独立大版本目标，不应在 Phase 4 尝试。

### 候选 11：最近文件列表 / 快速打开

- **推荐级别**：可选（但低优先级）
- **建议阶段**：Phase 4 后续
- **用户价值**：中
- **实现成本**：中
- **风险**：低
- **是否依赖旧 FreePiano 行为参考**：否
- **是否需要修改现有数据模型**：是（需要最近文件列表存储）
- **是否需要新增 UI**：是（Recent Files 菜单或子面板）
- **是否需要新增测试文档**：是
- **可能涉及的 devpiano 文件**：`SettingsModel`、`MainComponent`（menu 构建）、新 UI 组件
- **可复用的现有模块**：现有 SettingsStore 持久化框架
- **验收标准**：File 菜单下有最近文件列表（最多 10 个），点击后可打开
- **不做范围**：播放列表、分类管理

### 候选 12：文件拖拽打开 MIDI

- **推荐级别**：不推荐（应 defer）
- **建议阶段**：Phase 4 后续或 Phase 5+
- **用户价值**：中
- **实现成本**：中（JUCE FileDragAndDrop）
- **风险**：低
- **是否依赖旧 FreePiano 行为参考**：否
- **是否需要修改现有数据模型**：否
- **是否需要新增 UI**：是（drag-and-drop 区域）
- **是否需要新增测试文档**：是
- **可能涉及的 devpiano 文件**：`MainComponent`（或独立顶层组件）
- **可复用的现有模块**：同候选 1
- **验收标准**：拖拽 .mid 文件到窗口上触发导入
- **不做范围**：拖拽到非窗口区域

**原因**：需要处理焦点、drop 位置和已有 UI 的交互边界，容易引入复杂 UI 状态管理。优先做 Import 按钮，拖拽可作为后续增强。

---

## 8. 推荐纳入 Phase 4 的最小组合

### 最推荐做什么

**Phase 4 最小组合：MIDI 文件导入 MVP + 导入路径记忆 + playback / tempo / 多轨边界验证**

三项功能同属一个薄层实现，共享 FileChooser 和 `RecordingTake` 填充逻辑：

1. **MIDI 文件导入**（核心）：`juce::MidiFile::readFrom()` → 解析为 `RecordingTake` → 可直接 `startInternalPlayback()`
2. **最近导入路径记忆**（体验增强）：`SettingsModel` 新增 `lastMidiImportPath` 字段，`FileChooser` 复用上次路径
3. **Playback / tempo / 多轨边界验证**（质量保证）：确认导入播放、PPQ/timeFormat、自动选轨与导出语义边界；导入 playback take 明确不开放 MIDI 再导出，但允许导出 WAV

### 为什么

- 三项功能共享同一实现入口（`MidiFileImporter`），增量成本极低
- 形成"打开 / 回放"最小闭环，用户价值立即可见；"导入 / 导出"roundtrip 已明确不作为 MIDI 再导出用户功能
- 不修改任何已有数据模型或核心 audio 链路
- 不引入复杂编辑器或多轨概念
- 与现有 `MidiFileExporter`（逆向）和 `RecordingEngine::startPlayback()`（复用）高度共复用

### 不推荐现在做什么

- 轻量演奏文件保存/打开（.devpiano-performance.json）：需要独立设计，与 MIDI 导入是不同方向
- 旧 .fpm 文件兼容：平台耦合高，应 defer
- 完整 MIDI 编辑器 / 多轨 / piano roll：超出 Phase 4 范围
- 文件拖拽打开：UI 复杂度高，可作为 Phase 4 后续小增强
- VST3 插件离线渲染（Phase 3-2）：已在 Phase 3 搁置，不应在 Phase 4 并行推进

### 哪些功能应 defer

| 功能 | Defer 原因 |
|------|-----------|
| VST3 插件离线渲染（Phase 3-2） | 设计评估已完成但未在真实 VST3 环境验证，不应并行推进 |
| 旧 .fpm 文件兼容 | 平台耦合、格式复杂、用户价值有限 |
| 完整 MIDI 编辑器 / 钢琴卷帘 | 复杂度超出 Phase 4，应为独立大版本 |
| 文件拖拽打开 | UI 交互复杂，可作 Phase 4 后续小增强 |
| 最近文件列表 | UI 复杂度较高，可作 Phase 4 后续 |
| 多轨 / tempo map | 超出 Phase 4 范围 |

---

## 9. 建议拆分的小轮次

### Phase 4-1：MIDI 文件导入核心

- **目标**：实现 `juce::MidiFile` → `RecordingTake` 转换，`Import MIDI` 按钮触发回放
- **修改范围**：`source/Recording/MidiFileImporter.*`（新增）、`ControlsPanel`（新增 Import 按钮）、`MainComponent`（新增 handler）、`SettingsModel`（新增 `lastMidiImportPath` 字段）
- **不修改范围**：录制引擎、回放引擎（复用）、导出引擎（复用）、数据模型结构
- **预期文件**：
  - `source/Recording/MidiFileImporter.h/.cpp`（新增，约 150-250 行）
  - `source/UI/ControlsPanel.h/.cpp`（Import 按钮）
  - `source/MainComponent.cpp`（handler）
  - `source/Settings/SettingsModel.h`（新增字段）
- **验收标准**：
  - [x] 点击 Import MIDI → FileChooser 打开（默认目录为上次导入路径）
  - [x] 选择 .mid 文件后自动开始回放（或设置 `currentTake` 并启用 Play 按钮）
  - [x] 导入不含 note 的 .mid 时写 Logger，安全返回，不崩溃
  - [x] 回放结束状态正确（idle）
  - [x] Import MIDI 后 Stop，Export MIDI 保持 disabled，Export WAV 可用
  - [x] Playing 期间 Import MIDI 禁用；需先 Stop 再导入另一个 MIDI
  - [x] Recording 期间 Import MIDI 按钮禁用，避免录制 take 与导入 playback take 状态冲突
- **风险**：低——纯新增文件，不触碰现有录制/导出链路
- **回滚方式**：删除 `MidiFileImporter.*`，移除 ControlsPanel Import 按钮和 MainComponent handler

#### Phase 4-2：自动选择含 note 最多的轨道（兼容性修正）

- **定位**：Phase 4-1 的兼容性修正子功能，不升级为完整多轨支持。
- **目标**：解决常见 Type 1 MIDI 文件中 track 0 只有 tempo/meta、实际 note 分布在 track 1+ 时导入无声的问题。
- **修改范围**：`source/Recording/MidiFileImporter.*`。
- **行为建议**：
  - 扫描所有轨道的 note on/off 数量。
  - 默认选择 note 事件最多的轨道导入。
  - 继续保持单轨导入到 `RecordingTake` 的 MVP 模型。
  - 保留原始 channel、note number、velocity。
- **不做范围**：
  - 不合并所有轨道。
  - 不保留原始 track 结构。
  - 不做完整 GM 播放器。
  - 不导入 program change / CC / pitch bend / sustain pedal。
  - 不提供 UI 选择轨道。
- **验收标准**：
  - [x] track 0 只有 tempo/meta、track 1+ 有 note 的 MIDI 文件可自动选择有 note 的轨道并回放。
  - [x] Logger 输出总轨数、每轨 note 事件数、选中的 track index、被忽略轨道数。
  - [x] 所有轨道都没有 note 时安全返回失败并写 Logger，不崩溃。
  - [x] 本程序导出的单轨 MIDI 文件仍正常导入回放。
- **风险**：低——仍是单轨导入，不引入多轨合并导致的嘈杂、鼓轨或多音色问题。
- **回滚方式**：恢复默认固定导入 track 0。

### Phase 4-3：Import MIDI 按钮状态收敛

- **目标**：将 Import MIDI 按钮状态纳入统一录制/回放按钮状态刷新
- **状态**：已实现并通过人工验收

### Phase 4-4：MIDI import playback 边界 + 多轨/tempo 处理

- **目标**：确保导入 → 回放质量，明确禁止导入 playback take 再导出 MIDI，并处理多轨和 tempo map 边界
- **状态**：已实现并通过 2026-05-01 人工验收；多轨、非 960 PPQ、tempo meta event 和复杂 tempo map 限制均未发现明显问题。
- **修改范围**：`MidiFileImporter`（多轨处理、tempo map 警告）、导出按钮状态流与文档/测试边界；不需要让 `MidiFileExporter` 支持导入 take 再导出
- **不修改范围**：同 Phase 4-1
- **预期文件**：同 Phase 4-1 的 `MidiFileImporter` 扩展
- **验收标准**：
  - [x] 单轨 .mid：导入 → 回放正常；Stop 后 Export MIDI 保持 disabled，Export WAV 可用
  - [x] 导入 .mid 后不提供"再导出 MIDI"的用户路径；这是预期边界而非缺陷
  - [x] 导入 .mid 后可导出 WAV；VST3 插件音色离线渲染仍归 Phase 3-5 计划并暂时搁置
  - [x] 多轨 .mid：Logger 清晰提示选中轨道和忽略轨道，不崩溃
  - [x] 非 960 PPQ .mid：导入后播放速度不因强制 PPQ 覆盖而明显错误
  - [x] 有 tempo meta event 的 .mid：导入后事件时间线基本符合原文件
  - [x] 复杂 tempo map 的限制已在 Logger 或文档中明确，不误写为完整支持
- **风险**：低
- **回滚方式**：同 Phase 4-1

### Phase 4-5：最近路径记忆 + 回放控制小增强

- **目标**：完善用户体验
- **状态**：已实现，2026-05-01 人工验收通过。
- **修改范围**：`SettingsStore`（读写导入/导出路径字段）、`MainComponent`（导出 FileChooser 默认目录、Back 处理）、`ControlsPanel`（Back 按钮）
- **不修改范围**：数据模型、audio 链路
- **预期文件**：`SettingsStore.*`、`MainComponent.*`、`ControlsPanel.*`
- **验收标准**：
  - [x] Import MIDI 时 FileChooser 默认定位到上次导入的目录
  - [x] Export MIDI / Export WAV 时 FileChooser 默认定位到上次导出目录
  - [x] Playback 中点击 `Back` 后从当前 take 开头重新播放
- **风险**：低
- **回滚方式**：移除新字段和新增方法

### Phase 4-6：合并所有轨道 note 到单一 timeline（已搁置）

- **目标**：在不引入完整多轨数据模型的前提下，将所有轨道中的 note on/off 合并到一个 `RecordingTake` 时间线，提高外部多轨 MIDI 文件的内容完整度。
- **状态**：已搁置。当前"自动选择 note 最多的单轨"已经是更合适的默认模式；merge-all 以后再考虑。
- **建议阶段**：Phase 4 后续/backlog；不作为 Phase 4 默认行为。
- **修改范围**：`MidiFileImporter`（跨轨收集、排序、诊断日志）。
- **不修改范围**：`RecordingTake` 数据结构、`RecordingEngine`、audio callback、完整 GM 播放器。
- **实现要点**：
  - 遍历所有轨道，收集 note on/off。
  - 保留原始 channel、note number、velocity。
  - 合并后按 `timestampSamples` 排序。
  - 同一 timestamp 下优先 note-off，再 note-on，降低 stuck note / 重触发风险。
  - `lengthSamples` 取所有合并事件的最大 timestamp。
- **主要风险**：
  - GM 多轨文件在当前单乐器插件或 fallback synth 上播放时，所有声部会使用同一音色，可能显得拥挤或嘈杂。
  - channel 10 鼓轨用普通音色播放可能听感异常。
  - fallback synth 声部数有限，大型编曲可能 voice stealing。
- **验收标准**：
  - [ ] 多轨 MIDI 文件导入后能听到多个轨道的 note 内容。
  - [ ] 合并后事件顺序稳定，不产生明显 stuck note。
  - [ ] Logger 输出 merge-all 模式、总轨数、每轨 note 数、合并后事件数和时长。
- **回滚方式**：恢复 Phase 4-2 的 auto-select note-rich track 策略。

### Phase 4-7：MIDI playback 虚拟键盘可视化（后续增强）

- **目标**：导入 MIDI 文件并播放时，根据当前 playback note on/off 实时显示虚拟键盘按下/松开的视觉状态。
- **状态**：已实现，2026-05-01 人工验收通过。
- **建议阶段**：Phase 4 后续；作为播放体验增强，不阻塞 Phase 4-1/4-2 导入兼容性修复。
- **修改范围**：`AudioEngine` playback MIDI 渲染后的虚拟键盘状态同步。
- **不修改范围**：MIDI 导入语义、`RecordingTake` 文件结构、导出链路、音频发声路径、audio callback 中的 UI 访问。
- **实现边界**：
  - 只做视觉反馈，不改变实际发声 MIDI buffer。
  - 不从 audio callback 直接操作 UI Component。
  - 可通过 message-thread 可消费的 playback visual state，或安全同步到专用 keyboard display state。
  - 应与电脑键盘实时演奏显示区分清楚，避免播放状态与用户手动按键状态互相覆盖。
- **验收标准**：
  - [x] Import MIDI 后播放时，虚拟键盘能随播放音符实时按下和松开。
  - [x] Stop、播放结束、导入另一个 MIDI、关闭插件或重建音频设备时，虚拟键盘不会残留按下状态。
  - [x] 用户手动按电脑键盘演奏时，既有虚拟键盘显示行为不回退。
  - [x] 不在 audio callback 中直接调用 UI 方法或分配大量内存。
  - [x] 大量 note 事件播放时 UI 仍保持可用，不明显卡顿。
- **风险**：中——需要跨 audio playback 与 UI 展示边界同步状态，重点防止线程安全问题、残留按键和 UI 抖动。
- **回滚方式**：移除 playback visual state 同步逻辑，恢复仅显示实时键盘输入的虚拟键盘行为。

### Phase 4-8：主窗口尺寸自适应与恢复（UI polish，可选）

- **目标**：首次启动时使用能恰好容纳当前主要界面内容的默认窗口尺寸；用户手动调整窗口大小后，下次启动恢复该尺寸。
- **状态**：已实现，2026-05-01 人工验收通过。
- **建议阶段**：Phase 4 后续；UI polish，不阻塞 MIDI 导入/回放兼容性修复。
- **拆分子任务**：
  - **Phase 4-8a：首次启动 preferred window size**
    - 没有保存窗口状态时，根据 Header / PluginPanel / ControlsPanel / KeyboardPanel 等主要内容设置合适默认尺寸。
    - 设置合理最小窗口大小，避免内容被压缩到不可用。
  - **Phase 4-8b：窗口大小持久化恢复**
    - 保存用户手动调整后的窗口宽高。
    - 下次启动优先恢复保存尺寸。
    - 对异常值、过小尺寸或明显不可用尺寸做 clamp。
- **修改范围**：`MainComponent` / application window 初始化逻辑、`SettingsModel`、`SettingsStore`。
- **不修改范围**：音频/MIDI/插件链路、导入导出逻辑、复杂多显示器窗口管理。
- **验收标准**：
  - [x] 首次启动或无保存状态时，窗口默认尺寸能完整呈现当前主要 UI 内容。
  - [x] 用户手动调整窗口大小并关闭程序后，下次启动能恢复该窗口尺寸。
  - [x] 保存的异常尺寸不会导致窗口不可见或小到不可操作。
  - [x] 不高频写盘；窗口尺寸保存应使用已有 settings 保存节流或关闭时保存。
- **风险**：低到中——主要风险是不同 DPI / 缩放 / 屏幕尺寸下的默认尺寸和恢复尺寸边界。
- **回滚方式**：移除窗口尺寸字段和恢复逻辑，恢复固定默认 `setSize()` 行为。

---

## 10. 风险与边界

### MIDI 文件导入的 tempo / PPQ 边界

- **当前设计**：导入时保留 MIDI 文件头中的原始 time format（PPQ 或 SMPTE），通过 `juce::MidiFile::convertTimestampTicksToSeconds()` 转换到秒，再转换为 `RecordingTake` 的 sample timeline。
- **已修复问题**：Phase 4-1 初版曾在读取后强制 `setTicksPerQuarterNote(960)`，会覆盖外部 MIDI 文件的真实 PPQ/SMPTE time format，导致大量非 960 PPQ 文件速度错误或无法正常播放。当前实现不再覆盖 time format。
- **当前效果**：人工验证确认本程序导出的 MIDI 文件、非 960 PPQ 文件、含 tempo meta event 文件和基础复杂 tempo map 限制均未发现明显问题。PPQ/timeFormat 修复已覆盖 Phase 4-4 主要 playback 边界。
- **剩余影响**：SMPTE 边界文件、缺少标准 tempo meta event 的文件和更复杂的外部 MIDI 仍作为后续兼容性观察项；当前 `RecordingTake` 不保存 tempo map 原始结构，导入后只保留已折算后的绝对时间线。
- **未来**：如需精确往返复杂 tempo map，需要在导入器中保留/合并 tempo 信息，或在 `RecordingTake` 之外引入更完整的 MIDI 文件表示。

### 多轨 MIDI 的处理边界

- **当前设计**：Phase 4-1 初版只导入第一轨（track 0），忽略其他轨；Phase 4-2 已作为兼容性修正实现，自动选择含 note 最多的单一轨道。
- **原因**：devpiano 当前是单 timeline 模型，Phase 4-1/4-2 不引入完整多轨模型；默认合并所有轨道可能在当前单乐器播放链路中显得嘈杂。当前决定：继续保留"选择 note 最多的单轨"作为最合适模式。
- **影响**：多轨文件导入后其他轨仍会被忽略；Phase 4-2 已解决常见 track 0 只有 tempo/meta、实际音符在后续轨道时导入无声的问题，但不会保证多轨内容完整。
- **是否需要解决**：常见 track 0 只有 meta 导致无声的问题已通过 Phase 4-2 解决；完整多轨仍不属于 Phase 4-2/4-4。
- **未来**：Phase 4-6 已搁置；若以后重新考虑 merge-all，应作为显式可选导入模式。完整多轨支持则需要在 `RecordingTake` 层面引入 track 概念。

### channel / velocity / sustain pedal 的处理边界

- **当前设计**：导入 note on/off 时保留原始 `juce::MidiMessage` 中的 channel 与 velocity。
- **限制**：Phase 4-1 导入器目前只收集 note on/off；sustain pedal、pitch bend、program change、CC 等非 note 事件暂未导入，因此依赖这些事件表达演奏语义的外部 MIDI 文件可能听起来较生硬或与原文件不同。

### 是否支持非 note 事件

- **当前设计**：Phase 4-1 只支持 note on/off。
- **后续候选**：可在不进入 audio callback 的前提下继续导入安全的 channel voice 消息（如 sustain CC、pitch bend、program change），以改善外部 MIDI 文件回放保真度。
- **sysex**：不支持，导入时丢弃
- **meta 事件**：忽略，不阻塞导入

### 导入 MIDI 后是否应该立即转换为 PerformanceEvent

- **当前设计**：是——导入时直接将 `juce::MidiMessage` 填充到 `RecordingTake.events[]`，时间转换为 sample-based
- **原因**：与录制路径一致，回放和导出逻辑无需区分来源
- **是否需要保留原始 MIDI 文件结构**：不需要——`RecordingTake` 是内部格式，不暴露给用户

### 旧 FreePiano song 格式是否应该兼容

- **建议**：不应在 Phase 4 做，应明确 defer
- **原因**：.fpm 格式耦合旧 Windows 平台代码（zlib、DIK 扫描码、fread/fwrite），完整兼容成本极高且容易引入旧架构问题
- **替代方案**：用户应使用标准 MIDI 文件（.mid）与 devpiano 交互

### 为什么不应该在 Phase 4 做完整编辑器

- **复杂度**：编辑器需要 selection、undo/redo、cut/copy/paste、quantize、snap-to-grid 等，远超 Phase 4 范围
- **数据模型**：当前 `RecordingTake.events` 是 immutable vector，编辑器需要 mutable 结构
- **UI**：需要独立 Component、mouse handling、paint，复杂度高
- **测试**：编辑器需要大量自动化测试，不适合小轮次

### Phase 4 与未来 Phase 5/Phase 6 的边界

| Phase 4 | Phase 5 | Phase 6 |
|---------|---------|---------|
| MIDI 导入（单向） | 轻量演奏文件保存/打开 | 完整工程文件（.devpiano-project） |
| Roundtrip 验证 / playback 可视化增强 | 最近文件列表 | 多轨 / tempo map |
| 最近路径记忆 | 文件拖拽打开 | Piano roll / 事件编辑器 |
| 回放控制小增强 | 基础 MIDI 编辑（delete notes） | 量化 / snap-to-grid |

---

## 11. 最终建议

### 1. Phase 4 当前是否已正式推进？

**是，且已完成核心实现。** Phase 4-1、Phase 4-2、Phase 4-3、Phase 4-4、Phase 4-5、Phase 4-7、Phase 4-8 已实现并通过人工验收；Phase 4-3 已收敛 Import MIDI 与录制 / 回放按钮状态；Phase 4-4 已收敛为"导入 playback take 禁止 MIDI 再导出"的边界规则，并覆盖多轨、非 960 PPQ、tempo meta event 和复杂 tempo map 限制验证；Phase 4-6 已搁置。

### 2. Phase 4 的推荐名称是什么？

**Phase 4：MIDI 文件能力补齐**

或更具体的：**Phase 4：MIDI 文件导入与回放闭环**

### 3. Phase 4 最适合的第一项功能是什么？

**MIDI 文件导入 MVP（Phase 4-1）**，已完成。

`juce::MidiFile::readFrom()` → `RecordingTake` → `startInternalPlayback()`，形成最小可用导入回放闭环。

### 4. MIDI 导入是否适合成为 Phase 4-1？

**是，且已完成。** 原因：
- 实现成本低（150-250 行新代码）
- 可直接复用现有回放链路
- 用户价值立竿见影
- 与录制/导出共享内部 `RecordingTake` 结构，但导入 playback take 明确不开放 MIDI 再导出；导入后允许 WAV 导出
- 不修改任何已有数据模型

### 5. 哪些功能应明确 defer？

以下功能应明确 defer，不在 Phase 4 范围：

| 功能 | Defer 原因 |
|------|-----------|
| VST3 插件离线渲染（Phase 3-2） | 未在真实环境验证，应独立推进；导入 MIDI 后 WAV 导出已走现有 fallback synth 路径，VST3 插件音色离线渲染仍后置 |
| 旧 .fpm 文件兼容 | 平台耦合、格式复杂 |
| 完整 MIDI 编辑器 / 钢琴卷帘 | 超出 Phase 4 范围 |
| 完整多轨支持 | 当前单 timeline 模型不支持；Phase 4-6 merge-all 已搁置，当前保留 note-rich 单轨选择作为默认模式 |
| 完整工程文件（.devpiano-project） | 需要全面设计 |
| 文件拖拽打开 | UI 交互复杂度高 |
| 最近文件列表 UI | 可作为 Phase 4 后续小增强 |
