# Project: devpiano

你正在协助开发 **devpiano**——一款以 JUCE 为框架、内置 7 大声学系统全物理建模钢琴并支持 VST3 插件的现代电脑键盘钢琴应用，聚焦软件键盘演奏与 MIDI 文件处理。

项目定位、核心能力与明确非目标详见 [`docs/reference/project-scope.md`](docs/reference/project-scope.md)。

开发环境采用：**WSL 主工作树 + Windows 镜像树 + CMake + Ninja + Windows/MSVC 构建验证**。

---

## 1. 项目边界与目录职责

### 主源码与构建目录

- `/source/`
  - 当前 JUCE 主实现目录。
  - 所有新增和重构后的业务源码（`.cpp` / `.h`）必须放在这里。
- `/submodules/JUCE/`
  - JUCE 框架 git 子模块。
  - **绝对不要修改其中任何代码**。
- `/submodules/JIVE/`
  - JIVE 声明式 UI 框架 git 子模块（MIT）。
  - 提供 `juce::ValueTree` 布局引擎 + JSON 样式表 + Flex/Grid 自适应。
  - **不要修改其中任何代码**。
- `/submodules/melatonin_inspector/`
  - melatonin_inspector 运行时 Component 检查器 git 子模块（MIT）。
  - 提供可视化组件检查、位置拖动、实时改色、FPS 仪表。
  - **不要修改其中任何代码**。
- `/build-wsl-clang/`
  - WSL 本地 Debug 构建目录。
  - clangd / LSP 编译数据库来源：`build-wsl-clang/compile_commands.json`。
- `/build-wsl-clang-release/`
  - WSL 本地 Release 构建目录。
  - **默认不使用**，仅在用户明确要求时才进行 Release 构建。
- Windows 镜像目录
  - 默认：`G:\source\projects\devpiano`
  - 可由环境变量 `WIN_MIRROR_DIR` 覆盖。
- Windows 验证构建目录
  - Debug：镜像树下 `build-win-msvc`（默认）。
  - Release：镜像树下 `build-win-msvc-release`（**默认不使用**，仅在用户明确要求时才进行 Release 构建）。

### 文档目录

当前 docs 结构已重组，阅读和维护时按以下职责区分：

- [`docs/README.md`](docs/README.md)：文档总入口，按读者角色组织推荐阅读顺序。
- [`docs/guides/quickstart.md`](docs/guides/quickstart.md)：快速恢复环境与常用命令。
- [`docs/guides/wsl-windows-msvc-workflow.md`](docs/guides/wsl-windows-msvc-workflow.md)：WSL / Windows 镜像 / MSVC 验证详细工作流。
- [`docs/guides/development.md`](docs/guides/development.md)：日常开发与构建细节。
- [`docs/guides/troubleshooting.md`](docs/guides/troubleshooting.md)：常见问题排查。
- [`docs/reference/project-scope.md`](docs/reference/project-scope.md)：项目定位、核心能力与明确非目标。
- [`docs/reference/architecture.md`](docs/reference/architecture.md)：当前系统架构、模块职责与主要链路。
- [`docs/reference/acceptance.md`](docs/reference/acceptance.md)：阶段验收标准。
- [`docs/reference/features/`](docs/reference/features/)：功能行为说明与专项手工测试，涵盖键盘映射、插件宿主、录制回放、MIDI 导入、Performance Preset、离线渲染等。
- [`docs/issues/known-issues.md`](docs/issues/known-issues.md)：已知问题、已修复风险的回归项与长期设计约束。
- [`docs/decisions/`](docs/decisions/README.md)：ADR，记录已确定的架构/工程决策。
- [`docs/roadmap/roadmap.md`](docs/roadmap/roadmap.md)：唯一项目状态、阶段路线与近期重点来源。
- [`docs/roadmap/current-iteration.md`](docs/roadmap/current-iteration.md)：当前迭代入口。
- [`docs/archive/`](docs/archive/README.md)：历史资料与已完成计划归档，内容只作参考，当前信息以现行文档为准。

---

## 2. 核心架构要求

1. 音频 / MIDI 后端：使用 JUCE `AudioDeviceManager` 替代旧原生 WASAPI / ASIO / DirectSound 路径。
2. 插件宿主：使用 JUCE `AudioPluginFormatManager` / `AudioPluginInstance` / VST3 主路径替代旧 VST 加载逻辑。
3. 内置物理建模音源：使用自主研发、纯 C++ 算法驱动、覆盖 7 大声学子系统（Hammer, String, Bridge, Soundboard, Cabinet, Air, Room）的**全物理建模钢琴（`PianoSynthVoice`）**作为默认发声来源，支持与正弦波（`SineSynthVoice`）平滑切换。
4. 键盘输入：使用 JUCE `KeyListener` / `KeyPress` 捕获电脑键盘事件，基于稳定 key code 映射为 `MidiMessage`。
5. UI 与样式：使用 **JIVE 声明式 UI 框架**（`juce::ValueTree` + JSON 样式表 + Flex/Grid 自适应布局）替代传统手工坐标排版。
6. 配置与状态：优先使用 JUCE `ApplicationProperties` / `ValueTree` 与项目内状态模型。
7. 录制 / 回放 / 导出等高级功能应先定义现代数据模型，不直接继承旧 `song.*` 内部表示。
8. 国际化：使用 JUCE `Translation` / `LocalisedStrings` 机制管理运行时语言切换，中文 locale 表作为编译期常量嵌入。

---

## 3. 高优先级行动规则

- 保持代码极简、现代、可维护，使用 C++20/23 风格。
- **不要修改 `/submodules/` 下任何子模块代码**。
- **新增业务代码只放在 `/source/` 下的合适子目录**。
- **代码探索与依赖影响面首选 codegraph**：在探索模块架构、追溯调用链（calls/callers）、类型派生或修改前评估影响面（Blast Radius）时，**必须首先调用 `codegraph` (`xd://mcp__codegraph_explore`)**，禁止未调用 codegraph 就发起盲目的多轮 `grep`/`read`。
- **第三方库与框架 API 查证首选 context7**：涉及 JUCE 8、Steinberg VST3 SDK、JIVE 等第三方库的新增调用或参数不确定时，**必须首先通过 `context7` (`resolve-library-id` + `query-docs`) 查询官方最新文档与示例**，禁止凭记忆猜测 API。
- 优先小步修改、小范围验证，不要一次性大改整个系统。
- **WSL 主工作树是唯一主源码来源，仅用于编辑代码和刷新 `compile_commands.json`**，所有构建验证和软件测试在 Windows 侧进行。
- **不要让 Windows/MSVC 直接跨边界在 WSL 主工作树上长期构建**；Windows 只使用镜像树做验证。
- WSL 构建和 Windows 构建必须分离：
  - WSL（仅用于编辑代码 + 刷新 compile_commands.json）：`build-wsl-clang`
  - Windows（构建验证 + 软件测试）：镜像树下 `build-win-msvc`
- **默认仅使用 Debug 构建**。Release 构建（`--release`）仅在用户明确要求时才执行，或由用户手动进行。不要主动发起 Release 构建。
- 关键修改后优先使用项目脚本验证，而不是直接手写 `cmake --build .`。
- **clang-tidy 只在迭代边界执行全量检查（`./scripts/dev.sh tidy --all`），禁止提交前逐文件运行**——诊断由 clangd 波浪线实时提示（`.clangd` 已启用），全量在迭代边界兜底。例外：修改 `.clang-tidy` / `.clang-format` 配置后可用单文件快速验证配置效果。
- 需要快速检查环境时，先运行：

```bash
./scripts/dev.sh self-check
```

---

## 4. 提交信息规范

### 格式

```
<type>: <short description>

<optional body — full sentences, explain what and why>
```

### 类型（`<type>`）

| 类型 | 用途 | 示例 |
|------|------|------|
| `feat:` | 新功能 | `feat: wire language switching through MainComponent` |
| `fix:` | Bug 修复 | `fix: resolve window foreground, keyboard focus, and virtual-keyboard playback issues` |
| `refactor:` | 代码重构（无行为变化） | `refactor: migrate diagnostics logging to juce::Logger with DevPianoLogger subclass` |
| `docs:` | 文档变更 | `docs: align known-issues §1 with current code state` |
| `chore:` | 构建/工具/依赖维护 | `chore: update JUCE submodule to develop tip` |
| `test:` | 测试新增或修改 | `test: add first batch of unit tests for KeyMapTypes and MidiFileImporter` |
| `style:` | 代码格式（clang-format 等） | `style: apply clang-format to all source code` |

### 规则

- 短描述使用**祈使语气、英文、小写开头、句尾无句号**。
- 单行不超过 72 字符；需换行时换行前不要有标点。
- 短描述本身应能概括改动。需要更多上下文时，空一行后写 body。
- Body 使用完整英文句子，说明**做了什么**和**为什么**，而非重复代码。
- 多处修改用 `- ` 无序列表分行列出。
- 一个提交只做一件事：不要将相互无关的改动混入同一个提交。

### 禁止

- `update file` / `fix bug` / `misc changes` — 无信息量。
- 中文短描述（少数重构类可以用英文名词，但优先英文）。
- 超过 72 字符的 subject。
- 一个提交同时包含 feat + fix + docs + refactor。

### 参考

本规范遵循 [Conventional Commits](https://www.conventionalcommits.org/) 的子集，未严格引入 `scope` 和 `!` 后缀等特性。

---

```bash
# 环境自检
./scripts/dev.sh self-check

# 格式化 source/ 下所有 .cpp/.h
./scripts/dev.sh format

# 检查格式合规（CI 模式）
./scripts/dev.sh format --check

# 增量静态检查（clang-tidy，仅配置变更验证/例外场景使用；日常提交前不做）
./scripts/dev.sh tidy

# 全量静态检查（迭代边界门禁，约 19 分钟；唯一例行 clang-tidy 执行点）
./scripts/dev.sh tidy --all

# 仅刷新 WSL configure / compile_commands.json（clangd/LSP 依赖此文件）
./scripts/dev.sh wsl-build --configure-only

# 运行单元测试（配置 BUILD_TESTS=ON → 构建 → 执行）
./scripts/dev.sh test

# Windows MSVC 验证构建（内置同步，一般不需要单独 win-sync）
./scripts/dev.sh win-build


# 编译耗时性能剖析与火焰图分析 (-ftime-trace，分析最耗时文件/头文件/模板)
./scripts/dev.sh time-trace
# 正式发布打包（Windows x64 zip + sha256，完成 win-build --release 后执行）
./scripts/dev.sh package
```

> **注意**：以上命令默认使用 Debug 构建。如需 Release 构建，用户会明确指示或自行执行 `--release` 参数，不要主动使用。

涉及环境恢复、同步、MSVC 验证、路径问题时，优先参考：

- [`docs/guides/quickstart.md`](docs/guides/quickstart.md)
- [`docs/guides/wsl-windows-msvc-workflow.md`](docs/guides/wsl-windows-msvc-workflow.md)
- ADR：[`docs/decisions/ADR-001-wsl-primary-windows-mirror-workflow.md`](docs/decisions/ADR-001-wsl-primary-windows-mirror-workflow.md)

---

## 5. 文档维护规则

- 项目状态和长期路线只写入：[`docs/roadmap/roadmap.md`](docs/roadmap/roadmap.md)。
- 项目定位、核心能力与非目标写入：[`docs/reference/project-scope.md`](docs/reference/project-scope.md)。
- 当前任务只写入：[`docs/roadmap/current-iteration.md`](docs/roadmap/current-iteration.md)。
- 架构说明写入：[`docs/reference/architecture.md`](docs/reference/architecture.md)。
- 功能行为说明写入：[`docs/reference/features/`](docs/reference/features/)。
- 测试、验收、手工回归写入：[`docs/reference/features/`](docs/reference/features/)（与功能说明合并）。
- 已确定的重要工程/架构决策写入：[`docs/decisions/`](docs/decisions/README.md)。
- 旧文档和过期规划进入：[`docs/archive/`](docs/archive/README.md)。
- 不要在多个文档中重复维护同一份“当前状态”；以 [`docs/roadmap/roadmap.md`](docs/roadmap/roadmap.md) 为准。
- 不要把未实现能力写成已实现能力。
- 移动或重命名文档后，检查 README、相关引用和 archive 替代关系。

---

## 6. JUCE/C++ 专项指令

- 优先保持 UI、音频/MIDI、插件宿主、设置/状态模型解耦。
- 修改 UI 时，保持 `Component` 层级清晰、职责单一。
- 修改 MIDI / 音频 / 插件宿主时，保持后端逻辑独立于 UI。
- 新状态优先通过 `AppState` / builder / UI 子组件边界表达，避免 `MainComponent` 再次膨胀。
- 插件相关改动重点关注：scan / load / unload / editor / audio device rebuild / exit 生命周期组合。
- 键盘相关改动重点关注：note on/off 成对、长按、连按、焦点切换、输入法、修饰键。
- 对大范围重命名或结构调整，先小步重构，再使用 LSP 和构建验证。
- 写 JUCE/第三方库代码时，**不确定的 API 一律不凭记忆**：
  - 第一优先：用 `context7` 检索官方 API 规范与最新切片（消灭 API 幻觉与弃用方法误用）。
  - 第二优先：查阅 `submodules/JUCE/` 本地源码确认实际使用的版本细节与准确签名（本项目构建以本地子模块为准）。
  - 编译错误提示 API 不存在或签名不符时，先回本地源码确认，再修，禁止猜测替代 API。
- 对稳定常用 API（Component、Button、Slider、ValueTree 等）不要求每次查证，避免无谓查询。
- 版本敏感领域（音频设备、插件宿主、线程安全、新模块）优先查证：这些 API 跨版本变动大。

---

## 7. 相关功能与测试文档：
- 物理建模钢琴音源功能及测试：[`docs/reference/features/builtin-piano-synthesis.md`](docs/reference/features/builtin-piano-synthesis.md)
- 键盘映射功能及测试：[`docs/reference/features/keyboard-mapping.md`](docs/reference/features/keyboard-mapping.md)
- 插件宿主功能及生命周期测试：[`docs/reference/features/plugin-hosting.md`](docs/reference/features/plugin-hosting.md)
- 16 通道 MIDI 矩阵及调号系统：[`docs/reference/features/midi-channel-matrix.md`](docs/reference/features/midi-channel-matrix.md)
- 逐键个性化定制与绑定编辑：[`docs/reference/features/per-key-customization.md`](docs/reference/features/per-key-customization.md)
- 声明式 UI 架构与设计系统：[`docs/reference/features/declarative-ui-and-theming.md`](docs/reference/features/declarative-ui-and-theming.md)
- 运行时多语言国际化：[`docs/reference/features/internationalization.md`](docs/reference/features/internationalization.md)
- MIDI 文件导入功能及测试：[`docs/reference/features/midi-file-import.md`](docs/reference/features/midi-file-import.md)
- 录制回放与 MIDI 导出：[`docs/reference/features/recording-playback.md`](docs/reference/features/recording-playback.md)
- Performance Preset 功能与测试：[`docs/reference/features/performance-presets.md`](docs/reference/features/performance-presets.md)
- 性能持久化：[`docs/reference/features/performance-persistence.md`](docs/reference/features/performance-persistence.md)
- 插件离线渲染：[`docs/reference/features/plugin-offline-rendering.md`](docs/reference/features/plugin-offline-rendering.md)
- 测试夹具清单：[`docs/reference/features/fixture-inventory.md`](docs/reference/features/fixture-inventory.md)
- 阶段验收标准：[`docs/reference/acceptance.md`](docs/reference/acceptance.md)
- 已知问题与回归线索：[`docs/issues/known-issues.md`](docs/issues/known-issues.md)

---

## 8. 推荐工作流与工具决策

### 8.1 推荐开发工作流

1. **明确目标与边界**：结合任务目标，必要时先阅读 `docs/reference/` 下相关架构与功能设计文档。
2. **代码探索与影响面分析 (codegraph)**：使用 `codegraph` 检索相关符号、调用链与 Blast Radius，明确改动波及范围，禁止盲目多轮 grep。
3. **API 规范查证 (context7)**：涉及 JUCE / 第三方库机制与参数时，通过 `context7` 查证官方 API 与用法示例。
4. **精确修改与符号协同 (lsp + edit)**：使用 `lsp`（跳转定义、引用查找、重命名、代码诊断）+ `edit` 在 WSL 主工作树中小步修改。
5. **代码风格与静态检查**：修改后先用 LSP diagnostics 检查，运行 `./scripts/dev.sh format` 保证风格一致。
6. **刷新编译数据库（必要时）**：如增删源文件或变更头文件依赖，运行 `./scripts/dev.sh wsl-build --configure-only`。
7. **回归测试与 Windows 构建验证**：
   - 运行单元测试：`./scripts/dev.sh test`
   - Windows MSVC 验证构建：`./scripts/dev.sh win-build`
8. **环境异常自检**：涉及环境或路径问题时，先运行 `./scripts/dev.sh self-check`。

### 8.2 开发工具决策矩阵

| 开发阶段 / 动作 | 唯一首选工具 | 辅助 / 确认工具 | 严格禁止的行为 |
|---|---|---|---|
| **探索模块架构 / 追溯调用流 / 修改前评估影响面 (Blast Radius)** | `codegraph` | `lsp references` | 盲目发起多轮 grep + 逐个 read 代码文件 |
| **查询 JUCE / VST3 / 第三方库 API 签名与用法** | `context7` | `submodules/` 本地源码 | 凭记忆猜测或臆造 API |
| **符号重命名 / 精确跳转定义 / 跨文件引用 / 代码诊断** | `lsp` | `codegraph` | 手写正则/sed 批量替换跨文件符号 |
| **阅读非代码文件（.md, .json, .cmake）或精确局部编辑** | `read` / `edit` | `write` | 用 read 遍历大量代码文件寻找符号 |
---

## 9. 结束输出要求

每轮结束时必须给出：下一轮建议做什么，哪个或哪些是你最推荐的
