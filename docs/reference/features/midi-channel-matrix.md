# 16 通道 MIDI 矩阵与全局调号系统功能说明

> 用途：说明 devpiano 的 16 通道 MIDI 矩阵路由（`ChannelMatrix`）、`MidiChannelMapper` 服务、每通道独立变换、按键跟随（`followKey`）与全局调号（Key Signature）系统。
> 适用范围：集成于电脑键盘演奏与 Performance Preset 中。项目状态以 [roadmap](../../roadmap/roadmap.md) 为准。
> 更新时机：矩阵数据结构、通道路由规则或调号计算逻辑发生变化时。

---

## 1. 概述与设计定位

在键盘钢琴与多音色编曲演奏中，不同的键位往往需要输出到不同的 MIDI 通道以驱动不同音色（如左手伴奏通道 1、右手主旋律通道 2、打击乐通道 10）。
`ChannelMatrix` 为 devpiano 提供了标准 16 通道输入到输出的矩阵路由与内联变换层：

- **16 通道独立定制**：每个逻辑输入通道拥有独立的输出通道映射、半音移调、八度偏移、固定力度覆盖、音色号（Program）、音色库（Bank MSB）、延音控制器（Sustain CC）与按键跟随开关。
- **全局调号系统（Key Signature）**：支持 -7 ~ +7 半音移调（如降 B 大调、升 F 大调），可与特定通道的 `followKey` 开关联动，实现“旋律随调号移调、打击乐通道保持原音高不变”。
- **停用矩阵**：`active == false` 时 `applyTransform()` 直接返回原消息，不应用矩阵或 followKey；不据此承诺调用本身零计算成本。
- **预设与文件边界**：矩阵与输入调号保存于整数 v2 预设及 v4 演奏快照；普通预设激活不覆写全局调号或本机分区模式。矩阵只在现场输入阶段执行一次，录制保存最终音乐音高/通道，回放/WAV/MIDI 不再套矩阵或快照移调。

---

## 2. 数据结构与位域紧凑设计

### 2.1 PerChannelConfig 配置项

`source/Midi/ChannelMatrix.h` 中为每个通道定义了紧凑的结构体：

```cpp
struct PerChannelConfig {
    uint8_t outputChannel : 4 = 0;   // 实际 MIDI 输出通道 (0-15，0 = 通道 1)
    int8_t  transpose     : 7 = 0;   // 半音移调偏移量 (-48 .. +48)
    int8_t  octaveShift   : 2 = 0;   // 八度偏移量 (-1, 0, +1)
    uint8_t velocity      : 7 = 64;  // 固定力度覆盖 (0-127，64 表示使用原始力度)
    uint8_t program       : 7 = 0;   // Program Change 音色号 (0-127)
    uint8_t bankMSB       : 7 = 0;   // Bank MSB 音色库选择 (0-127)
    uint8_t sustainCC     : 7 = 64;  // 延音踏板控制器编号 (默认 64)
    bool    followKey     : 1 = false;// 是否跟随全局调号移调
};
```

- **内存优化**：利用 C++ 位域（Bit-fields）打包，结构体体积极小，避免频繁复制与缓存未命中；
- **默认透传基线**：默认 `outputChannel` 对应通道自身，`transpose = 0`，`velocity = 64`（保留演奏原始动态）。
- **默认调号跟随**：`ChannelMatrix` 构造函数将 16 通道中除通道 10（GM 打击乐，索引 9）外的全部旋律通道默认开启 `followKey`，通道 10 保持旁路——确保打击乐音高不受全局调号影响，与实时移调链路（`AudioEngine` 回放移调按同一掩码旁路通道 10）保持一致。

---

## 3. 变换规则与计算公式

### 3.1 Note On 变换（`applyMatrixToNoteOn`）

当键盘按下或鼠标点击触发 Note On 时：

1. **输出通道计算**：
   $$\text{Channel}_{\text{out}} = \text{config.outputChannel} + 1$$
2. **音高与移调计算**：
   $$n_1 = \operatorname{clamp}(0, 127, n_{\mathrm{input}} + t + 12o),\quad n_{\mathrm{out}} = \operatorname{clamp}(0, 127, n_1 + k)$$
   - 先执行矩阵半音/八度钳位，再执行 followKey 调号钳位；边界处不能合并为一次钳位。仅当 `midiTranspose` 和当前输入通道的 `followKey` 同时开启时，$k$ 为 `keySignature`（-7..+7），否则为 0。
3. **力度覆盖计算**：
   - 原始力度 `<= 0` 保持静音；有声输入且 `config.velocity != 64` 时使用固定值，否则将 `originalVelocity * 127` 转为整数并钳至 `0..127`。转换按当前实现截断，不承诺四舍五入；配置固定值 0 也可静音。

### 3.2 Note Off 变换与发音身份守恒

在 devpiano 中，Note Off 遵循双重防悬挂保障机制：

1. **独立消息变换（`applyTransform`）**：在映射配置不变时，Note On/Off 的通道和音高变换对称；该无状态 helper 不保存起音身份，不能用于变化中的持有音释放。实际交互与回放使用各自锁定的身份。
2. **交互演奏发音身份守恒（`sendNoteOn` / `sendNoteOff`）**：
   - 键盘按下时，`sendNoteOn` 返回 `devpiano::core::MidiNoteIdentity`（锁定经矩阵变换后的实际输出音高与输出通道），调用方（`KeyboardMidiMapper`）将其存入 `heldKeys` 快照；
   - 松键时，`sendNoteOff(identity, velocity, keyboardState)` 严格消费该快照注销发音；
   - **抗替换鲁棒性**：即使在按键按住期间动态修改了矩阵映射、八度偏移、调号，甚至运行时重新创建并替换了整个 `MidiChannelMapper` 实例，NoteOff 依然 100% 依据按下时的原始身份释放目标通道与音高，从根本上杜绝悬挂音；
3. **输入通道边界防护**：`configForChannel` 对零基索引执行 `juce::jlimit(0, 15, inputChannel)`；负数钳到 0，超过上界钳到 15，不把所有越界输入都描述为通道 15。

---

## 4. 全局调号（Key Signature）系统

### 4.1 调号与移调模式

devpiano 在 `SettingsModel` 与 `AppState` 中维护全局调号：
- **`keySignature`**：整数 `[-7, +7]` 的半音偏移；不是 SMF 调号 meta 中“升降号个数”到调名的映射，显示与 MIDI 导入元数据分别处理。
- **`midiTranspose` 开关**：
  - 当为 `true` 时，MIDI 输出音高随调号平移；
  - 当为 `false` 时，物理 MIDI 输出保持原调；仅在启用带调号唱名模式（`NoteDisplayMode::fixedDo`，计算公式为 `(noteIndex + keySignature) % 12`）时，虚拟钢琴键盘的唱名标签随调号变化（唱名移调但音高不移调模式）；绝对简谱唱名模式（`NoteDisplayMode::doReMi`）始终以 C=1 为绝对基准，不受全局调号影响。

### 4.2 按键跟随矩阵（Follow Key Grid）

在设置面板（`SettingsLayoutModel`）中，提供了一个 **8 列 × 2 行的 JIVE CSS Grid 开关组**：
- 用户可独立勾选任意通道的 `Follow Key`；
- 例如：通道 1（主旋律钢琴）开启跟随，移调 +2 半音（C调变D调）；通道 10（打击乐）关闭跟随，依然触发标准 General MIDI 鼓组音高。
- **默认状态**：新装/重置后 15 个旋律通道默认开启跟随，通道 10 默认关闭（构造时由 `ChannelMatrix` 统一设定）；`midiTranspose` 关闭时全部开关置灰不可编辑。

普通预设选择保留全局调号；`RecordedPreset.acoustic` 记录录制时输入开关/偏移，不重新变换 Take 音符（嵌入预设 v2，录制文件 v4）。`followKey` 只控制现场输入与投影，释放使用起音时锁定的最终身份。

## 5. 架构接入与服务（`MidiChannelMapper`）

`MidiChannelMapper` 是贯穿输入与发声的核心服务：
- **电脑键盘路径**：`KeyboardMidiMapper` 将 key code 转换为初始 `(channel, note, vel)` → 调用 `MidiChannelMapper::sendNoteOn` → 经矩阵变换后发送给 `juce::MidiKeyboardState`；
- **UI 鼠标演奏路径**：`CustomKeyboard` 鼠标点击 → 调用 `MidiChannelMapper::sendNoteOn`；
- **非 Note 消息透传**：CC、Pitch Wheel 等控制器消息由 `applyTransform()` 安全透传，不破坏控制流。

---

## 6. 专项确定性测试清单

单元测试位于 `source/tests/MidiChannelMapperTest.cpp`（隶属于 `DevPiano/Core` 测试套件）：

| 测试用例 | 验证目标 | 状态 |
|---|---|:---:|
| `testDefaultPassThroughWithoutTranspose` | 验证默认矩阵且未启用移调时消息原样透传 | [x] 已通过 |
| `testDefaultMatrixWithGlobalTranspose` | 验证默认矩阵开启移调时，旋律通道随调号平移，Ch10 打击乐通道旁路保持原音高 | [x] 已通过 |
| `testOutputChannelRemap` | 验证通道输出重定向（如通道 0 映射至通道 9，输出通道为 10） | [x] 已通过 |
| `testTransposeAndOctaveClamping` | 验证半音移调与八度偏移计算，以及 [0, 127] 音高边界严格钳位 | [x] 已通过 |
| `testVelocityOverride` | 验证固定力度覆盖（!= 64 覆盖，== 64 保留原始力度） | [x] 已通过 |
| `testFollowKeyWithGlobalTranspose` | 验证 followKey 开关对旋律与打击乐通道的调号跟随独立控制 | [x] 已通过 |
| `testNoteOnOffSymmetry` | 验证 NoteOn 与 NoteOff 经矩阵变换后通道与音高严格对称 | [x] 已通过 |
| `testNonNoteMessagesPassThrough` | 验证 CC、Pitch Wheel 等非 Note 消息完全原样透传 | [x] 已通过 |
| `testOutOfRangeInputChannelClamps` | 验证非法越界输入通道（如 > 15）安全钳位至第 16 通道配置 | [x] 已通过 |
| `testKeyboardStateReceivesTransformedNotes` | 验证 `sendNoteOn` 与 `sendNoteOff` 准确驱动 `MidiKeyboardState` 发声与释放 | [x] 已通过 |
| `testMappedIdentitySurvivesMapperReplacement` | 验证发声中途替换 `MidiChannelMapper` 实例后持有的发音身份依然安全释放，杜绝悬挂音 | [x] 已通过 |
