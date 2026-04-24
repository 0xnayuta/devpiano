# Project: FreePiano-JUCE
你正在协助我将老旧的 Windows FreePiano 项目重构为基于 JUCE 框架的现代 C++ 音频应用。
开发环境：WSL2 主工作树 + Windows 镜像树 + CMake + Ninja（Windows 侧使用 Visual Studio 2026 的 MSVC 工具链做验证构建）。

## 目录结构说明：
- `/JUCE/`：JUCE 框架的 git 子模块，不要修改里面的任何代码。
- `/source/`：所有的源代码（.cpp, .h）必须存放在这个目录下。
- `/freepiano-src/`：原 FreePiano 旧版本源码，仅作为迁移参考。
  - 不参与当前 JUCE 主构建。
  - 可用于理解旧系统行为、模块关系与迁移路径。
  - 不要直接复制其中的平台相关实现到新架构；应优先提炼行为后用 JUCE 抽象重建。
- 根目录：用于存放顶层 `CMakeLists.txt`、构建脚本与工作流文档。
- WSL 主构建目录：`/root/repos/devpiano/build-wsl-clang`
- Windows 镜像目录：默认 `G:\source\projects\devpiano`（可由环境变量 `WIN_MIRROR_DIR` 覆盖）
- Windows 验证构建目录：镜像树下的 `build-win-msvc`

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
- 理解历史功能或迁移旧模块时，可阅读 `freepiano-src/`；但日常主搜索、编辑、重构、构建验证仍应以 `source/` 和 WSL 主工作树为准。
- **WSL 主工作树是唯一主源码来源**；日常编辑、grep/rg、clangd/LSP、脚本执行都应以 WSL 主树为准。
- **不要让 Windows/MSVC 直接跨边界在 WSL 主工作树上长期构建**；Windows 侧只使用镜像树做验证。
- WSL 构建与 Windows 构建必须分离：
  - WSL：`build-wsl-clang`
  - Windows：镜像树下 `build-win-msvc`
- 在关键修改后优先使用项目脚本验证，而不是 `cmake --build .`：
  - WSL 本地：`./scripts/dev.sh wsl-build`
  - 仅刷新编译数据库：`./scripts/dev.sh wsl-build --configure-only`
  - Windows MSVC 验证：`./scripts/dev.sh win-build`
- 需要快速检查环境是否满足当前工作流时，先运行：`./scripts/dev.sh self-check`

## 已安装 Pi 扩展的行动指令：

### 1. `pi-lsp`
- 把 `lsp` 作为当前 JUCE/C++ 重构任务的首选工具。
- 修改 `source/*.h`、`source/*.cpp` 后，先用 `lsp` 检查：`diagnostics`、`workspace-diagnostics`、`definition`、`references`、`symbols`、`rename`、`codeAction`。
- 做局部重构、类型排错、符号跳转、引用分析时，先用 `lsp`，不要只依赖全文搜索。
- 需要理解某个类、方法、成员的来源、类型或调用链时，先做 `lsp` 查询。
- 先看 `lsp` 结果，再决定是否运行完整编译。
- 优先让 `clangd` / 编译数据库暴露真实问题；不要在没有诊断支撑时盲改 C++ 代码。
- 当前 clangd / 编译数据库基于 WSL 构建目录：`build-wsl-clang/compile_commands.json`。
- 如编译数据库缺失或怀疑过期，先执行：`./scripts/dev.sh wsl-build --configure-only`

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
- 不要把 Windows 镜像树或 `build-win-msvc` 当作日常搜索、索引或上下文理解重点目录。
- Windows 侧验证的目的是确认 MSVC / CMake / Ninja 在镜像树上可通过，不是替代 WSL 主工作流。

## 工作流文档优先级：
- 涉及当前 WSL/Windows 混合开发、同步、MSVC 验证、环境恢复、自检命令时，优先参考：
  - `docs/development/wsl-windows-msvc-workflow.md`
  - `docs/getting-started/quickstart.md`

## 推荐工作流：
1. 先用 `understand` 理解旧逻辑、模块关系和迁移路径。
2. 再用 `lsp` 配合 `read` / `edit` 在 **WSL 主工作树**实施具体 C++ / JUCE 修改。
3. 在长时间多轮修改与多次构建修错过程中，按 context-mode 的工作方式保持状态清晰、输出精简。
4. 在关键修改后优先运行：
   - `./scripts/dev.sh wsl-build`
   - 必要时 `./scripts/dev.sh wsl-build --configure-only` 刷新 clangd / compile_commands
5. 需要只同步到 Windows 镜像树时，可运行：
   - `./scripts/dev.sh win-sync`
6. 需要做 Windows/MSVC 验证时，再运行：
   - `./scripts/dev.sh win-build`
   - 或按需使用 `--no-sync` / `--reconfigure` / `--clean-win-build`
7. 遇到环境或路径问题时，先运行：
   - `./scripts/dev.sh self-check`
8. 必要时再结合 `lsp diagnostics` 定位残余错误。
