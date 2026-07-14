# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前状态

当前活跃阶段：**Phase 6 进行中 — 6-8/6-9/6-10（note editor）已完成，6-10（多事件扩展）、6-11 待完成**。

**Phase 5 已完成**（5.1-5.8 + 人工回归均通过）。`MainComponent.cpp` 从 1587 行降至 606 行，远低于 1200 行目标。详细完成记录见 [`../archive/phase5-architecture-convergence.md`](../archive/phase5-architecture-convergence.md)。

**已完成的 Phase 6 子项：**
- **Phase 6-1**：演奏文件 Save/Open（`.devpiano` JSON 序列化/反序列化）。
- **Phase 6-2**：播放速度控制（`0.5x–2.0x`，实时生效，不持久化）。
- **Phase 6-5**：MIDI 导入增强（CC64 sustain、pitch bend、program change 事件收集与计数）。
- **Phase 6-6**：Diagnostics 最小层（`DP_LOG_INFO/WARN/ERROR`、`DP_DEBUG_LOG`、`DP_TRACE_MIDI` 宏接口，替换散落 `Logger::writeToLog`）。
- **Phase 6-7**：MIDI/Performance 测试夹具与最小回归样本库（7 个 MIDI fixture，1 个 `.devpiano` fixture）。
- **Phase 6-8**：自定义钢琴键盘（`CustomKeyboard` Component，着色模式、fade 动画、音符显示模式、绑定编辑对话框）。
- **Phase 6-9**：16 通道 MIDI 矩阵（`ChannelMatrix` 数据模型 + `MidiChannelMapper` 路由服务）。
- **Phase 6-10（note editor）**：每键绑定编辑对话框（`KeyBindingEditDialog`，MIDI note/channel/velocity 编辑）。CC / program change / pitch bend / channel pressure 多事件类型扩展待实现。

**待完成的 Phase 6 子项：**
- **Phase 6-11**：GUI 设置补齐 — `KeyboardDisplaySettings`（colourMode / noteDisplay / fadeSpeed）已持久化；ChannelMatrix UI 编辑器、MIDI 重映射 UI、fade speed slider、颜色/音符模式选择器、resizable toggle 待实现。

**暂缓 / 已替换：**
- Phase 6-3（最近文件列表 + 拖拽打开）暂缓：拖放部分归入 Phase 7-3，最近文件列表归入 Phase 8 远期展望。
- Phase 6-4（基础 MIDI 编辑 delete notes）暂缓，无近期计划。

**基础设施更新：**
- JUCE 子模块已升级至 `develop` 最新（3233cd13 / JUCE 8.0.14），弃用 API 已同步迁移（如 `Font` → `FontOptions`）。

---

## 当前 P0 任务（Phase 6-8..6-11）

以下为当前迭代的核心开发目标，对应 roadmap §3 Phase 6。已完成项标记 ✅。

### Phase 6-8：自定义钢琴键盘渲染与交互 ✅

- ✅ 基于 `juce::Graphics` 重写键盘 `Component`，替代 `MidiKeyboardComponent`。
- ✅ 按键淡入淡出动画（timer 驱动的 alpha lerp）。
- ✅ 每键自动着色模式：经典 / 通道 / 力度。
- ✅ 每键颜色/标签自定义。
- ✅ 音符显示模式：Do Re Mi / 固定 Do / 音符名称。
- ✅ 点击键盘键弹出绑定编辑对话框。
- 丢弃：DirectX 9、FreeType 字体、TGA/PNG 纹理贴图。
- 设计原则：所有渲染使用 JUCE `Graphics` 原始绘制——不加载旧纹理、不构建独立渲染管线。

### Phase 6-9：16 通道 MIDI 矩阵数据模型 ✅ （设置 UI 待 Phase 6-11 完成）

- ✅ 数据结构：`std::array<PerChannelConfig, 16>`。
- ✅ 每通道配置：velocity、transpose、octave shift、output channel、program、bank MSB、sustain CC、follow key。
- 设置 UI：滚动列表或网格，每行一个通道（待 Phase 6-11 完成）。
- ✅ 实时应用于 `MidiRouter` 路由逻辑。

### Phase 6-10：扩展绑定系统 — note-only 编辑器 ✅，多事件类型扩展待实现

- ✅ 每键 GUI 绑定编辑面板（`KeyBindingEditDialog`，MIDI note/channel/velocity 编辑）。
- 扩展 `KeyActionType`，每个键支持 CC / program change / pitch bend / channel pressure。
- 保留 note on/off 自动 keyup 生成。
- 设计原则：不做旧 256 个设置组、不做键组复制/粘贴/插入/清除 UI。

### Phase 6-11：GUI 设置页面关键项补齐（待实现）

`KeyboardDisplaySettings`（colourMode / noteDisplay / fadeSpeed）已持久化；以下 UI 控件待实现：

- ChannelMatrix UI 编辑器（滚动列表或网格，每行一个通道）。
- MIDI 输入重映射 UI（Settings → MIDI 页）。
- 键淡入速度 slider（Settings → GUI 页）。
- 自动颜色模式选择器。
- 音符显示模式选择器。
- 显示 MIDI / VSTi 乐器过滤复选框。
- 可调整大小窗口 toggle。

---

## 下一阶段预留

Phase 6-11 完成后按计划进入 **Phase 7**（体验完善——离线渲染、拖放、语言、导出 UX）：

- **Phase 7-1（P0）**：VST3 插件离线渲染（从搁置恢复，非 UI 线程创建 `AudioPluginInstance` 副本实现 VST3 音色 WAV 导出）。
- **Phase 7-2（P1）**：播放速度精确控制（替换步进按钮为 `juce::Slider` + 数值标签）。
- **Phase 7-3（P1）**：拖放文件支持（`FileDragAndDropTarget`：`.devpiano` / `.mid` / `.freepiano.layout` / `.vst3`）。
- **Phase 7-4（P1）**：运行时中英文语言切换（JUCE `Translation` 机制，替换旧 `language_strdef.h` 体系）。
- **Phase 7-5（P1）**：歌曲信息编辑对话框（编辑 `PerformanceFileMetadata`）。
- **Phase 7-6（P1）**：Export 进度对话框（`ProgressBar` + 取消支持）。
- **Phase 7-7（P1）**：全屏模式（F11 切换，`Desktop::setKioskModeComponent`）。<br>  注：绑定编辑对话框（原 Phase 7-7）已在 Phase 6-10 完成，此条目替换为全屏模式。
- **Phase 7-8（P2）**：MIDI 输入重映射 UI（与 Phase 6-11 合并，视完成情况决定是否需要追加）。

> 影响 Phase 7 优先级的外部因素：Phase 6-11 GUI 设置项完成度将决定 Phase 7 实际启动时间；若 Phase 6-11 进展缓慢，可提前并行启动 Phase 7-1 离线渲染。

---

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
