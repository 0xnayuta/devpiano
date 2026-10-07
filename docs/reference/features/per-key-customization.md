# 逐键个性化、按键绑定与虚拟键盘定制功能说明

> 用途：说明 devpiano 的 88 键虚拟钢琴键盘（`CustomKeyboard`）、逐键自定义标签（`customKeyLabels`）、逐键独立颜色（`customKeyColours`）、按键绑定编辑对话框（`KeyBindingEditDialog`）与调色板交互体系。
> 当前状态：已全量实现并稳定集成于键盘演奏与 Performance Preset 中（Phase 8 基础落地，Phase 34~35 扩展 12-TET 和声色盘与击键力度动态高亮）。
> 更新时机：键盘几何渲染、着色模式、按键捕获交互或绑定数据结构发生变化时。

---

## 1. 概述与设计定位

为了让电脑键盘弹奏更直观、视觉反馈更丰富，devpiano 彻底替换了 JUCE 原生粗糙的 `MidiKeyboardComponent`，构建了专属于键盘钢琴的 **88 键拟真自绘钢琴键盘** 与 **逐键深度定制体系**：

1. **拟真 88 键自绘渲染**：黑白键几何排版、发光圆角、按下动态位移与平滑余晖动画（Fade In/Out）；
2. **多模式着色与音符标记**：支持 4 种着色模式（经典、按通道、按力度、和声调色板）与绝对数字唱名（`doReMi`）、带调号偏移唱名（`fixedDo`）、科学音高名（`noteName`）3 种标记；具体计算见下表，不按枚举名称反推乐理语义；
3. **128 项逐键个性化标签与颜色**：每个 MIDI 音符（0~127）可独立附加自定义文字标签（如“主旋律起始音”、“高音Solo”）与专属色块高亮；
4. **右键即时绑定与交互编辑**：在虚拟键盘上右键任意琴键，即可弹出 `KeyBindingEditDialog` 声明式弹窗，一键录制电脑按键绑定并选取专属颜色。

---

## 2. 虚拟钢琴键盘视觉渲染体系

### 2.1 4 种色彩渲染模式（`KeyColourMode`）

| 模式 | 标识 | 渲染行为 |
|---|---|---|
| **经典模式（Classic）** | `classic` | 白键高亮采用产品强调色（`#00C8F0`），黑键高亮为深亮蓝；黑白键均随击键力度动态混入白色辉光，提供最清晰纯净的视觉反馈。 |
| **通道模式（Channel）** | `channel` | 按照触发该音符的 MIDI 通道（1~16）自动映射为 16 种高辨识度区分色（彩虹色盘），直观展示多通道编曲层次。 |
| **力度模式（Velocity）** | `velocity` | 随按键力度从绿（弱奏 $v \to 0$，色相 $64^\circ$）平滑过渡到红（重奏 $v \to 127$，色相 $0^\circ$），并在高亮中叠加力度白色辉光，动态呈现触键力度变化。 |
| **和声模式（Harmony）** | `harmony` | 依据 12-TET 半音阶和声色环（`getPitchClassHarmonyColour`）映射 12 种音程几何相色（八度同色、三全音互补），同频绽放三和弦几何色相，与 QWERTY 键盘看板及和弦 HUD 同频联动。 |

### 2.2 3 种音符标注模式（`NoteDisplayMode`）

| 模式 | 标识 | 标注内容 |
|---|---|---|
| **固定调/绝对唱名（Do-Re-Mi）** | `doReMi` | 始终以 C=1 为绝对基准标注固定数字唱名（`1`、`#1`、`2` 等加八度偏移），不受全局调号影响。 |
| **带调号偏移唱名（Fixed-Do）** | `fixedDo` | 按 `(noteIndex + keySignature) % 12` 计算并规范化到 `[0,11]`，八度偏移仍来自原 MIDI 音符；例如 MIDI 60、`keySignature=+2` 显示 `2+0`。不将枚举名称解释成“当前调主音必定标为 1”的乐理承诺。 |
| **科学音高名（Note Name）** | `noteName` | 使用代码中的升号音名表，如 `C4`、`F#5`；不随调号改变，也不自动改为降号异名。 |

绑定提示由 `QwertyViewModel::pianoKeys` 按最终输出音高提供，而不是钢琴组件自行反查原始布局。提示保留在映射投影中，setLayout→setSettings、resize 或视口更新后重新消费，不因重建 `KeyRenderState` 丢失。

### 2.3 平滑余晖消隐动画

- `CustomKeyboard` 的余晖使用每帧收缩系数 `fadeSpeed`，范围 `0.50 .. 0.99`，默认 `0.92`。设置、预设/设置文件加载与显示组件统一限幅；`1` 及更大输入钳至 `0.99`。松键后收敛到当前 `previewAlpha`（默认 `0`），到阈值即吸附目标并停止 Timer，alpha 保持 `[0,1]`。`QwertyComponent` 的 50 fps 余晖独立保持原语义。

---

## 3. 逐键个性化定制（Per-Key Labels & Colours）

在 `source/UI/KeyboardTypes.h` 的 `KeyboardSettings` 中定义了 128 项定长数组：

```cpp
struct KeyboardSettings {
    // ... 色彩与标注模式 ...
    std::array<juce::String, 128> customKeyLabels;
    std::array<juce::Colour, 128> customKeyColours;
};
```

- **自定义标签优先呈现**：若某个琴键设置了 `customKeyLabels[note]`，虚拟键盘在其上方优先绘制该自定义文本；
- **自定义颜色覆盖**：若某个琴键设置了有效 `customKeyColours[note]`（非透明），在按下和空闲时叠加该专属高亮色；
- **预设与文件随行**：逐键标签与颜色完整持久化在 `.devpiano.preset`（Schema 整数版本 2）及原生录制文件快照（Schema 整数版本 3）中，切换预设时秒级切换全部键位标记。
- **索引不随输出漂移**：数组仍以配置输入音符为索引，显示时消费映射层准备的输入索引。例如 `A/60` 经矩阵移到 `72` 后，原 `customKeyLabels[60]` 和颜色显示在输出 `72` 上，不复制或改写为索引 `72`。
- **编辑输入而非输出**：已绑定琴键编辑原绑定；未绑定的输出位置使用准备好的候选输入。例如矩阵 `+12` 下在输出 `73` 新绑，保存输入 `61`，之后发音为 `73`，不会再次移调到 `85`。

---

## 4. 按键绑定编辑弹窗（`KeyBindingEditDialog`）

### 4.1 交互触发流程

1. 在虚拟钢琴键盘的任意黑白键上**鼠标右键点击**；
2. `CustomKeyboard::onBindingEditRequested()` 交付投影中的配置输入 MIDI 音符索引；不把最终输出当作新的配置音符；
3. 弹出基于 `JiveModalDialog` 驱动的 `KeyBindingEditDialog` 声明式模态弹窗。

```text
[右键点击琴键 C4 (60)] ──► KeyBindingEditDialog::launch()
    │
    ├── 1. 当前绑定信息：使用完整消息模板显示绑定按键或未绑定说明
    ├── 2. 按键捕获模式：点击 [Bind Key...] ──► 捕获下一次键盘按键 ──► 自动建立映射
    ├── 3. 自定义标签输入：单行文本框编辑当前琴键标签
    ├── 4. 8 色快捷调色板：集成 ColourSwatchButton 预选色块 + [Clear Colour] 清除色
    └── 5. 提交操作：[OK] 返回 KeyBindingEditResult，由宿主应用绑定／标签／颜色修改；[Cancel] 不提交
```

### 4.2 操作区与内容尺寸

已绑定与未绑定状态共用 460 逻辑像素内容宽度，高度由实际表单测量，不再分别预留固定高度。左侧保留 [Unbind] 或 [Bind Key...]，右侧 [OK]／[Cancel] 共用标准操作行；底部留白、按钮高度与最小正文间距见 [通用弹窗规则](declarative-ui-and-theming.md#33-内容定尺与统一底部操作区)。缩放不收缩按钮点击区域，不改变配置输入 MIDI 音符、按键捕获或取消语义。


---

## 5. 确定性测试清单

单元测试位于 `source/tests/KeyMapTypesTest.cpp`、`source/tests/KeyboardHitMappingTest.cpp` 与 `source/tests/CadenceVelocityTest.cpp`（隶属于 `DevPiano/Core`、`DevPiano/UI` 与 `DevPiano/Input` 测试套件）：

| 测试套件 / 用例 | 验证目标 | 状态 |
|---|---|:---:|
| `KeyMapTypesTest` | 验证默认布局键位映射、keyCode 规范化一致与强类型 MIDI 标量转换有界性 | [x] 已通过 |
| `KeyboardHitMappingTest` | 验证黑键与白键点击区域判定（黑键优先命中，白键边缘无缝接合） | [x] 已通过 |
| `KeyboardHitMappingTest` | 验证鼠标拖拽滑音（Glissando）事件流与多通道色彩正确映射 | [x] 已通过 |
| `QwertyViewModelTest` | 最终投影、绑定标签跨设置/几何/视口重建保持，以及鼠标/回放输入身份隔离 | [x] 已通过 |
| `KeyboardHitMappingTest` | 合法端点及超范围 fade 输入有界收缩，达到目标后 Timer 停止；逐键颜色在最终输出位置保留 | [x] 已通过 |
| `PerformancePresetTest` | 验证 128 项自定义标签与 ARGB 颜色的预设往返；实际绑定编辑/按键捕获/配置索引消费者见 Phase F/G 实施记录 | [x] 文件及实际界面验证 |
| `CadenceVelocityTest` | 验证快速与慢速打字律动力度曲线估算、超时复位与力度随机抖动 | [x] 已通过 |
