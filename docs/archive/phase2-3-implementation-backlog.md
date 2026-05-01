# Phase 2-3 实现 Backlog 完成记录

> 用途：记录 Phase 2-3 的详细实现计划与完成情况，作为历史参考。  
> 读者：需要了解实现历史细节的开发者。  
> 更新时机：本文件为归档文档，不再更新。当前信息请参考 `docs/roadmap/roadmap.md`。

## 背景

本文档包含 Phase 2 和 Phase 3 的详细实现 Backlog，所有条目均已完成。

条目格式：`目标 — 关键约束 — 前置条件`

---

## Phase 3-1..3-7：布局 Preset 系统

**Phase 3-1：JSON preset 文件格式**
- 目标：定义 `.freepiano.layout` JSON schema，含 version、id、name、bindings（含 keyCode/displayText/action）
- 关键约束：使用 JUCE keyCode，不含 velocity/transpose 等演奏参数
- 前置条件：无
- 状态：已完成（当前还包含 `displayName` 字段）

**Phase 3-2：Preset 读写工具函数**
- 目标：在 `source/` 下新建 `LayoutPreset.h/.cpp`，提供 `loadLayoutPreset(path)` → `KeyboardLayout`、`saveLayoutPreset(layout, path)` → `bool`
- 关键约束：使用 JUCE `File` / `JSON` API，不引入新外部依赖
- 前置条件：Phase 3-1 完成
- 状态：已完成

**Phase 3-3：用户目录 Preset 自动发现**
- 目标：`ControlsPanel` 启动时扫描用户配置目录，聚合内置 preset + `.freepiano.layout` 文件到 layout 列表
- 关键约束：用户 preset 只在目录中可见，内置 preset 不可被删除
- 前置条件：Phase 3-2 完成
- 状态：已完成

**Phase 3-4：Preset 加载（文件对话框）**
- 目标：`ControlsPanel` 或 `PluginPanel` 提供"导入"入口，通过原生文件对话框选择任意位置的 `.freepiano.layout` 并应用
- 关键约束：复用已有 `KeyboardMidiMapper::setLayout()` 路径，不新增独立加载逻辑
- 前置条件：Phase 3-2 完成
- 状态：已完成（当前入口位于 `ControlsPanel`，导入后复制到用户布局目录并应用）

**Phase 3-5：Preset 保存（Save Layout）**
- 目标：`ControlsPanel` 提供"另存为"入口，将当前 `KeyboardLayout` 保存为 JSON
- 关键约束：保存到文件对话框选择的目标路径；保存到用户目录时可被后续扫描恢复
- 前置条件：Phase 3-2、Phase 3-3 完成
- 状态：已完成

**Phase 3-6：Preset 删除**
- 目标：在 `ControlsPanel` 中支持删除用户 preset（内置不可删）
- 关键约束：只删文件，不动内置 preset
- 前置条件：Phase 3-3 完成
- 状态：已完成（当前为独立 `Delete` 按钮 + 确认对话框）

**Phase 3-7：Preset 重命名显示名称**
- 目标：支持修改用户 preset 在下拉菜单中的显示名称
- 关键约束：只改 JSON `name` / `displayName`，不改文件名和稳定 `layoutId`
- 前置条件：Phase 3-3 完成
- 状态：已完成（当前为独立 `Rename` 按钮）

---

## Phase 3-8..3-13：录制 / 回放 / 导出

**Phase 3-8：演奏事件模型与时钟**
- 目标：定义 `PerformanceEvent { timestampSamples, source, MidiMessage }` 与 `RecordingTake { sampleRate, lengthSamples, events }`
- 关键约束：不复制旧 `song_event_t` 字节数组；内部优先使用 sample-based timeline
- 前置条件：无
- 状态：已完成并接入 `AudioEngine` 录制 / 回放主链路

**Phase 3-9：AudioEngine MIDI block 边界**
- 目标：在 `AudioEngine::getNextAudioBlock()` 中确定 block-local `MidiBuffer` 的录制 handoff 边界
- 关键约束：录制的是交给插件 / fallback synth 前的 pre-render `MidiBuffer` 演奏事件
- 前置条件：Phase 3-8 完成
- 状态：已完成

**Phase 3-10：录制 UI（录制/停止按钮，录音状态显示）**
- 目标：在 `ControlsPanel` 添加录制/停止按钮和录音状态指示
- 关键约束：复用已有 `AudioEngine` / `MidiRouter` 路由
- 前置条件：Phase 3-9 完成
- 状态：已完成

**Phase 3-11：回放逻辑**
- 目标：实现 `PlaybackState { idle, playing }`，按当前 audio block 将到期事件写入 `MidiBuffer`
- 关键约束：回放不走 `KeyboardMidiMapper`；播放完毕自动停止并清理悬挂音
- 前置条件：Phase 3-8、Phase 3-9 完成
- 状态：已完成

**Phase 3-12：MIDI 文件导出（`juce::MidiFile`）**
- 目标：将 `PerformanceEvent` 序列化为标准 MIDI Type 1 文件
- 关键约束：使用 JUCE `MidiFile`，兼容任何 DAW
- 前置条件：Phase 3-8、Phase 3-9 完成
- 状态：已完成

**Phase 3-13：WAV 离线渲染**
- 目标：将 MIDI 文件通过离线音频链路渲染为 WAV 文件
- 关键约束：离线渲染，不影响实时播放
- 前置条件：Phase 3-12 完成
- 状态：Phase 3-13a/b/c/d 已完成；VST3 插件离线渲染（Phase 3-13e）后置

---

## Phase 2-1..2-4：插件扫描产品化增强

**Phase 2-1：扫描失败文件明细**
- 目标：记录 `PluginDirectoryScanner::getFailedFiles()` 中的具体失败文件路径
- 关键约束：先进入 Logger / 状态摘要，不在 audio callback 中做日志或 UI
- 前置条件：现有 `PluginHost::scanVst3Plugins()` 保持可用
- 状态：已完成

**Phase 2-2：多目录扫描输入与持久化**
- 目标：明确多个 VST3 搜索目录的输入格式、UI 表达和 `juce::FileSearchPath` 持久化语义
- 关键约束：保留默认搜索路径 fallback；无效目录不阻塞其他有效目录
- 前置条件：Phase 2-1 可独立实施
- 状态：已完成

**Phase 2-3：扫描结果持久化 / 启动恢复优化**
- 目标：评估并实现 `KnownPluginList` 缓存，降低启动恢复对同步扫描的依赖
- 关键约束：缓存失效时安全回退重扫
- 前置条件：现有启动恢复计划与插件列表 UI 稳定
- 状态：已完成

**Phase 2-4：扫描空状态 / 失败状态 / 恢复失败提示**
- 目标：区分"尚未扫描""扫描成功但无插件""扫描失败""上次插件恢复失败"等用户可见状态
- 关键约束：避免状态文本过长；细节先写 Logger
- 前置条件：Phase 2-1 / Phase 2-2 可提供更完整上下文
- 状态：已完成

---

## Phase 5-1..5-4：MainComponent 职责收敛（已合并至 Phase 5）

**Phase 5-1：RecordingFlowSupport 录制 / 回放 UI 流程 helper**
- 状态：已完成

**Phase 5-2：ExportFlowSupport 导出选项与默认文件名 helper**
- 状态：已完成

**Phase 5-3：PluginFlowSupport 继续收敛 scan / restore / cache 流程**
- 状态：已完成

**Phase 5-4：ReadOnlyStateRefresh 边界命名清理**
- 状态：已完成

## 相关文档

- 项目路线图：`docs/roadmap/roadmap.md`
- 当前迭代状态：`docs/roadmap/current-iteration.md`
- 架构概览：`docs/architecture/overview.md`
