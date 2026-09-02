# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

**Phase 27：JUCE 9.0.1 框架升级、内化代码治理与全平台技术栈跃迁 [进行中，2026-09-02 开始]**

本轮迭代专项目标为：
1. 将核心依赖 `submodules/JUCE` 从 `8.0.15` 升级至官方最新发布版本 **JUCE 9.0.1**（Tag: `9.0.1`, Commit: `e18f7f5`）；
2. 消除 JUCE 9 全体系在非 UI（音频引擎、VST3 宿主、导出管线）与 UI（字体、SVG、组件生命周期）层的 API 破坏性变更；
3. **内化 UI 代码质量治理闭环**：对 `source/UI/jive/core/` 进行 C++20 现代化重构与警告消除，解除 `.clang-tidy` 豁免，将其正式纳入全量 CI 静态分析门禁；
4. 完成 Linux Clang + Windows MSVC 双平台全套三闸门与 12,187+ 单元测试 100% 绿灯验收。

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
### Phase 27-E：全系统功能回归、三闸门闭环与发布打包验证 [待开始]
> 目标：双平台编译与测试 100% 绿灯，完成手工端到端冒烟与正式打包验证。

- [ ] **全套单元测试与静态分析闭环**：
  - 运行全量单元测试（`./scripts/dev.sh test`），确保 12,187+ 断言全部通过
  - 执行全量静态分析（`./scripts/dev.sh tidy --all`）
  - 代码格式化合规检查（`./scripts/dev.sh format --check`）
- [ ] **Windows MSVC 验证与正式发布打包**：
  - 执行 Windows MSVC Debug 构建验证（`./scripts/dev.sh win-build`）
  - 执行 Windows MSVC Release 构建验证（`./scripts/dev.sh win-build --release`）
  - 执行正式打包流水线（`./scripts/dev.sh package`），校验 zip 产物与 sha256 签名完整性
- [ ] **端到端功能冒烟测试**：
  - 键盘演奏与 7 大物理声学系统合成发声
  - VST3 外部插件加载、参数调节与离线渲染导出

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
