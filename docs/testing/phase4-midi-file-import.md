# MIDI 文件导入与回放验收测试（Phase 4）

> 用途：记录 Phase 4 MIDI 文件导入、回放、兼容性修正和后续增强的专项验收测试。  
> 读者：开发者、测试者、阶段验收者。  
> 更新时机：Phase 4 导入/回放行为变化、验收结果更新、后续 Phase 4 后续功能实现时。

## 1. 测试范围

覆盖：

- Phase 4-1：MIDI 文件导入核心。
- Phase 4-2：自动选择含 note 最多的轨道。
- Phase 4-3：Import MIDI 按钮状态收敛。
- Phase 4-4：MIDI import playback 边界 + 多轨/tempo 处理。
- Phase 4-5：最近路径记忆 + 回放控制小增强。
- Phase 4-6：合并所有轨道 note 到单一 timeline（已搁置）。
- Phase 4-7：MIDI playback 虚拟键盘可视化。
- Phase 4-8：主窗口尺寸自适应与恢复。

不覆盖：

- 完整 GM 播放器。
- 完整多轨工程模型。
- Piano roll / MIDI 编辑器。
- 旧 FreePiano `.fpm` song 格式兼容。

## 2. 测试环境

- 在 Windows 镜像树中运行 `DevPiano.exe`。
- 构建验证优先使用：

```bash
./scripts/dev.sh wsl-build --configure-only
./scripts/dev.sh win-build
```

- 如链接失败并提示 `DevPiano.exe` 无法写入，先关闭正在运行的程序后重试。

## 3. 测试素材建议

至少准备以下 MIDI 文件：

1. **DevPiano 自导出单轨 MIDI**：由本程序录制后 Export MIDI 生成。
2. **空文件或无 note MIDI**：用于验证错误路径。
3. **Type 1：track 0 只有 tempo/meta，track 1 有 notes**：用于验证 Phase 4-2 自动选轨。
4. **Type 1：多个 track 都有 notes**：用于验证 note-rich track 选择；Phase 4-6 merge-all 已搁置。
5. **非 960 PPQ MIDI**：用于验证不覆盖原始 `timeFormat`。
6. **包含 tempo meta event 的 MIDI**：用于验证时间转换。
7. **大量 note 的 MIDI**：用于观察播放稳定性和 UI 可用性。
8. 可选：包含 sustain CC、pitch bend、program change 的 MIDI，用于记录当前保真度限制。

## 4. Phase 4-1：MIDI 文件导入核心

状态：已实现，需持续回归。

验收项：

- [x] 点击 Import MIDI 后 FileChooser 打开。
- [x] 选择有效 `.mid` 文件后可导入为 `RecordingTake` 并开始回放。
- [x] 本程序导出的单轨 MIDI 可正常导入回放。
- [x] 导入空文件或读取失败文件时写 Logger，不崩溃。
- [x] 导入不含 note 的 MIDI 时写 Logger，不崩溃。
- [x] 回放结束后 UI 状态回到 idle。
- [x] Import MIDI 后 Stop，Export MIDI 保持 disabled，Export WAV 可用。
- [x] Playing 期间 Import MIDI 按钮禁用；需要先 Stop 再导入另一个 MIDI。
- [x] Recording 期间 Import MIDI 按钮禁用，避免录制 take 与导入 playback take 状态冲突。

回归重点：

- 导入失败路径必须安全返回。
- 切换导入文件时必须释放旧播放中的 active notes。
- 导入 playback take 不应开启 Export MIDI，但应允许 Export WAV。
- 录制 / 播放期间只允许 Stop 等安全操作，Import MIDI 不应可点击。

## 5. Phase 4-2：自动选择含 note 最多的轨道

状态：已实现，2026-05-01 人工验收通过。

验收项：

- [x] track 0 只有 tempo/meta、track 1+ 有 note 的 MIDI 文件可自动选择有 note 的轨道并回放。
- [x] Logger 输出总轨数、每轨 note 事件数、选中的 track index、被忽略轨道数。
- [x] 所有轨道都没有 note 时安全返回失败并写 Logger，不崩溃。
- [x] 本程序导出的单轨 MIDI 文件仍正常导入回放。

回归重点：

- 默认仍只导入单一轨道，不合并所有轨道。
- 自动选择策略应选择 note 事件最多的轨道。
- 应保留原始 channel、note number、velocity。

## 6. Phase 4-4：MIDI import playback 边界 + 多轨/tempo 处理

状态：边界已定义并通过 2026-05-01 人工验收；多轨、非 960 PPQ、tempo meta event 和复杂 tempo map 限制均未发现明显问题。

决定：导入的 MIDI playback take 禁止再次导出为 MIDI。用户已有原始 `.mid` 文件，没有“导入后再导出 MIDI”的需求；导入 MIDI 后允许导出 WAV（当前仍走既有 fallback synth WAV 离线渲染路径，VST3 插件离线渲染仍归 Phase 3-2 后续计划）。

验收项：

- [x] 单轨 `.mid`：导入 → 回放正常；Stop 后 Export MIDI 保持 disabled，Export WAV 可用。
- [x] 导入 `.mid` 后不提供“再导出 MIDI”的用户路径；这是预期边界而非缺陷。
- [x] 导入 `.mid` 后可导出 WAV；VST3 插件音色离线渲染仍归 Phase 3-2 并暂时搁置。
- [x] 多轨 `.mid`：Logger 清晰提示选中轨道和忽略轨道，不崩溃。
- [x] 非 960 PPQ `.mid`：导入后播放速度不因强制 PPQ 覆盖而明显错误。
- [x] 有 tempo meta event 的 `.mid`：导入后事件时间线基本符合原文件。
- [x] 复杂 tempo map 的限制已在 Logger 或文档中明确，不误写为完整支持。

## 7. Phase 4-5：最近路径记忆 + 回放控制小增强

状态：已实现，2026-05-01 人工验收通过。

验收项：

- [x] Import MIDI 时 FileChooser 默认定位到上次导入目录。
- [x] Export MIDI / Export WAV 时 FileChooser 默认定位到上次导出目录。
- [x] Playback 中点击 `Back` 后从当前 take 开头重新播放。

## 8. Phase 4-6：合并所有轨道 note 到单一 timeline（已搁置）

状态：未实现 / 已搁置。

决定：当前继续使用 Phase 4-2 的“自动选择 note 最多的单轨”作为默认和推荐模式。merge-all 可能在单乐器播放链路中带来嘈杂、鼓轨和多音色问题，后续再考虑是否作为显式可选导入模式。

验收项：

- [ ] 暂不执行：多轨 MIDI 文件导入后能听到多个轨道的 note 内容。
- [ ] 暂不执行：合并后事件顺序稳定，不产生明显 stuck note。
- [ ] 暂不执行：Logger 输出 merge-all 模式、总轨数、每轨 note 数、合并后事件数和时长。
- [x] 当前单乐器播放链路下的嘈杂、鼓轨和多音色限制已明确记录，Phase 4-6 因此搁置。

## 9. Phase 4-7：MIDI playback 虚拟键盘可视化（后续增强）

状态：已实现，2026-05-01 人工验收通过。

验收项：

- [x] Import MIDI 后播放时，虚拟键盘能随播放音符实时按下和松开。
- [x] Stop、播放结束、导入另一个 MIDI、关闭插件或重建音频设备时，虚拟键盘不会残留按下状态。
- [x] 用户手动按电脑键盘演奏时，既有虚拟键盘显示行为不回退。
- [x] 不在 audio callback 中直接调用 UI 方法或分配大量内存。
- [x] 大量 note 事件播放时 UI 仍保持可用，不明显卡顿。

## 10. Phase 4-8：主窗口尺寸自适应与恢复（后续增强）

状态：已实现，2026-05-01 人工验收通过。

验收项：

- [x] 首次启动或无保存状态时，窗口默认尺寸能完整呈现当前主要 UI 内容。
- [x] 用户手动调整窗口大小并关闭程序后，下次启动能恢复该窗口尺寸。
- [x] 保存的异常尺寸不会导致窗口不可见或小到不可操作。
- [x] 不高频写盘；窗口尺寸保存应使用已有 settings 保存节流或关闭时保存。

## 11. 建议最小回归集合

每次修改 `MidiFileImporter`、播放切换逻辑或 MIDI playback 状态时，至少验证：

- [ ] DevPiano 自导出单轨 MIDI 可导入回放。
- [ ] Type 1 / track 0 meta-only 文件可自动选择 note-rich track。
- [ ] 空文件或无 note MIDI 安全失败。
- [ ] 播放中导入另一个 MIDI 不留下旧音符持续发声。
- [ ] Record 期间 Import MIDI 按钮禁用；Stop 回到 Idle 后恢复可用。
- [ ] Import MIDI 后 Stop，Export MIDI / WAV 仍 disabled。
- [ ] `./scripts/dev.sh wsl-build --configure-only` 通过。
- [ ] `./scripts/dev.sh win-build` 通过。

### 11.1 MIDI 导入播放首音回归（2026-05-02 人工验收通过）

针对 [`known-issues.md`](known-issues.md) §8 记录的“首个音符接近 0 秒时几乎无声”问题，已实施 playback-start pre-roll / arming 修复；Windows 侧人工回归确认测试样本均不再复现。

- [x] 单轨 MIDI，首个 note-on 恰好在 tick/time `0`：导入后首音可听，虚拟键盘显示一致。
- [x] 单轨 MIDI，首个 note-on 非常接近 0 秒（例如 1 sample 或约 1ms）：导入后首音可听。
- [x] 零时刻和弦：所有和弦音都可听，所有对应虚拟按键都按下。
- [x] 连续 Stop → Import/Play 或 Back/restart 多次：首音稳定可听，不出现 stuck note。
- [x] 加载 / 卸载 VST3 插件或音频设备重建后立即导入播放：首个导入音符不被吞掉。
- [x] 启动清理用 all-notes-off 不会与首个 playback note-on 在同一个可听 block 内互相抵消。
- [x] 内置 fallback synth 与至少一个 VST3 乐器路径均通过；首音响度和 attack 与后续同 velocity 音符无明显异常差异。

后续观察要求：凡是后续改动涉及 MIDI import / playback 启动链路，都应重新执行本节回归。重点触发范围包括：

- `MidiFileImporter` 的时间转换、事件过滤、事件排序或 take 构建。
- `RecordingEngine::startPlayback()`、`renderPlaybackBlock()`、`advancePlaybackPosition()`。
- `RecordingSessionController::startInternalPlayback()`、`replaceTakeAndStartPlayback()`。
- `AudioEngine::prepareToPlay()`、`getNextAudioBlock()`、`renderPlaybackEventsIfNeeded()`。
- audio warmup、playback pre-roll / arming、all-notes-off、音频设备 rebuild。
- VST3 加载 / 卸载后立即播放导入 MIDI 的路径。

代码审查时重点确认：

- `timestampSamples == 0` 仍是合法 playback 事件，不被裁剪或整体平移。
- playback-start pre-roll 不推进 `RecordingEngine` playback position。
- pending all-notes-off 仍先于首个 playback note 被 drain，不与首个 note-on 同 sample 进入同一个可听 block。
- 首个 playback note 不会重新成为设备重建后的第一个渲染事件。

人工回归最小样本：

- 首个 note 在 0 秒的单轨 MIDI。
- 首个 note 非常接近 0 秒的单轨 MIDI。
- 0 秒和弦 MIDI。

若出现“虚拟键盘按下但首音无声”、“首音明显弱于后续同 velocity 音”、“加载插件后首次 import 更容易吞首音”或“0 秒和弦只响一部分音”，应视为该回归项可能受影响并重新调查。

## 12. 已知限制

- Phase 4-2 默认只选择一个 note 最多的轨道，不合并所有轨道；这是当前推荐模式。
- Program change、CC、pitch bend、sustain pedal 等非 note 事件暂未导入。
- 当前不是完整 GM 播放器；外部 GM MIDI 可能听起来与原文件不同。
- fallback synth 声部数有限，大型 MIDI 编曲可能出现拥挤或缺音。
- 完整 tempo map roundtrip、完整多轨模型、merge-all 导入和 MIDI 编辑器均为后续阶段范围。
- **MIDI 导入播放首音无声**：已通过 playback-start pre-roll / arming 修复并完成 Windows 侧人工回归；测试样本均不再复现。修复不改写导入时间线。详见 [`known-issues.md`](known-issues.md) §8。
