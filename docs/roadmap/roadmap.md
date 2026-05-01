# devpiano Roadmap

> 用途：作为唯一的项目状态、阶段路线与近期重点来源。  
> 读者：项目维护者、规划下一轮开发的人。  
> 更新时机：阶段目标变化、功能完成度变化、重大风险变化时。

## 1. 项目目标

将旧版 Windows FreePiano 重构为基于 JUCE 的现代 C++ 音频应用。

核心替代方向：

- 旧 WASAPI / ASIO / DSound 后端 -> JUCE `AudioDeviceManager`。
- 旧 VST 加载逻辑 -> JUCE `AudioPluginFormatManager` / `AudioPluginInstance`。
- 旧 Windows 键盘输入逻辑 -> JUCE `KeyListener` / `KeyPress` + 可配置 MIDI 映射。
- 旧 GDI / 原生控件 UI -> JUCE `Component` 树。
- 旧配置系统 -> `ApplicationProperties` / `ValueTree` / 项目内状态模型。

## 2. 当前状态摘要

**已完成阶段：**
- Phase 1-4（功能开发）：M0-M8 全部完成，核心功能已可用。
- Phase 5.1-5.7（架构收敛）：MainComponent 职责下沉，AH-1..AH-7 已完成。

**当前阶段：**
- Phase 5.8+：继续 MainComponent 瘦身，目标从约 1587 行降至 1200 行以下。

**搁置项：**
- 外部 MIDI 硬件依赖验证（待硬件条件恢复）。
- VST3 插件离线渲染（M6-6e）后置。

## 3. 阶段路线图

### Phase 1：工程骨架与最小演奏（M0-M1）

状态：已完成。

**M0：工程骨架可运行**
- [x] JUCE GUI 程序可启动。
- [x] Debug 构建可通过。
- [x] 主窗口可显示。
- [x] 音频设备可初始化。

**M1：最小演奏链路成立**
- [x] 电脑键盘可触发 note on / note off。
- [x] 虚拟钢琴键盘可联动显示。
- [x] 程序可发声，来源为内置 fallback synth 或已加载插件。
- [x] 长按、快速连按、焦点切换等基础场景已完成一轮验证。

### Phase 2：插件系统与键盘映射（M2-M4）

状态：已完成。

**M2：最小插件扫描能力成立**
- [x] VST3 格式可用。
- [x] 可扫描默认或指定目录。
- [x] 可显示扫描到的插件列表。
- [x] 扫描失败文件明细、多目录扫描输入、扫描结果持久化（M3-P1..P4）均已完成并通过人工验证。

**M3：插件实例化并发声**
- [x] 可选择并加载 VST3 乐器。
- [x] 键盘输入可驱动插件发声。
- [x] 插件处理链路接入 `processBlock`。
- [x] 插件卸载后程序保持可用。
- [x] 可打开并操作插件 editor。
- [x] scan / load / unload / editor / 重扫 / 直接退出等主要生命周期组合路径已完成一轮人工回归。
- [x] 加载插件后切换音频设备设置路径已完成人工回归。
- [~] 外部 MIDI 输入到插件的通路已具备，但退出场景 `6.3` 因无硬件暂未验证。

**M4：键盘映射系统可配置**
- [x] 映射主路径不再强依赖字符输入，使用 JUCE 稳定 keyCode。
- [x] 已建立 `KeyboardLayout` / `KeyBinding` 等核心类型。
- [x] 默认布局可加载。
- [x] 当前布局可保存和恢复。
- [x] 已提供最小布局操作入口（切换 / 保存 / 恢复默认）。
- [x] 默认布局全量手工验证已完成。
- [x] 自定义 Preset 加载/保存/导入/重命名/删除与启动恢复已完成（见 Phase 3）。
- [~] 图形化布局编辑器（当前无 UI，不在近期范围）。

### Phase 3：UI 与高级功能（M5-M7）

状态：已完成。

**M5：UI 进入正式可用阶段**
- [x] 插件扫描、选择、加载、卸载路径完整。
- [x] 插件状态、MIDI 状态、fallback / plugin 发声来源已有基础展示。
- [x] 插件 editor 窗口已独立托管。
- [x] UI 已拆分为头部、插件、参数、键盘等区域。
- [x] `MainComponent` 插件流程职责已完成两轮收敛。
- [~] 错误提示、空状态提示、正式产品 UI 细节仍需完善（低优先级）。

**M6：高级功能恢复（布局 Preset + 录制 / 回放 / MIDI 导出）**
- [x] 预研阶段已完成，8 项设计决策已锁定。
- [x] 实现演奏事件模型骨架（`PerformanceEvent` + sample-based timeline）。
- [x] 实现 `AudioEngine::setRecordingEngine(...)` 与 pre-render `MidiBuffer` 最小录制边界。
- [x] 实现无 UI 内部录制启停入口。
- [x] 实现无 UI 内部回放启停入口。
- [x] 实现录制 UI（Record / Stop / Play 按钮，Idle / Recording / Playing 状态显示）。
- [x] 实现回放逻辑（事件时间线推进，事件重路由到 `AudioEngine`）。
- [x] 实现 MIDI 文件导出（`juce::MidiFile`，Export MIDI 按钮）。
- [x] 实现 WAV 离线渲染（fallback synth 离线渲染核心 + UI 接入 + 专项测试通过）。
- [~] VST3 插件离线渲染（M6-6e）后置，不阻塞 WAV MVP。

**M7：布局 Preset 系统**
- [x] 预研阶段已完成，5 项设计决策已锁定。
- [x] 实现 JSON preset 格式（`.freepiano.layout`）。
- [x] 实现用户目录 preset 自动发现。
- [x] 实现 Preset 导入、保存、重命名、删除。
- [x] 实现启动恢复（按持久化 `layoutId` 恢复）。
- [x] 已补充功能说明与专项测试。

### Phase 4：MIDI 文件导入（M8）

状态：已完成。

**M8：MIDI 文件导入与回放兼容性**
- [x] M8-1：MIDI 文件导入核心（Import MIDI、导入为 `RecordingTake`、导入后回放、错误路径安全返回）。
- [x] M8-1b：自动选择含 note 最多的轨道。
- [x] M8-1c：Import MIDI 按钮状态收敛。
- [x] M8-2：MIDI import playback 边界 + 多轨/tempo 处理。
- [x] M8-3：最近路径记忆 + 回放控制小增强。
- [~] M8-5：合并所有轨道 note 到单一 timeline（已搁置）。
- [x] M8-6：MIDI playback 虚拟键盘可视化。
- [x] M8-7：主窗口尺寸自适应与恢复。

功能与测试文档：[`../features/phase4-midi-file-import.md`](../features/phase4-midi-file-import.md)、[`../testing/phase4-midi-file-import.md`](../testing/phase4-midi-file-import.md)。

### Phase 5：架构收敛与 MainComponent 瘦身

状态：进行中（5.1-5.7 已完成，5.8+ 待规划）。

**目标：** 将 `MainComponent.cpp` 从约 1587 行降至 1200 行以下，通过提取 helper 收敛职责。

**5.1：录制会话状态结构化**（原 AH-1，已完成）
- 将 `currentTake` + `currentRecordingState` + `currentTakeCanBeExported` 合并为单一 `RecordingSession` 结构体。

**5.2：导出流程统一**（原 AH-2，已完成）
- 将 `handleExportMidiClicked()` 和 `handleExportWavClicked()` 中的重复模式统一为 `runExportRecordingFlow()`。

**5.3：布局 CRUD 流程收敛**（原 AH-3，已完成）
- 将布局 handler 中的流程性逻辑抽到 `applyLayoutAndCommit()`、`runLayoutFileChooser()` 等 helper。

**5.4：设置窗口生命周期收敛**（原 AH-4，已完成）
- 将设置窗口管理逻辑抽到 `getSettingsContent()` 等 helper。

**5.5：AppState 清理**（原 AH-5，已完成）
- 移除 `PluginState` / `RuntimePluginState` 中的 UI 派生字段。

**5.6：ControlsPanel 按钮状态统一**（原 AH-6，已完成）
- 将录制/回放/导入导出按钮的 enabled 状态收敛为单一 `RecordingControlsState`。

**5.7：MIDI 导入流程下沉**（原 AH-7，已完成）
- 将 `handleImportMidiClicked()` 收敛为 FileChooser 生命周期 + 顶层编排，抽出 `getLastMidiImportDirectory()`、`tryImportMidiFile()`、`replaceTakeAndStartPlayback()` 三个 helper。

**5.8+：待规划**
- 设置窗口管理提取（约 100 行）。
- 布局管理继续收敛（约 100 行）。
- 插件操作提取（约 130 行）。
- 录制/回放编排提取（约 100 行）。
- 插件 editor window 生命周期提取（约 50 行）。

## 4. 当前近期重点

优先级从高到低：

1. **Phase 5.8+：MainComponent 继续瘦身**
   - 目标：从约 1587 行降至 1200 行以下。
   - 详见 [`current-iteration.md`](current-iteration.md)。

2. **M8 边界稳定**
   - 保持 M8-2 边界：导入 playback take 禁止 MIDI 再导出；导入后允许 WAV 导出。
   - 继续搁置 M8-5 merge-all。

3. **M3 插件宿主持续稳定**
   - 低优先级持续观察退出阶段 Debug 告警。

4. **搁置项（待条件恢复）**
   - 外部 MIDI 硬件依赖验证。
   - VST3 插件离线渲染（M6-6e）。

## 5. 主要风险

| 风险 | 当前判断 | 应对方向 |
|---|---|---|
| 插件生命周期复杂 | 中 | 维护专项生命周期测试，重点覆盖 editor、卸载、重扫、退出。 |
| 外部 MIDI 硬件依赖 | 中 | 外部 MIDI 录制/回放/退出场景因无硬件暂缓；状态已记录至 [`known-issues.md`](../testing/known-issues.md)。 |
| 键盘映射边界多 | 低中 | 基础映射已全量验证；布局 preset 已补充专项回归清单。 |
| `MainComponent` 职责回流 | 低中 | 已通过 Phase 5.1-5.7 架构收敛；MIDI 导入流程已下沉为 3 个 helper。 |
| 录制/回放实现风险 | 中 | MVP 主链路已接入；下一阶段优先收紧实时音频线程边界。 |
| 布局 Preset 实现风险 | 低 | 核心能力已完成并补充功能/测试文档。 |
| 文档状态漂移 | 中 | 本文件作为唯一 roadmap；当前任务只写入 [`current-iteration.md`](current-iteration.md)。 |

## 6. 完成标准参考

阶段性验收标准见：

- [`../testing/acceptance.md`](../testing/acceptance.md)

专项测试见：

- [`../testing/phase2-keyboard-mapping.md`](../testing/phase2-keyboard-mapping.md)
- [`../testing/phase3-layout-presets.md`](../testing/phase3-layout-presets.md)
- [`../testing/phase3-recording-playback.md`](../testing/phase3-recording-playback.md)
- [`../testing/phase4-midi-file-import.md`](../testing/phase4-midi-file-import.md)
- [`../testing/phase2-plugin-host-lifecycle.md`](../testing/phase2-plugin-host-lifecycle.md)

---

## 7. 实现 Backlog

以下条目来自 Phase 2/3，已按实现顺序排列。Phase 3 核心条目已完成，后续新工作优先从 Phase 3 取用。

每个条目格式：`目标 — 关键约束 — 前置条件`

---

### Phase 3-1..3-7：布局 Preset 系统

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

### Phase 3-8..3-13：录制 / 回放 / 导出

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

### Phase 2-1..2-4：插件扫描产品化增强

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

### Phase 5-1..5-4：MainComponent 职责收敛（已合并至 Phase 5）

**Phase 5-1：RecordingFlowSupport 录制 / 回放 UI 流程 helper**
- 状态：已完成

**Phase 5-2：ExportFlowSupport 导出选项与默认文件名 helper**
- 状态：已完成

**Phase 5-3：PluginFlowSupport 继续收敛 scan / restore / cache 流程**
- 状态：已完成

**Phase 5-4：ReadOnlyStateRefresh 边界命名清理**
- 状态：已完成
