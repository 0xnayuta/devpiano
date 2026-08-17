# ADR 007: clang-tidy 只做检查，格式化与机械修复交给 clang-format

## 状态

已采用。

## 背景

2026-08-16 执行 clang-tidy 全量归零（AUDIT-001 ENG-002，约 800 条诊断 → 0）期间，实测 `clang-tidy --fix` 在本项目代码上**破坏源码**：

- `source/Recording/PerformanceFile.cpp`、`source/Recording/RecordingEngine.cpp` 出现标识符截断与 token 交错插入（`realtimeMidiBuffe } r;`、`sourcePlayb {ack`）。
- 4 个头文件（`ExportFlowSupport.h`、`WavExportTask.h`、`PluginOfflineRenderer.h`、`WavFileExporter.h`）被连带改写为 `const const juce&::File&`、`std::function<bool(do&uble)>` 等损坏形式。

根因分析（结合 Clang 底层模型）：

- Clang 工具链（`tooling::Replacement` / `SourceManager` / `clang-apply-replacements`）内部以 **UTF-8 字节偏移**为规范契约，YAML 偏移数值大于可视字符数是正常行为。
- 破坏发生于**特定 checker 的 `--fix` 路径**：`readability-inconsistent-declaration-parameter-name` / `readability-named-parameter` 在**无参数名声明**（`Type,`）上依赖 `Lexer::getLocForEndOfToken` 寻找插入锚点，对无名参数 token 范围的边界计算存在缺陷；多个相邻参数的同时 Replacement 还会发生 **Fix-It 局部碰撞**。
- 项目代码含大量中文注释（多字节）与 JUCE 虚函数 override（无参数名），触发面广；破坏并非简单的"UTF-8 线性偏移漂移"（那是显示层问题），而是 checker 锚点缺陷 + 多 Fix 碰撞的复合结果。

## 决策

**静态检查工具链职责分离**：

1. `clang-tidy` 只做**检查**（纯 AST 静态规则分析），**禁止使用 `--fix` / `--fix-notes` 自动应用修复**。
2. **机械格式化与风格修复交给 `clang-format`**（对 token 流做字符级处理，UTF-8 安全）与**人工逐项修改**。
3. `.clang-format` 启用 `InsertBraces: true`——单行 `if/for/while` 的 braces 由格式化保证（与 clang-tidy `readability-braces-around-statements` 检查一致），不再依赖任何自动修复器。
4. `.clang-tidy` 配置约束：
   - 禁用 `-readability-named-parameter`、`-readability-inconsistent-declaration-parameter-name`：JUCE 虚函数 override 参数名由基类契约决定，强制命名对音频框架项目是噪音，且其 `--fix` 会改写头文件。
   - 禁用 4 类**存量风格噪音**：`-modernize-use-designated-initializers`、`-readability-math-missing-parentheses`、`-readability-redundant-inline-specifier`、`-bugprone-easily-swappable-parameters`——一次性清理成本高于价值，保留会淹没 `bugprone-*` / `clang-analyzer-*` 的真实信号。
   - `HeaderFilterRegex: '^.*/source/.*'`：只报告项目自身头文件诊断，排除 JUCE / 第三方库头。
   - `Checks` 使用 YAML 折叠标量（`>`）时**块内禁止写 `#` 注释**——折叠标量中 `#` 会拼入检查字符串，且会把其后跟随的禁用项整段吞掉（实测 `-modernize-use-designated-initializers` 失效、`performance-enum-size` 被意外启用）。说明文字一律放块外 YAML 注释。

## 原因

- **代码安全**：`--fix` 的锚点缺陷在多字节注释/无参数名场景下必然破坏源码；恢复被破坏文件 + 排查连带损坏的成本远高于手动修复本身。
- **工具职责天然分工**：clang-format 是字符级格式化器（token 流安全），clang-tidy 是 AST 级分析器（诊断准确但修复锚点脆弱）；把"改代码"交给 format、"发现问题"交给 tidy 各取所长。
- **门禁可执行**：`format --check` + clang-tidy 全量 0 诊断都可作为确定性门禁；`--fix` 的非确定性破坏行为无法作为 CI 步骤。
- **检查价值保留**：禁用的是"建议类"噪音（风格偏好）与 JUCE 契约噪音，`bugprone-*` / `clang-analyzer-*` / `performance-*` 等正确性检查全部保留——新代码仍受约束。

## 影响

正面影响：

- 全量 clang-tidy 44 文件 0 诊断，信号纯净（不再被 614 条 braces / 185 条 named-parameter 淹没）。
- `format --check` 归零 + `InsertBraces` 保证 braces 风格，与 clang-tidy 检查一致。
- `--fix` 破坏风险从工具链层面根除。

代价：

- 机械修复需人工/脚本完成（braces 614 条经 format 一次性解决；designated-init 等 4 类检查禁用后不再提示，依赖 code review 保持风格）。
- 禁用 `-bugprone-easily-swappable-parameters` 后，未来新增可互换参数不再被自动提示（调用点多为命名实参，风险低，接受）。
- 新枚举需显式选择底类型（`performance-enum-size` 保留，但靠人工加 `: std::uint8_t`）。

## 当前实践

- `.clang-format`：`InsertBraces: true` 已启用。
- `.clang-tidy`：见上文决策第 4 条；`modernize-use-using.IgnoreExternC: 'true'` 保留。
- **执行时机**：编辑期 clangd 波浪线实时提示（`.clangd` 已启用 `Diagnostics.ClangTidy`，与全量同源配置）；大迭代边界跑全量 `./scripts/dev.sh tidy --all`（约 19 分钟，唯一例行执行点）；**不进入 pre-commit**——实测单文件 18–211s、全量 44 文件约 19 分钟，提交前逐文件运行成本不成比例（纪律见 [`../../AGENTS.md`](../../AGENTS.md) 第 3 节）。
- 验证基线（2026-08-16）：全量 44 文件 clang-tidy 0 诊断；`wsl-build` 0 warning（项目代码）；`devpiano_tests` 33 类全绿；`format --check` 归零；`win-build` 通过。
- 归零过程中的完整分析与逐项修复记录见 [`../audit/AUDIT-001-code-quality-audit-2026-08-16.md`](../audit/AUDIT-001-code-quality-audit-2026-08-16.md) §7 复审 5。
