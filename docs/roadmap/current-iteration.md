# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前状态

当前活跃阶段：**Phase 7 — VST3 离线渲染与体验完善**。Phase 6 主体已完成。

**Phase 5** 已完成（5.1-5.8 + 人工回归均通过）。`MainComponent.cpp` 从 1587 行降至 606 行。

**Phase 6** 已完成子项：6-1（演奏文件持久化）、6-2（播放速度控制）、6-5（MIDI 导入增强）、6-6（Diagnostics 层）、6-7（测试夹具库）、6-8（自定义钢琴键盘）、6-9（16 通道 MIDI 矩阵）、6-10（note-only 绑定编辑器）、6-11（5 项设置控件：colourMode/noteDisplay/fadeSpeed/resizable/instrumentFilter toggle）。

**Phase 6 搁置/暂缓项：**
- Phase 6-10 多事件扩展（CC/pitch bend/channel pressure）— 明确搁置，用户场景不足 10%。
- Phase 6-11 ChannelMatrix UI 编辑器、MIDI 重映射 UI — 中低优先级，Phase 7 中按需追加。
- Phase 6-3（最近文件列表 + 拖拽打开）、Phase 6-4（基础 MIDI 编辑）— 维持暂缓。

**基础设施更新：**
- JUCE 子模块已升级至 `develop` 最新（3233cd13 / JUCE 8.0.14），弃用 API 已同步迁移（如 `Font` → `FontOptions`）。

---

## 当前 P0 任务 — Phase 7-1：VST3 插件离线渲染

当前已进入 **Phase 7**，P0 任务为打通 VST3 音色 WAV 导出闭环。

### Phase 7 启动前快速清扫（~5 min）

- 补 `showInstrumentFilter` toggle 的 show/hide 接入（`MainComponent.cpp:564` TODO）。

### Phase 7-1：VST3 插件离线渲染

**目标：** 实现以已加载 VST3 插件音色渲染 WAV 导出的能力。

- 非 UI 线程创建 `AudioPluginInstance` 副本进行离线渲染。
- 以 `RecordingTake` 事件为输入，逐 block 渲染到音频 buffer。
- 输出到现有 WAV 写入流程（`RecordingExporter` / `WavAudioFormat`）。
  - 输出采样率：选项，默认 44100 Hz。
  - 进度反馈：`ProgressBar` + 取消支持（与 Phase 7-6 合并）。

**关键风险与设计约束：**
- 插件状态同步：离线渲染实例需复制原始实例的 preset/program 状态。
- 时序精确性：MIDI-to-audio 渲染 block 边界对齐需与播放一致的 comb filter / arpeggiator 兼容。
- 线程安全：`AudioPluginInstance` 跨线程生命周期管理。
- 与现有 fallback synth 导出路径共存（Phase 3-4），用户可选择使用哪个音源导出。

**预计工期：** Phase 7-1 核心 ~3-5 天。

---

## Phase 7 后续子项（P1，按序推进）

Phase 7-1 完成后依次进入：

- **Phase 7-2（P1）**：播放速度精确控制（替换步进按钮为 `juce::Slider` + 数值标签）。
- **Phase 7-3（P1）**：拖放文件支持（`FileDragAndDropTarget`：`.devpiano` / `.mid` / `.freepiano.layout` / `.vst3`）。
- **Phase 7-4（P1）**：运行时中英文语言切换（JUCE `Translation` 机制，替换旧 `language_strdef.h` 体系）。
- **Phase 7-5（P1）**：歌曲信息编辑对话框（编辑 `PerformanceFileMetadata`）。
- **Phase 7-6（P1）**：Export 进度对话框（`ProgressBar` + 取消支持）。
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
