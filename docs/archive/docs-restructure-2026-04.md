# devpiano docs 最小重组记录（2026-04）

> 用途：归档 2026-04 文档目录最小重组这一轮已完成任务。  
> 状态：已完成。  
> 当前迭代入口：`../roadmap/current-iteration.md`。

## 本轮目标

在不重写大量内容的前提下，将现有 `docs/` 文档按新的顶层目录职责完成最小重组。

## 本轮任务

- [x] 建立 `index/`、`getting-started/`、`development/`、`architecture/`、`features/`、`decisions/`、`roadmap/` 目录。
- [x] 将当前架构文档移动到 `architecture/overview.md`。
- [x] 将旧代码迁移说明移动到 `architecture/legacy-migration.md`。
- [x] 将快速开始文档移动到 `getting-started/quickstart.md`。
- [x] 将 WSL / Windows / MSVC 工作流文档移动到 `development/wsl-windows-msvc-workflow.md`。
- [x] 将路线图移动到 `roadmap/roadmap.md`。
- [x] 将当前迭代文档移动到 `roadmap/current-iteration.md`。
- [x] 为主要目录补充轻量 README。
- [x] 新增 `index/doc-map.md` 作为文档地图。
- [x] 保留 `testing/` 下现有验收与专项测试文档。
- [x] 保留 `archive/` 下历史文档，并新增替代关系说明。
- [x] 更新主文档入口与主要路径引用。

## 本轮不做

- [ ] 不新增大量空的功能说明文档。
- [ ] 不把未实现功能写成已完成状态。
- [ ] 不拆分现有较长但职责清晰的专项测试文档。
- [ ] 不重写归档文档正文。

## 本轮验收标准

- [x] `docs/README.md` 能说明新的目录职责和推荐阅读顺序。
- [x] 各主要目录有轻量 README 入口。
- [x] 现行主文档已经移动到目标目录。
- [x] `archive/README.md` 标明旧文档被哪些新文档替代。
- [x] 根 README / README_en / AGENTS 中的主要文档路径已更新。

## 后续建议

- 从 `roadmap/roadmap.md`、`testing/keyboard-mapping.md`、`testing/plugin-host-lifecycle.md` 中逐步提炼 `features/keyboard-mapping.md` 与 `features/plugin-hosting.md`。
- 从现有工作流和迁移边界中提炼 ADR：WSL 主工作树决策、旧代码只作参考、JUCE 音频后端、VST3-first 插件宿主。
- 后续如架构文档继续增长，再拆分音频链路、插件宿主、键盘映射、状态模型等专题。
