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

当前项目已经完成第一轮可运行主干：

- [x] CMake + Ninja 构建基线可用。
- [x] WSL 主工作树 + Windows 镜像树 + MSVC 验证工作流可用。
- [x] JUCE GUI 主程序可启动。
- [x] 音频设备可初始化。
- [x] 电脑键盘可触发 MIDI note。
- [x] 虚拟钢琴键盘可联动显示。
- [x] 外部 MIDI 输入代码通路已接入。
- [x] 可扫描、选择、加载 VST3 插件。
- [x] 已加载插件可参与 `processBlock` 并发声。
- [x] 可打开支持 editor 的插件窗口。
- [x] 基础设置、插件恢复信息与布局标识具备持久化能力。
- [x] UI 已形成 `HeaderPanel` / `PluginPanel` / `ControlsPanel` / `KeyboardPanel` 的基础组件分层。

当前项目不再处于"从零接通主链路"的阶段，也已完成布局 Preset、录制 / 回放 / MIDI 导出 MVP、WAV fallback 离线渲染、MainComponent 录制/回放/导出状态流收敛（MC-1..MC-4）以及 M8 MIDI 文件导入核心能力。当前进入：

- M8 MIDI 文件导入与回放兼容性收敛。
- M8-2 import playback / tempo / 多轨边界语义已确认并通过人工验收：导入 playback take 禁止再次导出 MIDI；导入后 WAV 导出归 M6-6e 搁置。
- M8-3 最近路径与小型回放控制增强补齐。
- M8-5 merge-all 单 timeline 已搁置；当前保留 note-rich 单轨选择为推荐模式。
- 外部 MIDI 硬件依赖回归补齐（搁置，待硬件条件恢复）。

最近一轮 M8 人工回归已确认 M8-1b、M8-1c、M8-2、M8-6、M8-7 无明显问题。插件生命周期人工回归也已补齐大部分高风险组合路径：scan / load / unload / editor / 重扫 / 直接退出、音频设备设置切换、M3-P1..P4 插件扫描产品化增强均已完成一轮验证；外部 MIDI 硬件依赖验证仍因无设备暂缓，状态已记录至 [`known-issues.md`](../testing/known-issues.md)。
## 3. 阶段路线图

### M0：工程骨架可运行

状态：已通过。

- [x] JUCE GUI 程序可启动。
- [x] Debug 构建可通过。
- [x] 主窗口可显示。
- [x] 音频设备可初始化。

### M1：最小演奏链路成立

状态：已通过。

- [x] 电脑键盘可触发 note on / note off。
- [x] 虚拟钢琴键盘可联动显示。
- [x] 程序可发声，来源为内置 fallback synth 或已加载插件。
- [x] 长按、快速连按、焦点切换等基础场景已完成一轮验证。

### M2：最小插件扫描能力成立

状态：已通过。

- [x] VST3 格式可用。
- [x] 可扫描默认或指定目录。
- [x] 可显示扫描到的插件列表。
- [x] 扫描失败文件明细、多目录扫描输入、扫描结果持久化（M3-P1..P4）均已完成并通过人工验证。

### M3：插件实例化并发声

状态：已通过。

- [x] 可选择并加载 VST3 乐器。
- [x] 键盘输入可驱动插件发声。
- [x] 插件处理链路接入 `processBlock`。
- [x] 插件卸载后程序保持可用。
- [x] 可打开并操作插件 editor。
- [x] scan / load / unload / editor / 重扫 / 直接退出等主要生命周期组合路径已完成一轮人工回归。
- [x] 加载插件后切换音频设备设置路径已完成人工回归；不同 `Audio device type` 下的 `buffer size` / `sample rate` 可调范围差异已确认属于后端模式语义范围内的正常现象。
- [~] 外部 MIDI 输入到插件的通路已具备，但退出场景 `6.3` 因无硬件暂未验证（详见 [`known-issues.md`](../testing/known-issues.md) §1）。
- [~] 特定插件 / Debug 注入环境下退出阶段可能仍有 JUCE / VST3 调试告警，低优先级持续观察。

### M4：键盘映射系统可配置

状态：已通过。

- [x] 映射主路径不再强依赖字符输入，使用 JUCE 稳定 keyCode。
- [x] 已建立 `KeyboardLayout` / `KeyBinding` 等核心类型。
- [x] 默认布局可加载。
- [x] 当前布局可保存和恢复。
- [x] 已提供最小布局操作入口（切换 / 保存 / 恢复默认）。
- [x] 默认布局全量手工验证已完成（Q/A/Z/数字行、多键组合、启动一致性均通过）。
- [x] 虚拟键盘翻页后映射稳定性问题已修复并验证。
- [x] 自定义 Preset 加载/保存/导入/重命名/删除与启动恢复已完成（见 M7）。
- [~] 图形化布局编辑器（当前无 UI，不在近期范围）。

### M5：UI 进入正式可用阶段

状态：已通过。

- [x] 插件扫描、选择、加载、卸载路径完整。
- [x] 插件状态、MIDI 状态、fallback / plugin 发声来源已有基础展示。
- [x] 插件 editor 窗口已独立托管。
- [x] UI 已拆分为头部、插件、参数、键盘等区域。
- [x] `MainComponent` 插件流程职责已完成两轮收敛（`PluginFlowSupport` 提取，startup restore plan 驱动）加上 MC-1..MC-4 状态流收敛（RecordingFlowSupport / ExportFlowSupport / PluginFlowSupport 收敛 / 状态函数命名清理）。
- [~] 错误提示、空状态提示、正式产品 UI 细节仍需完善（低优先级）。

### M6：高级功能恢复（布局 Preset + 录制 / 回放 / MIDI 导出）

状态：MVP 已恢复。布局 Preset 保存/加载/导入/重命名/删除/启动恢复已完成；录制 / 停止 / 回放 / MIDI 导出最小闭环已接入。WAV 离线渲染、MP4 导出、复杂编辑、tempo map 等仍为后续增强。

- [x] 预研阶段已完成，8 项设计决策已锁定（见 [`../features/M6-recording-playback.md`](../features/M6-recording-playback.md)）。
- [x] 实现演奏事件模型骨架（`PerformanceEvent` + sample-based timeline）。
- [x] 实现 `AudioEngine::setRecordingEngine(...)` 与 pre-render `MidiBuffer` 最小录制边界。
- [x] 明确 owner / detach / preallocation / overflow 规则（`MainComponent` 持有，`AudioEngine` 非拥有引用，容量耗尽时丢弃新事件并计数）。
- [x] 实现无 UI 内部录制启停入口（暂停 audio callback 后执行 reserve/start 或 stop）。
- [x] 实现无 UI 内部回放启停入口（`startInternalPlayback` / `stopInternalPlayback`，stop 时 all-notes-off）。
- [x] 实现录制 UI（Record / Stop / Play 按钮，Idle / Recording / Playing 状态显示）。
- [x] 实现回放逻辑（事件时间线推进，事件重路由到 `AudioEngine`）。
- [x] 实现 MIDI 文件导出（`juce::MidiFile`，Export MIDI 按钮）。
- [x] 实现 WAV 离线渲染（M6-6a/b/c/d：fallback synth 离线渲染核心 + UI 接入 + 专项测试通过；E.1–E.9 全部通过）。
- [~] VST3 插件离线渲染（M6-6e）后置，不阻塞 WAV MVP。
- [ ] MP4 导出不作为近期主目标。

### M7：布局 Preset 系统

状态：核心能力已完成，专项回归清单已建立。

- [x] 预研阶段已完成，5 项设计决策已锁定（见 [`../features/M7-layout-presets.md`](../features/M7-layout-presets.md)）。
- [x] 实现 JSON preset 格式（`.freepiano.layout`，含 `version` / `id` / `name` / `displayName` / `bindings`）。
- [x] 实现用户目录 preset 自动发现（`ControlsPanel` 列表聚合内置 + 用户 preset）。
- [x] 实现 Preset 导入（原生文件对话框选择任意位置，复制到用户布局目录并应用）。
- [x] 实现 Preset 保存（保存到文件对话框选择的目标路径；保存到用户目录时可被后续扫描恢复）。
- [x] 实现 Preset 重命名显示名称（不改变文件名和稳定 `layoutId`）。
- [x] 实现 Preset 删除（内置不可删；删除当前用户 preset 时回退到 `FreePiano Minimal`）。
- [x] 实现启动恢复（按持久化 `layoutId` 恢复内置或用户 preset，并叠加 `keyMap`）。
- [~] ID 冲突当前通过“用户 preset id 由文件名派生 + 导入同名文件覆盖”处理，尚无单独冲突提醒 UI。
- [x] 已补充功能说明与专项测试：[`../features/M7-layout-presets.md`](../features/M7-layout-presets.md)、[`../testing/layout-presets.md`](../testing/layout-presets.md)。

### M8：MIDI 文件导入与回放兼容性

状态：核心导入与多项体验增强已完成；Import MIDI 按钮状态、M8-2 playback / tempo / 多轨边界、M8-3、M8-6、M8-7 均已通过人工验收；merge-all 已搁置。

- [x] M8-1：MIDI 文件导入核心（Import MIDI、导入为 `RecordingTake`、导入后回放、错误路径安全返回）。
- [x] M8-1b：自动选择含 note 最多的轨道，解决常见 Type 1 MIDI track 0 只有 tempo/meta 导致无声的问题。
- [x] M8-1c：Import MIDI 按钮状态收敛。Record / Play / Stop / Back / Import MIDI / Export MIDI / Export WAV 已纳入统一录制 / 回放按钮状态刷新；Recording 期间禁用 Import MIDI，Playing 期间允许安全导入另一个 MIDI 替换 playback。已通过人工验收。
- [x] M8-2：MIDI import playback 边界 + 多轨/tempo 处理。PPQ/timeFormat 修正、轨道诊断和自动选轨已具备；导入 playback take 禁止再次导出 MIDI。多轨、非 960 PPQ、tempo meta event 和复杂 tempo map 限制人工验收未发现明显问题。导入 MIDI 后允许导出 WAV 归入 M6-6e，暂时搁置。
- [x] M8-3：最近路径记忆 + 回放控制小增强。最近导入/导出路径已实现；播放中点击 `Back` 可从当前 take 开头重新播放。已通过人工验收。
- [~] M8-5：合并所有轨道 note 到单一 timeline。当前未实现且已搁置；继续保留 note-rich 单轨选择为推荐模式，merge-all 以后再考虑。
- [x] M8-6：MIDI playback 虚拟键盘可视化，已通过人工验收。
- [x] M8-7：主窗口尺寸自适应与恢复，已通过人工验收。

功能与测试文档：[`../features/M8-midi-file-and-freepiano-gap.md`](../features/M8-midi-file-and-freepiano-gap.md)、[`../testing/midi-file-import.md`](../testing/midi-file-import.md)。

## 4. 当前近期重点

布局 Preset（M7）核心能力已完成；M6 高级功能 MVP 已恢复；M8 MIDI 文件导入核心能力、note-rich 自动选轨、Import MIDI 按钮状态收敛、M8-2 playback / tempo / 多轨边界、playback 虚拟键盘可视化和主窗口尺寸恢复均已完成。当前重点转为保持 M8 边界稳定与架构健康。

优先级从高到低：

1. **M8 边界稳定**
   - 保持 M8-2 边界：导入 playback take 禁止 MIDI 再导出；导入后 WAV 导出进入 M6-6e backlog。
   - 继续搁置 M8-5 merge-all，避免默认合并带来嘈杂/鼓轨/多音色问题。

2. **保持架构健康**
   - 避免 `MainComponent` 再次膨胀。
   - 新状态优先通过 `AppState` / builder / UI 子组件边界表达。

3. **M3 插件宿主持续稳定与产品化增强**
   - M3-P1..P4 已完成，低优先级持续观察退出阶段 Debug 告警。

4. **M6 / 外部 MIDI 硬件依赖项（搁置，待条件恢复）**
   - M6 录制 / 回放稳定化最后一项边界检查暂搁置。
   - 外部 MIDI 录制 / 回放验证、插件生命周期退出场景 `6.3` 暂搁置。
   - 状态已持久化记录在 [`../testing/recording-playback.md`](../testing/recording-playback.md)。

5. **M7 布局 Preset 持续打磨**（核心能力已完成）
   - 按 [`../testing/layout-presets.md`](../testing/layout-presets.md) 做专项回归。
   - 后续只保留低优先级增强，如冲突提示、图形化布局编辑器、per-key label/color。

## 5. 主要风险

| 风险 | 当前判断 | 应对方向 |
|---|---|---|
| 插件生命周期复杂 | 中 | 维护专项生命周期测试，重点覆盖 editor、卸载、重扫、退出；退出场景 6.3 因硬件条件暂缓。 |
| 外部 MIDI 硬件依赖 | 中 | 外部 MIDI 录制/回放/退出场景 6.3 均因无硬件暂缓；状态已记录至 [`known-issues.md`](../testing/known-issues.md)。 |
| 键盘映射边界多 | 低中 | 基础映射已全量验证；布局 preset 已补充专项回归清单，后续改动按清单回归。 |
| `MainComponent` 职责回流 | 低 | 已通过 MC-1..MC-4 四轮收敛建立了 RecordingFlowSupport / ExportFlowSupport / PluginFlowSupport 边界；`MainComponent` 只保留 UI 组件拥有权、JUCE 生命周期入口、窗口生命周期和顶层装配。 |
| 录制/回放实现风险 | 中 | MVP 主链路已接入；下一阶段优先收紧实时音频线程边界、回放结束通知、采样率缩放与 Stop 清理悬挂音路径。 |
| 布局 Preset 实现风险 | 低 | 核心能力已完成并补充功能/测试文档；后续主要是低优先级体验增强。 |
| 文档状态漂移 | 中 | 本文件作为唯一 roadmap；当前任务只写入 [`current-iteration.md`](current-iteration.md)。 |

## 6. 完成标准参考

阶段性验收标准见：

- [`../testing/acceptance.md`](../testing/acceptance.md)

专项测试见：

- [`../testing/keyboard-mapping.md`](../testing/keyboard-mapping.md)
- [`../testing/layout-presets.md`](../testing/layout-presets.md)
- [`../testing/recording-playback.md`](../testing/recording-playback.md)
- [`../testing/midi-file-import.md`](../testing/midi-file-import.md)
- [`../testing/plugin-host-lifecycle.md`](../testing/plugin-host-lifecycle.md)

---

## 7. 实现 Backlog

以下条目来自 M6 / M7，已按实现顺序排列。M7 核心条目已完成，后续新工作优先从 M6 取用。

每个条目格式：`目标 — 关键约束 — 前置条件`

---

### M7：布局 Preset 系统

**M7-1：JSON preset 文件格式**
- 目标：定义 `.freepiano.layout` JSON schema，含 version、id、name、bindings（含 keyCode/displayText/action）
- 关键约束：使用 JUCE keyCode，不含 velocity/transpose 等演奏参数
- 前置条件：无
- 状态：已完成（当前还包含 `displayName` 字段）

**M7-2：Preset 读写工具函数**
- 目标：在 `source/` 下新建 `LayoutPreset.h/.cpp`，提供 `loadLayoutPreset(path)` → `KeyboardLayout`、`saveLayoutPreset(layout, path)` → `bool`
- 关键约束：使用 JUCE `File` / `JSON` API，不引入新外部依赖
- 前置条件：M7-1 完成
- 状态：已完成

**M7-3：用户目录 Preset 自动发现**
- 目标：`ControlsPanel` 启动时扫描用户配置目录，聚合内置 preset + `.freepiano.layout` 文件到 layout 列表
- 关键约束：用户 preset 只在目录中可见，内置 preset 不可被删除
- 前置条件：M7-2 完成
- 状态：已完成

**M7-4：Preset 加载（文件对话框）**
- 目标：`ControlsPanel` 或 `PluginPanel` 提供"导入"入口，通过原生文件对话框选择任意位置的 `.freepiano.layout` 并应用
- 关键约束：复用已有 `KeyboardMidiMapper::setLayout()` 路径，不新增独立加载逻辑
- 前置条件：M7-2 完成
- 状态：已完成（当前入口位于 `ControlsPanel`，导入后复制到用户布局目录并应用）

**M7-5：Preset 保存（Save Layout）**
- 目标：`ControlsPanel` 提供"另存为"入口，将当前 `KeyboardLayout` 保存为 JSON
- 关键约束：保存到文件对话框选择的目标路径；保存到用户布局目录时可被后续扫描恢复
- 前置条件：M7-2、M7-3 完成
- 状态：已完成

**M7-6：Preset 删除**
- 目标：在 `ControlsPanel` 中支持删除用户 preset（内置不可删）
- 关键约束：只删文件，不动内置 preset
- 前置条件：M7-3 完成
- 状态：已完成（当前为独立 `Delete` 按钮 + 确认对话框）

**M7-7：Preset 重命名显示名称**
- 目标：支持修改用户 preset 在下拉菜单中的显示名称
- 关键约束：只改 JSON `name` / `displayName`，不改文件名和稳定 `layoutId`
- 前置条件：M7-3 完成
- 状态：已完成（当前为独立 `Rename` 按钮）

---

### M6：录制 / 回放 / 导出

**M6-1：演奏事件模型与时钟**
- 目标：定义 `PerformanceEvent { timestampSamples, source, MidiMessage }` 与 `RecordingTake { sampleRate, lengthSamples, events }`
- 关键约束：不复制旧 `song_event_t` 字节数组；内部优先使用 sample-based timeline，进入 `MidiBuffer` 时转换为 block-local sample offset
- 前置条件：无
- 状态：已完成并接入 `AudioEngine` 录制 / 回放主链路

**M6-2：AudioEngine MIDI block 边界**
- 目标：在 `AudioEngine::getNextAudioBlock()` 中确定 block-local `MidiBuffer` 的录制 handoff 边界，并用 `RecordingEngine::recordMidiBufferBlock()` 表达事件时间戳转换方式
- 关键约束：录制的是交给插件 / fallback synth 前的 pre-render `MidiBuffer` 演奏事件，不是音频；不新增第二套 MIDI 监听链路；第一版把混合后的 block MIDI 标记为 `realtimeMidiBuffer`；`timestampSamples` 是唯一权威时间线；audio callback 中不做文件 IO、UI 或阻塞操作
- 前置条件：M6-1 完成
- 状态：已完成。边界设计、辅助 API、`AudioEngine::setRecordingEngine(...)` 注入、owner/preallocation 规则、UI 入口和回放入口均已接入

**M6-3：录制 UI（录制/停止按钮，录音状态显示）**
- 目标：在 `ControlsPanel` 或 `HeaderPanel` 添加录制/停止按钮和录音状态指示
- 关键约束：复用已有 `AudioEngine` / `MidiRouter` 路由，不新增 MIDI 监听链路
- 前置条件：M6-2 完成
- 状态：已完成。`ControlsPanel` 已提供 Record / Stop / Play / Export MIDI 与状态文本

**M6-4：回放逻辑**
- 目标：实现 `PlaybackState { idle, playing }`，按当前 audio block 将到期事件写入 `MidiBuffer`
- 关键约束：回放不走 `KeyboardMidiMapper`（避免键盘映射不一致）；回放事件进入现有 plugin / fallback synth 发声路径；播放完毕自动停止并清理悬挂音
- 前置条件：M6-1、M6-2 完成
- 状态：已完成。M6-4 最小 UI（Record/Stop/Play 按钮）已完成并接入 MainComponent。

**M6-5：MIDI 文件导出（`juce::MidiFile`）**
- 目标：将 `PerformanceEvent` 序列化为标准 MIDI Type 1 文件
- 关键约束：使用 JUCE `MidiFile`，兼容任何 DAW
- 前置条件：M6-1、M6-2 完成
- 状态：已完成。`MidiFileExporter::exportTakeAsMidiFile()` + `ControlsPanel` Export MIDI 按钮已接入。

**M6-6：WAV 离线渲染**
- 目标：将 MIDI 文件通过离线音频链路（不经过实时音频设备）渲染为 WAV 文件
- 关键约束：离线渲染，不影响实时播放；需单独处理插件离线渲染、当前插件状态和导出期间 UI / 音频设备边界
- 前置条件：M6-5 完成
- 状态：M6-6a/b/c/d 已完成；fallback synth WAV 导出、UI 接入和专项测试 E.1–E.9 均已通过。VST3 插件离线渲染（M6-6e）后置。

---

### M3：插件扫描产品化增强

**M3-P1：扫描失败文件明细**
- 目标：记录 `PluginDirectoryScanner::getFailedFiles()` 中的具体失败文件路径，而不是只显示失败数量
- 关键约束：先进入 Logger / 状态摘要，不在 audio callback 中做日志或 UI；扫描失败不应导致崩溃
- 前置条件：现有 `PluginHost::scanVst3Plugins()` 保持可用
- 状态：已完成。`PluginHost` 保留 `lastScanFailedFiles`，扫描失败路径写入 Logger，UI 摘要提示 `see log`

**M3-P2：多目录扫描输入与持久化**
- 目标：明确多个 VST3 搜索目录的输入格式、UI 表达和 `juce::FileSearchPath` 持久化语义
- 关键约束：保留默认搜索路径 fallback；无效目录不阻塞其他有效目录；路径输入框可先继续承载多目录字符串
- 前置条件：M3-P1 可独立实施，不互相阻塞
- 状态：已完成。路径输入框支持 `FileSearchPath` 多目录字符串；扫描前过滤不存在的目录，空路径回退默认路径，扫描后持久化规范化路径

**M3-P3：扫描结果持久化 / 启动恢复优化**
- 目标：评估并实现 `KnownPluginList` 缓存，降低启动恢复对同步扫描的依赖
- 关键约束：缓存失效时安全回退重扫；插件文件不存在或格式不匹配时不崩溃；不改变当前手动扫描主路径
- 前置条件：现有启动恢复计划与插件列表 UI 稳定
- 状态：已完成。扫描后保存 `KnownPluginList::createXml()` 到设置；启动时优先 `recreateFromXml()` 恢复缓存，缓存缺失或为空时回退重扫

**M3-P4：扫描空状态 / 失败状态 / 恢复失败提示**
- 目标：区分“尚未扫描”“扫描成功但无插件”“扫描失败”“上次插件恢复失败”等用户可见状态
- 关键约束：避免状态文本过长；细节先写 Logger；不以阻塞弹窗作为主反馈
- 前置条件：M3-P1 / M3-P2 可提供更完整上下文，但本项可先小步实现
- 状态：已完成。插件状态文本可区分未扫描、无可用目录、扫描成功但无插件、扫描有失败文件、缓存为空和上次插件加载失败

---

### MC：MainComponent 职责收敛

**MC-1：RecordingFlowSupport 录制 / 回放 UI 流程 helper**
- 目标：抽离 `Record / Stop / Play` 的状态转换和按钮状态更新策略
- 关键约束：不移动 audio callback 逻辑；不改变 `RecordingEngine` 数据模型；保持现有手工回归行为
- 前置条件：M6 录制 / 回放 MVP 稳定
- 状态：已完成。新增 `source/Recording/RecordingFlowSupport.*`，承载 Record / Play / Stop intent 到 command / next-state 的纯流程决策

**MC-2：ExportFlowSupport 导出选项与默认文件名 helper**
- 目标：抽离 MIDI / WAV 默认文件名、空 take 判断、WAV options 构建和导出日志前缀
- 关键约束：`FileChooser` 生命周期仍由 `MainComponent` 持有；不改变 `MidiFileExporter` / `WavFileExporter` 核心实现
- 前置条件：M6-5 MIDI 导出与 M6-6 WAV 导出已完成
- 状态：已完成。新增 `source/Export/ExportFlowSupport.*`，`MainComponent` 导出 handler 只保留 FileChooser 生命周期和导出函数调用

**MC-3：PluginFlowSupport 继续收敛 scan / restore / cache 流程**
- 目标：把启动缓存恢复、scan path normalise、KnownPluginList cache 更新等流程继续从 `MainComponent` 收到 `PluginFlowSupport`
- 关键约束：不移动 plugin editor window 拥有权；不改变 `PluginHost` 实例生命周期
- 前置条件：M3-P1..P4 已完成并通过人工验证
- 状态：已完成。`PluginFlowSupport` 已接管 cached plugin list 恢复、扫描后 recovery 更新和 `KnownPluginList` cache 写回

**MC-4：ReadOnlyStateRefresh 边界命名清理**
- 目标：统一 `createRuntime*StateSnapshot()` / `applyReadOnlyUiState()` / `refreshReadOnlyUiState()` 的命名和调用时机
- 关键约束：不引入复杂状态管理框架；`AppState` 仍是快照模型，不变成全局可变 store
- 前置条件：MC-1..MC-3 至少完成一部分后再做，避免过早抽象
- 状态：已完成。状态函数命名已收敛为 `build*Snapshot()`、`renderReadOnlyUiState()`、`refreshReadOnlyUiStateFromCurrentSnapshot()` 和 `refreshMidiStatusFromCurrentSnapshot()`
