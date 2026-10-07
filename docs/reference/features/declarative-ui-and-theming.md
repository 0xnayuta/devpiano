# JIVE 声明式 UI、设计系统与通用弹窗架构说明

> 用途：说明 devpiano 基于内生声明式 UI 运行时（`source/UI/jive/core/`）的界面布局体系、Design Tokens 设计变量、StyleCatalog 全局样式表、统一宿主门面（`ViewHost`）、通用模态弹窗（`JiveModalDialog`）与 Native 组件注入机制。
> 当前状态：已全量落地（Phase 11 主窗口迁移、Phase 15 设置面板重构、Phase 28 ViewHost 治理、Phase 29~32 声学调律卡片与 Phase 34~35 QWERTY / 节拍器 / 和弦 HUD / 时间轴持续演进）。
> 更新时机：UI 布局模型、设计 Token、样式表规则、弹窗模板或组件工厂扩展时。

---

## 1. 架构定位与设计哲学

传统 JUCE 界面排版深度依赖在 `Component::resized()` 中手算像素绝对坐标（`setBounds` / `removeFromTop`）。随着界面复杂度增长，手写坐标容易引发级联排版错误、高 DPI 适配困难以及弹窗样板代码繁重。

devpiano 使用按 [ADR-014](../../decisions/ADR-014-internalize-ui-infrastructure-and-deprecate-jive-submodule.md) 内化的声明式 UI 运行时（`source/UI/jive/core/`）；JIVE 是其来源与内部命名，不再是外部子模块依赖。布局边界如下：

1. **结构与样式彻底分离**：
   - 界面结构以 `juce::ValueTree` 树形模型声明；
   - 视觉样式由 `design_tokens.json` 与 `style_sheets.json` 集中管理；
   - 布局由 JIVE 的 FlexBox 与 CSS Grid 引擎自动计算，`MainComponent::resized()` 仅需轻量将顶层尺寸委托给 `ViewHost`。
2. **通用声明式弹窗体系（`JiveModalDialog`）**：
   - 统一由声明式模板驱动预设新建/重命名/删除确认、歌曲元数据编辑与 WAV 导出进度浮层，彻底消灭手写坐标的弹窗 Content 类。
3. **零破坏 Native 组件工厂注入**：
   - 高性能自绘内核（88 键虚拟键盘 `CustomKeyboard`、`AdsrCurveComponent`、`TimelineBar`、`QwertyComponent`）以及原生辅助组件（`StatusBarMidiDot`、`ColourSwatchButton`）保留 C++ 高性能实现，通过 JIVE 组件工厂无缝注入布局树。
4. **编译期二进制内嵌（零外部文件依赖）**：
   - 静态 Token 与样式表由 CMake `juce_add_binary_data` 编译期内嵌为 `devpiano_binary_data`，确保单可执行文件在脱离源码目录时 100% 具备完整的主题与布局规则。

---

## 2. JIVE 声明式布局体系

### 2.1 主窗口布局树（`LayoutModel`）

主窗口由 `source/UI/jive/LayoutModel.cpp` 声明为整棵 ValueTree，涵盖 5 个核心面板：

```text
Component (root, display="flex", flex-direction="column", id="window")
├── MainArea           (flex-direction="column", flex-grow=1, padding=16)
│   ├── HeaderPanel        (flex-direction="row", align-items="centre", height=36)
│   │   ├── Title ("devpiano")
│   │   └── SettingsButton (Native 注入 DrawableButton，齿轮图标)
│   ├── PluginPanel        (flex-direction="column")
│   │   ├── PluginActionRow (Plugin Status, Plugin Selector, Plugin Filter, Load, Unload, Open Editor, Toggle)
│   │   └── PluginExpandedArea (Path Label, PathEditor Native 注入, Browse Button, Scan VST3 Button)
│   └── ContentRow         (flex-direction="column", flex-grow=1)
│       ├── ControlsPanel      (flex-direction="row", height=284, min-height=220)
│       │   ├── PresetCard         (width=230, ComboBox + New/Rename/Delete + Export/Import + Save/Open + Export WAV/Recent/Info)
│       │   ├── AdsrCard           (flex-grow=2, 8 个旋钮: Volume/Brightness/Hammer/Resonance/Attack/Decay/Sustain/Release + AdsrCurve Native 注入 + TimelineBar Native 注入 A-B Loop/Seek 时间轴)
│       │   └── TransportCard      (width=230, Record/Play/Stop/Back + SpeedSlider + Metronome 控制区: Metro 开关/BPM 菜单/Tap 测速)
│       ├── QwertyCard         (flex-direction="column", 折叠 32px / 展开 192px)
│       │   ├── HeaderRow          (Title, 和弦 HUD Badge "qwerty-chord-badge", 布局分组切换 "[Group A]", 折叠/展开 Toggle)
│       │   └── QwertyVisualizer   (Native 注入 QwertyComponent，5 行物理键盘网格与和弦 HUD 浮层)
│       └── KeyboardArea       (flex-grow=1, min-height=146, max-height=200)
│           └── KeyboardViewport   (Native 注入 CustomKeyboard 88 键自绘画布与横向滚动条)
└── StatusBar          (flex-direction="row", height=24)
    ├── Left: StatusBarMidiDot (Native 注入) + Plugin Name + 节拍器状态指示器（BPM/拍号/动态节拍灯）
    ├── Centre: Audio Info (驱动类型、采样率、缓冲大小、延迟与 CPU 占用)
    └── Right: Performance Indicators (调号、移调状态、键盘布局与分组、踏板状态)
```

`MainComponent` 构造时创建 `LayoutModel` 的 ValueTree，由 `ViewHost::loadLayout()` 封装内化 `jive::Interpreter` 解释布局，并通过 `MainComponentJiveAccessors.cpp` 操作具体子组件状态。

主窗口默认尺寸为 1180 × 780；QWERTY 展开时最小尺寸为 980 × 740，折叠时最小高度为 580 px，两状态差值仍为 160 px。最小高度同时容纳展开的插件面板。88 键键床采用 21.5 px 白键宽度和 6.4:1 白键长宽比，总宽约 1118 px；默认键盘视口宽 1148 px 时完整显示，最小窗口下视口宽 948 px 并保留整段键床供横向滚动。键床视口最小高度为 146 px，其中 138 px 用于完整键床、8 px 用于水平滚动条，避免滚动条遮挡白键。键床在更宽或更高的视口中居中，不拉伸键形。Controls 面板最小高度为 220 px（默认高度 284 px），包含三张核心卡片（预设卡片 230 px、音色/包络/时间轴卡片 flex-grow=2、播放/录制/节拍器卡片 230 px）。

展开的 QWERTY 卡片外高固定为 192 px，折叠外高固定为 32 px，展开/折叠时主窗口同步调整 160 px；两种状态的最小窗口高度也按此差值配对，避免窗口受限时 QWERTY 卡片参与 flex 收缩并改变控制卡片行高度。标题行固定占 22 px，其中标题文字组件为 18 px；与 Performance Preset 和 Transport Controls 的 18 px 标题加 4 px 下间距一致。标题行右侧集成实时和弦指示 Badge（`qwerty-chord-badge`）、布局分组切换按钮（`[Group A]` / `[Group B]`）与折叠切换按钮。`source/UI/QwertyComponent.h` 负责渲染 5 行 ANSI 物理键盘网格，并在**右上角**绘制半透明和弦 HUD Badge（148 × 24 px，半透明黑底 + 12-TET 和声色相边框与圆点），渐隐呈现和弦名称与转位属性。

Performance Preset 的 New、Rename、Delete 保持在上方，Export、Import、Save、Open、Export WAV、Recent、Info 作为底部文件操作组。音色与 ADSR 包络卡片包含单行 8 旋钮（Volume、Brightness、Hammer、Resonance、Attack、Decay、Sustain、Release）、自绘 ADSR 曲线（保留至少 48 px）以及底部的 `TimelineBar` 练习时间轴（高度 34 px，顶部 8 px 间距），集成播放进度、总时长、A-B 循环标记（Set A、Set B、Clear Loop）与鼠标拖拽 Seek 寻道。Transport Controls 的 Metro 开关、节奏选择和 Tap 固定为底部一组；两处均通过 flex-grow 占位吸收卡片内剩余高度。节奏按钮显示当前 BPM（例如 `120 BPM`），悬停提示和设置菜单提供当前拍号与预备拍（Count-in）选项；Tap 从第二次点击起更新 BPM，使用最近最多 3 个间隔的均值，连续点击间隔超过 2 秒时重新开始累积。传输区第二排按钮与 Playback Speed 标题之间留 8 px 间距；四个录制/播放按钮使用同一白色实心矢量风格，回到开头图标采用起始竖线加单个左指三角。状态栏高度为 24 px，其节拍器指示器在开启时显示 `• <BPM> <拍号> <节拍灯>`，强拍点亮实心圆 `●`，弱拍显示空心圆 `○`，待机显示中圆点 `•`，伴随每拍亮度平滑衰减。

---

### 2.2 设置面板声明式架构（`SettingsLayoutModel`）

`SettingsComponent` 完全由 `SettingsLayoutModel.cpp` 声明的模块化 ValueTree 驱动，由统一宿主门面 `ViewHost` 接管渲染与生命周期，划分为 6 个核心卡片：

1. **音频设备卡片（`makeAudioDeviceSectionTree`）**：全声明式布局，包含设备类型下拉框（`audio-device-type-combo`）、输出设备下拉框（`audio-output-device-combo`）与测试按钮（`audio-test-button`）、活动输出通道（`audio-active-channels-combo`）、采样率下拉框（`audio-sample-rate-combo`）、缓冲大小下拉框（`audio-buffer-size-combo`）与 ASIO 设备控制面板按钮（`asio-control-panel-button`，仅 ASIO 模式展开）；
2. **调号与通道跟随卡片（`makeKeySignatureSectionTree`）**：全局调号选择器、MIDI 移调开关以及采用 **JIVE CSS Grid（8 列 × 2 行）** 声明的 16 通道跟随开关（`followKeyArea` + 16 个 `follow-key-N` 复选框）；
3. **键盘显示与语言卡片（`makeKeyboardDisplaySectionTree`）**：按键着色模式（Classic / Channel / Velocity / Harmony 4 种模式）、音符标注模式（DoReMi / FixedDo / NoteName）、按键淡出速度滑块、乐器过滤器开关、**延音踏板策略选择（`sustain-policy-combo`：Normal / Sync Pedal）**与运行时中英文切换；
4. **声学与调律卡片（`makeAcousticsSectionTree`，Phase 29~32 演进）**：采用模块化卡片排版，包含 9 项关键物理声学控件：
   - **Row 1 琴盖开合度**（`lid-position-combo`）：全开（Full Open）、半开（Half Stick）、闭盖（Closed Lid）；
   - **Row 2 触键力度曲线**（`touch-curve-combo`）：标准（Standard）、轻触（Light）、重触（Heavy）、宽动态（Wide Dynamic）；
   - **Row 3 古典微调律制**（`temperament-combo`）：平均律（Equal）、纯律（Just）、毕达哥拉斯律（Pythagorean）、中庸全音律（Meantone）、韦克迈斯特三律（Werckmeister III）、基恩伯格三律（Kirnberger III）；
   - **Row 4 基准音高微调**（`reference-pitch-slider`）：400.0 ~ 480.0 Hz（步进 0.1 Hz），直接绑定 `TemperamentEngine` 的统一上下限；
   - **Row 5 立体声空间视角**（`perspective-combo`）：演奏者视角（Player）与听众视角（Audience）；
   - **Row 6 房间混响预设**（`reverb-space-combo`）：室内乐（Chamber）、音乐厅（Concert Hall）、录音棚（Studio）；
   - **Row 7 混响干湿比**（`reverb-wet-slider`）：0% ~ 100% 混响湿声电平调节；
   - **Row 8 机械动作噪声**（`pedal-noise-slider`）：0% ~ 100% 延音踏板扫掠声与共鸣冲击音量调节；
   - **Row 9 琴槌毛毡老化**（`felt-ageing-slider`）：0% ~ 100% 琴槌毛毡磨损压实与老化穿透力调节；
5. **诊断日志卡片（`makeDiagnosticsSectionTree`）**：结构化实时日志查看器（`ListEditor` 原生 TextEditor 注入）、日志绝对路径展示与“打开日志目录”（`open-log-dir-button`）原生文件管理器直达按钮；
6. **保存与操作卡片（`makeSaveActionSectionTree`）**：右对齐（`flex-end`）保存与关闭按钮。

---

## 3. 通用声明式模态弹窗（`JiveModalDialog`）

`source/UI/jive/JiveModalDialog.h/.cpp` 提供了全局通用的模态弹窗基础设施，统一了暗黑主题视觉、Flex-end 按钮排版、回车确认/ESC 取消与安全析构生命周期：

### 3.1 标准预置模板

| 模板方法 | 默认内容宽度／高度规则 | 适用场景 | 交互特性 |
|---|:---:|---|---|
| `launchSingleInput` | 宽 380；高度按内容测量 | 预设新建（Save As New）、预设重命名 | 单行文本框，自动捕获焦点，支持最大字符数限制与回车即时提交 |
| `launchConfirm` | 宽 380；消息至少高 40，长文本按字符换行增高 | 预设删除确认、覆盖确认 | 显式换行和不可分割长名称完整显示，确认/取消双按钮 |
| `launchMetadataEdit` | 宽 420；高度按内容测量 | 歌曲元数据编辑（Song Title + Notes） | 标题单行；Notes 保留至少 80 逻辑像素的编辑区域，可多行键入；取消不提交，诊断 `ListEditor` 保持只读 |
| `makeProgressLayout` | 宽 380；高度按内容测量 | WAV 音频离线导出进度 | 状态与进度，取消只发布协作请求；工作实际结束后才关闭和释放，不承诺厂商永久阻塞时即时完成 |

### 3.2 自定义弹窗扩展（`launchCustom`）

自定义 `juce::ValueTree` 布局通过 `onInitHost(const ViewHost&)`、`onConfirmHost(const ViewHost&)`（返回 false 可拦截关闭）与 `onCancel` 接入。单行输入、确认、元数据及逐键绑定使用相同门面，不再提供 raw `GuiItem` 回调或检索 helper。业务样式刷新只调用 `ViewHost::refreshStyles()`，不持有解释器或根 `GuiItem`。

### 3.3 内容定尺与统一底部操作区

- `ViewHost::fitToContent(width)` 在 UI 线程按最终可用宽度测量声明式内容，并根据实际控件边界、margin、padding 与 border 校准高度。旧固定窗口高度参数已移除；业务不访问 raw `GuiItem`，也不自行估算字体宽高。
- `makeDialogRoot()` 与 `makeDialogButtons()` 供标准模板及按键编辑器复用。`DesignTokens` 定义底部留白 28、按钮高度 28、相邻按钮间距 8、正文到操作行最小间距 12，单位均为 JUCE 逻辑像素；不根据截图的物理像素写死补偿。
- 窗口先按内容收紧，再由操作区内的零高度伸缩项吸收原生缩放取整的残余空间，保持可见按钮到内容区下沿的留白一致；不是保留过高的窗口后把空白搬到正文中。绑定／解除绑定在左，确认／取消在右。
- `launchWindow()` 创建窗口并确定原生标题栏模式后，准确设置内容区尺寸、重新居中，再进入异步模态状态；不修改 JUCE 子模块，不减去固定标题栏高度。
- `WavExportTask` 的独立 `ProgressContentWrapper` 使用相同测量与窗口入口，仍保留自己的后台线程、协作取消及完成收尾。系统文件选择器、第三方插件编辑器和独立取色弹层不套用本规则。
- Windows 直接消费者验证中英窗口与 100%／150%／200% 内容缩放、长名称／多行消息、确认／取消、验证拒绝、按键捕获、Notes 及真实导出成功／协作取消。缩放只改变临时进程的 JUCE 比例；实际 HWND 系统 DPI 为 144，不冒称跨物理 DPI 显示器切换认证。


---

## 4. 设计系统 Token 与样式注入

### 4.1 Design Tokens 单一事实源（`design_tokens.json`）

定义在 `source/UI/jive/design_tokens.json` 中，由 `devpiano::jive::DesignTokens` 单例提供强类型访问：
- **背景与表面色**：`mainBg` (`#111316`)、`panelBg` / `cardBg` (`#181A1F`)、`controlBg` (`#22252C`)
- **品牌与强调色**：`primary` (`#00C8F0`，电光青 Product Accent)、`primaryAlpha30` (`#4D00C8F0`)、`recordActive` (`#E05345`)、`playActive` (`#2ECC71`)
- **文字与边框**：`textPrimary` (`#F0F2F5`)、`textSecondary` (`#A0A6B2`)、`textDisabled` (`#555B66`)、`cardBorder` (`#2B2F38`，Structure Neutral)
- **旋钮与交互覆层**：`rotaryCapTop` (`#353942`)、`rotaryRingTop` (`#565C69`)、`highlightOverlay` (`#18FFFFFF`)、`pressOverlay` (`#33000000`)
- **圆角与间距**：`borderRadiusDefault` (6px)、`borderRadiusCard` (8px)、`borderRadiusTooltip` (4px)、`statusBarHeight` (24px)
- **字体与继承**：`DesignTokens::getUnifiedUiFont()` 为 Native 与声明式文字提供统一字体策略；`#window` 的 `@font-family-ui` / `@font-size-label` 作为声明式全局缺省，子节点可显式覆写字号。标题、微型标签和按键说明保留各自的排版层级，不把“统一字体”误写成所有文字使用同一字号。
- **加载与热重载**：`StyleBootstrap` 先加载内嵌 Token，再建立 LookAndFeel，随后加载内嵌样式；开发环境存在 `source/UI/jive/*.json` 时可覆盖内嵌基准。发布运行不依赖这些文件，业务热重载经 `ViewHost::refreshStyles()` 更新已有树。

### 4.2 StyleCatalog 全局注入（`style_sheets.json`）

定义在 `source/UI/jive/style_sheets.json` 中。在 `jive::Interpreter` 解释 ValueTree 前，`StyleCatalog::applyToTree()` 递归遍历节点，根据节点的 `type` 和 `id` 将 CSS 风格的样式属性（padding, margin, background, border, font-size 等）合并至节点的 `style` 属性中。

未单独定制的按钮（包括 QWERTY 折叠和分组按钮）共用 `Button` 的 normal、hover、active、disabled 状态；Metro toggle 的中性轮廓与 `checked` 状态在 `style_sheets.json` 声明，`DevPianoLookAndFeel` 将其背景绘制交给 JIVE `BackgroundCanvas`，避免 JUCE TextButton 的 latched 辉光覆盖声明式样式。

---

## 5. Native 原生组件工厂注入模式

复杂图形与性能敏感 Native 组件通过 `ViewHost` 注册生产工厂；额外组件可在 `configureComponentFactory` 回调内注册，不从业务获取解释器或 raw GuiItem：

```cpp
devpiano::ui::ViewHost host;
host.registerDefaultComponents();
host.registerKeyboardComponents(keyboardState);
host.loadLayout(layoutTree);
host.setBounds(getLocalBounds());
```

相关组件源码位置：
- 88 键虚拟键盘视口：`source/UI/native/KeyboardViewport.{h,cpp}`（包裹 `source/UI/CustomKeyboard.{h,cpp}`）；
- ADSR 包络曲线：`source/UI/native/AdsrCurveComponent.{h,cpp}`；
- 练习时间轴：`source/UI/native/TimelineBar.{h,cpp}`；
- QWERTY 键盘看板与和弦 HUD：`source/UI/QwertyComponent.{h,cpp}`；
- 状态栏 MIDI 信号灯：`source/UI/native/StatusBarMidiDot.{h,cpp}`；
- 键位编辑调色盘：`source/UI/native/ColourSwatchButton.{h,cpp}`。

- JIVE 负责管理外部容器的外边距、尺寸约束与 Flex/Grid 定位；
- 原生组件负责高帧率自绘与鼠标事件响应，兼具声明式排版与原生性能。

---

## 6. 专项确定性测试清单

UI 单元测试位于 `source/tests/`。依据 [ADR-016](../../decisions/ADR-016-headless-safe-pure-in-memory-ui-testing-pattern.md)，自动化只通过纯内存 `ViewHost`／独立组件解释布局、绘制和注入事件，不调用 `addToDesktop()`、`launchCustom()` 或创建原生顶层窗口，不因 headless 环境跳过；异步按钮事件显式使用 `devpiano::test::drainMessages(N)`。下表的 Windows 窗口消费者是独立手工／直接验收证据，不是自动化单元测试中的桌面窗口。

| 测试文件 | 用例类别 | 验证目标 | 状态 |
|---|---|---|:---:|
| `JiveModalDialogTest` | 内容定尺与操作区几何 | 实际组件的底部留白、按钮高度／间距、左侧操作不重叠、Notes 有效编辑高度；额外 1 逻辑像素的窗口取整不破坏底部对齐 | [x] 已通过 |
| `JiveModalDialogTest` | 长消息与混排裁剪边界 | 不同宽度下的英文、不可分割长名称、码点构造中文、显式换行及中英混排；最后一行完整，正文不侵入操作区 | [x] 已通过 |
| `JiveModalDialogTest` | 生产输入机制 | 使用生产 ViewHost 构建 Notes，注入字符和回车验证多行编辑；诊断 ListEditor 拒绝输入 | [x] 已通过 |
| Windows 生产窗口消费者 | 真实窗口与生命周期 | 中英及内容缩放矩阵、输入确认／取消、验证拒绝、绑定编辑／按键捕获、真实 WAV 成功及协作取消；用户目录无变化 | [x] 直接界面验证 |
| `StyleCatalogTest` | 动态圆角 | 固定 bounds 连续 radius 0→30→0，角像素立即匹配当前半径，不等待 resize | [x] 已通过 |
| `LayoutGoldenTest` | 16 通道 CSS Grid | 解释后的跟随开关两行对齐、列顺序、行高与网格容器几何，不单独复制布局树声明 | [x] 已通过 |
| `SettingsLayoutModelTest` | 生产组件解释与输入隔离 | 实际宿主中的设备/调号/显示/声学控件类型；语言刷新保留滚动位置，控件与背景鼠标滚轮隔离 | [x] 已通过 |
| 生产 Settings 窗口 | 调号与跟随交互 | 实际 transpose 开关关闭时 followKey 禁用、开启时可编辑；不以属性赋值回读当业务证明，见 Phase G/H 消费者证据 | [x] 实际界面验证 |
| `StyleCatalogTest` | Metro toggle checked 样式 | 验证 Metro 与传输按钮使用一致的中性轮廓，checked 状态不引入额外强调色 | [x] 已通过 |
| `StyleCatalogTest` | 语义标题语言联动 | 代表性容器、编辑器、预设与速度控件标题随语言切换变化并恢复，不固定具体译文 | [x] 已通过 |
| `QwertyViewModelTest`    | 和声色相与对比度 | 验证 12-TET 和声色相间隔、八度同色、三全音互补与文字对比度算法 | [x] 已通过 |
| `LayoutGoldenTest`       | 主窗口与设置布局几何 | 主窗口／设置解释及 1280x720、1920x1080 几何；980x740 展开与 980x580 折叠态下，88 键键床完整位于横向滚动条可视高度内；弹窗几何由 JiveModalDialogTest 独立验证 | [x] 已通过 |
| `ViewHostTest` | 宿主生命周期、查找与活组件状态 | 解释加载与清理、强类型/缺失组件查找、enabled/visible 属性传播到真实按钮；尺寸重排由布局几何用例保护 | [x] 已通过 |
| `ChordRecognitionTest`   | 和弦实时识别（`DevPiano/Core` 默认门禁） | 验证单音、音程、三和弦、七和弦识别、转位推导与置信度，为 QWERTY HUD 提供数据支撑；确认默认日志实际执行，非单独补跑 | [x] 已通过 |
| `MetronomeTest`          | 节拍器状态机 | 验证 BPM 节拍计算、强弱拍判断、预备拍调度与生命周期稳定性 | [x] 已通过 |
