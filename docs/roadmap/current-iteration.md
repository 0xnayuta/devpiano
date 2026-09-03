# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

**Phase 28：Devpiano 声明式 UI 基础设施深度治理与接口冻结 (Declarative UI Infrastructure Governance & API Freeze) [进行中，2026-09-03 开始]**

本轮迭代专项目标为：在已完成 JIVE 子模块退役、核心代码内化入 `source/UI/jive/core/` 与 CSS Grid 核心升格的基础上，彻底清除 JIVE 的“外部通用框架思维”遗留死重，确立 Devpiano 内生 UI 专属定位与清晰的 API 边界（ViewHost 门面），构建全覆盖的布局金标回归测试（Layout Golden Tests），规范化命名空间与宏前缀，最终实施 UI Infrastructure API Freeze（接口冻结），使后续业务开发（包含声学控制、预设网格等）彻底解耦于底层运行时细节。

*(注：Phase 27 已于 2026-09-02 全部胜利完成，包含 JUCE 9.0.1 升级、内化代码 CI 门禁闭环与发布打包)*
---

### Phase 27-A：子模块指针升级与工程构建环境基线更新 [已完成，2026-09-02]
> 目标：检出 JUCE 9.0.1 官方版本，更新构建预设与工具链基线。

- [x] **子模块指针检出**：
  - 进入 `submodules/JUCE` 并切换至 tag `9.0.1`（Commit: `e18f7f5`）
  - 确认子模块状态干净，更新 `AGENTS.md`、`README.md` 中的版本基线说明
- [x] **构建系统与工具链就绪**：
  - 运行 `./scripts/dev.sh wsl-build --configure-only` 验证 CMake 3.28+ 与 JUCE 9 的配置协同
  - 校验 `juceaide` 编译期辅助工具链在 JUCE 9 下的正常运行与 `compile_commands.json` 导出
  - 核查 MSVC 19 预编译头（PCH）与 C++20 编译选项兼容性

---

### Phase 27-B：非 UI 领域 Breaking Changes 适配与 API 兼容修复 [已完成，2026-09-02]
> 目标：消除音频后端、插件宿主与离线导出管线的编译断点与运行时行为变动。

- [x] **VST3 宿主与插件客户端接口适配**：
  - 校验 `AudioPluginInstance`、`PluginHost` 与 `PluginFlowSupport` 接口兼容性（已使用 `addDefaultFormatsToManager`）
  - 确认 `PluginOperationController` 全面使用 `createEditorAndMakeActive()` 编辑器生命周期
  - 校验插件状态 XML / 内存 Blob 序列化在 JUCE 9 下的二进制兼容性
- [x] **音频设备与导出流防护**：
  - 核对 `AudioDeviceManager` 与 `AudioEngine` 初始化参数及多线程安全变动
  - 确认 `WavFileExporter` 与 `PluginOfflineRenderer` 全面使用现代 `AudioFormatWriterOptions`
- [x] **测试验证**：
  - 12,187+ 单元测试 100% 跑通通过（`AudioEngineTest`, `PluginHostTest`, `PluginOfflineRendererTest`, `RenderPipelineTest` 等）

---

### Phase 27-C：内化 UI 基础设施 (JIVE core) JUCE 9 适配与接口对齐 [已完成，2026-09-02]
> 目标：确保内生 UI 运行时与自绘组件在 JUCE 9 下渲染与排版 100% 正常。

- [x] **字体构造与文本测量全量对齐（`FontOptions`）**：
  - 确认 `DevPianoLookAndFeel.cpp`、`CustomKeyboard.cpp`、`DesignTokens.cpp` 与 `source/UI/jive/core/` 彻底消除旧式 `juce::Font(...)` 直接构造，全面采用 `juce::FontOptions` 现代初始化
  - 确认 `TextComponent` 与 `juce::AttributedString` / `GlyphArrangement` 布局计算无像素偏差
- [x] **SVG 与矢量图形渲染验证**：
  - 确认 `VectorIconFactory.h` 采用 `juce::DrawablePath` 纯路径构建，不依赖已废弃的 XML `createFromSVG`
  - 确认 `DevPianoLookAndFeel` 的 `drawDrawableButton` 与 `DrawableButton::ImageFitted` 正常呈现
- [x] **自绘组件与交互生命周期回归**：
  - 88 键虚拟钢琴自绘（`CustomKeyboard`）、发光粒子与按键响应
  - 插件面板展开/折叠重排（`layOutChildren`）、全局模态弹窗与设置窗口
- [x] **测试验证**：
  - `StyleCatalogTest`、`JiveModalDialogTest` 与全量 12,187+ 单元测试 100% 跑通

---

### Phase 27-D：内化代码质量治理与纳入 CI 全量静态分析门禁 [已完成，2026-09-02]
> 目标：重构消除 `source/UI/jive/core/` 历史遗留语法，解禁 `.clang-tidy` 过滤，实现 100% 源码同等标准治理。

- [x] **代码现代化与规范对齐（C++20 Modernization）**：
  - 修复 `jive_Object`、`jive_Property`、`jive_Timer`、`jive_Event`、`jive_ComponentInteractionState` 等类的 move 构造/赋值 `noexcept` 声明与 `override` 显式标注
  - 优化 const 引用传参并修复 dynamic_cast copy 构造
- [x] **解禁并纳入 CI Clang-Tidy 门禁**：
  - 更新 `.clang-tidy`：移除对 `source/UI/jive/core/` 的 HeaderFilterRegex 排除规则（仅排除未编译的 `extensions/`）
  - 调整 `.clang-tidy` checks 排除非适用的 `-clang-analyzer-optin.*`（解决 JUCE 位掩码枚举检查）与语法风格检查
  - 更新 `.github/workflows/ci.yml`、`scripts/dev.sh` 与 `tools/tidy-cache.py`，使内生 UI 基础设施与核心业务代码平等接受 CI 增量/全量静态分析
- [x] **验证通过**：
  - 本地运行 `./scripts/dev.sh tidy` 在所有内化修改文件上 100% 通过（0 错误 0 警告）
  - 全量单元测试 `./scripts/dev.sh test` 12,187+ 断言 100% 通过

---

### Phase 27-E：全系统功能回归、三闸门闭环与发布打包验证 [已完成，2026-09-02]
> 目标：双平台编译与测试 100% 绿灯，完成手工端到端冒烟与正式打包验证。

- [x] **全套单元测试与静态分析闭环**：
  - 运行全量单元测试（`./scripts/dev.sh test`），确保 12,187+ 断言全部通过
  - 代码格式化合规检查（`./scripts/dev.sh format --check`）100% 绿灯
  - 静态检查增量（`./scripts/dev.sh tidy`）0 错误 0 警告
- [x] **Windows MSVC 验证与正式发布打包**：
  - 执行 Windows MSVC Debug 构建验证（`./scripts/dev.sh win-build`）100% 成功
  - 执行 Windows MSVC Release 构建验证（`./scripts/dev.sh win-build --release`）100% 成功
  - 执行正式打包流水线（`./scripts/dev.sh package`），校验 Windows x64 zip 产物与 sha256 签名完整性
- [x] **端到端功能冒烟测试**：
  - 键盘演奏与 7 大物理声学系统合成发声
  - VST3 外部插件加载、参数调节与离线渲染导出
---

## Phase 28：Devpiano 声明式 UI 基础设施深度治理与接口冻结 (Declarative UI Infrastructure Governance & API Freeze)

### Phase 28-A：API 边界收敛与 ViewHost 门面构建 (API Boundary Convergence & ViewHost Facade) [已完成，2026-09-03]
> 目标：消除业务层（MainComponent、SettingsComponent、JiveModalDialog）对底层 `::jive::Interpreter` 与 `::jive::GuiItem` 的裸露直接依赖，建立强类型 RAII 门面。

- [x] **构建 `devpiano::ui::ViewHost`（统一 UI 宿主门面）**：
  - 内部完整封装 `Interpreter` 实例与 `GuiItem` 树生命周期，统一接管解析、组件工厂注入、`safeCleanupJiveTree` 析构时序与悬挂指针防范
  - 提供业务单一交互入口：`viewHost.loadLayout(const juce::ValueTree&)`、`viewHost.getRootComponent()`、`viewHost.applyStyles(StyleCatalog&)`
- [x] **强类型组件查找与访问器收敛**：
  - 提供 `viewHost.find<T>(const juce::String& id)` 模板方法，消除业务侧分散的 `dynamic_cast`
  - 重构 `MainComponent.cpp`、`MainComponentJiveAccessors.cpp`、`SettingsComponent.cpp`、`JiveModalDialog.cpp` 与 `WavExportTask.cpp`，彻底消除裸 `Interpreter` / `GuiItem` 成员指针
- [x] **线程安全断言（UI Thread Assertions）**：
  - 在 `ViewHost::loadLayout`、`ViewHost::reset`、`Interpreter::interpret()`、`StyleCatalog::applyToTree()` 入口显式加入 `JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED`，防范非 UI 线程异步调用数据竞争
---

### Phase 28-B：全量声明式 UI 布局金标测试与防线建立 (Layout Golden Tests & Regression Armor) [已完成，2026-09-03]
> 目标：构建真正的 DOM 解释与几何排版金标回归测试套件，彻底杜绝运行时解析/排版静默故障。

- [x] **全量声明式布局解释烟测（Interpretation Golden Smoke Test）**：
  - 覆盖全应用所有 ValueTree 构建函数：`makeRootLayout()`、`makeSettingsLayoutTree()`、`makeSingleInputLayout()`、`makeConfirmLayout()`、`makeMetadataEditLayout()`、`makeProgressLayout()`、`makeKeyBindingEditLayout()`
  - 通过 `ViewHost` 走 100% 真实的解释执行，断言所有核心组件（按钮、滑块、下拉框、文本框、CSS Grid 网格）实例化非空且挂载正常
- [x] **关键节点像素几何金标测试（Deterministic Layout Bounds Test）**：
  - 在典型分辨率（1280x720 与 1920x1080）下触发几何排版
  - 对关键 UI 区域坐标断言金标基线：状态栏高度（24px 设计 Token 基线与底部吸附）、键盘区可见性与边界、跟音 CSS Grid 8 列等宽、间距与行高
- [x] **焦点隔离与交互回归固化（Focus & Glissando Invariants）**：
  - 将虚拟键盘鼠标拖动滑音（Glissando）、鼠标释放及键盘焦点不抢占机制纳入自动化断言，守护交互稳定
---

### Phase 28-C：通用死重清理与规范化命名规整 (Dead Code Elimination & Namespace / Prefix Normalization) [已完成，2026-09-03]
> 目标：清理 JIVE 遗留未用死代码，统一宏定义前缀与全局命名空间。

- [x] **清理 JIVE 内嵌单测与孤立算法（Dead Code Cleanup）**：
  - 移除 33 个核心 `.cpp/.h` 文件末尾残留的 `#if JIVE_UNIT_TESTS` 内嵌单测代码块（累计清除 7,470 行死代码，运行时代码量收敛 50% 以上）
  - 移除历史垫片 `jive_JuceVersion.h` 与过时的 JUCE 6/7/8.0.2 预编译分支，直接对齐现代 JUCE 9.0.1 `didModifyProperty`
  - 经仔细甄别取舍，坚决保留战略资产：`jive_Bezier.h`、`jive_TransferFunction.h`、`jive_Visitor.h`、`jive_IgnoredComponent` 以及缓动过渡体系（`jive_Transitions`, `jive_Easing`），杜绝误伤必要底层能力
- [x] **宏定义前缀与全局命名空间规整（Namespace & Macro Normalization）**：
  - 统一宏前缀：在 CMakeLists 与头文件中引入 `DEVPIANO_UI_ENABLE_GRID=1` 与 `DEVPIANO_UI_WITH_STYLES=1`，并通过 `jive_layouts.h` 建立与 `JIVE_*` 的双向兼容桥梁
  - 规范全局命名空间：将 `DesignTokens` 提升收归至 `devpiano::ui` 权威命名空间，并保留 `devpiano::jive::` 与 `devpiano::ui::jive::` 别名确保平滑兼容
- [x] **清理裸 `new` 与现代 C++ RAII 终审**：
  - 随单测块移除消除了 50+ 处未受保护的 `new jive::Object`，核准核心运行时内存模型
---

### Phase 28-D：质量审查闭环与 UI 基础设施接口冻结 (Quality Audit Closure & Infrastructure API Freeze) [待执行]
> 目标：双端编译与门禁验收通过，正式宣布 UI 基础设施进入 Freeze 状态，研发重心全面重归物理建模与声学演奏业务。

- [ ] **全量静态检查与双端验证**：
  - 执行 `./scripts/dev.sh format --check`、`./scripts/dev.sh tidy --all`（全量无警告）、`./scripts/dev.sh test`
  - 执行 Windows MSVC Debug 构建验证 `./scripts/dev.sh win-build`
- [ ] **宣布 UI Infrastructure API Freeze（接口冻结公约生效）**：
  - 确立规则：后续任何业务开发（包含 Phase 待定声学控制、预设管理等），只准编写原生 Component 与在 `LayoutModel` 中调用 C++ DSL 组装 ValueTree，严禁穿透修改底层 `source/UI/jive/core/`（或 `runtime/`）引擎代码
  - 将 UI 基础设施标记为稳定底层技术资产
---

## 历史实现 Backlog

- ADR-014 实施归档（内化 Devpiano UI 基础设施与 JIVE 子模块退役治理）：[`../archive/adr-014-internalize-ui-infrastructure.md`](../archive/adr-014-internalize-ui-infrastructure.md)
- AUDIT-002 修复阶段归档（全量 62 项缺陷修复与质量门禁闭环）：[`../archive/audit-002-code-quality-fix-phases.md`](../archive/audit-002-code-quality-fix-phases.md)
- Phase 26 完成记录（MIDI 多轨并轨与综合时间线合并）：[`../archive/phase26-midi-multi-track-timeline-merge.md`](../archive/phase26-midi-multi-track-timeline-merge.md)
- Phase 25 完成记录（Linux 原生桌面构建与音频驱动适配）：[`../archive/phase25-linux-desktop-and-audio-path.md`](../archive/phase25-linux-desktop-and-audio-path.md)
- Post-v1.0.0 文档体系治理与打包流水线自动化完成记录：[`../guides/release-workflow.md`](../guides/release-workflow.md)
- Phase 24 完成记录（生命力与非线性动力学绽放）：[`../archive/phase24-vitality-and-dynamic-blooming.md`](../archive/phase24-vitality-and-dynamic-blooming.md)
- Phase 23 完成记录（大师级音色校准与 Pianoteq 对齐精调）：[`../archive/phase23-master-voicing-realism-calibration.md`](../archive/phase23-master-voicing-realism-calibration.md)
- Phase 22 完成记录（物理声学极致深化与机械拟真）：[`../archive/phase22-physical-modeling-acoustic-refinement.md`](../archive/phase22-physical-modeling-acoustic-refinement.md)
- Phase 21 完成记录（踏板交感共鸣与琴盖空间声学）：[`../archive/phase21-sympathetic-resonance-lid-acoustics.md`](../archive/phase21-sympathetic-resonance-lid-acoustics.md)
- Phase 11 完成记录（声明式 UI 架构）：[`../archive/phase11-declarative-ui-jive.md`](../archive/phase11-declarative-ui-jive.md)
