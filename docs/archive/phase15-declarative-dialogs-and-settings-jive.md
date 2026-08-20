# Phase 15：UI 架构统一至 JIVE（声明式弹窗与设置面板重构）（历史归档）

> 用途：记录 Phase 15（UI 架构全面统一至 JIVE）的设计背景、技术方案、实施细节与全量验收记录。
> 归档说明：本文档为 Phase 15 完成后的历史归档记录。现行 JIVE UI 与弹窗架构规范请参考 [`docs/reference/features/declarative-ui-and-theming.md`](../reference/features/declarative-ui-and-theming.md)。

---

## 1. 背景与目标

在 Phase 11 中，主窗口的 5 个主要面板（Header, Plugin, Controls, KeyboardArea, StatusBar）已成功迁移至 JIVE 声明式 `juce::ValueTree` + Flex/Grid 布局体系。然而，主窗口之外的子窗口与交互弹窗仍处于传统 JUCE 手动像素排版与独立自绘体系：

1. **设置窗口（`SettingsComponent`）**：包含 300+ 行手写绝对坐标与流式切分代码（`removeFromTop`、`setBounds`、硬编码宽高），16 通道跟随开关通过两层循环手算网格坐标，语言切换时需销毁重建原生组件，与主窗口 JIVE 体系完全割裂。
2. **自定义模态弹窗（`PresetNameDialog`, `PresetConfirmDialog`, `PerformanceMetadataDialog`）**：各自基于 `DialogContentBase` 派生独立组件，在 `resized()` 中手算组件坐标与边距，存在重复样板代码。
3. **长时间操作反馈（`WavExportTask`）**：继承自 JUCE 原生 `ThreadWithProgressWindow`（基于 AlertWindow），视觉风格偏传统 OS 样式。

**阶段目标**：
1. 构建通用的 JIVE 声明式模态弹窗基础设施（`JiveModalDialog`），将预设与歌曲元数据弹窗迁移为声明式驱动；
2. 将 `SettingsComponent` 重构为 `SettingsLayoutModel`（JIVE ValueTree + CSS Grid/Flex 布局），`AudioDeviceSelectorComponent` 封装为 Native 注入项，彻底移除 300+ 行手写坐标；
3. 将 WAV 导出等长耗时进度交互接入现代化 JIVE 模态 Overlay 或标准弹窗；
4. 明确保留 `CustomKeyboard` 原生自绘内核、`PluginEditorWindow` 原生宿主与系统 `FileChooser` 的合理边界。

---

## 2. 实施细节与完成记录

### Phase 15-A：通用 JiveModalDialog 基础设施与声明式模板 ✅
- 新建 `source/UI/jive/JiveModalDialog.h/.cpp`：以 JIVE `ValueTree` 驱动的模态对话框容器（继承 `juce::DialogWindow`），由 `jive::Interpreter` 统一解释布局并注入 `StyleCatalog` 全局样式；
- 声明式模板支持：`makeSingleInputLayout`（单行文本输入模板，380×150）、`makeConfirmLayout`（确认取消模板，380×140）与 `makeMetadataEditLayout`（歌曲信息单行+多行文本模板，420×260）；
- 统一深色主题外观、输入框焦点样式、Flex-end 右对齐按钮排版、回车确认 / ESC 取消键盘交互与异步焦点捕获；
- 封装安全的回调完成序列（防 double-callback、防 UAF、`launchAsync` 模态状态安全退出、`safeCleanupJiveTree` 防止 StyleSheet 析构 UAF）；
- 确定性测试：`source/tests/JiveModalDialogTest.cpp` 新增 5 类 10+ 项断言（ValueTree 节点层级、属性、尺寸、`findButtonById` 与 `findTextEditorById` 组件检索、单行/多行编辑器配置与安全析构），全量测试通过。

### Phase 15-B：预设与元数据弹窗统一迁移 ✅
- `PresetNameDialog`（重命名 / 新建预设）全面改接 `JiveModalDialog::launchSingleInput` 声明式模板；
- `PresetConfirmDialog`（删除预设确认）全面改接 `JiveModalDialog::launchConfirm` 声明式模板；
- `PerformanceMetadataDialog`（歌曲标题与备注编辑）全面改接 `JiveModalDialog::launchMetadataEdit` 声明式模板；
- 清理 `source/UI/PresetDialogs.cpp`（222 行 → 26 行）和 `source/UI/PerformanceMetadataDialog.cpp`（136 行 → 33 行）中的手写 `resized()` 坐标计算与冗余 Content 类（净消减 ~300 行死代码）；
- 验证：全流程弹窗交互与单元测试通过，`DevPianoTests` / `DevPiano.exe` 全量构建测试无警告。

### Phase 15-C：设置面板 JIVE 声明式重构（SettingsLayoutModel） ✅
- 新建 `source/Settings/jive/SettingsLayoutModel.h/.cpp`：使用 `juce::ValueTree` 声明设置面板层级（音频设备区、调号区、通道跟随区、键盘显示区、语言区、诊断区与操作栏）；
- 16 通道跟随开关（`followKeyToggles`）采用 JIVE CSS Grid（8 列 × 2 行）网格声明，彻底消灭手写双循环坐标排版；
- `juce::AudioDeviceSelectorComponent` 封装为 Native 注入组件，由 JIVE 容器管理外边距与自适应宽度；
- 重构 `SettingsComponent`：由 `jive::Interpreter` 一次解释整棵设置树，控件直接绑定 `editingState` 属性；
- 移除 `SettingsComponent.h` 中 300+ 行 `layoutContent()` 与手动 `setBounds` 代码（精简为纯 JIVE Viewport 驱动）；
- 确定性测试：`source/tests/SettingsLayoutModelTest.cpp` 新增 5 类测试（树结构、16 通道 Grid、显示选项、组件动态检索、动态通道跟随显示切换与安全析构），全量测试全绿。

### Phase 15-D：模态操作反馈与导出进度 JIVE 现代化 ✅
- 在 `JiveModalDialog` 中扩展 `makeProgressLayout` 声明式进度弹窗模板（包含状态提示文本、JIVE ProgressBar 与取消按钮）；
- 重构 `WavExportTask`（消除对原生 `ThreadWithProgressWindow` / `AlertWindow` 的依赖），以 JIVE 声明式进度弹窗统一替代，提供现代暗黑主题视觉；
- 维持线程模型与异常安全性：后台线程独立渲染音频，消息线程 30 Hz 定时器平滑刷新 JIVE 进度与状态文本，严密保留取消/失败残留目标文件自动清理（ERR-009/012/015）；
- 单元测试：`JiveModalDialogTest` 新增 `testProgressLayoutBuilder` 测试用例，断言进度条节点与状态信息结构，全量测试全绿。

### Phase 15-E：测试补齐、三闸门与 Windows 视觉回归 ✅
- 编写 JIVE 弹窗与设置布局模型解析的确定性单元测试（`JiveModalDialogTest` 与 `SettingsLayoutModelTest` 全绿）；
- 运行三闸门验证通过：`wsl-build --configure-only` 0 warning / `test` 3101 断言全绿 / `format --check` 0 违规 / `win-build` MSVC 编译验证成功；
- Windows 侧手工完整回归通过：预设新建/重命名/删除弹窗、歌曲信息编辑保存取消、设置窗口 16 通道 Grid 开关与即时多语言切换、WAV 离线导出进度展示与取消清理。

---

## 3. 验收标准达成情况

- [x] 所有自定义弹窗（Preset / Metadata）与设置窗口（Settings）完全消灭手动像素排版与 `setBounds` 切分代码。
- [x] 16 通道跟随矩阵采用 JIVE CSS Grid 优雅排版，响应式良好。
- [x] `AudioDeviceSelectorComponent` 作为 Native 组件无缝注入 JIVE 树。
- [x] 弹窗与设置面板在运行时中英文切换时无需销毁重建，界面响应流畅。
- [x] 全量单元测试全绿，三闸门通过。
- [x] Windows 侧手工回归清单验证无任何视觉撕裂或交互回归。
