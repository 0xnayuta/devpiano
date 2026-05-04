# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。  
> 读者：当前开发者、协作者、代码审查者。  
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前状态

当前活跃阶段：**Phase 6 进行中**

**Phase 5 已完成**（5.1-5.8 + 人工回归均通过）。`MainComponent.cpp` 从 1587 行降至 606 行，远低于 1200 行目标。详细完成记录见 [`../archive/phase5-architecture-convergence.md`](../archive/phase5-architecture-convergence.md)。

**Phase 6-1 已完成**：演奏文件 Save/Open 核心功能。
  - `PerformanceFile.h/.cpp` JSON 序列化/反序列化。
  - ControlsPanel Save/Open 按钮（布局待美化）。
  - `RecordingSessionController` 接入 Save/Open 流程。
  - **调试插曲**：首次调试时出现 `WeakReference` 访问冲突崩溃，根因是 Windows MSVC 侧 CMake 缓存未追踪源文件变更，导致新旧目标文件混链接。快速修复：删除 `build-win-msvc/CMakeCache.txt` 后重新 `win-build`。已记录至 [`../development/troubleshooting.md`](../development/troubleshooting.md)。
  - 详细说明见 [`roadmap.md`](roadmap.md) Phase 6 章节。

**Phase 6-2 已完成**：播放速度控制 ✅
  - ControlsPanel 增加速度显示（`1.00x`）和 `-`/`+` 按钮，位于播放控制行 Export WAV 按钮右侧，recording 状态下自动禁用，playing 状态下保持可操作。
  - `RecordingEngine.playbackSpeedMultiplier`（`std::atomic<double>`，默认 `1.0`，范围 `0.5–2.0`）。
  - `RecordingEngine.scaledPlaybackLengthSamples` / `playbackPositionSamples` 均为 `std::atomic<std::int64_t>`，跨 audio 线程安全。
  - `setPlaybackSpeedMultiplier(double)` 在播放中切换时，先重校准 `playbackPositionSamples`（`pos × oldSpeed / newSpeed`），再更新 `scaledPlaybackLengthSamples`。
  - `renderPlaybackBlock()` 中用 `combinedRatio = playbackSampleRateRatio / playbackSpeedMultiplier` 缩放时间戳，不修改原始 `RecordingTake` 数据。
  - 每次启动默认 1.0x，不持久化速度值。
  - `.devpiano` 打开后和 `.mid` 导入后均默认 `1.0x`，不受上次播放速度影响。
  - MIDI 导出使用原始 timeline，速度设置不影响导出时间戳。
  - `RecordingSessionController.handlePlaybackSpeedChange()` 同步 engine + UI，不写 settings。

**Phase 6-6 已完成**：Diagnostics 最小层 ✅
  - `source/Diagnostics/`（4 文件）：`DebugLog.h/.cpp`、`MidiTrace.h/.cpp`。
  - 接口：`DP_LOG_INFO/WARN/ERROR`、`DP_DEBUG_LOG`、`DP_TRACE_MIDI`。
  - Debug-only 宏在 Release 下无输出或零副作用。
  - 已接入 8 个业务文件（MidiRouter、MidiFileImporter、RecordingEngine、RecordingSessionController、PluginHost、LayoutFlowSupport、PluginFlowSupport、MainComponent）。
  - 散落 `Logger::writeToLog` 已全部替换为 `DP_LOG_*` 系列宏。
  - 为 Phase 6-5 MIDI 导入增强及后续播放体验任务建立统一诊断基础设施。

**Phase 6-7 已完成**：MIDI / Performance 测试夹具与最小回归样本库 ✅
  - `docs/testing/fixtures/midi/`（7 个 MIDI fixture）。
  - `docs/testing/fixtures/performance/simple-performance.json`（`.devpiano` JSON 格式）。
  - 所有合法 MIDI fixture 已通过 mido 库验证，`invalid.mid` 正确识别为非 MIDI 文件（预期行为）。
  - 为 Phase 6-5 MIDI 导入增强（sustain CC64/pitch bend/program change）提供 fixture 验证基准。
  - 详见 [`../features/phase6-performance-persistence.md`](../features/phase6-performance-persistence.md)。

**Phase 6-5 已完成**：MIDI 导入增强 ✅
  - `MidiFileImporter.cpp` 修改：遍历 track 事件时，对非 note 事件先判断是否为 CC/pitch bend/program change，是则收集为 `PerformanceEvent`，否则仅 trace 诊断后跳过。
  - 新增计数器：`ccCount`、`pitchBendCount`、`programChangeCount`。
  - CC/pitch bend/program change 与 note 事件共用同一时间线转换逻辑，更新 `lastTimestampSamples` 以正确计算 `RecordingTake.lengthSamples`。
  - Logger 输出更新：导入成功后输出 note-on/off 与 CC/pitch-bend/program-change 的分类计数。
  - fixture `sustain-pedal.mid`（含 CC64 on/off）已就绪。
  - 详见 [`../features/phase6-performance-persistence.md`](../features/phase6-performance-persistence.md)。

**Phase 6-3 下一阶段**：最近文件列表 + 拖拽打开。

---

## 已修复缺陷（保留回归项）

**启动 / 音频重建早期首音音高异常**：已修复并通过人工验证；保留 `25ms` audio warmup。详细记录见 [`../testing/known-issues.md`](../testing/known-issues.md) §2。

**MIDI 导入播放首音无声**：已通过 playback-start pre-roll / arming 修复并完成人工回归；后续触及 MIDI import / playback 启动链路时执行 Phase 4 §11.1 回归。详细记录见 [`../testing/known-issues.md`](../testing/known-issues.md) §8。

**Phase 6-2 播放速度倍率反用（音符间隔反向）**：已修复。`renderPlaybackBlock()` 中 `combinedRatio = playbackSampleRateRatio / playbackSpeedMultiplier`（原为 `*`，导致 0.5x 时音符反而压缩、2.0x 时音符反而拉长）。详细记录见 [`../testing/known-issues.md`](../testing/known-issues.md) §11.1。

**Phase 6-2 速度切换时音长时间悬停（note-off 吞没）**：已修复。`setPlaybackSpeedMultiplier()` 在切换时增加 `playbackPositionSamples` 重校准（`pos × oldSpeed / newSpeed`），保证 take 时间轴位置不变。详细记录见 [`../testing/known-issues.md`](../testing/known-issues.md) §11.2。

**Phase 6-2 播放状态三成员跨线程数据竞争**：已修复。`playbackSpeedMultiplier`（`double` → `std::atomic<double>`）、`scaledPlaybackLengthSamples` 和 `playbackPositionSamples`（`std::int64_t` → `std::atomic<std::int64_t>`）。详细记录见 [`../testing/known-issues.md`](../testing/known-issues.md) §11.3。

---

**搁置说明**：外部 MIDI 硬件依赖验证和 VST3 插件离线渲染（Phase 3-5）因当前条件暂无法测试，状态已记录至 [`../testing/known-issues.md`](../testing/known-issues.md) §1 和 §3。

---

## 验证命令

代码修改后优先执行：

```bash
./scripts/dev.sh wsl-build --configure-only
./scripts/dev.sh win-build
```

涉及环境或路径问题时执行：

```bash
./scripts/dev.sh self-check
```

## 相关文档

- 项目路线图：[`roadmap.md`](roadmap.md)
- 架构概览：[`../architecture/overview.md`](../architecture/overview.md)
- Phase 5 完成记录：[`../archive/phase5-architecture-convergence.md`](../archive/phase5-architecture-convergence.md)
