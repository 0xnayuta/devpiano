# ADR 013: 移除 melatonin_inspector 子模块，聚焦声明式 UI 架构与原生调试体系

## 状态

已采用。

## 背景

在项目早期（Phase 11，v0.3.0）引入 JIVE 声明式 UI 框架时，同步引入了 `melatonin_inspector`（[sudara/melatonin_inspector](https://github.com/sudara/melatonin_inspector)，MIT）作为 Git 子模块，旨在为 JUCE Component 树提供运行时的可视化层级检查、实时坐标拖拽修改、改色和 FPS 监控。

随着项目演进至 v1.0.1+，devpiano 的 UI 体系已全面成熟为由 **JIVE ValueTree + Flex/Grid 自适应布局 + `design_tokens.json` + `style_sheets.json`** 驱动的声明式单一事实源架构；同时，88 键虚拟钢琴键盘（`CustomKeyboard`）、ADSR 贝塞尔曲线（`AdsrCurveComponent`）及状态栏 MIDI 闪烁点（`StatusBarMidiDot`）均采用单组件 Direct Paint 渲染。

在此架构下，`melatonin_inspector` 的核心能力与实际开发工作流产生脱节，并对核心键盘焦点系统引入了不必要的耦合。

## 决策

**从 devpiano 项目中彻底移除 `melatonin_inspector` Git 子模块及所有相关构建依赖与侵入式代码。**

具体清理范围：

1. **Git 子模块注销**：从 `.gitmodules` 注销并删除 `submodules/melatonin_inspector`；
2. **构建系统解耦**：从 `CMakeLists.txt` 中移除 `add_subdirectory(submodules/melatonin_inspector)` 和 `target_link_libraries` 中的 `melatonin_inspector` 模块链接；
3. **主装配层净化**：从 `MainComponent.h` / `.cpp` 中移除 `#if DEBUG` 侵入式头文件包含、`inspector` 智能指针成员，以及析构函数中曾为防范 JIVE 销毁竞争而编写的防御性 teardown hack 代码；
4. **工具脚本与文档对齐**：清理 `tools/sync-to-win.ps1` 中的子模块探测残余，更新 `AGENTS.md`、`README.md`、`THIRD-PARTY-NOTICES.md` 及相关架构文档。

## 决策原因与技术论证

### 1. JIVE 声明式布局降低了 Inspector 的核心价值（首要原因）
- **工作流矛盾**：`melatonin_inspector` 最具吸引力的能力是对 Component 进行运行时的位置、尺寸拖拽与修改。但在 JIVE 声明式体系下，UI 的唯一真相源是 `LayoutModel.cpp` 中的 `ValueTree` 描述以及 JSON 样式表。任何在运行时被 Inspector 临时拖拽的组件坐标，在下一次 JIVE layout 计算或窗口 resize 时都会被瞬间重置覆盖；
- **唯一真相源**：devpiano 的样式微调与设计系统统一依托 `design_tokens.json` 与 `style_sheets.json`，无需也无法通过 Inspector 产生可持久化的布局产物。

### 2. 避免向核心键盘焦点系统注入非业务耦合与按键拦截
- `melatonin_inspector` 在构造时会强制执行：
  ```cpp
  root->addKeyListener(&keyListener);
  root->setWantsKeyboardFocus(true);
  ```
  并挂载内部的快捷键拦截体系（`Ctrl+I` 切换窗口、`Escape` 清除选区并消费按键事件）；
- **devpiano 是电脑键盘演奏应用**，键盘输入捕获与焦点管理（`restoreKeyboardFocus`、`grabKeyboardFocus`、`shouldTakeKeyboardFocus`）是系统最敏感的核心生命线。让一个仅用于调试的辅助工具向核心 `MainComponent` 挂载 `KeyListener` 并篡改 `Escape` 等按键语义，构成了设计层面的冗余耦合与不可控变量。

### 3. 核心交互组件均为单 Component 自绘体系，层级检查收益有限
- devpiano 的核心交互组件（如 88 键虚拟钢琴 `CustomKeyboard`、`AdsrCurveComponent`）内部并没有由 88 个独立子 Component 堆叠，而是采用单 Component 纯 Direct Paint 渲染（`paintWhiteKeys`、`paintBlackKeys`、`paintKeyLabels`、局部 `repaintKey` 脏矩形裁剪）；
- Inspector 无法穿透到自绘组件内部观察各个按键的物理状态、速度响应与几何映射；这些组件的调试完全依托于项目自主构建的结构化诊断日志（`DP_LOG_INFO`）、测试夹具与单元测试。

### 4. 精简子模块依赖拓扑与构建配置
- 将项目的外部子模块精简为 **JUCE** 与 **JIVE** 两个核心支柱，克隆与同步更轻量；
- 消除 `CMakeLists.txt` 中无条件链接 `melatonin_inspector` JUCE module 的依赖开销，杜绝非生产性代码污染。

## 影响

### 正面影响
- **代码纯粹度提升**：`MainComponent` 彻底消除 `#if DEBUG` 侵入式调试逻辑与析构时为了防止 StyleSheet 悬空引用的复杂防御 hack；
- **键盘系统更干净**：消除了 `melatonin_inspector` 全局 `KeyListener` 对应用按键事件链路的潜在干扰；
- **构建与分发更精简**：减少 1 个外部 Git 仓库依赖，`THIRD-PARTY-NOTICES.md` 更简洁，Windows 镜像同步脚本免去不必要的子模块存在性探测；
- **零功能损失**：主程序所有业务功能（物理建模发声、VST3 宿主、多轨录制回放、WAV 导出、16 通道矩阵等）100% 保持完好。

### 代价
- 失去运行时的实时 Component paint timing / histogram 浮窗视图。但该能力已被项目内置的性能日志与 `-ftime-trace` / 性能剖析工具所替代。

## 后续 UI 调试标准路径

- **布局与样式微调**：修改 `source/UI/jive/style_sheets.json` 与 `source/UI/jive/design_tokens.json`；
- **复杂自绘组件调试**：通过 `devpiano_tests` 对应的专用测试套件（如 `KeyboardHitMappingTest`、`PathEditorReproTest`、`StyleCatalogTest`）以及 `DevPianoLogger` 结构化日志进行确定性验证。
