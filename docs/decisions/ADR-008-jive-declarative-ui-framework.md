# ADR 008: 采用 JIVE 声明式 UI 框架驱动界面排版与弹窗体系

## 状态

已采用（Phase 11 引入主窗口，Phase 15 推广至全局弹窗与设置窗口）。

## 背景

在传统 JUCE 开发中，界面布局主要通过在 `Component::resized()` 中手写绝对坐标（`setBounds`、`removeFromTop/Left`）完成。
随着 devpiano 界面复杂度上升（顶部状态栏、插件控制区、参数区、88 键虚拟键盘、通道矩阵跟随开关、设置窗口、多个模态弹窗），手写像素排版暴露出诸多严重问题：

1. **高耦合与脆弱性**：修改任意面板间距或控件尺寸容易引发大范围级联像素计算错误；`MainComponent::resized()` 膨胀至数百行。
2. **响应式与自适应困难**：窗口缩放、高 DPI 屏适配、多语言文字长度差异导致排版频繁溢出或撕裂。
3. **弹窗样板代码繁重**：预设新建、重命名、确认删除、歌曲信息编辑等模态弹窗各自派生 `DialogContentBase` 子类，重复编写手算坐标。
4. **与现代前端/声明式范式脱节**：样式散落在各个 Component 中，无法统一管理设计变量（Design Tokens）与暗黑主题。

## 决策

1. **引入 JIVE（声明式 UI 框架，MIT）作为主 UI 布局引擎**：
   - 界面结构以 `juce::ValueTree` 树形结构声明，支持 FlexBox 与 CSS Grid 响应式布局。
   - `jive::Interpreter` 负责将 ValueTree 动态解释为 JUCE Component 树。
   - `MainComponent::resized()` 简化为 3 行：仅设置 JIVE root item 尺寸，布局计算完全由 JIVE Flex 引擎接管。
2. **确立 Native 组件注入规范**：
   - 高性能自绘组件（`CustomKeyboard` 钢琴键盘、`AdsrCurveComponent` ADSR 曲线、`StatusBarMidiDot` 状态灯）以及 JUCE 复杂原生组件（`AudioDeviceSelectorComponent`）保留原生 C++ 实现，通过 JIVE 自定义组件工厂（ComponentFactory）无缝注入 JIVE 树。
3. **构建通用声明式弹窗基础设施（`JiveModalDialog`）**：
   - 统一由 `JiveModalDialog` 声明式模板（`makeSingleInputLayout` / `makeConfirmLayout` / `makeMetadataEditLayout` / `makeProgressLayout`）驱动所有交互弹窗与长时间任务进度反馈。
   - 彻底废除手写坐标弹窗 Content 类。
4. **引入 Design Tokens 与 StyleCatalog 单一事实源**：
   - 颜色、字号、间距定义在 `design_tokens.json` 中，由 `StyleCatalog` 将 `style_sheets.json` 规则统一注入节点。

## 原因

- **声明式优势**：界面层级直观，代码量大幅下降（Phase 15 重构消除 600+ 行手写坐标代码）。
- **零破坏性**：业务逻辑与音频链路完全解耦，原生复杂组件通过工厂注入，不影响音频与 MIDI 性能。
- **一致的主题与视觉体验**：集中化的样式与设计变量彻底消灭了各窗口色彩与间距不一致问题。
- **现代化调试**：配合 `melatonin_inspector` 子模块，可实现运行时实时组件检查与布局微调。

## 影响

- 主窗口及所有子窗口布局维护成本显著降低。
- 运行时中英文切换时 JIVE 属性自动响应，无需销毁重建原生组件。
- 引入 JIVE 作为 git submodule，构建体系需依赖 `jive` 模块。
