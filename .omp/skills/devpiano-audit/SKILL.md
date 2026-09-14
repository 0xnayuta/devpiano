---
name: devpiano-audit
description: 对 devpiano 项目执行全面代码质量审计——按 docs/audit/AUDIT-000-audit-template.md 模板产出 AUDIT-XXX 报告；覆盖三闸门基线（wsl-build/test/format --check）、6 个 ADR 合规核对、10 个分领域审查（含 3.10 ADR 合规）；只读审计，发现进问题总表，修复另开迭代。
---

# devpiano 全面代码质量审计

> 触发本 skill 后：对 devpiano 项目执行一次全面、可复核的代码质量审计，按 `docs/audit/AUDIT-000-audit-template.md` 模板产出 `docs/audit/AUDIT-XXX-code-quality-audit-<日期>.md`（复制模板后填写；**第 8 章问题总表是唯一状态源**，第 0 章看板/结论/修复路线图必须与之一致）。
> **只读审计**：不改代码、不改 ADR、不改文档；发现一律进问题总表，修复另开迭代。

## 背景（必读）

- 项目定位与范围红线：`docs/reference/project-scope.md`
- 架构与技术栈：`docs/reference/architecture.md`
- 操作契约：`AGENTS.md`（§2 核心架构要求、§4 提交规范、§9 结束输出要求）
- 决策记录：`docs/decisions/` 全部 ADR（决策合规维度按模板 3.10 核对表逐条执行）
- 已知缺口：`docs/issues/known-issues.md`
- 路线图：`docs/roadmap/roadmap.md` + `docs/roadmap/current-iteration.md`

## 审计范围

- 代码：`source/**/*.cpp` `source/**/*.h`（14 个业务子模块 + `source/tests/`）
- 测试：`source/tests/`（KeyMapTypesTest、MidiFileImporterTest 等，接入 `devpiano_tests` 目标）
- 排除：`submodules/JUCE/`、`submodules/JIVE/`、`submodules/melatonin_inspector/`（禁止修改，不审）、`scripts/`、`docs/`、构建脚本与配置文件

## 执行方法（按序）

1. **三闸门基线**（记录到模板 4.1）：
   - `./scripts/dev.sh wsl-build` → Debug 构建成功（WSL 侧，同时刷新 compile_commands.json）
   - `./scripts/dev.sh test` → 全部单元测试通过
   - `./scripts/dev.sh format --check` → 0 差异
   - 可加：`./scripts/dev.sh self-check`（环境自检）、`clang-tidy -p build-wsl-clang source/**/*.cpp`（静态分析）
   - Windows/MSVC 验证 `./scripts/dev.sh win-build` 在镜像树执行；审计侧不可用时标 `未执行` 并说明
   - 任一红：先定位并如实记录（审计不修复，但基线必须真实）。
2. **ADR 合规核对**（模板 3.10 章 6 行表逐条执行，可执行检查）：
   - ADR-001：`win-build` 走 Windows 镜像树；`source/` 改动仅发生在 WSL 主工作树
   - ADR-002：`freepiano-src/` 已移除，无旧 FreePiano 源码引用残留（grep 零命中）
   - ADR-003：`PluginFlowSupport` 无成员变量，依赖经 callback 或参数显式注入
   - ADR-004：无旧 WASAPI/ASIO/DirectSound 原生后端残留（grep 零命中）
   - ADR-005：插件加载走 JUCE `AudioPluginFormatManager`，无旧 VST SDK 风格宿主代码
   - ADR-006：无外 MIDI 输入枚举/处理代码残留
3. **分领域审查**（模板第 3 章 3.1~3.10）：架构边界/代码质量/线程安全并发/安全边界/资源性能/错误处理可观测性/测试体系/文档配置契约/工程化构建/ADR 合规。每领域给评级（A~D）+ 结论 + 关联问题。
4. **验证与统计**：执行 4.1 命令表（Windows/MSVC 项标 `未执行` 并说明）；统计 4.2 文件指标（源文件数/头文件数/行数/测试用例数/最大文件）。
5. **产出问题总表**（第 8 章）：所有发现编号连续（前缀见模板 ID 表，12 个前缀含 `CMPL`），**证据必须可复核**（`文件:行` / 命令输出 / 文档引用），已关闭必须有证据。
6. **排期**：报告第 5 章修复路线图按 P0~P3 排期，覆盖第 8 章全部未处理问题；ID 覆盖率用 comm 校验（登记表 vs 路线图，零缺失）。

## 纪律（硬性）

- **只读审计**：审计阶段不改代码、不改 ADR、不改文档。修复路线图（第 5 章）只排期不实施。
- **ADR 两类处理**：ADR 事实性描述（数值/状态描述）被证伪 → 修正 ADR 原文并在问题证据列注明，不开 `CMPL`；实现违反 ADR 决策本体 → 开 `CMPL` 问题。
- **P0/P1 从严**：崩溃/数据损坏/音频毛刺无声/内存泄漏/线程安全缺陷 → P0~P1，不因"暂未触发"降级。
- **不夸大**：未实测/未验证的结论标 `[未验证]`；推断标 `[推断]`；与已知缺口（known-issues）重复的发现引用原编号，不重复开。
- **文档分工**：审计文档第 8 章只更新状态列（未处理→处理中→已关闭），不记修复详情；修复详情/历史写各迭代记录与 roadmap。
- 提交规范按 AGENTS.md §4（docs 类型，单逻辑单提交）。

## 交付

- `docs/audit/AUDIT-XXX-code-quality-audit-<日期>.md`（完整报告 0~8 章，序号接续已有报告）
- 三闸门复验结果（报告中 4.1 记录）
- 交付说明：关键发现摘要 + 待用户手动验证清单（Windows/MSVC 项）+ 建议的下一步（按路线图优先级）
