# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前状态

当前活跃阶段：**Phase 7 — VST3 离线渲染与体验完善**。Phase 6 / Phase 7-1~7-4 已完成。

**Phase 5** 已完成（5.1-5.8 + 人工回归均通过）。`MainComponent.cpp` 从 1587 行降至 606 行（Phase 5 结束时数据；后续 Phase 6/7 新增功能后当前约 750 行）。

**Phase 6** 已完成子项：6-1（演奏文件持久化）、6-2（播放速度控制）、6-5（MIDI 导入增强）、6-6（Diagnostics 层）、6-7（测试夹具库）、6-8（自定义钢琴键盘）、6-9（16 通道 MIDI 矩阵）、6-10（note-only 绑定编辑器）、6-11（5 项设置控件：colourMode/noteDisplay/fadeSpeed/resizable/instrumentFilter toggle）。

**Phase 7 已完成子项：**
- **Phase 7 启动前快速清扫** ✅ — instrumentFilter toggle show/hide 接入。
- **Phase 7-1：VST3 离线渲染** ✅ — 非 UI 线程创建 `AudioPluginInstance` 副本、WAV 导出、ExportDialog 进度对话框。
- **Phase 7-2：播放速度精确控制** ✅ — Slider + TextBox 替换步进按钮，`std::atomic` 跨线程安全。
- **Phase 7-3：拖放文件支持** ✅ — `FileDragAndDropTarget`（`.devpiano`/`.mid`/`.freepiano.layout`/`.vst3`），蓝色边框反馈。
- **Phase 7-4：运行时中英文语言切换** ✅ — JUCE `Translation` 机制替换 `language_strdef.h` 体系，嵌入式中文 locale 表，Settings ComboBox，切换即时生效。
---

## Phase 7 后续子项（P1，按序推进）

- **Phase 7-5（P1）**：歌曲信息编辑对话框（编辑 `PerformanceFileMetadata`）。
- **Phase 7-7（P1）**：全屏模式（F11 切换，`Desktop::setKioskModeComponent`）。

### Phase 6-11 遗留项（中低优先级，Phase 7 中按需追加）

- ChannelMatrix UI 编辑器 — 遇到多通道配置场景时切入。
- MIDI 输入重映射 UI — 遇到外部 MIDI 控制器通道变更场景时切入。

### 继续搁置

- Phase 6-10 多事件扩展（CC/pitch bend/channel pressure）— 不做计划。
- Phase 6-3（最近文件列表 + 拖拽打开）、Phase 6-4（基础 MIDI 编辑）。
- 外部 MIDI 硬件依赖验证。

## 已修复缺陷（保留回归项）

- **启动 / 音频重建早期首音音高异常**：已修复并通过人工验证；保留 `25ms` audio warmup。详细记录见 [`../issues/known-issues.md`](../issues/known-issues.md) §2。

- **MIDI 导入播放首音无声**：已通过 playback-start pre-roll / arming 修复并完成人工回归；后续触及 MIDI import / playback 启动链路时执行 Phase 4 §11.1 回归。详细记录见 [`../issues/known-issues.md`](../issues/known-issues.md) §8。

- **Phase 6-2 播放速度倍率反用（音符间隔反向）**：已修复。`renderPlaybackBlock()` 中 `combinedRatio = playbackSampleRateRatio / playbackSpeedMultiplier`（原为 `*`，导致 `0.5x` 时音符反而压缩、`2.0x` 时音符反而拉长）。详细记录见 [`../issues/known-issues.md`](../issues/known-issues.md) §11.1。

- **Phase 6-2 速度切换时音长时间悬停（note-off 吞没）**：已修复。`setPlaybackSpeedMultiplier()` 在切换时增加 `playbackPositionSamples` 重校准（`pos × oldSpeed / newSpeed`），保证 take 时间轴位置不变。详细记录见 [`../issues/known-issues.md`](../issues/known-issues.md) §11.2。

- **Phase 6-2 播放状态三成员跨线程数据竞争**：已修复。`playbackSpeedMultiplier`（`double` → `std::atomic<double>`）、`scaledPlaybackLengthSamples` 和 `playbackPositionSamples`（`std::int64_t` → `std::atomic<std::int64_t>`）。详细记录见 [`../issues/known-issues.md`](../issues/known-issues.md) §11.3。

---

## 搁置说明

- **外部 MIDI 硬件依赖验证**：暂缓，待硬件条件恢复。状态已记录至 [`../issues/known-issues.md`](../issues/known-issues.md) §1。

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
- 架构概览：[`../reference/architecture.md`](../reference/architecture.md)
- Phase 5 完成记录：[`../archive/phase5-architecture-convergence.md`](../archive/phase5-architecture-convergence.md)
