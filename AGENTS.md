# Project: FreePiano-JUCE
你正在协助我将老旧的 Windows FreePiano 项目重构为基于 JUCE 框架的现代 C++ 音频应用。
开发环境：CMake + Ninja（使用 Visual Studio 2026 的 MSVC 工具链）。

## 目录结构说明：
- `/JUCE/`：JUCE 框架的 git 子模块，不要修改里面的任何代码。
- `/source/`：所有的源代码（.cpp, .h）必须存放在这个目录下。
- 根目录：用于存放顶层 `CMakeLists.txt` 和构建脚本。

## 核心架构要求：
1. 音频/MIDI 后端：使用 JUCE 的 `AudioDeviceManager` 替代原有的原生 WASAPI/ASIO 代码。
2. 插件宿主：使用 JUCE 的 `AudioPluginFormatManager` (支持 VST3/VST) 替代旧的 VST 加载逻辑。
3. 键盘输入：利用 JUCE 的 `KeyListener` 捕获电脑键盘事件，映射为 `MidiMessage`。
4. UI：使用 JUCE 的 `Component` 树替代原 Windows GDI/原生控件。

## 高优先级行动规则：
- 保持代码极简、现代 (C++20/23)。
- 绝对不要修改 `/JUCE/` 子模块内的代码。
- 新增和重构后的业务代码只放在 `/source/`。
- 优先小步修改、小范围验证，不要一次性大改整个系统。
- 优先使用 `read` 和 `edit` 逐步提取和迁移旧逻辑，不要一次性读取过多旧代码。
- 在关键修改后使用 `bash` 执行 `cmake --build .` 验证编译。

## 已安装 Pi 扩展的行动指令：

### 1. `pi-lsp`
- 把 `lsp` 作为当前 JUCE/C++ 重构任务的首选工具。
- 修改 `source/*.h`、`source/*.cpp` 后，先用 `lsp` 检查：`diagnostics`、`workspace-diagnostics`、`definition`、`references`、`symbols`、`rename`、`codeAction`。
- 做局部重构、类型排错、符号跳转、引用分析时，先用 `lsp`，不要只依赖全文搜索。
- 需要理解某个类、方法、成员的来源、类型或调用链时，先做 `lsp` 查询。
- 先看 `lsp` 结果，再决定是否运行完整编译。
- 优先让 `clangd` / 编译数据库暴露真实问题；不要在没有诊断支撑时盲改 C++ 代码。

### 2. `context-mode`
- 在长会话、多文件修改、多轮构建修错场景下，控制上下文体积。
- 避免一次性读入或回传大量原始输出；先筛选、总结、分批处理。
- 始终明确并持续维护：当前目标、已修改文件、待验证步骤、下一步计划。
- 需要诊断上下文或会话问题时，再建议用户使用 context-mode 的相关命令；不要假设自己一定能直接调用这些命令。

### 3. `Understand-Anything`
- 在“理解旧系统”“梳理架构”“分析模块关系”“规划迁移路径”时，优先使用 `understand` 系列技能。
- 在开始迁移旧模块前、分析旧 FreePiano 主流程前、复盘当前 JUCE 架构前，优先做一次阶段性 `understand` 分析。
- 不要在每次小改动后都调用 `understand`；只在阶段性、大粒度分析时使用。

## JUCE/C++ 项目专项指令：
- 优先保持 UI、音频/MIDI、插件宿主逻辑解耦。
- 优先把平台相关旧逻辑迁移到 JUCE 抽象之上，不要把旧 Windows 细节直接搬进新架构。
- 优先使用 JUCE 的 `AudioDeviceManager`、`AudioPluginFormatManager`、`KeyListener`、`Component` 树实现对应能力。
- 修改 UI 代码时，优先保持 `Component` 层级清晰、职责单一。
- 修改 MIDI / 音频 / 插件宿主代码时，优先保持后端逻辑独立于 UI。
- 对大范围重命名或结构调整，先做小步重构，再用 `lsp` 和编译验证。
- 如有 `compile_commands.json` 或等效编译数据库，优先利用其配合 `lsp` 定位问题。

## 推荐工作流：
1. 先用 `understand` 理解旧逻辑、模块关系和迁移路径。
2. 再用 `lsp` 配合 `read` / `edit` 实施具体 C++ / JUCE 修改。
3. 在长时间多轮修改与多次构建修错过程中，按 context-mode 的工作方式保持状态清晰、输出精简。
4. 在关键修改后运行 `cmake --build .`；必要时再结合 `lsp diagnostics` 定位残余错误。