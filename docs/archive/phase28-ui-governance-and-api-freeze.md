# Phase 28 完成记录：Devpiano 声明式 UI 基础设施深度治理与接口冻结

> 归档日期：2026-09-03
> 前序依赖：Phase 27（JUCE 9.0.1 框架升级、UI 代码内化与全平台生态演进）
> 实施范围：`source/UI/`、`source/Settings/`、`source/Export/`、`CMakeLists.txt`、全套布局金标测试

---

## 1. 目标与背景

在 Phase 27 完成 JUCE 9.0.1 升级、注销 JIVE Git 外部子模块并内化核心至 `source/UI/jive/core/` 之后，内生 UI 基础设施仍带有大量上游通用框架的历史包袱：
1. 业务组件（`MainComponent`、`SettingsComponent`、`JiveModalDialog`、`WavExportTask`）直接裸露依赖底层 `::jive::Interpreter` 与 `::jive::GuiItem`，缺乏统一生命周期接管与 RAII 门面；
2. 业务侧散落裸指针类型强转与 `dynamic_cast`，存在析构顺序错乱及悬挂指针隐患；
3. 源码底部残留 33 处从不编译的 `#if JIVE_UNIT_TESTS` 宏块与多版本历史兼容垫片（`jive_JuceVersion.h` 等），总计 7,400+ 行死重代码；
4. 缺乏真正的全量布局解释烟测与像素几何金标回归测试；
5. 命名空间与宏前缀分裂（`devpiano::jive::` 与 `JIVE_*`）。

Phase 28 确立了 Devpiano 内生 UI 的专属定位，完成了 API 边界收敛与门面构建、建立了布局金标测试防线、精简了死重代码，并正式确立了 **UI Infrastructure API Freeze（接口冻结公约）**。

---

## 2. 实施细节（Phase 28-A ~ 28-D）

### Phase 28-A：API 边界收敛与 ViewHost 门面构建
- **核心组件**：构建 `devpiano::ui::ViewHost`（`source/UI/ViewHost.h/.cpp`），内部完整封装 `Interpreter` 实例与 `GuiItem` 树节点，析构与 `reset()` 时自动调度 `safeCleanupJiveTree` 确保组件与样式表安全解绑。
- **业务全面接轨**：重构 `MainComponent`、`MainComponentJiveAccessors`、`SettingsComponent`、`JiveModalDialog` 及 `WavExportTask`，彻底清除所有外部裸露的 `jive::Interpreter` 与 `jive::GuiItem` 成员指针。
- **线程安全断言**：在 `ViewHost::loadLayout`、`ViewHost::reset`、`Interpreter::interpret`、`StyleCatalog::applyToTree` 统一注入 `JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED`。
- **基础测试**：新增 `source/tests/ViewHostTest.cpp`（39 项断言通过）。

### Phase 28-B：全量声明式 UI 布局金标测试与防线建立
- **金标测试套件**：新增 `source/tests/LayoutGoldenTest.cpp`（134 项断言通过）：
  1. **全应用布局解释烟测**：覆盖全应用 7 大布局构建器（`makeRootLayout`、`makeSettingsLayoutTree`、`makeSingleInputLayout`、`makeConfirmLayout`、`makeMetadataEditLayout`、`makeProgressLayout`、`makeKeyBindingEditLayout`），走真实解释并验证组件非空挂载；
  2. **典型分辨率几何金标**：验证 1280x720 与 1920x1080 下根窗口、主区域与状态栏（严格保持 24px 高度单一真相源且吸附底部）的像素级坐标对齐；
  3. **16 通道 CSS Grid 对齐**：验证设置窗口中 8 列 × 2 行跟随按键的 Y 轴共线、列递增与 gap 间距；
  4. **焦点隔离与滑音配对**：验证 `CustomKeyboard` 绝不抢占键盘焦点；验证鼠标连续滑音（Glissando）在 key 之间拖拽时的即时 `NoteOff` / `NoteOn` 配对与无悬挂音符不变量。

### Phase 28-C：通用死重清理与规范化命名规整
- **死代码剥离**：彻底清除 33 个核心源码文件底部的 `#if JIVE_UNIT_TESTS` 块，删除 7,470 行死重代码；删除已无调用的 `jive_JuceVersion.h`；移除 4 个仅剩一行 include 的空壳 `.cpp`（`jive_Event.cpp`、`jive_Interpolate.cpp`、`jive_Property.cpp`、`jive_VariantConvertion.cpp`）。
- **战略资产加固**：经审查严加保留 `jive_IgnoredComponent`（基础容器与点击穿透）、`jive_Visitor.h`（C++20 visit 重载）、`jive_Bezier.h` 与 `jive_TransferFunction.h`（贝塞尔插值与动画求解内核）、`Transitions` / `Easing`（过渡动画战略储备）。
- **宏前缀与命名空间收敛**：在 CMakeLists 引入 `DEVPIANO_UI_ENABLE_GRID=1` 与 `DEVPIANO_UI_WITH_STYLES=1` 并建立双向兼容宏桥梁；将 `DesignTokens` 提升收纳至 `devpiano::ui::DesignTokens`。

### Phase 28-D：质量审查闭环与接口冻结
- **三闸门闭环**：
  - 代码格式合规：`./scripts/dev.sh format --check` 100% 通过；
  - 单元测试全覆盖：`./scripts/dev.sh test` 69 个测试套件、12,853 个断言 100% 绿灯；
  - 静态检查验证：`./scripts/dev.sh tidy` 在所有修改文件上 0 错误 0 警告；
  - Windows MSVC 验证：`./scripts/dev.sh win-build` 增量编译通过，`DevPiano.exe` 链接成功。
- **接口冻结公约正式生效**。

---

## 3. UI Infrastructure API Freeze（接口冻结公约）

自 Phase 28 验收之日起，工程正式确立以下 UI 开发纪律：

1. **底层引擎代码封存**：`source/UI/jive/core/`（及未来的 `source/UI/runtime/`）代码被列为稳定底层技术资产，**严禁因日常业务需求修改其内部实现**。
2. **业务开发严格走门面与 DSL**：
   - 界面布局构建：只准在 `source/UI/jive/LayoutModel.cpp`（或各业务组件的 LayoutModel）中编写 `juce::ValueTree` 声明式 DSL；
   - 界面交互与接线：只准通过 `devpiano::ui::ViewHost` 门面进行组件查找（`host.find<T>(id)`）与属性修改；
   - 复杂动效与自绘：只准编写封装良好的继承自 `juce::Component` 的原生组件（如 `CustomKeyboard`、`AdsrCurveComponent`、`ColourSwatchButton`），并通过组件工厂注入布局。
3. **研发重心全面回归**：项目核心研发精力彻底从 UI 基础设施治理移开，全面回归至自主研发的 7 大物理建模钢琴算法、MIDI 矩阵与演奏交互。
