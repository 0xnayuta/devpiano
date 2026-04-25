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

最近一轮插件生命周期人工回归已补齐大部分高风险组合路径：scan / load / unload / editor / 重扫 / 直接退出均已完成一轮验证；当前主要剩余问题是音频设置中的 `buffer size` 选项在 Windows 侧手工测试里只能选择 `10 ms`，以及外部 MIDI 打开状态下退出程序仍待真实设备验证。

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
- [~] 外部 MIDI 输入到插件的通路已具备，但仍待更多真实设备手工验证。
- [~] 加载插件后切换音频设备设置时，`buffer size` 选项当前只能选择 `10 ms`；更像是当前音频后端 / 设备能力限制或恢复回退行为，仍待继续确认。
- [~] 特定插件 / Debug 注入环境下退出阶段可能仍有 JUCE / VST3 调试告警，需要持续观察。

### M4：键盘映射系统可配置

状态：部分通过。

- [x] 映射主路径不再强依赖字符输入。
- [x] 已建立 `KeyboardLayout` / `KeyBinding` 等核心类型。
- [x] 默认布局可加载。
- [x] 当前布局可保存和恢复。
- [x] 已提供最小布局操作入口。
- [~] 完整布局编辑、自定义 Preset、默认布局全量回归仍需继续补齐。

### M5：UI 进入正式可用阶段

状态：部分通过。

- [x] 插件扫描、选择、加载、卸载路径完整。
- [x] 插件状态、MIDI 状态、fallback / plugin 发声来源已有基础展示。
- [x] 插件 editor 窗口已独立托管。
- [x] UI 已拆分为头部、插件、参数、键盘等区域。
- [~] `MainComponent` 仍承担一定流程控制和状态装配职责。
- [~] 错误提示、空状态提示、正式产品 UI 细节仍需完善。

### M6：高级功能恢复

状态：未开始 / 早期预研。

- [ ] 布局 Preset 保存 / 加载。
- [ ] 录制事件模型。
- [ ] 演奏录制。
- [ ] 回放。
- [ ] MIDI 导出。
- [ ] WAV 导出。
- [ ] MP4 导出后置，不作为近期主目标。

## 4. 当前近期重点

优先级从高到低：

1. 完善键盘映射系统。
   - 补齐默认布局全量回归。
   - 明确自定义布局 / Preset 设计。
   - 持续验证焦点、输入法、修饰键等边界。

2. 提升插件宿主稳定性。
   - 继续调查音频设置里的 `buffer size` 选项为何当前只能选择 `10 ms`。
   - 在具备外部 MIDI 设备后补齐退出场景 `6.3` 验证。
   - 观察并收敛退出阶段 Debug 告警。
   - 后续视需要拆分 `PluginScanner` 与实例生命周期宿主。

3. 设计录制 / 回放模型。
   - 先定义现代事件模型。
   - 再实现录制和回放。
   - 最后实现 MIDI / WAV 导出。

4. 保持 UI 与状态模型收敛。
   - 避免 `MainComponent` 再次膨胀。
   - 新状态优先通过 `AppState` / builder / UI 子组件边界表达。

## 5. 主要风险

| 风险 | 当前判断 | 应对方向 |
|---|---|---|
| 插件生命周期复杂 | 中高 | 维护专项生命周期测试，重点覆盖 editor、卸载、重扫、退出。 |
| 键盘映射边界多 | 中高 | 保持专项键盘测试，逐步完善布局模型和 Preset。 |
| `MainComponent` 职责回流 | 中 | 新功能优先下沉到独立模块或 helper，不把状态展示逻辑塞回主组件。 |
| 录制 / 导出尚无现代模型 | 中 | 先设计数据模型，不直接复制旧 `song.*` 内部表示。 |
| 文档状态漂移 | 中 | 本文件作为唯一 roadmap；当前任务只写入 [`current-iteration.md`](current-iteration.md)。 |

## 6. 完成标准参考

阶段性验收标准见：

- [`../testing/acceptance.md`](../testing/acceptance.md)

专项测试见：

- [`../testing/keyboard-mapping.md`](../testing/keyboard-mapping.md)
- [`../testing/plugin-host-lifecycle.md`](../testing/plugin-host-lifecycle.md)
