# Phase 11 Completion Record — 声明式 UI 架构迁移（JIVE + melatonin_inspector）

> 归档日期：2026-08-16
> 来源：[`../roadmap/current-iteration.md`](../roadmap/current-iteration.md) Phase 11
> 后续方向：v0.3.0 发布准备（版本号 / CHANGELOG / Release 构建 / 打包），流程见 [`../guides/release-workflow.md`](../guides/release-workflow.md)；代码质量复审见 [`../audit/AUDIT-001-code-quality-audit-2026-08-16.md`](../audit/AUDIT-001-code-quality-audit-2026-08-16.md)

---

## Phase 11：声明式 UI 架构迁移（JIVE + melatonin_inspector）

### 背景与动机

Phase 10 完成了主窗口 UI 的视觉现代化（暗黑主题、旋钮化、图标化），但暴露出两个结构性瓶颈：

**问题 A — Agent 空间推理盲区**：当前所有 UI 布局通过 `setBounds()` 硬编码像素坐标（如 `statusBar.setBounds(area.removeFromBottom(22))`）。Agent 无法"看见"界面，只能猜测坐标和尺寸，导致反复修改仍不精确。

**问题 B — 反馈周期过长**：改 UI → WSL 编译 → Windows MSVC 构建（2–5 分钟）→ 启动应用 → 肉眼检查 → 描述问题 → Agent 再改。每轮迭代分钟级，且依赖主观描述。

### 解决方案：三件套组合

| 组件 | 角色 | 解决问题 |
|---|---|---|
| **JIVE** ([ImJimmi/JIVE](https://github.com/ImJimmi/JIVE), MIT) | 声明式 UI 框架：`juce::ValueTree` 布局（类 HTML）+ `juce::DynamicObject` 样式表（类 CSS）+ Flex/Grid 自适应 | 问题 A：语义化布局替代像素推算；问题 B：JSON 样式热重载 |
| **melatonin_inspector** ([sudara/melatonin_inspector](https://github.com/sudara/melatonin_inspector), MIT) | 运行时 Component 检查器：点击查看 bounds/hierarchy、拖动调整位置、实时改色、FPS 仪表、paint 性能分析 | 问题 B：秒级可视化反馈替代分钟级编译循环；问题 A：精确坐标来源 |
| **design_tokens.json** (自建) | 单一样式真相源：颜色、字体、圆角、间距的命名 token，同时被 JIVE 样式表和 `DevPianoLookAndFeel` 引用 | 确保 JIVE 管理的面板与原生组件（对话框、CustomKeyboard、SettingsComponent）视觉统一 |

### 核心设计决策

1. **JIVE 替代范围**：仅替代 5 个静态面板 Component（HeaderPanel、PluginPanel、ControlsPanel、KeyboardPanel、StatusBar），不替代以下内容：
   - `CustomKeyboard`（~700 行自定义 paint，功能独一无二，通过 JIVE 组件工厂原生注入）
   - ADSR 曲线绘制（从 ControlsPanel 抽离为独立 `AdsrCurveComponent`，工厂注入）
   - 模态对话框（`KeyBindingEditDialog`、`PerformanceMetadataDialog`）——临时生命周期，非 JIVE 目标场景
   - VST 插件 editor 窗口（`PluginEditorWindow`）——UI 由第三方插件提供
   - 数据类型（`KeyboardTypes.h`、`PluginPanelStateBuilder`）——非 Component

2. **音频/插件/MIDI 零改动**：全部业务逻辑层（Core/Audio/Midi/Plugin/Recording/Export/Settings/Input/Diagnostics/Locale/Layout，共 60+ 文件）完全不涉及。

3. **MainComponent 角色转变**：从"UI 组装者 + Presenter"变为纯 Presenter，`resized()` 从 40 行硬编码缩减为 0 行（JIVE Flex/Grid 自动响应）。

### 目标架构

```
design_tokens.json  ←  唯一样式真相源
    ├── JIVE style_sheets.json（引用 tokens，管理主窗口面板）
    └── DevPianoLookAndFeel（启动时读取 tokens，管理对话框 + CustomKeyboard + ADSR）

LayoutModel (ValueTree) → jive::Interpreter + ComponentFactory → Component 树
    ├── JIVE 标准控件: Button / Slider / ComboBox / TextEditor / Label
    └── 原生注入: CustomKeyboard / AdsrCurveComponent

MainComponent (Presenter) → CallbackWiring → PluginOperationController / RecordingSessionController / ...

melatonin_inspector (DEBUG only): 运行时可视化检查与编辑
```

### 子阶段

#### Phase 11a — 基础设施集成 [已完成]

- [x] 添加 JIVE 为 git submodule（`submodules/JIVE`）
- [x] 添加 melatonin_inspector 为 git submodule（`submodules/melatonin_inspector`）
- [x] 更新 `CMakeLists.txt`：链接 `jive::jive_layouts`、`jive::jive_style_sheets`、`melatonin_inspector`（`#if DEBUG`）
- [x] 定义 `JIVE_GUI_ITEMS_HAVE_STYLE_SHEETS=1` 预处理器宏
- [x] 在 `MainComponent` 构造函数中添加 inspector 初始化（`#if DEVPIANO_ENABLE_INSPECTOR`）
- [x] 验证：WSL `--configure-only` + Windows MSVC 构建通过

#### Phase 11b — 样式基础设施 [已完成]

- [x] 创建 `source/UI/jive/design_tokens.json`：定义全部颜色、字体、圆角、间距 token
- [x] 创建 `source/UI/jive/style_sheets.json`：JIVE 样式表，引用 design tokens 并定义组件级样式（Button、Slider、ComboBox、Label、面板背景等）
- [x] 修改 `DevPianoLookAndFeel` 构造函数：从 `design_tokens.json` 读取颜色 → 设置 JUCE `ColourIds`
- [x] 验证：inspector 中检查主窗口 + 对话框 + SettingsComponent 颜色一致性

#### Phase 11c — 面板迁移（由简到繁） [已完成]

- [x] **HeaderPanel** → JIVE ValueTree 声明（`makeHeaderTree`）：title Text + settings Button（齿轮 DrawableButton）
- [x] **StatusBar** → JIVE ValueTree 声明（`makeStatusBarTree`）：MIDI dot + 3 个状态 Label + 顶部分隔线
- [x] **PluginPanel** → JIVE ValueTree 声明（`makePluginPanelTree`）：toolbar（selector + filter + 4 Button）+ 可展开 scan/path/list 区（`updatePluginPanelState` 动态刷新）
- [x] **ControlsPanel** → JIVE ValueTree 声明 + 原生 ADSR 曲线注入：5 个 DevKnob + SpeedSlider + `AdsrCurveComponent` + preset 行 + transport/export 行
- [x] **KeyboardPanel** → `KeyboardViewport`（Viewport 持有 CustomKeyboard，高度同步修复零高度问题），工厂注入
- [x] 每次迁移后验证：WSL clang + Windows MSVC 构建 + `StyleCatalogTest` 端到端断言

#### Phase 11d — 回调连接与 MainComponent 瘦身 [已完成]

- [x] 回调直接在 `MainComponent::initialiseUi()` 内经 `jive::findItemWithID` 连接（settings/plugin/controls/transport/preset 全部 ~40 个）
- [x] 组件工厂注册：`SettingsButton`、`PathEditor`、`ListEditor`、`DevKnob`、`AdsrCurve`、`RecordButton/PlayButton/StopButton/BackButton`、`CustomKeyboard`、`StatusBarMidiDot`
- [x] 创建 `source/UI/jive/LayoutModel.h/.cpp`：全部面板 + 根布局 ValueTree 工厂（`makeRootLayout` 单树解释，FlexBox 全权布局）
- [x] 创建 `source/UI/jive/StyleCatalog.h/.cpp`：解释前把 `style_sheets.json` 规则合并为自持有 `jive::Object` 写入各节点 `style` 属性（修复首轮迁移样式失效根因）
- [x] 重写 `MainComponent::initialiseUi()` → 单次解释整棵根布局树
- [x] `MainComponent::resized()` → 3 行（根组件 setBounds，JIVE FlexBox 自动响应）
- [x] 删除旧面板文件（HeaderPanel、PluginPanel、ControlsPanel、KeyboardPanel、StatusBar）
- [x] `MainComponent.cpp` 从 ~906 行 → ~1100 行（含访问器翻译单元 ~1850 行，访问器已抽到 `MainComponentJiveAccessors.cpp` 经 #include 合入）

#### Phase 11e — 热重载与工作流验证 [已完成]

- [x] 实现 `Ctrl+R` 快捷键：触发 JIVE 样式表 + 设计 Token 重新加载（`ValueTree` property change → 自动重绘）
- [x] 实现 `design_tokens.json` 与 `style_sheets.json` 文件变更监听（Debug 模式下 `timerCallback` 约 1s 自动检测重载）
- [x] `DevPianoLookAndFeel::refreshColours()`：热重载时动态刷新原生控件颜色体系并通知全局 LookAndFeel 变更
- [x] `StyleCatalog::refreshStyles()`：清空旧 style 对象并递归向 live `ValueTree` 注入新 `jive::Object` 样式
- [x] 单元测试覆盖：`StyleCatalogTest` 新增 `testDesignTokensHotReload` 与 `testStyleCatalogHotReloadOnLiveTree` 端到端断言

#### Phase 11f — 回归验证 [已完成]

- [x] 全量单元测试通过：`./scripts/dev.sh test`
- [x] Windows MSVC 构建通过：`./scripts/dev.sh win-build`
- [x] 手动回归清单：
  - [x] 电脑键盘演奏（note on/off 成对、长按、连按）
  - [x] VST3 插件扫描 / 加载 / 卸载 / editor 窗口 / 退出
  - [x] 录制 / 回放 / 停止 / 回到开头
  - [x] MIDI 文件导入 / 自动播放
  - [x] MIDI / WAV 导出
  - [x] Performance Preset CRUD + 快捷键
  - [x] 中英文语言切换
  - [x] 窗口缩放（JIVE Flex/Grid 自适应验证）
  - [x] KeyBindingEditDialog 右键点击键 → 编辑 → 保存
  - [x] PerformanceMetadataDialog 编辑 → 保存
  - [x] SettingsComponent 各项设置修改 → 保存 → 重启恢复

### 风险与缓解

| 风险 | 概率 | 缓解措施 |
|---|---|---|
| JIVE API 不稳定（198 stars，仍在迭代） | 中 | 固定 git commit hash，不追 main 分支 |
| JIVE 与 devpiano 的 JUCE 子模块版本不兼容 | 低 | Phase 11a 首次集成时验证 `jive_JuceVersion.h`，必要时升级 JUCE submodule |
| 复杂回调链路（ControlsPanel 17 个 `std::function`）在 JIVE `setup()` 中连接繁琐 | 低 | 按 ID 查找 + 统一 `CallbackWiring` 模块，不分散在各面板中 |
| melatonin_inspector 与 CustomKeyboard 的自定义 paint 交互异常 | 极低 | inspector 仅读取 Component bounds/properties，不干涉 paint 逻辑 |
| 迁移过程中功能回归 | 中 | 逐面板迁移（11c），每完成一个面板立即验证，不攒到批次末尾 |

### 相关参考项目

- [JIVE](https://github.com/ImJimmi/JIVE)：JUCE 声明式 UI 框架（MIT）
- [melatonin_inspector](https://github.com/sudara/melatonin_inspector)：JUCE 运行时 Component 检查器（MIT）
- [JUCE AudioPluginHost](https://github.com/juce-framework/JUCE/tree/master/extras/AudioPluginHost)：官方插件宿主参考实现
- [Kushview Element](https://github.com/kushview/element)：JUCE 开源插件宿主（GPLv3）
