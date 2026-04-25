# Project: FreePiano-JUCE

你正在协助将老旧的 FreePiano 项目重构为基于 JUCE 的现代 C++ 音频/MIDI 应用。

开发环境采用：**WSL 主工作树 + Windows 镜像树 + CMake + Ninja + Windows/MSVC 构建验证**。

---

## 1. 项目边界与目录职责

### 主源码与构建目录

- `/source/`
  - 当前 JUCE 主实现目录。
  - 所有新增和重构后的业务源码（`.cpp` / `.h`）必须放在这里。
- `/JUCE/`
  - JUCE 框架 git 子模块。
  - **绝对不要修改其中任何代码**。
- `/freepiano-src/`
  - 旧 FreePiano 源码，仅作为迁移参考。
  - 不参与当前 JUCE 主构建。
  - 只用于理解历史行为、默认布局、功能边界和旧模块关系。
  - 不要直接复制平台相关实现；应提炼行为后用 JUCE 抽象重建。
- `/build-wsl-clang/`
  - WSL 本地构建目录。
  - clangd / LSP 编译数据库来源：`build-wsl-clang/compile_commands.json`。
- Windows 镜像目录
  - 默认：`G:\source\projects\devpiano`
  - 可由环境变量 `WIN_MIRROR_DIR` 覆盖。
- Windows 验证构建目录
  - 镜像树下：`build-win-msvc`。

### 文档目录

当前 docs 结构已重组，阅读和维护时按以下职责区分：

- `docs/README.md`：文档总入口。
- `docs/index/doc-map.md`：按读者目标组织的文档地图。
- `docs/getting-started/quickstart.md`：快速恢复环境与常用命令。
- `docs/development/wsl-windows-msvc-workflow.md`：WSL / Windows 镜像 / MSVC 验证详细工作流。
- `docs/architecture/overview.md`：当前系统架构、模块职责与主要链路。
- `docs/architecture/legacy-migration.md`：旧 FreePiano 源码迁移边界与新旧模块映射。
- `docs/features/keyboard-mapping.md`：键盘映射功能说明。
- `docs/features/plugin-hosting.md`：插件宿主功能说明。
- `docs/testing/acceptance.md`：阶段验收标准。
- `docs/testing/keyboard-mapping.md`：键盘映射专项测试。
- `docs/testing/plugin-host-lifecycle.md`：插件宿主生命周期专项测试。
- `docs/decisions/`：ADR，记录已确定的架构/工程决策。
- `docs/roadmap/roadmap.md`：唯一项目状态、阶段路线与近期重点来源。
- `docs/roadmap/current-iteration.md`：当前迭代入口。
- `docs/archive/`：历史资料，内容只作参考，当前信息以现行文档为准。

---

## 2. 核心架构要求

1. 音频 / MIDI 后端：使用 JUCE `AudioDeviceManager` 替代旧原生 WASAPI / ASIO / DirectSound 路径。
2. 插件宿主：使用 JUCE `AudioPluginFormatManager` / `AudioPluginInstance` / VST3 主路径替代旧 VST 加载逻辑。
3. 键盘输入：使用 JUCE `KeyListener` / `KeyPress` 捕获电脑键盘事件，并映射为 `MidiMessage`。
4. UI：使用 JUCE `Component` 树替代旧 Windows GDI / 原生控件。
5. 配置与状态：优先使用 JUCE `ApplicationProperties` / `ValueTree` 与项目内状态模型。
6. 录制 / 回放 / 导出等高级功能应先定义现代数据模型，不直接继承旧 `song.*` 内部表示。

---

## 3. 高优先级行动规则

- 保持代码极简、现代、可维护，使用 C++20/23 风格。
- **不要修改 `/JUCE/` 子模块**。
- **新增业务代码只放在 `/source/` 下的合适子目录**。
- 优先小步修改、小范围验证，不要一次性大改整个系统。
- 理解旧逻辑时可以阅读 `/freepiano-src/`，但日常搜索、编辑、重构都以 WSL 主工作树和 `/source/` 为准。
- **WSL 主工作树是唯一主源码来源，仅用于编辑代码和刷新 `compile_commands.json`**，所有构建验证和软件测试在 Windows 侧进行。
- **不要让 Windows/MSVC 直接跨边界在 WSL 主工作树上长期构建**；Windows 只使用镜像树做验证。
- WSL 构建和 Windows 构建必须分离：
  - WSL（仅用于编辑代码 + 刷新 compile_commands.json）：`build-wsl-clang`
  - Windows（构建验证 + 软件测试）：镜像树下 `build-win-msvc`
- 关键修改后优先使用项目脚本验证，而不是直接手写 `cmake --build .`。
- 需要快速检查环境时，先运行：

```bash
./scripts/dev.sh self-check
```

---

## 4. 推荐开发命令

```bash
# 环境自检
./scripts/dev.sh self-check

# 仅刷新 WSL configure / compile_commands.json
./scripts/dev.sh wsl-build --configure-only

# 同步到 Windows 镜像树
./scripts/dev.sh win-sync

# Windows MSVC 验证构建
./scripts/dev.sh win-build
```

常用 Windows 验证选项：

```bash
./scripts/dev.sh win-build --no-sync
./scripts/dev.sh win-build --reconfigure
./scripts/dev.sh win-build --clean-win-build
```

涉及环境恢复、同步、MSVC 验证、路径问题时，优先参考：

- `docs/getting-started/quickstart.md`
- `docs/development/wsl-windows-msvc-workflow.md`
- ADR：`docs/decisions/0001-wsl-primary-windows-mirror-workflow.md`

---

## 5. 文档维护规则

- 项目状态和长期路线只写入：`docs/roadmap/roadmap.md`。
- 当前任务只写入：`docs/roadmap/current-iteration.md`。
- 架构说明写入：`docs/architecture/`。
- 功能行为说明写入：`docs/features/`。
- 测试、验收、手工回归写入：`docs/testing/`。
- 已确定的重要工程/架构决策写入：`docs/decisions/`。
- 旧文档和过期规划进入：`docs/archive/`。
- 不要在多个文档中重复维护同一份“当前状态”；以 `docs/roadmap/roadmap.md` 为准。
- 不要把未实现能力写成已实现能力。
- 移动或重命名文档后，检查 README、doc-map、相关引用和 archive 替代关系。

---

## 6. 旧代码迁移规则

旧源码目录：`/freepiano-src/`。

允许：

- 阅读旧代码以理解历史行为。
- 提取业务规则、默认键位、消息语义和功能边界。
- 对照旧模块验证新架构是否覆盖关键行为。

禁止或避免：

- 直接复制旧 Windows 平台实现。
- 重新引入以 `windows.h` 为中心的依赖链作为核心架构。
- 原样复刻旧 WASAPI / ASIO / DSound 后端。
- 原样复刻旧 VST SDK 风格宿主。
- 原样复刻旧 GDI GUI。
- 延续旧宏式、全局函数式设计。

迁移原则：**旧代码用于提炼行为，不用于直接复刻实现。**

参考：

- `docs/architecture/legacy-migration.md`
- ADR：`docs/decisions/0002-legacy-code-as-reference-only.md`

---

## 7. JUCE/C++ 专项指令

- 优先保持 UI、音频/MIDI、插件宿主、设置/状态模型解耦。
- 修改 UI 时，保持 `Component` 层级清晰、职责单一。
- 修改 MIDI / 音频 / 插件宿主时，保持后端逻辑独立于 UI。
- 新状态优先通过 `AppState` / builder / UI 子组件边界表达，避免 `MainComponent` 再次膨胀。
- 插件相关改动重点关注：scan / load / unload / editor / audio device rebuild / exit 生命周期组合。
- 键盘相关改动重点关注：note on/off 成对、长按、连按、焦点切换、输入法、修饰键。
- 对大范围重命名或结构调整，先小步重构，再使用 LSP 和构建验证。

相关功能与测试文档：

- 键盘映射功能：`docs/features/keyboard-mapping.md`
- 键盘映射测试：`docs/testing/keyboard-mapping.md`
- 插件宿主功能：`docs/features/plugin-hosting.md`
- 插件宿主生命周期测试：`docs/testing/plugin-host-lifecycle.md`
- 阶段验收：`docs/testing/acceptance.md`

---

## 8. 已安装 Pi 扩展的行动指令

### 8.1 `pi-lsp`

- 把 `lsp` 作为当前 JUCE/C++ 重构任务的首选工具。
- 修改 `source/*.h`、`source/*.cpp` 后，先用 `lsp` 检查 diagnostics / workspace-diagnostics。
- 做局部重构、类型排错、符号跳转、引用分析时，先用 `lsp`，不要只依赖全文搜索。
- 需要理解某个类、方法、成员的来源、类型或调用链时，先做 `lsp` 查询。
- 先看 `lsp` 结果，再决定是否运行完整编译。
- 当前 clangd / 编译数据库基于：`build-wsl-clang/compile_commands.json`。
- 如编译数据库缺失或怀疑过期，先执行：

```bash
./scripts/dev.sh wsl-build --configure-only
```

### 8.2 `Understand-Anything`

- 在“理解旧系统”“梳理架构”“分析模块关系”“规划迁移路径”时，优先使用 `understand` 系列技能。
- 在开始迁移旧模块前、分析旧 FreePiano 主流程前、复盘当前 JUCE 架构前，优先做阶段性 `understand` 分析。
- 不要在每次小改动后都调用 `understand`；只在阶段性、大粒度分析时使用。

---

## 9. 推荐工作流

1. 明确任务目标和涉及范围。
2. 如涉及旧系统行为或迁移路径，先阅读：
   - `docs/architecture/legacy-migration.md`
   - 必要时阅读 `/freepiano-src/` 相关旧模块。
3. 如涉及当前架构或功能边界，先阅读：
   - `docs/architecture/overview.md`
   - `docs/features/keyboard-mapping.md`
   - `docs/features/plugin-hosting.md`
4. 使用 `lsp` + `read` / `edit` 在 WSL 主工作树中小步修改。
5. 修改 `source/*.h` / `source/*.cpp` 后，先用 LSP diagnostics 检查。
6. 需要刷新 clangd 编译数据库时运行：

```bash
./scripts/dev.sh wsl-build --configure-only
```

7. 同步到 Windows 镜像树：

```bash
./scripts/dev.sh win-sync
```

8. Windows MSVC 验证构建：

```bash
./scripts/dev.sh win-build
```

9. 涉及环境或路径问题时，先运行：

```bash
./scripts/dev.sh self-check
```
