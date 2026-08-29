# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

**Phase 26：MIDI 多轨并轨与综合时间线合并（MIDI Multi-Track Timeline Merge） [已完成，2026-08-29]**

在完成 v1.0.0 正式发布与 Phase 25 Linux 双平台桌面构建与分发适配后，devpiano 进入 **Post-v1.0.0 平台拓展与高阶能力演进** 的核心业务能力阶段。
当前 MIDI 导入（`MidiFileImporter`）仅支持单轨选择（`chooseNoteRichTrack`），将其余音轨直接丢弃，导致双手分轨钢琴曲与多乐器伴奏 MIDI 只能回放单轨。本阶段将彻底重构 MIDI 导入与时间线合并机制，构建统一的 `MidiTrackMergeEngine`，支持标准 Type 0 / Type 1 MIDI 全轨道无损并轨、智能通道重映射、16 通道矩阵独立控制、虚拟键盘多通道色彩联动与全轨离线渲染：

1. **Phase 26-A：`MidiTrackMergeEngine` 多轨时间线精准合并内核**：
   - 提取并实现 `MidiTrackMergeEngine`，替换单一主轨选择逻辑；
   - 支持跨音轨（包含 Conductor/Tempo Track 0 与所有 Note/CC 音乐轨）按微秒/采样点绝对时间戳（`timestampSamples`）执行时间线稳定归并；
   - 完整保留并对齐跨轨 Note On/Off、CC（CC64 延音等）、Pitch Bend、Program Change 事件，保证播放顺序与时序绝对稳定。
2. **Phase 26-B：多轨通道策略与跨轨元数据提取（Track-to-Channel & Meta Parsing）**：
   - 支持双通道策略：**原始通道保持（Pass-through）** 与 **音轨转通道自动重映射（Track-to-Channel Auto-Assignment，当各轨均使用 Ch 1 时将不同 Track 分配至独立 MIDI 通道 1~16）**；
   - 提取并整合跨轨 Meta 信息（乐曲标题、音轨名称、Tempo Map、调号与拍号），并在导入摘要与系统日志中清晰展现；
   - 规范首音 0s 预备（Pre-roll）与各轨初始 Program Change / Controller 状态重置。
3. **Phase 26-C：16 通道矩阵控制与 88 键虚拟键盘多音轨多着色联动**：
   - 联动 16 通道 MIDI 矩阵（`ChannelMatrix`），支持对导入多轨各通道独立应用移调、八度偏移、音量加权与静音/独奏；
   - 联调 88 键虚拟键盘（`CustomKeyboard`）的 Channel 着色模式（`KeyColourMode::channel`），直观呈现不同音轨/声部的动态交互。
4. **Phase 26-D：全轨 WAV 离线渲染与单元测试全覆盖**：
   - 严格遵循只读 Playback Take 契约（保持 Export MIDI disabled，防止有损二次转换破坏原 MIDI Meta/Track 结构），支持全轨合并流直接离线导出为高质量 WAV 音频；
   - 补齐多轨 MIDI 导入专项单元测试套件（覆盖 Type 0、Type 1 双手钢琴分轨、多乐器交响、Conductor Track 跨轨速度变化等真实测试夹具）。

---

## 本轮子任务排期（Phase 26）

- [x] **Phase 26-A：`MidiTrackMergeEngine` 核心实现与全轨事件绝对时间戳归并**
- [x] **Phase 26-B：多轨通道策略（Pass-through / Auto-Assignment）与跨轨 Meta 解析**
- [x] **Phase 26-C：16 通道矩阵控制与 88 键虚拟键盘多音轨多着色联动**
- [x] **Phase 26-D：全轨 WAV 离线渲染验证与多轨测试套件全覆盖**

---

## 后续规划路线（Upcoming Backlog）

- **Phase 27：现实物理演奏交互与声学控制（Physical Voicing & Realistic Acoustic Interaction）**：
  - **琴盖开合度交互控制（Lid Position）**：在 JIVE UI 界面接入 Full Open / Half Stick / Closed 3 态直观选择，无缝切换底层已实现的 `lidAcoustics` 多级高频滚降与近场反射；
  - **弱音/移位踏板物理拟真（Una Corda / Soft Pedal，CC 67）**：在 `PianoSynthVoice` 中模拟击弦机右移 3 弦敲 2 弦与毛毡侧面软化物理机理，支持 CC 67 踏板信号与 UI 软踏板状态点亮；
  - **触键力度曲线（Touch Velocity Curve）**：在 `KeyboardMidiMapper` / Input 层提供 Standard / Light / Heavy / Wide Dynamic 4 种配重手感映射，自适应薄膜/机械轴/MIDI 键盘；
  - **配置持久化与预设系统联动**：将琴盖位置、Una Corda 状态与触键曲线完整纳入 `SettingsModel`、`SettingsSerialization` 与 `PerformancePreset`（`.devpiano.preset` JSON）。

---

## 历史实现 Backlog

- Phase 25 完成记录（Linux 原生桌面构建与音频驱动适配）：[`../archive/phase25-linux-desktop-and-audio-path.md`](../archive/phase25-linux-desktop-and-audio-path.md)
- Post-v1.0.0 文档体系治理与打包流水线自动化完成记录：[`../guides/release-workflow.md`](../guides/release-workflow.md)
- Phase 24 完成记录（生命力与非线性动力学绽放）：[`../archive/phase24-vitality-and-dynamic-blooming.md`](../archive/phase24-vitality-and-dynamic-blooming.md)
- Phase 23 完成记录（大师级音色校准与 Pianoteq 对齐精调）：[`../archive/phase23-master-voicing-realism-calibration.md`](../archive/phase23-master-voicing-realism-calibration.md)
- Phase 22 完成记录（物理声学极致深化与机械拟真）：[`../archive/phase22-physical-modeling-acoustic-refinement.md`](../archive/phase22-physical-modeling-acoustic-refinement.md)
- Phase 21 完成记录（踏板交感共鸣与琴盖空间声学）：[`../archive/phase21-sympathetic-resonance-lid-acoustics.md`](../archive/phase21-sympathetic-resonance-lid-acoustics.md)
- Phase 20 完成记录（微观物理动力学：纵向波先驱声与击键混沌微扰）：[`../archive/phase20-longitudinal-ping-micro-variation.md`](../archive/phase20-longitudinal-ping-micro-variation.md)
- Phase 19 完成记录（立体声音板共鸣箱与同音三弦微动力学）：[`../archive/phase19-stereo-modal-soundboard.md`](../archive/phase19-stereo-modal-soundboard.md)
- Phase 18 完成记录（88 键物理参数化与微观相位色散）：[`../archive/phase18-per-note-voicing-micro-phases.md`](../archive/phase18-per-note-voicing-micro-phases.md)
- Phase 17 完成记录（真实物理打击感钢琴音源重构）：[`../archive/phase17-physical-strike-hammer-piano.md`](../archive/phase17-physical-strike-hammer-piano.md)
- Phase 16 完成记录（虚拟键盘局部脏矩形重绘与预设覆盖确认）：[`../archive/phase16-keyboard-dirty-repaint-preset-confirm.md`](../archive/phase16-keyboard-dirty-repaint-preset-confirm.md)
- Phase 15 完成记录（声明式弹窗与设置面板重构）：[`../archive/phase15-declarative-dialogs-and-settings-jive.md`](../archive/phase15-declarative-dialogs-and-settings-jive.md)
- Phase 12–14 完成记录（内置物理建模钢琴音源三部曲）：[`../archive/phase12-14-builtin-piano-synthesis.md`](../archive/phase12-14-builtin-piano-synthesis.md)
- AUDIT-001 修复阶段归档：[`../archive/audit-001-code-quality-fix-phases.md`](../archive/audit-001-code-quality-fix-phases.md)
- Phase 11 完成记录（声明式 UI 架构）：[`../archive/phase11-declarative-ui-jive.md`](../archive/phase11-declarative-ui-jive.md)
