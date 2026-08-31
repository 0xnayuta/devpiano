# Phase 26 Completion Record — MIDI 多轨并轨与综合时间线合并

> 归档日期：2026-08-29
> 来源：[`../roadmap/current-iteration.md`](../roadmap/current-iteration.md) Phase 26
> 后续方向：`AUDIT-002` 全面代码质量审计与修复排期，详见 [`../roadmap/current-iteration.md`](../roadmap/current-iteration.md) 与 [`../roadmap/roadmap.md`](../roadmap/roadmap.md)

---

## 背景与目标

在完成 v1.0.0 正式发布与 Phase 25 Linux 双平台桌面构建与分发适配后，devpiano 进入 **Post-v1.0.0 平台拓展与高阶能力演进** 的核心业务能力阶段。

在旧版本中，MIDI 导入（`MidiFileImporter`）仅支持单轨选择（`chooseNoteRichTrack`），将其余音轨直接丢弃，导致双手分轨钢琴曲与多乐器伴奏 MIDI 只能回放单轨。Phase 26 彻底重构了 MIDI 导入与时间线合并机制，构建了统一纯静态无共享状态的 `MidiTrackMergeEngine`，支持标准 Type 0 / Type 1 MIDI 全轨道无损并轨、智能通道重映射、16 通道矩阵独立控制、虚拟键盘多通道色彩联动与全轨离线渲染。

---

## 子阶段完成详情

### Phase 26-A：`MidiTrackMergeEngine` 核心实现与全轨事件绝对时间戳归并
- 提取并实现纯静态无状态的 `MidiTrackMergeEngine` 内核（`source/Recording/MidiTrackMergeEngine.h/.cpp`），彻底替换旧单轨选择逻辑；
- 支持跨音轨（包含 Conductor/Tempo Track 0 与所有 Note/CC 音乐轨）按采样点绝对时间戳（`timestampSamples`）执行时间线稳定归并；
- 完整保留并对齐跨轨 Note On/Off、CC（CC64 延音等）、Pitch Bend、Program Change 事件，按 MIDI 事件优先级稳定排序，保证播放顺序与时序绝对稳定。

### Phase 26-B：多轨通道策略（Pass-through / Auto-Assignment）与跨轨 Meta 解析
- 支持双通道策略：
  - **原始通道保持（Pass-through）**：保留 MIDI 文件内原有的 Channel 映射；
  - **音轨转通道自动重映射（Track-to-Channel Auto-Assignment）**：当各轨均使用 Ch 1 且检测到多轨有 Note 时，自动将不同 Track 映射分配至独立 MIDI 通道 1~16；
- 完整提取并整合跨轨 Meta 信息（乐曲标题、音轨名称、Tempo Map、调号与拍号），并在导入摘要与系统日志中清晰呈现；
- 规范首音 0s 预备（Pre-roll）与各轨初始 Program Change / Controller 状态重置。

### Phase 26-C：16 通道矩阵控制与 88 键虚拟键盘多音轨多着色联动
- 联动 16 通道 MIDI 矩阵（`ChannelMatrix`），支持对导入多轨各通道独立应用移调、八度偏移、音量加权与静音/独奏；
- 联调 88 键虚拟键盘（`CustomKeyboard`）的 Channel 着色模式（`KeyColourMode::channel`），直观呈现不同音轨/声部的动态交互。

### Phase 26-D：全轨 WAV 离线渲染验证与多轨测试套件全覆盖
- 严格遵循只读 Playback Take 契约（保持 Export MIDI disabled，防止有损二次转换破坏原 MIDI Meta/Track 结构），支持全轨合并流直接离线导出为高质量 WAV 音频；
- 补齐多轨 MIDI 导入专项单元测试套件（`source/tests/MidiFileImporterTest.cpp`、`source/tests/RecordingEngineTest.cpp` 等），覆盖 Type 0、Type 1 双手钢琴分轨、多乐器交响、Conductor Track 跨轨速度变化等真实测试夹具。

---

## 验证与验收

- 单元测试：`./scripts/dev.sh test` 62 类全部通过，新增 MidiTrackMergeEngine 专项回归用例；
- 格式化合规：`./scripts/dev.sh format --check` 0 差异；
- 构建状态：`./scripts/dev.sh wsl-build` 0 错误 0 警告。
