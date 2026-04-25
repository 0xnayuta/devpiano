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

当前项目不再处于“从零接通主链路”的阶段，而是进入：

- 键盘映射系统完善。
- 插件宿主稳定性增强。
- UI 与状态模型继续收敛。
- 录制 / 回放 / 导出等高级功能设计与恢复。

最近一轮插件生命周期人工回归已补齐大部分高风险组合路径：scan / load / unload / editor / 重扫 / 直接退出、音频设备设置切换均已完成一轮验证；当前主要剩余问题是外部 MIDI 打开状态下退出程序仍待真实设备验证。

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

状态：基本通过。

- [x] VST3 格式可用。
- [x] 可扫描默认或指定目录。
- [x] 可显示扫描到的插件列表。
- [~] 扫描失败记录、多目录扫描、扫描结果持久化仍可继续完善。

### M3：插件实例化并发声

状态：基本通过。

- [x] 可选择并加载 VST3 乐器。
- [x] 键盘输入可驱动插件发声。
- [x] 插件处理链路接入 `processBlock`。
- [x] 插件卸载后程序保持可用。
- [x] 可打开并操作插件 editor。
- [x] scan / load / unload / editor / 重扫 / 直接退出等主要生命周期组合路径已完成一轮人工回归。
- [x] 加载插件后切换音频设备设置路径已完成人工回归；不同 `Audio device type` 下的 `buffer size` / `sample rate` 可调范围差异已确认属于后端模式语义范围内的正常现象。
- [~] 外部 MIDI 输入到插件的通路已具备，但退出场景 `6.3` 因无硬件暂未验证。
- [~] 特定插件 / Debug 注入环境下退出阶段可能仍有 JUCE / VST3 调试告警，低优先级持续观察。

### M4：键盘映射系统可配置

状态：基本通过。

- [x] 映射主路径不再强依赖字符输入，使用 JUCE 稳定 keyCode。
- [x] 已建立 `KeyboardLayout` / `KeyBinding` 等核心类型。
- [x] 默认布局可加载。
- [x] 当前布局可保存和恢复。
- [x] 已提供最小布局操作入口（切换 / 保存 / 恢复默认）。
- [x] 默认布局全量手工验证已完成（Q/A/Z/数字行、多键组合、启动一致性均通过）。
- [x] 虚拟键盘翻页后映射稳定性问题已修复并验证。
- [~] 自定义 Preset 加载/保存/导入/导出（见 M7）。
- [~] 图形化布局编辑器（当前无 UI，不在近期范围）。

### M5：UI 进入正式可用阶段

状态：基本通过。

- [x] 插件扫描、选择、加载、卸载路径完整。
- [x] 插件状态、MIDI 状态、fallback / plugin 发声来源已有基础展示。
- [x] 插件 editor 窗口已独立托管。
- [x] UI 已拆分为头部、插件、参数、键盘等区域。
- [x] `MainComponent` 插件流程职责已完成两轮收敛（`PluginFlowSupport` 提取，startup restore plan 驱动）。
- [~] 错误提示、空状态提示、正式产品 UI 细节仍需完善（低优先级）。

### M6：录制 / 回放 / 导出

状态：预研完成，待实现。

- [x] 预研阶段已完成，7 项设计决策已锁定（见 [`../features/recording-playback.md`](../features/recording-playback.md)）。
- [ ] 实现演奏事件模型（`PerformanceEvent` + 高精度时钟）。
- [ ] 实现录制 UI（录制/停止按钮，录音状态显示）。
- [ ] 实现回放逻辑（事件时间线推进，事件重路由到 `AudioEngine`）。
- [ ] 实现 MIDI 文件导出（`juce::MidiFile`）。
- [ ] 实现 WAV 离线渲染（MIDI → 离线音频链路 → WAV 文件）。
- [ ] MP4 导出不作为近期主目标。

### M7：布局 Preset 系统

状态：预研完成，待实现。

- [x] 预研阶段已完成，5 项设计决策已锁定（见 [`../features/layout-presets.md`](../features/layout-presets.md)）。
- [ ] 实现 JSON preset 格式（`.freepiano.layout`）。
- [ ] 实现用户目录 preset 自动发现（`ControlsPanel` 列表聚合内置 + 用户 preset）。
- [ ] 实现 Preset 加载（原生文件对话框选择任意位置）。
- [ ] 实现 Preset 保存（默认到用户配置目录）。
- [ ] 实现 Preset 删除（内置不可删）。
- [ ] ID 冲突时提醒用户重新命名。

## 4. 当前近期重点

录制/回放（M6）和布局 Preset（M7）的设计草案已完成，后续可直接从 backlog 取用。

优先级从高到低：

1. **M7 布局 Preset**（实现风险低，收益明确）
   - 从 JSON preset 格式和 preset 发现机制开始。
   - 逐步实现 load / save / delete UI。

2. **M6 录制/回放**（模型已锁定，实现需谨慎）
   - 从演奏事件模型和时钟基础开始。
   - 录制 UI → 回放逻辑 → MIDI 导出 → WAV 渲染（按序推进）。

3. **M3 插件宿主持续稳定**
   - 外部 MIDI 设备可用后补齐退出场景 `6.3` 验证。
   - 低优先级持续观察退出阶段 Debug 告警。

4. **保持架构健康**
   - 避免 `MainComponent` 再次膨胀。
   - 新状态优先通过 `AppState` / builder / UI 子组件边界表达。

## 5. 主要风险

| 风险 | 当前判断 | 应对方向 |
|---|---|---|
| 插件生命周期复杂 | 中 | 维护专项生命周期测试，重点覆盖 editor、卸载、重扫、退出。 |
| 键盘映射边界多 | 低中 | 基础映射已全量验证，M7 实现 preset 时注意不破坏现有行为。 |
| `MainComponent` 职责回流 | 低 | 已通过两轮收敛建立了 `PluginFlowSupport` 边界，后续保持纪律。 |
| 录制/回放实现风险 | 中 | 已完成设计预研，实现时按 M6-1 到 M6-6 顺序小步推进。 |
| 布局 Preset 实现风险 | 低中 | 设计草案已锁定，实现风险低。 |
| 文档状态漂移 | 中 | 本文件作为唯一 roadmap；当前任务只写入 [`current-iteration.md`](current-iteration.md)。 |

## 6. 完成标准参考

阶段性验收标准见：

- [`../testing/acceptance.md`](../testing/acceptance.md)

专项测试见：

- [`../testing/keyboard-mapping.md`](../testing/keyboard-mapping.md)
- [`../testing/plugin-host-lifecycle.md`](../testing/plugin-host-lifecycle.md)

---

## 7. 实现 Backlog

以下条目均来自 M6 / M7，已按实现顺序排列。可直接取用作为下一轮迭代的任务单元。

每个条目格式：`目标 — 关键约束 — 前置条件`

---

### M7：布局 Preset 系统

**M7-1：JSON preset 文件格式**
- 目标：定义 `.freepiano.layout` JSON schema，含 version、id、name、bindings（含 keyCode/displayText/action）
- 关键约束：使用 JUCE keyCode，不含 velocity/transpose 等演奏参数
- 前置条件：无

**M7-2：Preset 读写工具函数**
- 目标：在 `source/` 下新建 `LayoutPreset.h/.cpp`，提供 `loadLayoutPreset(path)` → `KeyboardLayout`、`saveLayoutPreset(layout, path)` → `bool`
- 关键约束：使用 JUCE `File` / `JSON` API，不引入新外部依赖
- 前置条件：M7-1 完成

**M7-3：用户目录 Preset 自动发现**
- 目标：`ControlsPanel` 启动时扫描用户配置目录，聚合内置 preset + `.freepiano.layout` 文件到 layout 列表
- 关键约束：用户 preset 只在目录中可见，内置 preset 不可被删除
- 前置条件：M7-2 完成

**M7-4：Preset 加载（文件对话框）**
- 目标：`ControlsPanel` 或 `PluginPanel` 提供"导入"入口，通过原生文件对话框选择任意位置的 `.freepiano.layout` 并应用
- 关键约束：复用已有 `KeyboardMidiMapper::setLayout()` 路径，不新增独立加载逻辑
- 前置条件：M7-2 完成

**M7-5：Preset 保存（默认目录）**
- 目标：`ControlsPanel` 提供"导出"或"另存为"入口，将当前 `KeyboardLayout` 保存为 JSON 到用户配置目录
- 关键约束：默认路径为用户配置目录；ID 冲突时提醒用户重新命名
- 前置条件：M7-2、M7-3 完成

**M7-6：Preset 删除**
- 目标：在 `ControlsPanel` layout 列表中支持右键删除用户 preset（内置不可删）
- 关键约束：只删文件，不动内置 preset
- 前置条件：M7-3 完成

---

### M6：录制 / 回放 / 导出

**M6-1：演奏事件模型与时钟**
- 目标：定义 `PerformanceEvent { timestamp, source, MidiMessage, inputIndex }` 和高精度录制时钟，与音频回调解耦
- 关键约束：不复制旧 `song_event_t` 字节数组；使用 `std::chrono`
- 前置条件：无

**M6-2：RecordingState 与录制生命周期**
- 目标：定义 `RecordingState { idle, recording, paused }`，管理 `recordPosition`、`songEnd` 等；接入 `KeyboardMidiMapper` 和 `MidiRouter` 的 MIDI 事件进行录制
- 关键约束：录制的是 MIDI 输出事件，不是音频；`song_update` 逻辑独立，不与音频回调紧耦合
- 前置条件：M6-1 完成

**M6-3：录制 UI（录制/停止按钮，录音状态显示）**
- 目标：在 `ControlsPanel` 或 `HeaderPanel` 添加录制/停止按钮和录音状态指示
- 关键约束：复用已有 `AudioEngine` / `MidiRouter` 路由，不新增 MIDI 监听链路
- 前置条件：M6-2 完成

**M6-4：回放逻辑**
- 目标：实现 `PlaybackState { idle, playing }` 和 `update(timestamp)`，将录制事件按时间线重新路由到 `AudioEngine`
- 关键约束：回放不走 `KeyboardMidiMapper`（避免键盘映射不一致）；播放完毕自动停止
- 前置条件：M6-1、M6-2 完成

**M6-5：MIDI 文件导出（`juce::MidiFile`）**
- 目标：将 `PerformanceEvent` 序列化为标准 MIDI Type 1 文件
- 关键约束：使用 JUCE `MidiFile`，兼容任何 DAW
- 前置条件：M6-1、M6-2 完成

**M6-6：WAV 离线渲染**
- 目标：将 MIDI 文件通过离线音频链路（不经过实时音频设备）渲染为 WAV 文件
- 关键约束：离线渲染，不影响实时播放；使用 JUCE `AudioFormatManager`
- 前置条件：M6-5 完成
