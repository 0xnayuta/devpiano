# 路线图扩展规划

> 用途：存放未来阶段的粗略规划与功能候选分析。当前路线图与阶段完成状态见 [`../roadmap/roadmap.md`](../roadmap/roadmap.md)。

## Phase 7：完整工程文件与多轨支持

状态：未开始，粗略规划。

**可能包含：**
- 完整工程文件（`.devpiano-project`）：包含演奏数据 + layout preset + 插件状态 + 音频设备配置。
- 多轨数据模型：`RecordingTake` 引入 track 概念。
- Tempo map 支持：导入/编辑/保存 tempo 变化。
- VST3 插件离线渲染（Phase 3-2 搁置项恢复）。
- Setting groups：多组独立的八度/移调/力度/通道配置。

## Phase 8：高级编辑与国际化

状态：未开始，粗略规划。

**可能包含：**
- Piano roll / 事件编辑器 UI。
- 量化 / snap-to-grid。
- 多语言 UI（英文/中文）。
- MP4 视频导出。
- 自动延音踏板（auto pedal）。
- Key fade 动画、GUI 透明度等装饰功能。

## Phase 6-3：最近文件列表 + 拖拽打开（暂缓）

**规划内容：**
- 最近打开的演奏文件/MIDI 文件列表（最多 10 条）。
- 拖拽 `.devpiano` / `.mid` 文件到窗口触发打开。

## Phase 6-4：基础 MIDI 编辑（delete notes）（暂缓）

**规划内容：**
- 选中音符 → 删除。需将 `RecordingTake.events` 改为可变结构。
- 最小编辑能力：只做删除，不做添加/移动/量化。
