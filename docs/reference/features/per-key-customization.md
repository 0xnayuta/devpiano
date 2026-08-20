# 逐键个性化、按键绑定与虚拟键盘定制功能说明

> 用途：说明 devpiano 的 88 键虚拟钢琴键盘（`CustomKeyboard`）、逐键自定义标签（`customKeyLabels`）、逐键独立颜色（`customKeyColours`）、按键绑定编辑对话框（`KeyBindingEditDialog`）与调色板交互体系。
> 当前状态：已全量实现并稳定集成于键盘演奏与 Performance Preset 中（Phase 8 成果）。
> 更新时机：键盘几何渲染、着色模式、按键捕获交互或绑定数据结构发生变化时。

---

## 1. 概述与设计定位

为了让电脑键盘弹奏更直观、视觉反馈更丰富，devpiano 彻底替换了 JUCE 原生粗糙的 `MidiKeyboardComponent`，构建了专属于键盘钢琴的 **88 键拟真自绘钢琴键盘** 与 **逐键深度定制体系**：

1. **拟真 88 键自绘渲染**：黑白键几何排版、发光圆角、按下动态位移与平滑余晖动画（Fade In/Out）；
2. **多模式着色与音符标记**：支持 3 种着色模式（经典、按通道、按力度）与 3 种音符标记（首调唱名 Do-Re-Mi、固定调 Fixed-Do、科学音高名 C4/D4）；
3. **128 项逐键个性化标签与颜色**：每个 MIDI 音符（0~127）可独立附加自定义文字标签（如“主旋律起始音”、“高音Solo”）与专属色块高亮；
4. **右键即时绑定与交互编辑**：在虚拟键盘上右键任意琴键，即可弹出 `KeyBindingEditDialog` 声明式弹窗，一键录制电脑按键绑定并选取专属颜色。

---

## 2. 虚拟钢琴键盘视觉渲染体系

### 2.1 3 种色彩渲染模式（`KeyColourMode`）

| 模式 | 标识 | 渲染行为 |
|---|---|---|
| **经典模式（Classic）** | `classic` | 白键高亮为天空蓝（`#38BDF8`），黑键高亮为亮蓝；提供最清晰纯净的视觉反馈。 |
| **通道模式（Channel）** | `channel` | 按照触发该音符的 MIDI 通道（1~16）自动映射为 16 种高辨识度区分色（彩虹色盘），直观展示多通道编曲层次。 |
| **力度模式（Velocity）** | `velocity` | 随按键力度从绿（弱奏 $v < 40$）经过黄、橙平滑渐变到红（重奏 $v > 100$），动态呈现触键力度变化。 |

### 2.2 3 种音符标注模式（`NoteDisplayMode`）

| 模式 | 标识 | 标注内容 |
|---|---|---|
| **首调唱名（Do-Re-Mi）** | `doReMi` | 根据全局调号（`keySignature`）动态计算唱名（如 C 调中 C=1、D 调中 D=1），数字大字显示，适合视唱练耳。 |
| **固定调（Fixed-Do）** | `fixedDo` | 始终以 C=1 为基准标注固定数字唱名，不受调号移调影响。 |
| **科学音高名（Note Name）** | `noteName` | 标注标准国际音名（如 `C4`、`F#5`、`Bb3`），方便与乐理对照。 |

### 2.3 平滑余晖消隐动画
- `CustomKeyboard` 内部运行 30 fps 定时器，按键松开后通过指数插值（`fadeSpeed` 参数控制，默认 0.92）平滑淡出高亮背景，重现真实琴弦震动的视觉余韵。

---

## 3. 逐键个性化定制（Per-Key Labels & Colours）

在 `source/Core/KeyMapTypes.h` 的 `KeyboardSettings` 中定义了 128 项定长数组：

```cpp
struct KeyboardSettings {
    // ... 色彩与标注模式 ...
    std::array<juce::String, 128> customKeyLabels;
    std::array<juce::Colour, 128> customKeyColours;
};
```

- **自定义标签优先呈现**：若某个琴键设置了 `customKeyLabels[note]`，虚拟键盘在其上方优先绘制该自定义文本；
- **自定义颜色覆盖**：若某个琴键设置了有效 `customKeyColours[note]`（非透明），在按下和空闲时叠加该专属高亮色；
- **预设随行**：逐键标签与颜色完整持久化在 `.devpiano.preset` 中，切换预设时秒级切换全部键位标记。

---

## 4. 按键绑定编辑弹窗（`KeyBindingEditDialog`）

### 4.1 交互触发流程
1. 在虚拟钢琴键盘的任意黑白键上**鼠标右键点击**；
2. 触发 `CustomKeyboard::onBindingEditRequested(midiNote)` 回调；
3. 弹出基于 `JiveModalDialog` 驱动的 `KeyBindingEditDialog` 声明式模态弹窗。

```text
[右键点击琴键 C4 (60)] ──► KeyBindingEditDialog::launch()
    │
    ├── 1. 当前绑定信息：显示当前映射的电脑按键（如 "Bound to: A" 或 "Unmapped"）
    ├── 2. 按键捕获模式：点击 [Bind Key...] ──► 捕获下一次键盘按键 ──► 自动建立映射
    ├── 3. 自定义标签输入：单行文本框编辑当前琴键标签
    ├── 4. 8 色快捷调色板：集成 ColourSwatchButton 预选色块 + [Clear Colour] 清除色
    └── 5. 提交操作：点击 [Save] ──► 触发 commitPreset() ──► 实时刷新键盘与持久化
```

---

## 5. 确定性测试清单

单元测试位于 `source/tests/KeyMapTypesTest.cpp` 与 `source/tests/KeyboardHitMappingTest.cpp`（隶属于 `DevPiano/Core` 与 `DevPiano/UI` 测试套件）：

| 测试用例 | 验证目标 | 状态 |
|---|---|:---:|
| `testDefaultLayoutAlphaNumeric` | 验证默认布局 36 键无冲突、keyCode 规范化一致 | [x] 已通过 |
| `testHitTestingGeometry` | 验证黑键与白键点击区域判定（黑键优先命中，白键边缘无缝接合） | [x] 已通过 |
| `testCustomKeyLabelsSerialization` | 验证 128 项自定义标签在 JSON 预设中完整保存并准确读回 | [x] 已通过 |
| `testCustomKeyColoursSerialization`| 验证自定义颜色 ARGB 字符串序列化与透明度正确恢复 | [x] 已通过 |
| `testKeyBindingUnbindAndRemap` | 验证解除绑定与重新分配新按键时映射表原子更新 | [x] 已通过 |
