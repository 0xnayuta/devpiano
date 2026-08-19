# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

**Phase 15：UI 架构统一至 JIVE（声明式弹窗与设置面板重构）[准备启动，2026-08-19]**

内置物理建模钢琴音源三部曲（Phase 12–14）已圆满完成（2026-08-19）：Phase 12 谐波加法 v1、Phase 13 刚性失谐与模态耗散 v2、Phase 14 增强模态合成 v3（递归振荡器 + 20 分音 + 双阶段衰减 + 拍频 + 8 峰音板）全部落地，经 Windows 侧人工听觉回归确认为唯一默认钢琴音色，全量 3101 断言全绿。历史细节已归档至 [`../archive/phase12-14-builtin-piano-synthesis.md`](../archive/phase12-14-builtin-piano-synthesis.md)。

本轮重点：推进主窗口之外的 UI 元素统一进 **JIVE 声明式 UI 框架**，消灭 `SettingsComponent` 与自定义 Dialog 中残存的 300+ 行手写绝对坐标排版，实现全应用视觉风格、响应式布局与国际化机制的一致性。

---

## Phase 15：UI 架构统一至 JIVE（声明式弹窗与设置面板重构）[排期中]

### 背景与目标

在 Phase 11 中，主窗口的 5 个主要面板（Header, Plugin, Controls, KeyboardArea, StatusBar）已成功迁移至 JIVE 声明式 `juce::ValueTree` + Flex/Grid 布局体系。然而，当前主窗口之外的子窗口与交互弹窗仍处于传统 JUCE 手动像素排版与独立自绘体系：

1. **设置窗口（`SettingsComponent`）**：包含 300+ 行手写绝对坐标与流式切分代码（`removeFromTop`、`setBounds`、硬编码宽高），16 通道跟随开关通过两层循环手算网格坐标，语言切换时需销毁重建原生组件，与主窗口 JIVE 体系完全割裂。
2. **自定义模态弹窗（`PresetNameDialog`, `PresetConfirmDialog`, `PerformanceMetadataDialog`）**：各自基于 `DialogContentBase` 派生独立组件，在 `resized()` 中手算组件坐标与边距，存在重复样板代码。
3. **长时间操作反馈（`WavExportTask`）**：继承自 JUCE 原生 `ThreadWithProgressWindow`（基于 AlertWindow），视觉风格偏传统 OS 样式。

**阶段目标**：
1. 构建通用的 JIVE 声明式模态弹窗基础设施（`JiveModalDialog`），将预设与歌曲元数据弹窗迁移为声明式驱动；
2. 将 `SettingsComponent` 重构为 `SettingsLayoutModel`（JIVE ValueTree + CSS Grid/Flex 布局），`AudioDeviceSelectorComponent` 封装为 Native 注入项，彻底移除 300+ 行手写坐标；
3. 将 WAV 导出等长耗时进度交互接入现代化 JIVE 模态 Overlay 或标准弹窗；
4. 明确保留 `CustomKeyboard` 原生自绘内核、`PluginEditorWindow` 原生宿主与系统 `FileChooser` 的合理边界。

---

### 前置事实（已核实代码）

| 事实 | 位置 | 影响 |
|---|---|---|
| 主窗口 JIVE 基础设施成熟（`Interpreter`, `StyleCatalog`, `DesignTokens`, `LayoutModel`） | `source/UI/jive/` | 可直接复用于子窗口与对话框 |
| `SettingsComponent` 包含 300+ 行 manual `setBounds` / `removeFromTop`，16 个 Toggle 循环排版 | `source/Settings/SettingsComponent.h` | 迁移为 JIVE CSS Grid 与 FlexBox 布局，预期代码量减少 60% |
| `PresetDialogs` 与 `PerformanceMetadataDialog` 结构高度相似（Title + Label + Editor + ButtonRow） | `source/UI/PresetDialogs.cpp` / `PerformanceMetadataDialog.cpp` | 可抽取统一的 `JiveModalDialog` 模板声明 |
| `WavExportTask` 依赖 `ThreadWithProgressWindow`（AlertWindow） | `source/Export/WavExportTask.h/.cpp` | 现代化为 JIVE 弹窗或主窗口 ModalOverlay |
| `CustomKeyboard` / `AdsrCurve` / `StatusBarMidiDot` 已确立 Native 注入模式 | `source/UI/native/` | `AudioDeviceSelectorComponent` 沿用相同 Native 注入模式接入 JIVE |

---

### 子任务排期

**Phase 15-A：通用 JiveModalDialog 基础设施与声明式模板 [已完成，2026-08-19]**

- [x] 新建 `source/UI/jive/JiveModalDialog.h/.cpp`：以 JIVE `ValueTree` 驱动的模态对话框容器（继承 `juce::DialogWindow`），由 `jive::Interpreter` 统一解释布局并注入 `StyleCatalog` 全局样式。
- [x] 声明式模板支持：`makeSingleInputLayout`（单行文本输入模板，380×150）、`makeConfirmLayout`（确认取消模板，380×140）与 `makeMetadataEditLayout`（歌曲信息单行+多行文本模板，420×260）。
- [x] 统一深色主题外观、输入框焦点样式、Flex-end 右对齐按钮排版、回车确认 / ESC 取消键盘交互与异步焦点捕获。
- [x] 封装安全的回调完成序列（防 double-callback、防 UAF、`launchAsync` 模态状态安全退出、`safeCleanupJiveTree` 防止 StyleSheet 析构 UAF）。
- [x] 确定性测试：`source/tests/JiveModalDialogTest.cpp` 新增 5 类 10+ 项断言（ValueTree 节点层级、属性、尺寸、`findButtonById` 与 `findTextEditorById` 组件检索、单行/多行编辑器配置与安全析构），全量测试通过。

**Phase 15-B：预设与元数据弹窗统一迁移 [待启动]**

- [ ] `PresetNameDialog`（重命名 / 新建预设）改接 `JiveModalDialog` 文本输入模板。
- [ ] `PresetConfirmDialog`（删除预设确认）改接 `JiveModalDialog` 确认模板。
- [ ] `PerformanceMetadataDialog`（歌曲标题与备注编辑）改接 `JiveModalDialog` 元数据模板。
- [ ] 清理 `source/UI/PresetDialogs.cpp` 和 `PerformanceMetadataDialog.cpp` 中的手写 `resized()` 坐标计算与冗余 Content 类。
- [ ] 验证：全流程弹窗交互、数据回传与单元测试。

**Phase 15-C：设置面板 JIVE 声明式重构（SettingsLayoutModel） [待启动]**

- [ ] 新建 `source/Settings/jive/SettingsLayoutModel.h/.cpp`：使用 `juce::ValueTree` 声明设置面板层级（音频设备区、调号区、通道跟随区、键盘显示区、语言区、诊断区与操作栏）。
- [ ] 16 通道跟随开关（`followKeyToggles`）采用 JIVE CSS Grid（8 列 × 2 行）网格声明，彻底消灭手写双循环坐标排版。
- [ ] `juce::AudioDeviceSelectorComponent` 封装为 Native 注入组件，由 JIVE 容器管理外边距与自适应宽度。
- [ ] 重构 `SettingsComponent`：由 `jive::Interpreter` 一次解释整棵设置树，控件直接绑定 `editingState` 属性。
- [ ] 移除 `SettingsComponent.h` 中 300+ 行 `layoutContent()` 与手动 `setBounds` 代码。
- [ ] 验证多语言切换时的 JIVE 属性响应与自适应排版。

**Phase 15-D：模态操作反馈与导出进度 JIVE 现代化 [待启动]**

- [ ] 评估并实现主窗口内嵌的 JIVE 模态遮罩层（`ModalOverlay`）或基于 `JiveModalDialog` 的进度浮层。
- [ ] `WavExportTask` 进度报告改接 JIVE 进度条与状态文本，支持流畅百分比刷新与优雅取消。
- [ ] 消除原生 AlertWindow 带来的视觉风格不一致。

**Phase 15-E：测试补齐、三闸门与 Windows 视觉回归 [待启动]**

- [ ] 编写 JIVE 弹窗与设置布局模型解析的确定性单元测试。
- [ ] 运行三闸门验证：
  - `wsl-build --configure-only`
  - `test`（测试套件全绿）
  - `format --check`（代码风格零违规）
  - `win-build`（MSVC 构建成功）
- [ ] Windows 侧手工完整回归：
  - 预设新建 / 重命名 / 删除弹窗交互；
  - 歌曲信息（Metadata）编辑弹窗保存与取消；
  - 设置窗口打开、音频设备切换、调号与 16 通道跟随开关、键盘色彩与音符显示切换、语言中英文即时切换；
  - WAV 文件离线导出进度展示与取消。

---

### 验收标准

- 所有自定义弹窗（Preset / Metadata）与设置窗口（Settings）完全消灭手动像素排版与 `setBounds` 切分代码。
- 16 通道跟随矩阵采用 JIVE CSS Grid 优雅排版，响应式良好。
- `AudioDeviceSelectorComponent` 作为 Native 组件无缝注入 JIVE 树。
- 弹窗与设置面板在运行时中英文切换时无需销毁重建，界面响应流畅。
- 全量单元测试全绿，三闸门（`wsl-build` 0 warning / `test` 全绿 / `format --check` 0 违规 / `win-build` MSVC 编译成功）通过。
- Windows 侧手工回归清单验证无任何视觉撕裂或交互回归。

---

### 验证命令

```bash
./scripts/dev.sh wsl-build --configure-only
./scripts/dev.sh test
./scripts/dev.sh format --check
./scripts/dev.sh win-build
```

---

## 历史实现 Backlog

- Phase 12–14 完成记录（内置物理建模钢琴音源）：[`../archive/phase12-14-builtin-piano-synthesis.md`](../archive/phase12-14-builtin-piano-synthesis.md)
- AUDIT-001 修复阶段归档：[`../archive/audit-001-code-quality-fix-phases.md`](../archive/audit-001-code-quality-fix-phases.md)
- Phase 11 完成记录（声明式 UI 架构）：[`../archive/phase11-declarative-ui-jive.md`](../archive/phase11-declarative-ui-jive.md)
- Phase 10 完成记录：[`../archive/phase10-ui-modernization.md`](../archive/phase10-ui-modernization.md)
