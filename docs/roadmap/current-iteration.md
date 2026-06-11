# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前状态

当前活跃阶段：**Phase 6-9/6-10/6-11 为剩余 P0 核心任务**。

**Phase 6-8（自定义钢琴键盘渲染与交互）已全部完成。**
渲染、动画、三种着色模式、三种音符显示模式、键标签布局注入、绑定编辑对话框均已在本次迭代中实现。

**Phase 7-7（点击键绑定编辑对话框）已于本次迭代提前完成。**（基础版本支持 note 类型绑定编辑；CC/program/pitch-bend 编辑器需在 Phase 6-10 扩展绑定系统完成后增强。）

**已完成的 Phase 6 子项：**
- **Phase 6-1**：演奏文件 Save/Open（`.devpiano` JSON 序列化/反序列化）。
- **Phase 6-2**：播放速度控制（`0.5x–2.0x`，实时生效，不持久化）。
- **Phase 6-5**：MIDI 导入增强（CC64 sustain、pitch bend、program change 事件收集与计数）。
- **Phase 6-6**：Diagnostics 最小层（`DP_LOG_INFO/WARN/ERROR`、`DP_DEBUG_LOG`、`DP_TRACE_MIDI` 宏接口，替换散落 `Logger::writeToLog`）。
- **Phase 6-7**：MIDI/Performance 测试夹具与最小回归样本库（7 个 MIDI fixture，1 个 `.devpiano` fixture）。
- **Phase 6-8**：自定义钢琴键盘渲染与交互（完整实现，含键盘 Component、淡入淡出动画、三种着色模式、三种音符显示模式、键标签、设置模型集成）。

**暂缓 / 已替换：**
- Phase 6-3（最近文件列表 + 拖拽打开）暂缓：拖放部分归入 Phase 7-3，最近文件列表归入 Phase 8 远期展望。
- Phase 6-4（基础 MIDI 编辑 delete notes）暂缓，无近期计划。

**Phase 2-5 已完成**（本次迭代插入并完成）：插件扫描 UX 增强（`AsyncUpdater` 分片扫描、逐 tick UI 刷新、扫描中操作 guards）。详见 [`../reference/features/plugin-hosting.md`](../reference/features/plugin-hosting.md) Phase 2-5 节。

---

## 本次迭代已完成工作

### Phase 6-8 四个 MVP 步骤（几何 + 绘制 + 鼠标 + 淡入淡出）

- `source/Core/KeyboardTypes.h` — 渲染数据类型、音符名称 helper、钢琴键几何判定
- `source/UI/CustomKeyboard.h/.cpp` — 全新 `juce::Graphics` 绘制钢琴键盘 Component
- 接收 `MidiKeyboardState&` 按键状态同步，`juce::Timer` 驱动 ≈30fps 淡入淡出动画
- 鼠标点击 `onNoteOn`/`onNoteOff` → `keyboardState.noteOn/Off`（与键盘输入同路径）

### Group A：键标签 + 音符显示模式

- `setKeyboardLayout(const KeyboardLayout&)` 反向查找 keyCode→midiNote 注入 `displayText`
- `paintKeyLabels()`：有绑定显示键名（A/S/D…），无绑定 fallback 到音符名
- 三种音符显示模式：`doReMi`（1+0）、`fixedDo`（调号偏移）、`noteName`（C4）

### Group B：通道 / 力度着色模式

- `perKeyChannel[128]`、`perKeyVelocity[128]` 从 `setKeyboardLayout` 提取
- 经典模式：暖橙 HSV 渐变（不变）
- 通道模式：16 色 FreePiano 调色板（`channelHues[16]`）
- 力度模式：绿色→红色 HSV 渐变（0→127）

### Group C：键盘显示设置模型集成

- `SettingsModel::KeyboardDisplaySettingsView` — colourMode / noteDisplay / fadeSpeed
- `SettingsStore` 持久化（`kKeyColourMode`、`kKeyNoteDisplay`、`kKeyFadeSpeed`）
- `MainComponent::syncUiFromSettings()` 推送到 `CustomKeyboard::setKeyboardSettings()`

### Group D：双击弹出绑定编辑对话框（Phase 7-7）

- `source/UI/KeyBindingEditDialog.h/.cpp` — 模态绑定编辑对话框
- 显示音名、计算机键标签、MIDI channel/note/velocity 编辑控件
- OK/ Cancel/ Unbind 三个按钮，自销毁生命周期
- 确认后更新 `KeyboardLayout` 并调用 `syncUiFromSettings()` 持久化

### 本次迭代新增/修改文件清单

```
新增：source/Core/KeyboardTypes.h
新增：source/UI/CustomKeyboard.h
新增：source/UI/CustomKeyboard.cpp
新增：source/UI/KeyBindingEditDialog.h
新增：source/UI/KeyBindingEditDialog.cpp
修改：source/UI/KeyboardPanel.h
修改：source/UI/KeyboardPanel.cpp
修改：source/MainComponent.cpp
修改：source/Settings/SettingsModel.h
修改：source/Settings/SettingsStore.cpp
修改：source/CMakeLists.txt
```

---

## 剩余 P0 任务（Phase 6-9 / 6-10 / 6-11）

### Phase 6-9：16 通道 MIDI 矩阵数据模型 + 设置 UI

- 数据结构：`std::array<PerChannelConfig, 16>`。
- 每通道配置：velocity（0-127）、transpose（-48..48）、octave shift（-1/0/+1）、output channel（0-15）、program（0-127）、bank MSB（0-127）、sustain CC（0-127）、follow key（bool）。
- 设置 UI：滚动列表或网格，每行一个通道。
- 实时应用于 `MidiRouter` 路由逻辑。

### Phase 6-10：扩展绑定系统（CC / program change / pitch bend / channel pressure）

- 扩展 `KeyActionType`，每个键支持绑定多种事件类型。
- 每键 GUI 绑定编辑面板（非旧脚本编辑器）— 基础版已在 Phase 7-7 提前实现。
- 保留 note on/off 自动 keyup 生成。
- 设计原则：不做旧 256 个设置组、不做键组复制/粘贴/插入/清除 UI。

### Phase 6-11：GUI 设置页面关键项补齐

- MIDI 输入重映射 UI（Settings → MIDI 页）。
- 键淡入速度 slider（Settings → GUI 页）。
- 自动颜色模式选择器。
- 音符显示模式选择器。
- 显示 MIDI / VSTi 乐器过滤复选框。
- 可调整大小窗口 toggle。

---

## 下一阶段预留

Phase 6-9..6-11 完成后按计划进入 **Phase 7**（剩余体验完善）：

- **Phase 7-1（P0）**：VST3 插件离线渲染（从搁置恢复，非 UI 线程创建 `AudioPluginInstance` 副本实现 VST3 音色 WAV 导出）。
- **Phase 7-2（P1）**：播放速度精确控制（替换步进按钮为 `juce::Slider` + 数值标签）。
- **Phase 7-3（P1）**：拖放文件支持（`FileDragAndDropTarget`：`.devpiano` / `.mid` / `.freepiano.layout` / `.vst3`）。
- **Phase 7-4（P1）**：运行时中英文语言切换（JUCE `Translation` 机制，替换旧 `language_strdef.h` 体系）。
- **Phase 7-5（P1）**：歌曲信息编辑对话框（编辑 `PerformanceFileMetadata`）。
- **Phase 7-6（P1）**：Export 进度对话框（`ProgressBar` + 取消支持）。
- **Phase 7-8（P2）**：全屏模式（F11 切换，`Desktop::setKioskModeComponent`）。
- **Phase 7-9（P2）**：MIDI 输入重映射 UI。

> Phase 7-7（绑定编辑对话框）已在本次迭代中提前完成（基础版本）。

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
