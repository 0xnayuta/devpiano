# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。  
> 读者：当前开发者、协作者、代码审查者。  
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 本轮目标

建立清晰、简要、准确、不冗杂、不重复的文档分层，并把旧文档迁移到新的主结构中。

## 本轮任务

- [x] 新增 `docs/README.md` 作为文档入口。
- [x] 保留 `quickstart-dev.md` 与 `dev-workflow-wsl-windows-msvc.md` 作为工作流文档。
- [x] 新增 `architecture.md`，只描述当前架构与模块职责。
- [x] 将 `LegacyCodeNotes.md` 改为 `legacy-migration.md`。
- [x] 新增 `roadmap.md`，作为唯一项目状态与路线图来源。
- [x] 新增 `current-iteration.md`，只记录当前任务。
- [x] 建立 `testing/` 目录。
- [x] 将键盘映射测试移动到 `testing/keyboard-mapping.md`。
- [x] 将插件宿主生命周期测试移动到 `testing/plugin-host-lifecycle.md`。
- [x] 新增 `testing/acceptance.md` 作为阶段性验收标准。
- [x] 建立 `archive/` 目录。
- [x] 将 `NinjaMigrationGuide.md` 移入 `archive/`。
- [x] 将旧规划类文档移入 `archive/`，避免与新分层重复。

## 涉及文件

新增 / 重写：

- `docs/README.md`
- `docs/architecture.md`
- `docs/roadmap.md`
- `docs/current-iteration.md`
- `docs/testing/acceptance.md`

移动 / 改名：

- `docs/LegacyCodeNotes.md` -> `docs/legacy-migration.md`
- `docs/KeyboardMappingTestCases.md` -> `docs/testing/keyboard-mapping.md`
- `docs/PluginHostLifecycleTestCases.md` -> `docs/testing/plugin-host-lifecycle.md`
- `docs/NinjaMigrationGuide.md` -> `docs/archive/NinjaMigrationGuide.md`

归档：

- `docs/OverallGoal.md`
- `docs/CurrentAssessmentAndPlan.md`
- `docs/MilestoneChecklist.md`
- `docs/NextPhaseTaskChecklist.md`

## 本轮验收标准

- [x] `docs/` 根目录下只保留当前主文档。
- [x] `roadmap.md` 是唯一项目状态与路线图来源。
- [x] `current-iteration.md` 不记录长期规划，只记录当前任务。
- [x] `architecture.md` 不混入任务清单。
- [x] `testing/` 下只放验收和专项测试。
- [x] 历史和过期内容进入 `archive/`。

## 完成记录

- 日期：2026-04-24
- 完成：已建立新的文档主结构，并完成旧文档移动 / 归档 / 精简入口。
- 未完成：尚未逐条精简已移动的专项测试文档内部重复引用；可在后续需要时继续清理。
- 风险：根目录 `README.md` / `README_en.md` 若引用旧文档路径，后续需要同步更新。
- 下一步：如继续文档整理，可检查全仓库旧路径引用并更新。
