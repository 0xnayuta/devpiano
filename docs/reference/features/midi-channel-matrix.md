# 16 通道 MIDI 矩阵与全局调号系统功能说明

> 用途：说明 devpiano 的 16 通道 MIDI 矩阵路由（`ChannelMatrix`）、`MidiChannelMapper` 服务、每通道独立变换、按键跟随（`followKey`）与全局调号（Key Signature）系统。
> 当前状态：已全量实现并稳定集成于键盘演奏与 Performance Preset 中。
> 更新时机：矩阵数据结构、通道路由规则或调号计算逻辑发生变化时。

---

## 1. 概述与设计定位

在键盘钢琴与多音色编曲演奏中，不同的键位往往需要输出到不同的 MIDI 通道以驱动不同音色（如左手伴奏通道 1、右手主旋律通道 2、打击乐通道 10）。
`ChannelMatrix` 为 devpiano 提供了标准 16 通道输入到输出的矩阵路由与内联变换层：

- **16 通道独立定制**：每个逻辑输入通道拥有独立的输出通道映射、半音移调、八度偏移、固定力度覆盖、音色号（Program）、音色库（Bank MSB）、延音控制器（Sustain CC）与按键跟随开关。
- **全局调号系统（Key Signature）**：支持 -7 ~ +7 半音移调（如降 B 大调、升 F 大调），可与特定通道的 `followKey` 开关联动，实现“旋律随调号移调、打击乐通道保持原音高不变”。
- **零开销与完全向后兼容**：当 `active == false` 时，所有 MIDI 消息 100% 原始透传（Pass-through），无任何性能损耗。
- **预设序列化集成**：矩阵配置与调号完整纳入 `.devpiano.preset` JSON 预设文件。

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
   $$\text{Note}_{\text{out}} = \text{clamp}\left(0, 127, \text{Note}_{\text{orig}} + \text{config.transpose} + 12 \times \text{config.octaveShift} + \text{keySigOffset}\right)$$
   - 其中 $\text{keySigOffset}$ 仅在 `global.midiTranspose == true` 且 `config.followKey == true` 时生效，取值为 $\text{keySignature}$（-7..+7）。
3. **力度覆盖计算**：
   $$\text{Velocity}_{\text{out}} = \begin{cases} \text{config.velocity}, & \text{若 } \text{config.velocity} \neq 64 \\ \text{clamp}\left(0, 127, \text{round}(\text{Velocity}_{\text{orig}} \times 127)\right), & \text{若 } \text{config.velocity} == 64 \end{cases}$$

### 3.2 Note Off 变换（`applyMatrixToNoteOff`）
Note Off 仅执行通道重定向与音高变换，速度严格与 Note On 的音高保持一致，确保制音器动作完全对称，不残留悬挂音。

---

## 4. 全局调号（Key Signature）系统

### 4.1 调号与移调模式
devpiano 在 `SettingsModel` 与 `AppState` 中维护全局调号：
- **`keySignature`**：整型范围 `[-7, +7]`，对应从 7 个降号（$\text{D}\flat$ 大调）到 7 个升号（$\text{C}\sharp$ 大调）的半音偏移量；
- **`midiTranspose` 开关**：
  - 当为 `true` 时，MIDI 输出音高随调号平移；
  - 当为 `false` 时，仅虚拟钢琴键盘的 Do-Re-Mi 标签随调号变化，物理 MIDI 输出保持原调（唱名移调但音高不移调模式）。

### 4.2 按键跟随矩阵（Follow Key Grid）
在设置面板（`SettingsLayoutModel`）中，提供了一个 **8 列 × 2 行的 JIVE CSS Grid 开关组**：
- 用户可独立勾选任意通道的 `Follow Key`；
- 例如：通道 1（主旋律钢琴）开启跟随，移调 +2 半音（C调变D调）；通道 10（打击乐）关闭跟随，依然触发标准 General MIDI 鼓组音高。
- **默认状态**：新装/重置后 15 个旋律通道默认开启跟随，通道 10 默认关闭（构造时由 `ChannelMatrix` 统一设定）；`midiTranspose` 关闭时全部开关置灰不可编辑。

---

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
| `testPassThroughWhenInactive` | 验证矩阵未激活时所有 16 通道 Note/CC 严格原样透传 | [x] 已通过 |
| `testChannelRemapping` | 验证通道 0 映射至通道 9，输出消息 channel 为 10 | [x] 已通过 |
| `testSemitoneAndOctaveTranspose` | 验证 transpose=+3 与 octaveShift=-1 时音高正确计算为 `orig + 3 - 12` | [x] 已通过 |
| `testVelocityOverride` | 验证 velocity 设置为 100 时覆盖原始力度；为 64 时保留原始力度 | [x] 已通过 |
| `testKeySignatureFollowKey` | 验证 `followKey=true` 时叠加调号偏移，`followKey=false` 时忽略调号 | [x] 已通过 |
| `testPitchBoundaryClamping` | 验证负移调与超高移调时音高严格限制在 `[0, 127]` 合法范围 | [x] 已通过 |
