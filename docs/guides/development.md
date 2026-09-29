# Development

> 用途：说明开发环境、构建、同步、日常工作流与协作规则相关文档的组织。
> 更新时机：开发相关文档新增、移动或职责变化时。

本目录放置开发环境、构建、同步、日常工作流与协作规则相关文档。

当前已有文档：

- [`wsl-windows-msvc-workflow.md`](wsl-windows-msvc-workflow.md)：WSL 主工作树 + Windows 镜像树 + MSVC 验证工作流。
- [`release-workflow.md`](release-workflow.md)：Windows x64 与 Linux x64 双平台 release、tag 与打包工作流 checklist。
- [`troubleshooting.md`](troubleshooting.md)：WSL 构建、Windows 镜像同步、MSVC 验证构建的常见问题排查，已覆盖 `.vs` 被误删、SQLite WAL 文件、`--check` 预览模式等问题。
- [`pr-agent.md`](pr-agent.md)：PR-Agent AI 代码审查工作流（部署配置、触发事件、中文响应与排查指南）。
- [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml)：GitHub Actions CI 质量门禁流水线（自动运行代码格式检查、Linux Clang 单元测试与 Windows MSVC 构建门禁）。
- [`.github/workflows/release.yml`](../../.github/workflows/release.yml)：GitHub Actions 正式发布流水线（Tag 触发自动化 Windows x64 与 Linux x64 双平台 Release 编译、分发包与 SHA256 生成与 Release 挂载）。
- [`scripts/analyze_build_time.py`](../../scripts/analyze_build_time.py)：基于 Clang `-ftime-trace` 的 C++ 编译耗时微观剖析与火焰图聚合引擎，支持导出 Perfetto / Chrome Tracing 交互式时间线（通过 `./scripts/dev.sh time-trace` 调度）。

相关入口：

- 快速开始：[`quickstart.md`](quickstart.md)
- Agent 协作规则：仓库根目录 [`../../AGENTS.md`](../../AGENTS.md)

后续可按需补充：

- `agent-collaboration.md`（规划中）：后续从 [`../../AGENTS.md`](../../AGENTS.md) 提炼给人类开发者阅读的协作规则摘要。
