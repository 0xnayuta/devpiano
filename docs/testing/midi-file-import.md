# MIDI 文件导入与回放验收测试（M8）

> 用途：记录 M8 MIDI 文件导入、回放、兼容性修正和后续增强的专项验收测试。  
> 读者：开发者、测试者、阶段验收者。  
> 更新时机：M8 导入/回放行为变化、验收结果更新、后续 M8-later 功能实现时。

## 1. 测试范围

覆盖：

- M8-1：MIDI 文件导入核心。
- M8-1b：自动选择含 note 最多的轨道。
- M8-2：MIDI roundtrip 验证 + 多轨/tempo 边界处理。
- M8-3：最近路径记忆 + 回放控制小增强。
- M8-5：合并所有轨道 note 到单一 timeline（计划中）。
- M8-6：MIDI playback 虚拟键盘可视化。
- M8-7：主窗口尺寸自适应与恢复。

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
3. **Type 1：track 0 只有 tempo/meta，track 1 有 notes**：用于验证 M8-1b 自动选轨。
4. **Type 1：多个 track 都有 notes**：用于验证 note-rich track 选择和后续 M8-5。
5. **非 960 PPQ MIDI**：用于验证不覆盖原始 `timeFormat`。
6. **包含 tempo meta event 的 MIDI**：用于验证时间转换。
7. **大量 note 的 MIDI**：用于观察播放稳定性和 UI 可用性。
8. 可选：包含 sustain CC、pitch bend、program change 的 MIDI，用于记录当前保真度限制。

## 4. M8-1：MIDI 文件导入核心

状态：已实现，需持续回归。

验收项：

- [x] 点击 Import MIDI 后 FileChooser 打开。
- [x] 选择有效 `.mid` 文件后可导入为 `RecordingTake` 并开始回放。
- [x] 本程序导出的单轨 MIDI 可正常导入回放。
- [x] 导入空文件或读取失败文件时写 Logger，不崩溃。
- [x] 导入不含 note 的 MIDI 时写 Logger，不崩溃。
- [x] 回放结束后 UI 状态回到 idle。
- [x] Import MIDI 后 Stop，Export MIDI / Export WAV 保持 disabled，不把导入 playback take 当作可导出录制 take。
- [x] 播放 A.mid 时导入 B.mid，旧 playback 音符会被停止，不产生持续残留音。

回归重点：

- 导入失败路径必须安全返回。
- 切换导入文件时必须释放旧播放中的 active notes。
- 导入 playback take 不应开启导出按钮。

## 5. M8-1b：自动选择含 note 最多的轨道

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

## 6. M8-2：MIDI roundtrip 验证 + 多轨/tempo 边界处理

状态：计划中 / 部分由 M8-1b 覆盖。

验收项：

- [ ] 单轨 `.mid`：导入 → 回放 → 导出 → DAW 打开后 note 个数、channel、velocity 范围基本一致。
- [ ] 多轨 `.mid`：Logger 清晰提示选中轨道和忽略轨道，不崩溃。
- [ ] 非 960 PPQ `.mid`：导入后播放速度不因强制 PPQ 覆盖而明显错误。
- [ ] 有 tempo meta event 的 `.mid`：导入后事件时间线基本符合原文件。
- [ ] 复杂 tempo map 的限制已在 Logger 或文档中明确，不误写为完整支持。

## 7. M8-3：最近路径记忆 + 回放控制小增强

状态：部分已实现（最近导入路径），其他为可选增强。

验收项：

- [x] Import MIDI 时 FileChooser 默认定位到上次导入目录。
- [ ] Export MIDI 时 FileChooser 默认定位到上次导出目录。
- [ ] Playback 中点击“回到开头”后 position 重置（可选，当前未实现）。

## 8. M8-5：合并所有轨道 note 到单一 timeline（后续增强）

状态：未实现 / 计划中。

验收项：

- [ ] 多轨 MIDI 文件导入后能听到多个轨道的 note 内容。
- [ ] 合并后事件顺序稳定，不产生明显 stuck note。
- [ ] Logger 输出 merge-all 模式、总轨数、每轨 note 数、合并后事件数和时长。
- [ ] 当前单乐器播放链路下的嘈杂、鼓轨和多音色限制已明确提示或记录。

## 9. M8-6：MIDI playback 虚拟键盘可视化（后续增强）

状态：已实现，2026-05-01 人工验收通过。

验收项：

- [x] Import MIDI 后播放时，虚拟键盘能随播放音符实时按下和松开。
- [x] Stop、播放结束、导入另一个 MIDI、关闭插件或重建音频设备时，虚拟键盘不会残留按下状态。
- [x] 用户手动按电脑键盘演奏时，既有虚拟键盘显示行为不回退。
- [x] 不在 audio callback 中直接调用 UI 方法或分配大量内存。
- [x] 大量 note 事件播放时 UI 仍保持可用，不明显卡顿。

## 10. M8-7：主窗口尺寸自适应与恢复（后续增强）

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
- [ ] Import MIDI 后 Stop，Export MIDI / WAV 仍 disabled。
- [ ] `./scripts/dev.sh wsl-build --configure-only` 通过。
- [ ] `./scripts/dev.sh win-build` 通过。

## 12. 已知限制

- M8-1b 默认只选择一个 note 最多的轨道，不合并所有轨道。
- Program change、CC、pitch bend、sustain pedal 等非 note 事件暂未导入。
- 当前不是完整 GM 播放器；外部 GM MIDI 可能听起来与原文件不同。
- fallback synth 声部数有限，大型 MIDI 编曲可能出现拥挤或缺音。
- 完整 tempo map roundtrip、完整多轨模型和 MIDI 编辑器均为后续阶段范围。
