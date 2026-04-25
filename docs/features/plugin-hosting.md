# 插件宿主功能说明

> 用途：说明当前 VST3 插件扫描、加载、处理、editor 和生命周期相关功能行为。  
> 相关测试：[`../testing/plugin-host-lifecycle.md`](../testing/plugin-host-lifecycle.md)。  
> 当前状态来源：[`../roadmap/roadmap.md`](../roadmap/roadmap.md)、[`../testing/plugin-host-lifecycle.md`](../testing/plugin-host-lifecycle.md)、[`../architecture/overview.md`](../architecture/overview.md)、[`../architecture/legacy-migration.md`](../architecture/legacy-migration.md)。

## 当前能力

当前插件宿主已经具备最小可用能力：

- 可识别并使用 VST3 插件格式。
- 可扫描默认或指定 VST3 目录。
- 可显示扫描到的插件名称列表。
- 可从扫描结果中选择并加载 VST3 乐器。
- 已加载插件可参与 `processBlock` 并发声。
- 电脑键盘输入可驱动已加载插件发声。
- 外部 MIDI 输入到插件的代码通路已接入，但仍待更多真实设备手工验证。
- 插件卸载后程序保持可用。
- 可打开支持 editor 的插件窗口。
- 基础插件恢复信息可持久化，例如最近扫描路径和上次插件名称。
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

近期优先方向：

1. 在具备外部 MIDI 设备后补齐生命周期测试 6.3 与更多真实设备场景验证。
2. 观察并收敛退出阶段 Debug 告警。
3. 按需拆分扫描职责与实例生命周期职责。
4. 继续完善扫描错误记录、多目录扫描和插件恢复细节。
