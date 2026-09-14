# JIVE 声明式 UI、设计系统与通用弹窗架构说明

> 用途：说明 devpiano 基于内生声明式 UI 运行时（`source/UI/jive/core/`）的界面布局体系、Design Tokens 设计变量、StyleCatalog 全局样式表、统一宿主门面（`ViewHost`）、通用模态弹窗（`JiveModalDialog`）与 Native 组件注入机制。
> 当前状态：已全量落地（Phase 11 主窗口迁移、Phase 15 设置面板重构、Phase 28 ViewHost 治理与 Phase 29~32 声学调律卡片持续演进）。
> 更新时机：UI 布局模型、设计 Token、样式表规则、弹窗模板或组件工厂扩展时。

---

## 1. 架构定位与设计哲学

传统 JUCE 界面排版深度依赖在 `Component::resized()` 中手算像素绝对坐标（`setBounds` / `removeFromTop`）。随着界面复杂度增长，手写坐标容易引发级联排版错误、高 DPI 适配困难以及弹窗样板代码繁重。

devpiano 全面引入 **JIVE（声明式 UI 框架）**，确立了以下现代 UI 架构规范：

1. **结构与样式彻底分离**：
   - 界面结构以 `juce::ValueTree` 树形模型声明；
   - 视觉样式由 `design_tokens.json` 与 `style_sheets.json` 集中管理；
   - 布局由 JIVE 的 FlexBox 与 CSS Grid 引擎自动计算，`MainComponent::resized()` 缩减至仅需 3 行。
2. **通用声明式弹窗体系（`JiveModalDialog`）**：
   - 统一由声明式模板驱动预设新建/重命名/删除确认、歌曲元数据编辑与 WAV 导出进度浮层，彻底消灭手写坐标的弹窗 Content 类。
3. **零破坏 Native 组件工厂注入**：
   - 高性能自绘内核（88 键虚拟键盘 `CustomKeyboard`、`AdsrCurveComponent`）以及 JUCE 复杂原生组件（`AudioDeviceSelectorComponent`）保留 C++ 高性能实现，通过 JIVE 组件工厂无缝注入布局树。
4. **编译期二进制内嵌（零外部文件依赖）**：
   - 静态 Token 与样式表由 CMake `juce_add_binary_data` 编译期内嵌为 `devpiano_binary_data`，确保单可执行文件在脱离源码目录时 100% 具备完整的主题与布局规则。

---

## 2. JIVE 声明式布局体系

### 2.1 主窗口布局树（`LayoutModel`）

主窗口由 `source/UI/jive/LayoutModel.cpp` 声明为整棵 ValueTree，涵盖 5 个核心面板：

```text
Component (root, display="flex", flex-direction="column")
├── HeaderPanel        (flex-direction="row", justify-content="space-between")
│   ├── Logo & Status Text
│   └── Transport Controls (Record / Stop / Play / Back / Speed Slider)
├── PluginPanel        (flex-direction="row", flex-wrap="wrap")
│   ├── Scan Path Editor & Scan Button
│   └── Plugin Selector & Open Editor Button
├── ControlsPanel      (flex-direction="row", align-items="center")
│   ├── Preset ComboBox & Save/Rename/Delete Action Buttons
│   ├── ADSR Curve Component (Native 注入)
│   └── Master Volume Rotary Slider
├── KeyboardArea       (flex-grow=1, display="flex")
│   └── KeyboardViewport (包含 CustomKeyboard 88 键原生画布)
└── StatusBar          (flex-direction="row", align-items="center")
    ├── StatusBarMidiDot (Native 注入 MIDI 呼吸灯)
    ├── Audio & Engine Diagnostics Info
    └── Language Indicator
```

`MainComponent` 在构造时通过 `jive::Interpreter` 一次性解释整棵布局树，并通过 `MainComponentJiveAccessors.cpp` 提供的强类型访问器操作具体子组件状态。

---

### 2.2 设置面板声明式架构（`SettingsLayoutModel`）

`SettingsComponent` 完全由 `SettingsLayoutModel.cpp` 声明的模块化 ValueTree 驱动，由统一宿主门面 `ViewHost` 接管渲染与生命周期，划分为 6 个核心卡片：

1. **音频设备卡片（`makeAudioDeviceSectionTree`）**：设备类型与输出设备下拉框、ASIO 控制面板按钮与采样率/缓冲大小指示；
2. **调号与通道跟随卡片（`makeKeySignatureSectionTree`）**：全局调号选择器、MIDI 移调开关以及采用 **JIVE CSS Grid（8 列 × 2 行）** 声明的 16 通道跟随开关（`followKeyToggles`）；
3. **键盘显示与语言卡片（`makeKeyboardDisplaySectionTree`）**：按键着色模式（Classic / Channel / Velocity）、音符标注模式（DoReMi / FixedDo / NoteName）、按键淡出速度滑块与运行时中英文切换；
4. **声学与调律卡片（`makeAcousticsSectionTree`，Phase 29~32 演进）**：采用模块化卡片排版，包含 9 项关键物理声学控件：
   - **Row 1 琴盖开合度**（`lid-position-combo`）：全开（Full Open）、半开（Half Stick）、闭盖（Closed Lid）；
   - **Row 2 触键力度曲线**（`touch-curve-combo`）：标准（Standard）、轻触（Light）、重触（Heavy）、宽动态（Wide Dynamic）；
   - **Row 3 古典微调律制**（`temperament-combo`）：平均律（Equal）、中庸全音律（Meantone）、韦克迈斯特三律（Werckmeister III）、基恩伯格三律（Kirnberger III）、纯律（Just）；
   - **Row 4 基准音高微调**（`reference-pitch-slider`）：400.0 ~ 480.0 Hz 连续微调（步进 0.1 Hz）；
   - **Row 5 立体声空间视角**（`perspective-combo`）：演奏者视角（Player）与听众视角（Audience）；
   - **Row 6 房间混响预设**（`reverb-space-combo`）：室内乐（Chamber）、音乐厅（Concert Hall）、录音棚（Studio）；
   - **Row 7 混响干湿比**（`reverb-wet-slider`）：0% ~ 100% 混响湿声电平调节；
   - **Row 8 机械动作噪声**（`pedal-noise-slider`）：0% ~ 100% 延音踏板扫掠声与共鸣冲击音量调节；
   - **Row 9 琴槌毛毡老化**（`felt-ageing-slider`）：0% ~ 100% 琴槌毛毡磨损压实与老化穿透力调节；
5. **诊断日志卡片（`makeDiagnosticsSectionTree`）**：结构化实时日志查看器；
6. **保存与操作卡片（`makeSaveActionSectionTree`）**：右对齐（`flex-end`）保存与关闭按钮。
---

## 3. 通用声明式模态弹窗（`JiveModalDialog`）

`source/UI/jive/JiveModalDialog.h/.cpp` 提供了全局通用的模态弹窗基础设施，统一了暗黑主题视觉、Flex-end 按钮排版、回车确认/ESC 取消与安全析构生命周期：

### 3.1 标准预置模板

| 模板方法 | 默认尺寸 | 适用场景 | 交互特性 |
|---|:---:|---|---|
| `launchSingleInput` | 380 × 150 | 预设新建（Save As New）、预设重命名 | 单行文本框，自动捕获焦点，支持最大字符数限制与回车即时提交 |
| `launchConfirm` | 380 × 140 | 预设删除确认、覆盖确认 | 消息文本展示，确认/取消双按钮 |
| `launchMetadataEdit` | 420 × 260 | 歌曲元数据编辑（Song Title + Notes） | 单行标题框 + 多行带滚动条备注框，Tab 键焦点切换 |
| `makeProgressLayout` | 380 × 140 | WAV 音频离线导出进度 | 状态文本 + JIVE 暗黑 ProgressBar + 随时取消按钮 |

### 3.2 自定义弹窗扩展（`launchCustom`）

支持传入任意自定义声明的 `juce::ValueTree`，并通过 `onInit`、`onConfirm`（支持表单校验拦截）、`onCancel` 回调实现高度定制的交互弹窗（如 `KeyBindingEditDialog` 逐键绑定与调色板编辑）。

---

## 4. 设计系统 Token 与样式注入

### 4.1 Design Tokens 单一事实源（`design_tokens.json`）

定义在 `source/UI/jive/design_tokens.json` 中，由 `devpiano::jive::DesignTokens` 单例提供强类型访问：
- **背景与表面色**：`mainBg` (`#18181B`)、`panelBg` (`#27272A`)、`controlBg` (`#3F3F46`)
- **品牌与强调色**：`accent` (`#38BDF8`)、`accentHover` (`#0284C7`)、`recordActive` (`#EF4444`)
- **文字与边框**：`textPrimary` (`#F4F4F5`)、`textSecondary` (`#A1A1AA`)、`border` (`#3F3F46`)
- **圆角与间距**：`radiusSmall` (4px)、`radiusMedium` (8px)、`paddingMedium` (12px)

### 4.2 StyleCatalog 全局注入（`style_sheets.json`）

定义在 `source/UI/jive/style_sheets.json` 中。在 `jive::Interpreter` 解释 ValueTree 前，`StyleCatalog::applyToTree()` 递归遍历节点，根据节点的 `type` 和 `id` 将 CSS 风格的样式属性（padding, margin, background, border, font-size 等）合并至节点的 `style` 属性中。

---

## 5. Native 原生组件工厂注入模式

对于复杂图形或性能敏感组件，JIVE 采用工厂模式（`ComponentFactory`）无缝桥接：

```cpp
auto& factory = interpreter->getComponentFactory();
factory.set("CustomKeyboard", [](const juce::ValueTree& tree) {
    return std::make_unique<devpiano::ui::KeyboardViewport>();
});
factory.set("AdsrCurve", [](const juce::ValueTree& tree) {
    return std::make_unique<devpiano::ui::AdsrCurveComponent>();
});
factory.set("AudioDeviceSelector", [](const juce::ValueTree& tree) {
    return std::make_unique<juce::AudioDeviceSelectorComponent>(...);
});
```

- JIVE 负责管理外部容器的外边距、尺寸约束与 Flex/Grid 定位；
- 原生组件负责高帧率自绘与鼠标事件响应，兼具声明式排版与原生性能。

---

## 6. 专项确定性测试清单

单元测试位于 `source/tests/JiveModalDialogTest.cpp` 与 `source/tests/SettingsLayoutModelTest.cpp`（隶属于 `DevPiano/UI` 测试套件）：

| 测试文件 | 用例类别 | 验证目标 | 状态 |
|---|---|---|:---:|
| `JiveModalDialogTest` | 模板结构构建 | 验证 SingleInput、Confirm、MetadataEdit、Progress 模板节点层级与初始属性 | [x] 已通过 |
| `JiveModalDialogTest` | 组件动态检索 | 验证 `findButtonById`、`findTextEditorById` 在多层 JIVE 树下的正确寻址 | [x] 已通过 |
| `JiveModalDialogTest` | 多行/单行配置 | 验证 Title 框为单行、Notes 框为多行（`isMultiLine() == true`） | [x] 已通过 |
| `JiveModalDialogTest` | 安全析构序列 | 验证模态关闭时 `safeCleanupJiveTree` 能够防止 StyleSheet 监听器 UAF | [x] 已通过 |
| `SettingsLayoutModelTest`| 16 通道 CSS Grid | 验证通道跟随开关以 8 列 × 2 行网格声明，16 个 Toggle 节点完备 | [x] 已通过 |
| `SettingsLayoutModelTest`| 原生组件注入 | 验证 `AudioDeviceSelector` 原生节点在 JIVE 容器中的正确嵌入与尺寸响应 | [x] 已通过 |
| `SettingsLayoutModelTest`| 设置项动态绑定 | 验证修改 ValueTree 属性直接联动底层状态并触发持久化 | [x] 已通过 |
