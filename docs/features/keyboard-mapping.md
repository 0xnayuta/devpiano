# 键盘映射功能说明

> 用途：说明电脑键盘到 MIDI note 的当前功能行为、已知边界和后续方向。  
> 相关测试：`../testing/keyboard-mapping.md`。  
> 当前状态来源：`../roadmap/roadmap.md`、`../testing/keyboard-mapping.md`、`../architecture/legacy-migration.md`。

## 当前能力

当前键盘映射已经具备最小可用能力：

- 电脑键盘可触发 MIDI note on / note off。
- 虚拟钢琴键盘可随电脑键盘输入联动高亮。
- 程序可通过内置 fallback synth 或已加载 VST3 插件发声。
- 映射主路径不再强依赖字符输入，优先使用更稳定的 key code 路径。
- 已建立 `KeyboardLayout` / `KeyBinding` 等核心类型。
- 默认布局可加载。
- 当前布局可保存和恢复。
- 已提供最小布局操作入口：布局切换、保存当前布局、恢复当前布局默认映射。
- 已完成一轮基础稳定性验证：单键按下/释放、长按、快速连按、焦点切换、输入法/大小写场景。
- 虚拟键盘组件仅作为显示/鼠标输入入口使用；电脑键盘到 MIDI 的主路径由项目内 `KeyboardMidiMapper` 独占，已禁用 JUCE `MidiKeyboardComponent` 自带 QWERTY 映射，避免虚拟键盘翻页/聚焦后重复触发。

## 主要链路

```text
JUCE KeyPress / KeyListener
  -> KeyboardMidiMapper
  -> juce::MidiMessage
  -> MidiMessageCollector
  -> AudioEngine
  -> PluginHost / fallback synth
  -> AudioDeviceManager
  -> Audio Output
```

## 当前已验证行为

已验证的重点行为包括：

- `Q/W/E/R/T/Y/U/I/O/P`、`A/S/D/F/G/H/J/K/L`、`Z/X/C/V/B/N/M`、`1/2/3/4/5/6/7/8/9/0` 默认布局基础映射已通过一轮手工发声验证。
- 单键按下与释放成对，不残留悬挂音。
- 多键同时按下与交错松开基础场景可用。
- 长按不会异常重复触发。
- 快速连按基础场景可用。
- 焦点切换后 held key 不残留。
- Caps Lock / Shift 不破坏基础映射。
- 中文输入法激活时仍可发声，且不弹出候选词栏。
- 键盘映射可驱动已加载 VST3 插件发声。

详见：`../testing/keyboard-mapping.md`。

## 当前限制

当前仍未完成或仍需补充验证的能力：

- 默认布局基础行已完成一轮手工发声验证；虚拟键盘翻页后映射稳定性问题已定位、修复，并已通过 Windows 侧人工专项回归。
- 完整自定义布局编辑能力尚未完成。
- 自定义映射保存/恢复测试仍依赖后续完整 UI 能力。
- 外部 MIDI 与电脑键盘混合输入仍待真实设备验证。
- 布局 Preset / 导入导出体系尚未完整建立。

## 与旧 FreePiano 的关系

旧 FreePiano 中的 `keyboard.*` / `config.*` 可用于参考：

- 默认键位布局；
- 映射规则；
- 功能边界；
- 历史交互预期。

新实现不直接沿用旧扫描码处理和平台绑定输入逻辑，而是基于 JUCE 输入事件和项目内布局模型重建。

更多迁移边界见：`../architecture/legacy-migration.md`。

## 后续方向

近期优先方向：

1. 补齐默认布局全量回归。
2. 明确自定义布局 / Preset 设计。
3. 持续验证焦点、输入法、修饰键等边界。
4. 后续再完善布局导入 / 导出能力。
