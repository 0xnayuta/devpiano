# ADR-014 实施归档：内化 Devpiano UI 基础设施与 JIVE 子模块退役治理

- **归档日期**：2026-09-02
- **关联决策**：[`docs/decisions/ADR-014-internalize-ui-infrastructure-and-deprecate-jive-submodule.md`](../decisions/ADR-014-internalize-ui-infrastructure-and-deprecate-jive-submodule.md)
- **PR 追踪**：[PR #11: feat(ui): internalize UI infrastructure and deprecate JIVE submodule (ADR-014)](https://github.com/0xnayuta/devpiano/pull/11)
- **状态**：全部 Phase 0 ~ Phase 3 完成并通过双端五大 CI 自动化门禁。

---

## 一、实施背景与目标

依据 ADR-014 架构决策，针对外部 `submodules/JIVE` 子模块体积冗余大（>24,000 行）、上游 v2 分支发生巨幅破坏性架构断代、阻碍后续升级 JUCE 9.0.1 的痛点，devpiano 执行了**「路线 B：先提后升（Extract Minimal Closure -> Submodule Deinit -> JUCE 9.0.1 Ready）」**治理策略：
1. 精准提取 devpiano 真实使用的 3,800 行核心最小闭包至 `source/UI/jive/core/`；
2. 剥离 >15,000 行未用死代码（XML 解释器、未使用控件装饰器、FileObserver、Perfetto 等）；
3. 战略归档 `Grid` 布局组件至 `source/UI/jive/extensions/grid/`（供后续矩阵系统按需激活）；
4. 彻底退役注销 `submodules/JIVE`，建立项目自包含编译；
5. 修复已知的 JUCE 9 文本与图像接口断点，完成全套门禁闭环。

---

## 二、详细实施阶段与完成成果

### ADR-014 Phase 0：基线测试冻结与合规归档准备 [已完成，2026-09-01]

- [x] 执行环境自检与全量测试基线验证（`./scripts/dev.sh test`，12,187+ 断言全绿）。
- [x] 在根目录更新 `THIRD-PARTY-NOTICES.md`，记录 JIVE 原作者（James Johnson）、MIT 许可证全文与快照 Commit（`89d5787`）。
- [x] 归档并验证 `design_tokens.json` 与 `style_sheets.json` 静态资产完整性。

### ADR-014 Phase 1：提取 JIVE 核心最小依赖闭包（`source/UI/jive/core/`） [已完成，2026-09-01]

- [x] **创建目录结构**：
  - `source/UI/jive/core/`（基础核心运行时）
  - `source/UI/jive/extensions/grid/`（战略储备扩展）
- [x] **迁移 Core 基础层**（保留文件头 MIT 版权声明）：
  - 属性系统：`BoxModel`、`Property`、`PropertyBehaviours`、`Object`、`ReferenceCountedValueTreeWrapper`
  - 几何与交互：`BorderRadii`、`Length`、`Orientation`、`Event`、`ComponentInteractionState`
  - 变体转换：`FlexVariantConverters`、`MiscVariantConverters`、`VariantConvertion`、`AttributedStringVariantConverters`
  - 动效与时钟：`Transition`、`Transitions`、`Easing`、`Timer`、`TimeParser`、`Interpolate`、`Visitor`、`Bezier`、`TransferFunction`
- [x] **迁移 Layout 核心层**：
  - 解释与节点：`Interpreter`、`GuiItem`、`GuiItemDecorator`、`CommonGuiItem`、`ContainerItem`、`ContainerItemChild`、`ComponentFactory`、`View`
  - 弹性排版：`FlexContainer`、`FlexItem`、`LayoutStrategy`、`Display`、`Overflow`
  - 基础块项：`BlockContainer`、`BlockItem`
- [x] **迁移核心控件包装与样式引擎**：
  - 控件：`Button`、`ComboBox`、`Slider`、`ProgressBar`、`Text`、`Label`、`NormalisedProgressBar`、`TextComponent`、`IgnoredComponent`
  - 样式与画布：`StyleSheet`、`StyleIdentifier`、`StyleSelectors`、`Colours`、`Fill`、`Gradient`、`BackgroundCanvas`、`Canvas`、`FontUtilities`、`StringStreams`
- [x] **归档战略扩展（KEEP-LATER）**：
  - 将 `GridContainer`、`GridItem`、`GridVariantConverters` 归入 `source/UI/jive/extensions/grid/`。
- [x] **清理与重定向头文件包含**：
  - 全局重定向 `source/` 下各调用点至本地头文件（`#include "UI/jive/core/..."`）。
  - 构建系统（`CMakeLists.txt`）切换为直接编译 `source/UI/jive/core/` 源码。

### ADR-014 Phase 2：注销 JIVE Git 子模块与 CMakeLists 纯化 [已完成，2026-09-01]

- [x] **CMakeLists.txt 纯化**：
  - 移除 `add_subdirectory(submodules/JIVE)`。
  - 移除 target 链接中的 `jive::jive_layouts`、`jive::jive_style_sheets`、`jive::jive_core`。
  - 将 `source/UI/jive/core/` 源码加入 `devpiano` 与 `devpiano_tests` 目标编译清单。
- [x] **Git Submodule 退役**：
  - 执行 `git submodule deinit -f submodules/JIVE` 与 `git rm -f submodules/JIVE`。
  - 移除 `.gitmodules` 中的 JIVE 条目，清理同步脚本（`tools/sync-to-win.ps1`）探测残余。
- [x] **构建与测试验证**：
  - 运行 `./scripts/dev.sh wsl-build --configure-only` 刷新 `compile_commands.json`。
  - 运行 `./scripts/dev.sh test` 确保 100% 单元测试在无子模块环境下通过。

### ADR-014 Phase 3：JUCE 9.0.1 兼容性修复与三闸门全量验证 [已完成，2026-09-01]

- [x] **适配 JUCE 9 UI API**：
  - `FontUtilities::calculateStringWidth` 优化为调用 `juce::GlyphArrangement::getStringWidth`，消除弃用方法。
  - 确认 `TextComponent` 基于现代 `juce::TextLayout` 与 `juce::AttributedString`。
  - 确认全应用图标通过 `DrawablePath` 注入 `DrawableButton`，无 raw `Drawable` Component 继承断点。
  - `StyleCatalog` 深度值比对与对象缓存（`cachedStyles`）稳定，全量样式测试通过。
- [x] **UI 交互全量功能回归**：
  - 主窗口 88 键拟真键盘、自绘包络、工具栏状态响应。
  - 插件面板高度折叠/展开动画与重新排版（`layOutChildren`）。
  - 全局模态弹窗、设置窗口、中英文切换与 Token 热重载。
- [x] **CI 静态分析与时序优化**：
  - 修复 `StyleCatalogTest.cpp` 细粒度头文件引用；
  - 优化 CI 静态分析流程（PCH 解耦与 `devpiano_binary_data` 编译期头文件先行动作）。
- [x] **全套五大门禁 100% 绿灯**：
  - Code Format Gate (`clang-format`)：PASS
  - Incremental Static Analysis (`clang-tidy`)：PASS
  - Linux Build, Unit Tests & Release Gate：PASS (12,187+ 测试通过)
  - Windows MSVC Build & Unit Tests：PASS
  - PR Agent：PASS

---

## 三、治理成效数据

| 指标 | 治理前（JIVE Submodule） | 治理后（内化 UI Infrastructure） | 收益 |
| :--- | :--- | :--- | :--- |
| **代码总量（LOC）** | 24,591 行 | 3,842 行（Core） | **削减 84.4% 冗余代码** |
| **Git 子模块数量** | 2 个（JUCE, JIVE） | 1 个（JUCE） | **降低 50% 外部依赖管理成本** |
| **构建目标依赖** | 链接 3 个外部 target | 直接参与业务源码编译 | **彻底消除链接与符号查找问题** |
| **JUCE 9 升级风险** | 高危（受制于 JIVE 上游 v2 断代） | 极低（完全掌控代码与接口适配） | **实现自主可控的平滑跃迁** |
