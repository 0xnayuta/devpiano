# 插件宿主功能说明

> 用途：说明当前 VST3 插件扫描、加载、处理、editor 和生命周期相关功能行为。  
> 相关测试：[`../testing/plugin-host-lifecycle.md`](../testing/plugin-host-lifecycle.md)。  
> 当前状态来源：[`../roadmap/roadmap.md`](../roadmap/roadmap.md)、[`../testing/plugin-host-lifecycle.md`](../testing/plugin-host-lifecycle.md)、[`../architecture/overview.md`](../architecture/overview.md)、[`../architecture/legacy-migration.md`](../architecture/legacy-migration.md)。

## 当前能力

当前插件宿主已经具备最小可用能力：

- 可识别并使用 VST3 插件格式。
- 可扫描默认或指定 VST3 目录。
- 插件路径输入支持 `juce::FileSearchPath` 语义的多目录字符串（分号或逗号分隔）；扫描前会过滤不存在的目录并持久化规范化后的路径。
- 可显示扫描到的插件名称列表。
- 可从扫描结果中选择并加载 VST3 乐器。
- 已加载插件可参与 `processBlock` 并发声。
- 电脑键盘输入可驱动已加载插件发声。
- 外部 MIDI 输入到插件的代码通路已接入，但仍待更多真实设备手工验证。
- 插件卸载后程序保持可用。
- 可打开支持 editor 的插件窗口。
- 基础插件恢复信息可持久化，例如最近扫描路径和上次插件名称。
- 已扫描插件列表会以 JUCE `KnownPluginList` XML 缓存到设置；启动时优先恢复缓存，避免每次启动都同步重扫。
- 扫描失败时会保留失败文件路径明细，并将每个失败文件写入 Logger（UI 摘要显示失败数量）。
- 扫描状态会区分尚未扫描、无可用扫描目录、扫描成功但无插件、扫描失败文件、缓存恢复为空和上次插件加载失败。
- 未加载插件时，程序可使用内置 fallback synth 保持最小可发声能力。

## 主要链路

### 插件扫描与加载

```text
Plugin scan path
  -> PluginHost scan
  -> KnownPluginList
  -> selected PluginDescription
  -> AudioPluginInstance
  -> prepareToPlay / processBlock / releaseResources
```

### 演奏到插件发声

```text
Computer keyboard / external MIDI
  -> MidiMessageCollector / MidiKeyboardState
  -> AudioEngine
  -> loaded VST3 plugin
  -> AudioDeviceManager
  -> Audio Output
```

## 当前实现边界

当前插件宿主以 JUCE 插件宿主能力为基础：

- `PluginHost` 负责插件格式管理、VST3 扫描、实例加载、prepare/release 与卸载。
- `AudioEngine` 优先驱动已加载插件实例的 `processBlock`。
- 没有加载插件时，`AudioEngine` 回退到内置 fallback synth。
- 插件 editor 由独立窗口托管。

## 当前已验证行为

已在项目状态中确认的能力包括：

- VST3 扫描、选择、加载路径已经成立。
- 已加载 VST3 插件可参与音频处理并发声。
- 支持打开并操作插件 editor。
- 插件卸载后程序保持可用。
- 已完成一轮插件生命周期基线人工回归，当前已验证：
  - 扫描后加载插件并试弹。
  - 连续加载 / 卸载同一插件 3~5 次。
  - 已加载插件状态下重新扫描。
  - 打开并关闭 editor。
  - 打开 editor 后卸载插件。
  - 打开 editor 后重新扫描。
  - 连续执行 load / unload / scan。
  - 加载插件后直接退出程序。
  - 打开 editor 后直接退出程序。

当前仍待继续补齐或复核的生命周期相关场景：

- 外部 MIDI 打开状态下直接退出程序：因当前缺少外部 MIDI 设备，尚未完成手工验证。

详见：[`../testing/plugin-host-lifecycle.md`](../testing/plugin-host-lifecycle.md)。

## 当前限制和风险

当前仍需继续完善或观察：

- 扫描失败记录、多目录扫描、扫描结果持久化仍可继续完善。
- 外部 MIDI 输入到插件的真实设备场景仍待更多手工验证。
- 不同 `Audio device type` 下的 `buffer size` / `sample rate` 可调范围存在后端语义差异；当前已确认这不构成插件生命周期缺陷，但仍需在用户文档或设置说明中逐步提高可解释性。
- 特定插件 / Debug 注入环境下退出阶段可能仍有 JUCE / VST3 调试告警，需要持续观察。
- 外部 MIDI 打开状态下退出程序（生命周期测试 6.3）仍待设备条件补齐。
- 后续视需要拆分 `PluginScanner` 与实例生命周期宿主职责。

## 与旧 FreePiano 的关系

旧 FreePiano 中的 `synthesizer_vst.*` / `vst/*` 可用于理解历史功能边界和插件交互预期。

新实现不沿用旧版 VST SDK 风格接口，而是使用 JUCE：

- `AudioPluginFormatManager`
- `KnownPluginList`
- `PluginDirectoryScanner`
- `AudioPluginInstance`

更多迁移边界见：[`../architecture/legacy-migration.md`](../architecture/legacy-migration.md)。

## 后续方向

### 插件扫描产品化 backlog（排期）

以下条目是产品化增强排期，不阻塞当前 M6 / M7 MVP：

1. **M3-P1：扫描失败文件明细**（已实现）
   - 目标：记录 `PluginDirectoryScanner::getFailedFiles()` 中的具体文件路径，而不是只显示失败数量。
   - UI：`PluginPanel` 先显示摘要，例如 `2 failed`；详细路径先进入 Logger，后续再考虑展开面板或导出日志。
   - 约束：不要在 audio callback 中写日志；扫描仍由现有 scan action 触发。
   - 验证：准备一个损坏或不兼容的 VST3 文件，确认失败文件路径可追踪且程序不崩溃。

2. **M3-P2：多目录扫描输入与持久化**（已实现）
   - 目标：明确用户可输入多个 VST3 搜索目录，并按 `juce::FileSearchPath` 语义持久化。
   - UI：继续复用当前路径输入框，使用平台路径分隔符；后续如需要再升级为目录列表编辑器。
   - 约束：保留默认 VST3 搜索路径 fallback；无效目录不应阻塞其他有效目录扫描。
   - 验证：输入两个以上目录，其中一个为空或无效，扫描仍能完成并给出可理解摘要。

3. **M3-P3：扫描结果持久化 / 启动恢复优化**（已实现）
   - 目标：评估并实现 `KnownPluginList` 持久化，降低启动恢复对同步重扫的依赖。
   - 策略：先保存 `KnownPluginList::createXml()` 结果或等价 XML；启动时优先加载缓存，必要时再重扫。
   - 约束：缓存失效时必须安全回退到重新扫描；不得加载已经不存在或格式不匹配的插件描述。
   - 验证：重启后无需手动扫描即可看到上次插件列表；删除插件文件后不会崩溃，状态可恢复。

4. **M3-P4：扫描空状态 / 失败状态 / 恢复失败提示**（已实现）
   - 目标：让用户区分“尚未扫描”“扫描成功但无插件”“扫描失败”“上次插件恢复失败”。
   - UI：`PluginPanel` 状态文本增加清晰短句；错误细节仍可先写 Logger。
   - 约束：不引入阻塞式弹窗作为主反馈；避免让状态文本过长。
   - 验证：空目录、无权限目录、插件恢复失败、手动取消 / 修改路径等路径都有可理解反馈。

> 2026-04-30：M3-P1..P4 已完成 Windows 人工验证，未发现明显问题。详见 [`../testing/plugin-host-lifecycle.md`](../testing/plugin-host-lifecycle.md)。

### 其他后续方向

1. 在具备外部 MIDI 设备后补齐生命周期测试 6.3 与更多真实设备场景验证。
2. 观察并收敛退出阶段 Debug 告警。
3. 按需拆分扫描职责与实例生命周期职责。
