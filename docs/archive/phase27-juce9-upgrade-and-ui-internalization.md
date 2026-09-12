# Phase 27 完成记录：JUCE 9.0.1 框架升级、UI 基础设施内化与全平台生态演进

> 归档日期：2026-09-02
> 前序依赖：Phase 26（MIDI 多轨并轨与综合时间线合并）、ADR-014（内化 Devpiano 声明式 UI 基础设施并退役 JIVE 外部子模块）
> 实施范围：`submodules/JUCE`、`source/UI/jive/core/`、`source/Audio/`、`source/Plugins/`、`source/Export/`、`CMakeLists.txt`、`.clang-tidy`、CI 门禁流水线

---

## 1. 目标与背景

随着项目向跨平台现代化演进，原有的底层框架与依赖结构面临两大核心演进诉求：
1. **JUCE 框架现代化升级**：从旧版 JUCE 迁移至官方最新稳定版 **JUCE 9.0.1**，获取现代音频设备驱动支持、强化 VST3 插件宿主兼容性，并对齐最新 C++20/23 标准工具链；
2. **UI 依赖主权确立（ADR-014 落地）**：彻底解耦并退役维护停滞的外部 Git 子模块 `submodules/JIVE`，将其实质核心（`juce::ValueTree` 声明式解析、CSS Grid、自适应排版）内化至 `source/UI/jive/core/`，完全自主掌握排版算法演进，并对其施加与业务源码同等严格的代码质量与静态分析标准；
3. **平滑化解 Breaking Changes**：全面治理 JUCE 9 引入的破坏性变动（如 `juce::Font` 构造方式弃用、音频流格式导出器选项规范化、插件生命周期 API 对齐等），确保双平台（Linux/WSL 与 Windows MSVC）无缝通过。

---

## 2. 实施细节（Phase 27-A ~ 27-E）

### Phase 27-A：子模块指针升级与工程构建环境基线更新

- **子模块指针检出**：
  - 进入 `submodules/JUCE` 并切换检出至官方 tag `9.0.1`（Commit: `e18f7f5`）；
  - 核查子模块状态干净，并在 `AGENTS.md`、`README.md`、`development.md` 中全面更新版本基线。
- **构建系统与工具链就绪**：
  - 运行 `./scripts/dev.sh wsl-build --configure-only` 验证 CMake 3.28+ 与 JUCE 9.0.1 的配置协同；
  - 校验 `juceaide` 辅助工具链在 JUCE 9 下的正常执行与 `compile_commands.json` 导出无误；
  - 核查 MSVC 19 预编译头（PCH）与 C++20 选项（`/std:c++20`）构建兼容性。

### Phase 27-B：非 UI 领域 Breaking Changes 适配与 API 兼容修复

- **VST3 宿主与插件生命周期适配**：
  - 校验 `AudioPluginInstance`、`PluginHost` 与 `PluginFlowSupport` 接口兼容性（统一使用 `addDefaultFormatsToManager`）；
  - 确认 `PluginOperationController` 全面采用 `createEditorAndMakeActive()` 现代编辑器生命周期；
  - 校验插件状态 XML 及内存 Blob 序列化在 JUCE 9 下的二进制兼容性。
- **音频设备与导出流防护**：
  - 核对 `AudioDeviceManager` 与 `AudioEngine` 初始化参数及多线程安全变动；
  - 确认 `WavFileExporter` 与 `PluginOfflineRenderer` 全面使用现代 `AudioFormatWriterOptions`；
  - 消除音频多线程调度在设备重启时的潜在竞争。
- **测试验证**：
  - 12,187+ 单元测试 100% 跑通（`AudioEngineTest`, `PluginHostTest`, `PluginOfflineRendererTest`, `RenderPipelineTest` 等）。

### Phase 27-C：内化 UI 基础设施 (JIVE core) JUCE 9 适配与接口对齐

- **字体构造与文本测量全量对齐（`FontOptions`）**：
  - 彻底消除 `DevPianoLookAndFeel.cpp`、`CustomKeyboard.cpp`、`DesignTokens.cpp` 与 `source/UI/jive/core/` 中旧式 `juce::Font(...)` 直接构造，全面采用 `juce::FontOptions` 现代初始化；
  - 确认 `TextComponent` 与 `juce::AttributedString` / `GlyphArrangement` 布局计算无像素级偏差。
- **SVG 与矢量图形渲染验证**：
  - 确认 `VectorIconFactory.h` 采用 `juce::DrawablePath` 纯路径构建，不依赖已废弃的 XML `createFromSVG`；
  - 确认 `DevPianoLookAndFeel` 的 `drawDrawableButton` 与 `DrawableButton::ImageFitted` 矢量按钮正常渲染。
- **自绘组件与交互生命周期回归**：
  - 88 键虚拟钢琴自绘（`CustomKeyboard`）、发光粒子与按键响应流畅无撕裂；
  - 插件面板展开/折叠重排（`layOutChildren`）、全局模态弹窗与设置窗口几何自适应正常。
- **测试验证**：
  - `StyleCatalogTest`、`JiveModalDialogTest` 与全量测试 100% 跑通。

### Phase 27-D：内化代码质量治理与纳入 CI 全量静态分析门禁

- **代码现代化与规范对齐（C++20 Modernization）**：
  - 全面修复 `jive_Object`、`jive_Property`、`jive_Timer`、`jive_Event`、`jive_ComponentInteractionState` 等类的 move 构造/赋值 `noexcept` 声明与 `override` 显式标注；
  - 优化 const 引用传参并修复 dynamic_cast 临时对象 copy 构造问题。
- **解禁并纳入 CI Clang-Tidy 门禁**：
  - 更新 `.clang-tidy`：移除对 `source/UI/jive/core/` 的 HeaderFilterRegex 排除规则（仅排除未编译的 `extensions/` 历史遗留目录）；
  - 微调 checks 规则排除不适用的位掩码枚举检查；
  - 更新 `.github/workflows/ci.yml`、`scripts/dev.sh` 与 `tools/tidy-cache.py`，使内生 UI 基础设施与核心业务代码享受完全平等的静态分析检验标准。
- **验证通过**：
  - 运行 `./scripts/dev.sh tidy` 在所有内化修改文件上 100% 通过（0 错误 0 警告）；
  - 全量单元测试 `./scripts/dev.sh test` 12,187+ 断言 100% 通过。

### Phase 27-E：全系统功能回归、三闸门闭环与发布打包验证

- **全套单元测试与静态分析闭环**：
  - 运行全量单元测试（`./scripts/dev.sh test`），确保 12,187+ 断言全部通过；
  - 代码格式化合规检查（`./scripts/dev.sh format --check`）100% 绿灯；
  - 静态检查增量（`./scripts/dev.sh tidy`）0 错误 0 警告。
- **Windows MSVC 验证与正式发布打包**：
  - 执行 Windows MSVC Debug 构建验证（`./scripts/dev.sh win-build`）100% 成功；
  - 执行 Windows MSVC Release 构建验证（`./scripts/dev.sh win-build --release`）100% 成功；
  - 执行正式打包流水线（`./scripts/dev.sh package`），校验 Windows x64 zip 产物与 sha256 签名完整性。
- **端到端功能冒烟测试**：
  - 键盘演奏与 7 大物理声学系统合成发声正常；
  - VST3 外部插件加载、参数调节与离线渲染导出顺畅。

---

## 3. 产出物与技术遗产

1. **统一的现代依赖基线**：锁定 JUCE 9.0.1 稳定版本，消除了对不稳定上游开发分支的依赖；
2. **完全内生的 UI 排版基础设施**：完全消除 JIVE Git 外部子模块，核心代码落户 `source/UI/jive/core/`；
3. **全覆盖的代码质量门禁**：UI 核心与业务代码统一遵循 C++20 与静态分析准则；
4. **承上启下**：为 Phase 28 的 ViewHost 门面封装、API Freeze 与 Layout Golden Tests 奠定了坚实整洁的代码底座。
